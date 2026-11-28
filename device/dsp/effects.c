/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "effects.h"
#include "fastmath.h"
#include "plat.h"

enum { TRACK_DELAY = 12288, MIX_DELAY = 4800, CHORUS = 1024, ROOM = 2048, SLICE_CAPTURE = 640, SLICE_RAMP = 128 };
enum { MAIN_FX, FILTER, SLICER, INSERT, DISPLAY, REVERB, DELAY, CHORUS_FX, DRIVE };

const char *const slap_fx_pages[SLAP_FX_PAGES] = {
    "MAIN FX", "FILTER", "SLICER", "INSERTS", "FX PROC", "REVERB", "DELAY", "CHORUS", "DRIVE"
};
const char *const slap_fx_labels[SLAP_FX_PAGES][4] = {
    {"REVERB", "DELAY", "CHORUS", "DRIVE"},
    {"LOW", "BAND", "HIGH", "RES"},
    {"MODE", "PATTERN", "RATE", "DEPTH"},
    {"TYPE", "AMOUNT", "TONE", "RATE"},
    {"MIX", 0, 0, 0},
    {"WET", "DECAY", "SIZE", "DAMP"},
    {"WET", "TIME", "FEEDBACK", "TONE"},
    {"WET", "DEPTH", "RATE", "TONE"},
    {"WET", "GAIN", "TONE", "SHAPE"}
};
const char *const slap_insert_names[8] = {
    "OFF", "SOFT", "HARD", "FOLD", "FUZZ", "CRUSH", "PHASER", "FLANGER"
};

static const uint16_t slicer_patterns[20] = {
    0xdddd, 0xaaaa, 0xa5a5, 0xe8e8, 0xcaca, 0xf888, 0xf0cc, 0xfaaa, 0x888a, 0x8c8e,
    0xab56, 0x825a, 0xd665, 0x9292, 0xf33c, 0x804f, 0xe28b, 0x876a, 0xfc52, 0x9669
};
static volatile uint32_t settings[SLAP_FX_TRACKS][SLAP_FX_PAGES];
static volatile int8_t traces[SLAP_FX_TRACKS][SLAP_FX_PLOT];
static volatile uint32_t trace_head;

typedef struct {
    float lp, bp, fx_lp, reverb_lp, delay_lp, chorus_lp, drive_lp, crush, phase, gate;
    float allpass[3], live_allpass[3];
    float live_insert[5];
    float coef, damp, low, band, high, dp[4], ch[4], rv[4], dr[4];
    float depth, mix, amount, tone, speed, filter_scale, phase_step;
    uint32_t time, mode, insert, clock_step, gate_sample, block_index;
    uint32_t gate_boundary;
    unsigned gate_step, slice_rate, nominal_clock;
    uint32_t delay_pos, chorus_pos, room_pos, delay_div, crush_count;
    uint32_t revision, bpm, room_short, room_long, live, assigned;
    unsigned tape_mode, tape_length;
    uint32_t tape_controls;
    float tape_age, tape_phase, tape_wet, tape_tone;
    float previous_sample, transition;
    unsigned transition_pending;
    uint16_t pattern;
    unsigned capture_head, capture_count, capture_phase, repeat_phase;
    float capture_sum;
} Fx;
static Fx fx[SLAP_FX_TRACKS + 1];
static int16_t slice_line[SLAP_FX_TRACKS + 1][SLICE_CAPTURE];

// These coefficients stay fixed throughout a callback, including while the
// worker renders or Core 0 rolls back an FX job.
typedef struct {
    float room_damping, room_feedback, echo_damping, echo_feedback;
    float drive_gain, drive_damping, chorus_damping, drive_fold, chorus_depth;
    unsigned live_insert_mask;
} FxCoefficients;
static FxCoefficients coefficients[SLAP_FX_TRACKS + 1];
static volatile uint32_t settings_revision[SLAP_FX_TRACKS];
static PLAT_POOL int16_t echo_line[SLAP_FX_TRACKS][TRACK_DELAY];
static PLAT_POOL int16_t chorus_line[SLAP_FX_TRACKS][CHORUS];
static PLAT_POOL int16_t room_line[SLAP_FX_TRACKS][ROOM];
/* Core 0 owns the post-mixer bus. Its short echo never touches track buffers. */
static PLAT_POOL int16_t mix_echo[MIX_DELAY], mix_side[MIX_DELAY], mix_chorus[CHORUS], mix_room[ROOM];
static float mix_wet;
static unsigned mix_engaged;
static uint32_t previous_live_key;
static uint8_t smooth_controls[4];


#if defined(PLAT_HOST) && !defined(PLAT_WEB)
typedef struct {
    Fx state;
    unsigned track, delay_count, chorus_count, room_count, slice_count;
    int16_t delay[SLAP_FX_BATCH / 4], chorus[SLAP_FX_BATCH], room[SLAP_FX_BATCH];
    int16_t slice[SLAP_FX_BATCH / 4];
    int8_t trace[SLAP_FX_PLOT];
} FxCheckpoint;

static PLAT_POOL FxCheckpoint checkpoints[SLAP_FX_WORKER_TRACKS];

unsigned slap_fx_work(unsigned track)
{
    const Fx *f = &fx[track];
    return (f->live_insert[0] > 0.0f) * 2u + (f->live_insert[1] > 0.0f) * 2u +
           (f->live_insert[2] > 0.0f) * 3u + (f->live_insert[3] > 0.0f) * 4u +
           (f->live_insert[4] > 0.0f) * 5u + (f->tape_mode != 0) * 6u + (f->filter_scale > 0.0f) * 2u + (f->dr[0] > 0.0f) * 3u +
           (f->ch[0] > 0.0f) * 3u + (f->insert != 0) * 3u +
           (f->mode != 0) * 2u + (f->rv[0] > 0.0f) * 4u + (f->dp[0] > 0.0f) * 2u;
}

// Copy only the circular-buffer entries the worker can overwrite in this block.
static void fx_ring_copy(int16_t *saved, int16_t *ring, unsigned length,
                         unsigned pos, unsigned count, int restore)
{
    for (unsigned i = 0; i < count; ++i) {
        if (restore) ring[pos] = saved[i];
        else saved[i] = ring[pos];
        if (++pos == length) pos = 0;
    }
}

static void fx_checkpoint_rings(FxCheckpoint *checkpoint, int restore)
{
    unsigned t = checkpoint->track;
    const Fx *f = &checkpoint->state;
    fx_ring_copy(checkpoint->delay, echo_line[t], TRACK_DELAY, f->delay_pos,
                 checkpoint->delay_count, restore);
    fx_ring_copy(checkpoint->chorus, chorus_line[t], CHORUS, f->chorus_pos,
                 checkpoint->chorus_count, restore);
    fx_ring_copy(checkpoint->room, room_line[t], ROOM, f->room_pos,
                 checkpoint->room_count, restore);
    fx_ring_copy(checkpoint->slice, slice_line[t], SLICE_CAPTURE, f->capture_head,
                 checkpoint->slice_count, restore);
}

void slap_fx_checkpoint(unsigned slot, unsigned track, unsigned frames)
{
    FxCheckpoint *checkpoint = &checkpoints[slot];
    const Fx *f = &fx[track];
    checkpoint->track = track;
    checkpoint->state = *f;
    checkpoint->delay_count = f->dp[0] > 0.0f || f->tape_mode ? (frames + 3u - ((4u - f->delay_div) & 3u)) / 4u : 0u;
    checkpoint->chorus_count = f->ch[0] > 0.0f || f->insert == 7 || f->live_insert[3] > 0.0f ? frames : 0u;
    checkpoint->room_count = f->rv[0] > 0.0f ? frames : 0u;
    checkpoint->slice_count = f->mode == 2u ? (frames + f->capture_phase) / 4u : 0u;
    for (unsigned i = 0; i < SLAP_FX_PLOT; ++i)
        checkpoint->trace[i] = traces[track][i];
    fx_checkpoint_rings(checkpoint, 0);
}

void slap_fx_rollback(unsigned slot)
{
    FxCheckpoint *checkpoint = &checkpoints[slot];
    fx[checkpoint->track] = checkpoint->state;
    for (unsigned i = 0; i < SLAP_FX_PLOT; ++i)
        traces[checkpoint->track][i] = checkpoint->trace[i];
    fx_checkpoint_rings(checkpoint, 1);
}
#endif

static uint32_t bytes(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return a | b << 8 | c << 16 | d << 24;
}
static float unit(unsigned x) { return x * (1.0f / 127.0f); }
static inline __attribute__((always_inline)) float clamp_signal(float x) { return fm_clampf(x, -0.999f, 0.999f); }
static inline __attribute__((always_inline)) int16_t pack(float x) { return (int16_t)(clamp_signal(x) * 32767.0f); }
static inline __attribute__((always_inline)) float unpack(int16_t x) { return x * (1.0f / 32768.0f); }
static inline __attribute__((always_inline)) float soft(float x) { return x / (1.0f + fm_fabsf(x)); }

void slap_fx_init(void)
{
    mix_wet = 0.0f;
    mix_engaged = 0;
    previous_live_key = 0;
    fx[SLAP_FX_TRACKS] = (Fx){.gate = 1.0f};
    for (unsigned t = 0; t < SLAP_FX_TRACKS; ++t) {
        settings[t][FILTER] = bytes(0, 0, 0, 22);
        settings[t][SLICER] = bytes(0, 0, 1, 127);
        settings[t][INSERT] = bytes(0, 48, 68, 52);
        settings[t][DISPLAY] = 100;
        settings[t][REVERB] = bytes(0, 48, 56, 65);
        settings[t][DELAY] = bytes(0, 2, 52, 87);
        settings[t][CHORUS_FX] = bytes(0, 60, 52, 94);
        settings[t][DRIVE] = bytes(0, 54, 108, 39);
        fx[t].gate = 1.0f;
        ++settings_revision[t];
    }
}

unsigned slap_fx_value(unsigned track, unsigned page, unsigned knob)
{
    if (track >= SLAP_FX_TRACKS || page >= SLAP_FX_PAGES || knob >= 4) return 0;
    if (page == MAIN_FX) return settings[track][REVERB + knob] & 255u;
    return (settings[track][page] >> (8 * knob)) & 255u;
}

const char *slap_live_parameter_name(unsigned id)
{
    static const char *const gesture[] = {"AMOUNT", "RATE", "TONE", "SPACE"};
    static const char *const direct[] = {"CUTOFF", "RES", "SOFT", "HARD", "FOLD", "FLANG", "PHASER"};
    if (id < 4u) return gesture[id];
    if (id >= SLAP_LIVE_DIRECT)
        return id < SLAP_LIVE_PARAMETERS ? direct[id - SLAP_LIVE_DIRECT] : 0;
    unsigned slot = id - 4u;
    if (slot >= SLAP_FX_PAGES * 4u || slot / 4u == MAIN_FX) return 0;
    return slap_fx_labels[slot / 4u][slot % 4u];
}

const char *slap_live_parameter_effect(unsigned id)
{
    if (id < 4u) return "LIVE KEY";
    if (id >= SLAP_LIVE_DIRECT) return id < SLAP_LIVE_PARAMETERS ? "LIVE DSP" : 0;
    return slap_live_parameter_name(id) ? slap_fx_pages[(id - 4u) / 4u] : 0;
}

unsigned slap_live_parameter_limit(unsigned id)
{
    unsigned slot = id - 4u;
    unsigned page = slot / 4u, knob = slot % 4u;
    if (id < 4u) return 63u;
    if (page == SLICER && knob < 3u) {
        static const unsigned limits[] = {2, 19, 3};
        return limits[knob];
    }
    if (page == INSERT && knob == 0) return 7;
    if (page == DELAY && knob == 1) return 5;
    return 127;
}

unsigned slap_live_parameter_value(unsigned id, unsigned raw)
{
    return (raw * slap_live_parameter_limit(id) + 31u) / 63u;
}

void slap_fx_change(unsigned track, unsigned page, unsigned knob, int delta)
{
    if (!delta || track >= SLAP_FX_TRACKS || page >= SLAP_FX_PAGES ||
        knob >= 4 || !slap_fx_labels[page][knob]) return;
    if (page == MAIN_FX) { page = REVERB + knob; knob = 0; }
    int cap = 127;
    if (page == INSERT && !knob) cap = 7;
    if (page == SLICER && knob < 3u) {
        static const uint8_t max[] = {2, 19, 3};
        cap = max[knob];
    }
    if (page == DELAY && knob == 1) cap = 5;
    unsigned shift = knob * 8u;
    uint32_t word = settings[track][page];
    unsigned raw = (word >> shift) & 255u;
    unsigned v;
    if (page == SLICER && knob == 3) {
        unsigned percent = (unsigned)clamp((int)((raw * 100u + 63u) / 127u) + delta, 0, 100);
        v = (percent * 127u + 50u) / 100u;
    } else v = (unsigned)clamp((int)raw + delta, 0, cap);
    uint32_t changed = (word & ~(255u << shift)) | v << shift;
    if (changed != word) {
        settings[track][page] = changed;
        ++settings_revision[track];
    }
}

void slap_fx_transport_step(unsigned position, unsigned step_frames, unsigned straight_frames)
{
    for (unsigned track = 0; track <= SLAP_FX_TRACKS; ++track) {
        Fx *f = &fx[track];
        if (!f->mode || (track == SLAP_FX_TRACKS && !mix_engaged)) continue;
        unsigned rate = f->slice_rate;
        if (rate == 0u && (position & 1u)) continue;
        unsigned subdivisions = rate ? 1u << (rate - 1u) : 1u;
        unsigned gate_step = rate ? position * subdivisions : position / 2u;
        f->clock_step = rate ? step_frames / subdivisions : straight_frames * 2u;
        f->gate_step = gate_step & 15u;
        f->gate_sample = f->gate_step * f->clock_step;
        f->gate_boundary = f->gate_sample + f->clock_step;
        f->repeat_phase = f->capture_phase = 0;
        f->capture_sum = 0.0f;
        if (position == 0u) f->capture_count = 0;
    }
}

void slap_fx_begin(unsigned bpm)
{
    slap_fx_begin_live(bpm, 0);
}

static uint32_t fx_word(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return a | b << 8 | c << 16 | d << 24;
}

void slap_fx_begin_live(unsigned bpm, uint32_t live)
{
    slap_fx_begin_assigned(bpm, live, 0x03020100u);
}

void slap_fx_begin_assigned(unsigned bpm, uint32_t live, uint32_t assignments)
{
    static const unsigned slice_lengths[4] = {8, 4, 2, 1};
    static const unsigned echo_eighths[6] = {1, 2, 3, 4, 6, 8};
    uint32_t controls = 0x21332134u;
    unsigned key = live & 127u;
    for (unsigned k = 0; k < 4u; ++k) {
        unsigned id = (assignments >> (8u * k)) & 255u;
        unsigned shift = 7u + k * 6u;
        int raw = (int)(live >> shift & 63u);
        if (key == previous_live_key && key && slap_live_parameter_limit(id) >= 63u)
            raw = clamp(raw, (int)smooth_controls[k] - 8, (int)smooth_controls[k] + 8);
        smooth_controls[k] = (uint8_t)raw;
        live = (live & ~(63u << shift)) | (uint32_t)raw << shift;
    }
    previous_live_key = key;
    for (unsigned k = 0; k < 4u; ++k) {
        unsigned id = (assignments >> (8u * k)) & 255u;
        if (id < 4u) {
            unsigned shift = 8u * id;
            unsigned raw = (live >> (7u + 6u * k)) & 63u;
            controls = (controls & ~(255u << shift)) | raw << shift;
        }
    }
    mix_engaged = (live & 7u) == SLAP_FX_TRACKS && (live >> 3 & 15u);
    for (unsigned t = 0; t <= SLAP_FX_TRACKS; ++t) {
        Fx *f = &fx[t];
        const int master = t == SLAP_FX_TRACKS;
        if (master && !mix_engaged) {
            f->live = 0;
            continue; /* The master crossfade keeps the current effect during release. */
        }
        unsigned revision = master ? 0 : settings_revision[t];
        uint32_t action = (live & 7u) == t ? live >> 3 : 0;
        uint32_t assigned = action ? assignments : 0;
        if (f->revision == revision && f->bpm == bpm && f->live == action &&
            f->assigned == assigned) continue;
        f->assigned = assigned;
        uint32_t previous_action = f->live;
        if (!master && previous_action && (previous_action & 15u) != (action & 15u))
            f->transition_pending = 1;
        f->live = action;
        f->revision = revision;
        f->bpm = bpm;
        uint32_t filter = master ? 0 : settings[t][FILTER], slice = master ? 0 : settings[t][SLICER];
        uint32_t insert = master ? 0 : settings[t][INSERT], delay = master ? 0 : settings[t][DELAY];
        uint32_t room = master ? 0 : settings[t][REVERB], chorus = master ? 0 : settings[t][CHORUS_FX];
        uint32_t mix = master ? 127 : settings[t][DISPLAY];
        uint32_t drive = master ? 0 : settings[t][DRIVE];
        unsigned mode = action & 15u;
        if (mode == 3u || mode == 10u || mode == 11u) {
            if (f->tape_mode != mode || (previous_action & 15u) != mode) {
                f->tape_mode = mode;
                f->tape_age = f->tape_phase = 0.0f;
                unsigned capacity = master ? MIX_DELAY : TRACK_DELAY;
                unsigned step = 44100u * 15u / (4u * bpm);
                f->tape_length = mode == 3u ? step : step / 2u;
                if (f->tape_length > capacity / 2u - 8u && mode == 11u)
                    f->tape_length = capacity / 2u - 8u;
                if (f->tape_length > capacity - 8u) f->tape_length = capacity - 8u;
            }
            f->tape_controls = (action & 15u) | (controls & 63u) << 4 |
                (controls >> 8 & 63u) << 10 | (controls >> 16 & 63u) << 16 |
                (controls >> 24 & 63u) << 22;
        }
        if (mode) {
            unsigned amount = (controls & 63u) * 2u;
            unsigned speed = (controls >> 8 & 63u) * 2u;
            unsigned tone = (controls >> 16 & 63u) * 2u;
            unsigned space = (controls >> 24 & 63u) * 2u;
            switch (mode) {
            case 1: slice = fx_word(1, 1, speed >> 5, amount); break;
            case 2: slice = fx_word(2, 1, speed >> 5, amount); break;
            case 3: break; // Beat Repeat owns its capture independently of the slicer.
            case 4: filter = fx_word(5 + tone / 2, amount, speed / 3, space); break;
            case 5: delay = fx_word(amount, 1 + speed / 32, space / 2, tone); break;
            case 6:
                room = fx_word(amount * 3u / 4u, 69 + space / 3u, 64 + speed / 2u, 52 + tone / 2u);
                chorus = fx_word(amount / 5u, 39, 26, 74);
                break;
            case 7: insert = fx_word(5, amount, tone, speed); mix = amount; break;
            case 8: chorus = fx_word(amount, space, speed, tone); break;
            case 9:
                insert = fx_word(5, amount / 2u, tone, speed / 2u);
                slice = fx_word(2, 12, speed >> 5, amount);
                mix = amount / 2u;
                break;
            }
        }
        unsigned direct_cutoff = 0, direct_res = 0;
        for (unsigned i = 0; i < 5u; ++i) f->live_insert[i] = 0.0f;
        if (mode) {
            uint32_t *words[SLAP_FX_PAGES] = {0, &filter, &slice, &insert, &mix, &room, &delay, &chorus, &drive};
            for (unsigned k = 0; k < 4u; ++k) {
                unsigned id = assigned >> (8u * k) & 255u;
                if (id < 4u || !slap_live_parameter_name(id)) continue;
                unsigned value = slap_live_parameter_value(id, live >> (7u + 6u * k) & 63u);
                if (id >= SLAP_LIVE_DIRECT) {
                    unsigned control = id - SLAP_LIVE_DIRECT;
                    if (control == 0u) direct_cutoff = 1u + value;
                    else if (control == 1u) direct_res = 1u + value;
                    else f->live_insert[control - 2u] = unit(value);
                    continue;
                }
                unsigned slot = id - 4u;
                unsigned shift = 8u * (slot % 4u);
                uint32_t *word = words[slot / 4u];
                *word = (*word & ~(255u << shift)) | value << shift;
            }
        }
        f->low = unit(filter & 255u);
        f->band = unit((filter >> 8) & 255u);
        f->high = unit((filter >> 16) & 255u);
        float total = f->low + f->band + f->high;
        f->filter_scale = total > 0.0f ? 1.0f / total : 0.0f;
        f->damp = 1.55f - 1.34f * unit(filter >> 24);
        f->coef = 0.025f + 0.34f * (0.15f + f->low + f->band * .55f + f->high * .33f) / 2.1f;
        if (direct_cutoff || direct_res) {
            f->low = 1.0f;
            f->band = f->high = 0.0f;
            f->filter_scale = 1.0f;
            if (direct_cutoff) f->coef = 0.018f * fm_exp2f(unit(direct_cutoff - 1u) * 4.9f);
            if (direct_res) f->damp = 1.55f - 1.34f * unit(direct_res - 1u);
        }
        unsigned slice_mode = slice & 255u;
        uint16_t pattern = slicer_patterns[(slice >> 8) & 255u];
        unsigned rate = (slice >> 16) & 255u;
        unsigned clock_step = 44100u * 60u / bpm * slice_lengths[rate] / 16u;
        if (f->nominal_clock != clock_step || f->mode != slice_mode || f->pattern != pattern) {
            if (!master && f->mode) f->transition_pending = 1;
            f->clock_step = f->nominal_clock = clock_step;
            f->gate_sample = f->gate_step = 0;
            f->gate_boundary = clock_step;
            f->capture_head = f->capture_count = f->capture_phase = f->repeat_phase = 0;
            f->capture_sum = 0.0f;
            f->gate = 1.0f;
        }
        f->mode = slice_mode;
        f->pattern = pattern;
        f->slice_rate = rate;
        f->depth = unit(slice >> 24);
        f->insert = insert & 255u;
        f->amount = unit((insert >> 8) & 255u);
        f->tone = unit((insert >> 16) & 255u);
        f->speed = unit(insert >> 24);
        f->mix = unit(mix & 255u);
        f->time = 11025u * 60u / bpm * echo_eighths[(delay >> 8) & 255u] / 8u;
        unsigned delay_capacity = master ? MIX_DELAY : TRACK_DELAY;
        if (f->time >= delay_capacity) f->time = delay_capacity - 1;
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k * 8u;
            f->dp[k] = unit((delay >> shift) & 255u);
            f->ch[k] = unit((chorus >> shift) & 255u);
            f->rv[k] = unit((room >> shift) & 255u);
            f->dr[k] = unit((drive >> shift) & 255u);
        }
        coefficients[t] = (FxCoefficients) {
            .room_damping = 0.12f + 0.58f * (1.0f - f->rv[3]),
            .room_feedback = 0.31f + 0.63f * f->rv[1],
            .echo_damping = 0.11f + f->dp[3] * 0.78f,
            .echo_feedback = 0.12f + 0.80f * f->dp[2],
            .drive_gain = 1.0f + 16.0f * f->dr[1],
            .drive_damping = 0.009f + f->dr[2] * 0.9f,
            .chorus_damping = 0.025f + 0.7f * f->ch[3],
            .drive_fold = 0.32f * f->dr[3],
            .chorus_depth = 0.1f + 0.9f * f->ch[1],
            .live_insert_mask = (f->live_insert[0] > 0.0f) | ((f->live_insert[1] > 0.0f) << 1) |
                ((f->live_insert[2] > 0.0f) << 2) | ((f->live_insert[3] > 0.0f) << 3) |
                ((f->live_insert[4] > 0.0f) << 4)
        };
        f->phase_step = (0.18f + 3.5f * fm_maxf(f->ch[2], f->speed)) / 44100.0f;
        f->room_long = 300u + (unsigned)(f->rv[2] * 1100.0f);
        f->room_short = f->room_long * 11u / 17u;
    }
}

static inline __attribute__((always_inline)) unsigned preceding(unsigned pos, unsigned n, unsigned length)
{
    return pos >= n ? pos - n : pos + length - n;
}

static inline __attribute__((always_inline)) float fold(float x)
{
    float y = (x + 1.0f) * 0.25f;
    y -= fm_floorf(y);
    return 1.0f - 4.0f * fm_fabsf(y - 0.5f);
}

static inline __attribute__((always_inline)) float insert_sample(Fx *f, float x, float lfo, float flange)
{
    float drive = 1.0f + f->amount * 12.0f;
    float out = x;
    switch (f->insert) {
    case 1: out = 0.55f * soft(drive * x); break;
    case 2: out = 0.38f * fm_clampf(drive * x, -1.0f, 1.0f); break;
    case 3: out = 0.42f * fold(drive * x); break;
    case 4: out = 0.53f * soft(drive * x + 4.0f*drive*x*fm_fabsf(x)); break;
    case 5:
        if (!f->crush_count) {
            f->crush_count = 1u + (unsigned)(28.0f * f->speed);
            float levels = 10.0f + 100.0f * (1.0f - f->amount);
            f->crush = fm_floorf(x * levels + 0.5f) / levels;
        }
        --f->crush_count;
        out = f->crush;
        break;
    case 6: {
        out += f->allpass[2] * (0.1f + 0.64f * f->amount);
        float a = 0.07f + lfo * (0.35f + 0.43f * f->tone);
        for (unsigned k = 0; k < 3; ++k) {
            float v = -a * out + f->allpass[k];
            f->allpass[k] = out + a * v;
            out = v;
        }
        out = 0.55f * (x + out);
        break;
    }
    case 7: out = 0.56f * (x + flange * (0.35f + f->amount * 0.65f)); break;
    default: break;
    }
    f->fx_lp += (out - f->fx_lp) * (0.013f + 0.92f*f->tone);
    return x + (f->fx_lp - x) * f->mix;
}

unsigned slap_fx_active_mask(void)
{
    unsigned mask = 0;
    for (unsigned track = 0; track < SLAP_FX_TRACKS; ++track) {
        const Fx *f = &fx[track];
        if (f->transition_pending || f->transition != 0.0f ||
            (f->live_insert[0] + f->live_insert[1] + f->live_insert[2] + f->live_insert[3] + f->live_insert[4]) > 0.0f ||
            f->tape_mode || f->filter_scale > 0.0f || f->dr[0] > 0.0f || f->ch[0] > 0.0f ||
            f->insert || f->mode || f->rv[0] > 0.0f || f->dp[0] > 0.0f)
            mask |= 1u << track;
    }
    return mask;
}

static inline __attribute__((always_inline)) float tape_read(const int16_t *ring, unsigned capacity, unsigned pos, float delay)
{
    unsigned index = (unsigned)delay;
    float a = unpack(ring[preceding(pos, index, capacity)]);
    float b = unpack(ring[preceding(pos, index + 1u, capacity)]);
    return a + (b - a) * (delay - index);
}

static inline __attribute__((always_inline)) float tape_sample(Fx *f, int16_t *echo, unsigned capacity, float dry, float *side)
{
    unsigned mode = f->live & 15u;
    int held = mode == f->tape_mode;
    float amount = (f->tape_controls >> 4 & 63u) * (1.0f / 63.0f);
    float rate = (f->tape_controls >> 10 & 63u) * (1.0f / 63.0f);
    float tone = (f->tape_controls >> 16 & 63u) * (1.0f / 63.0f);
    int captured = f->tape_age >= (float)f->tape_length;
    int engage = held && (mode != 3u || captured);
    f->tape_wet += ((engage ? (mode == 10u ? 1.0f : amount) : 0.0f) - f->tape_wet) * 0.009f;
    unsigned pos = f->delay_pos;
    unsigned write_phase = f->delay_div++ & 3u;
    if (write_phase == 0u && (f->tape_mode != 3u || !captured)) {
        echo[pos] = pack(dry);
        if (side && f->tape_mode == 10u) mix_side[pos] = pack(*side);
        if (++pos == capacity) pos = 0;
        f->delay_pos = pos;
    }
    float wet;
    if (f->tape_mode == 10u) {
        // The read head starts at normal speed and coasts to zero.
        // Its maximum drift fits inside the shortest tape ring.
        if (held && f->tape_age < 1.0f) {
            f->tape_age = fm_minf(1.0f, f->tape_age + (0.000045f + 0.000018f * rate));
            float progress = f->tape_age;
            f->tape_phase += 0.25f * progress * (2.0f - progress);
        }
        float speed = 1.0f - f->tape_age;
        float gain = fm_minf(1.0f, speed * (3.0f + 4.0f * amount));
        float delay = f->tape_phase + 2.0f - 0.25f * write_phase;
        wet = gain > 0.0f ? tape_read(echo, capacity, pos, delay) * gain : 0.0f;
        if (side) {
            float side_wet = gain > 0.0f ? tape_read(mix_side, capacity, pos, delay) * gain : 0.0f;
            *side += (side_wet - *side) * f->tape_wet;
        }
    } else if (f->tape_mode == 3u) {
        if (f->tape_age < (float)f->tape_length) {
            f->tape_age += 0.25f;
            wet = dry;
        } else {
            float length = (float)f->tape_length;
            float phase = f->tape_phase;
            f->tape_phase += 0.25f;
            if (f->tape_phase >= length) f->tape_phase -= length;
            float sample = tape_read(echo, capacity, pos, fm_maxf(1.0f, length - phase));
            float window = fm_minf(1.0f, fm_minf(phase, length - phase) * (1.0f / 20.0f));
            wet = dry + (sample - dry) * window;
        }
    } else {
        float length = (float)f->tape_length;
        float ready = 2.0f * length + 4.0f;
        if (f->tape_age < ready) f->tape_age += 0.25f;
        if (f->tape_age < ready) {
            wet = dry;
        } else {
            float phase = f->tape_phase;
            // The write head advances by 1/4 ring sample per output sample.
            // Increasing the delay by twice that advances the read head backwards at unit speed.
            f->tape_phase += 0.25f;
            if (f->tape_phase >= length) f->tape_phase -= length;
            float window = fm_minf(1.0f, fm_minf(phase, length - phase) * (1.0f / 32.0f));
            float reverse = tape_read(echo, capacity, pos, 2.0f + 2.0f * phase - 0.25f * write_phase);
            wet = dry + (reverse - dry) * window;
        }
    }
    if ((f->tape_mode == 11u && f->tape_age < 2.0f * (float)f->tape_length + 4.0f) ||
        (f->tape_mode == 3u && !captured))
        f->tape_tone = dry;
    else f->tape_tone += (wet - f->tape_tone) * (0.05f + 0.80f * tone);
    if (!held && f->tape_wet < 0.00005f) f->tape_mode = 0;
    return dry + (f->tape_tone - dry) * f->tape_wet;
}

static inline __attribute__((always_inline)) float process_sample(
    unsigned track, float dry, int16_t *echo, unsigned capacity, int16_t *chorus, int16_t *room, float *side)
{
    Fx *f = &fx[track];
    const FxCoefficients *c = &coefficients[track];
    float x = dry;
    if (f->filter_scale > 0.0f) {
        float hp = x - f->lp - f->damp * f->bp;
        f->bp += f->coef * hp;
        f->lp += f->coef * f->bp;
        x += (f->low*(f->lp-x) + f->band*(f->bp-x) + f->high*(hp-x)) * f->filter_scale;
        x = fm_clampf(x, -0.9f, 0.9f);
    }
    if (f->dr[0] > 0.0f) {
        float driven = x * c->drive_gain;
        driven = 0.68f * soft(driven) + c->drive_fold * fold(driven);
        f->drive_lp += (driven - f->drive_lp) * c->drive_damping;
        x += f->dr[0] * (f->drive_lp - x);
    }
    const unsigned live = c->live_insert_mask;
    float soft_amount = f->live_insert[0], hard_amount = f->live_insert[1], fold_amount = f->live_insert[2];
    if (live & 1u) x += soft_amount * (0.55f * soft(x * (1.0f + 12.0f * soft_amount)) - x);
    if (live & 2u) x += hard_amount * (0.45f * fm_clampf(x * (1.0f + 15.0f * hard_amount), -1.0f, 1.0f) - x);
    if (live & 4u) x += fold_amount * (0.48f * fold(x * (1.0f + 23.0f * fold_amount)) - x);
    if (f->ch[0] > 0.0f || f->insert || (live & 24u)) {
        f->phase += f->phase_step;
        if (f->phase >= 1.0f) f->phase -= 1.0f;
        float lfo = 1.0f - 2.0f * fm_fabsf(f->phase - 0.5f);
        if (f->ch[0] > 0.0f || f->insert == 7 || (live & 8u)) {
            unsigned pos = f->chorus_pos;
            unsigned tap = 12u + (unsigned)(lfo * (f->insert == 7 ? 105.0f : 500.0f) *
                            c->chorus_depth);
            float ch = unpack(chorus[(pos - tap) & (CHORUS - 1u)]);
            if ((live & 8u)) {
                unsigned flange_tap = 5u + (unsigned)(lfo * 95.0f);
                float flange = unpack(chorus[(pos - flange_tap) & (CHORUS - 1u)]);
                chorus[pos] = pack(x + flange * 0.23f * f->live_insert[3]);
                x += (0.55f * flange - 0.22f * x) * f->live_insert[3];
            } else chorus[pos] = pack(x);
            f->chorus_pos = (pos + 1u) & (CHORUS - 1u);
            if (f->insert) x = insert_sample(f, x, lfo, ch);
            f->chorus_lp += (ch - f->chorus_lp) * c->chorus_damping;
            x += 0.7f * f->ch[0] * f->chorus_lp;
        } else {
            x = insert_sample(f, x, lfo, 0.0f);
        }
    }
    if (live & 16u) {
        float sweep = 0.12f + 0.72f * (1.0f - 2.0f * fm_fabsf(f->phase - 0.5f));
        float signal = x + 0.38f * f->live_allpass[2];
        for (unsigned stage = 0; stage < 3u; ++stage) {
            float next = f->live_allpass[stage] - sweep * signal;
            f->live_allpass[stage] = signal + sweep * next;
            signal = next;
        }
        x += (signal - x) * (0.5f * f->live_insert[4]);
    }
    if (f->mode) {
        unsigned remaining = f->gate_boundary - f->gate_sample;
        int open = (f->pattern >> (15u - f->gate_step)) & 1u;
        if (f->mode == 2) {
            if (open) {
                f->capture_sum += x;
                if (++f->capture_phase == 4u) {
                    slice_line[track][f->capture_head] = pack(f->capture_sum * 0.25f);
                    if (++f->capture_head == SLICE_CAPTURE) f->capture_head = 0;
                    if (f->capture_count < SLICE_CAPTURE) ++f->capture_count;
                    f->capture_sum = 0.0f;
                    f->capture_phase = 0;
                }
            } else if (f->capture_count >= 2u * SLICE_RAMP / 4u) {
                unsigned phase = f->repeat_phase;
                unsigned length = (f->capture_count - 1u) * 4u;
                unsigned index = f->capture_head + SLICE_CAPTURE - f->capture_count + phase / 4u;
                if (index >= SLICE_CAPTURE) index -= SLICE_CAPTURE;
                float repeated = unpack(slice_line[track][index]);
                unsigned fraction = phase & 3u;
                if (fraction) {
                    unsigned next = index + 1u == SLICE_CAPTURE ? 0u : index + 1u;
                    float b = unpack(slice_line[track][next]);
                    repeated += (b - repeated) * (fraction * 0.25f);
                }
                unsigned fade = phase;
                if (length - phase < fade) fade = length - phase;
                if (remaining - 1u < fade) fade = remaining - 1u;
                float window = fade >= SLICE_RAMP ? 1.0f : fade * (1.0f / SLICE_RAMP);
                x += (repeated - x) * f->depth * window;
                if (++f->repeat_phase == length) f->repeat_phase = 0;
            }
        } else {
            f->gate += ((open ? 1.0f : 1.0f - f->depth) - f->gate) * 0.032f;
            x *= f->gate;
        }
        if (++f->gate_sample == f->gate_boundary) {
            if (++f->gate_step == 16u) {
                f->gate_step = f->gate_sample = 0;
                f->gate_boundary = 0;
            }
            f->gate_boundary += f->clock_step;
            f->repeat_phase = f->capture_phase = 0;
            f->capture_sum = 0.0f;
        }
    }
    if (f->rv[0] > 0.0f) {
        unsigned idx = f->room_pos;
        float a = unpack(room[(idx - f->room_long) & (ROOM - 1u)]);
        float b = unpack(room[(idx - f->room_short) & (ROOM - 1u)]);
        f->reverb_lp += ((a * 0.61f + b * 0.39f) - f->reverb_lp) * c->room_damping;
        room[idx] = pack(x * 0.58f + f->reverb_lp * c->room_feedback);
        f->room_pos = (idx + 1u) & (ROOM - 1u);
        x += f->rv[0] * f->reverb_lp;
    }
    if (f->tape_mode) x = tape_sample(f, echo, capacity, x, side);
    else if (f->dp[0] > 0.0f) {
        if ((f->delay_div++ & 3u) == 0) {
            unsigned pos = f->delay_pos;
            float wet = unpack(echo[preceding(pos, f->time, capacity)]);
            f->delay_lp += (wet - f->delay_lp) * c->echo_damping;
            echo[pos] = pack(x + f->delay_lp * c->echo_feedback);
            if (++pos == capacity) pos = 0;
            f->delay_pos = pos;
        }
        x += f->dp[0] * f->delay_lp;
    }
#ifdef PLAT_HOST
    if (track < SLAP_FX_TRACKS && (f->block_index++ & 63u) == 0) {
        unsigned point = (f->block_index >> 6) % SLAP_FX_PLOT;
        traces[track][point] = (int8_t)(clamp_signal(x * 2.0f) * 55.0f);
        if (!track) trace_head = point;
    }
#endif
    return x;
}

int slap_fx_trace(unsigned track, unsigned point)
{
    return traces[track][(trace_head + point + 1u) % SLAP_FX_PLOT];
}

#ifndef PLAT_HOST
__attribute__((section(".c1_text.slap_fx"), noinline))
#endif
float slap_fx_sample(unsigned track, float dry)
{
    Fx *f = &fx[track];
    float output = process_sample(track, dry, echo_line[track], TRACK_DELAY,
                                  chorus_line[track], room_line[track], 0);
    if (f->transition_pending) {
        f->transition_pending = 0;
        f->transition = f->previous_sample - output;
    }
    if (f->transition != 0.0f) {
        output += f->transition;
        f->transition *= 0.996f;
        if (fm_fabsf(f->transition) < 0.00002f) f->transition = 0.0f;
    }
    f->previous_sample = output;
    return output;
}

unsigned slap_fx_brake_speed(unsigned track)
{
    if (track > SLAP_FX_TRACKS || fx[track].tape_mode != 10u) return 100u;
    return (unsigned)((1.0f - fx[track].tape_age) * 100.0f);
}

unsigned slap_fx_mix_active(void)
{
    return mix_engaged || mix_wet > 0.0f;
}

void slap_fx_mix_sample(float *left, float *right)
{
    float target = mix_engaged ? 1.0f : 0.0f;
    mix_wet += (target - mix_wet) * 0.004f;
    if (!mix_engaged && mix_wet < 0.00005f) {
        mix_wet = 0.0f;
        return;
    }
    float middle = (*left + *right) * 0.5f;
    float side = (*left - *right) * 0.5f;
    int stereo_stop = fx[SLAP_FX_TRACKS].tape_mode == 10u;
    float wet = process_sample(SLAP_FX_TRACKS, middle, mix_echo, MIX_DELAY, mix_chorus, mix_room,
        stereo_stop ? &side : 0);
    *left += (wet + (stereo_stop ? side : 0.0f) - *left) * mix_wet;
    *right += (wet - (stereo_stop ? side : 0.0f) - *right) * mix_wet;
}

unsigned slap_fx_slicer_state(unsigned track)
{
    if (track >= SLAP_FX_TRACKS) return 0;
    const volatile Fx *current = &fx[track];
    unsigned mode = current->mode;
    unsigned step = current->gate_step & 15u;
    unsigned open = current->pattern >> (15u - step) & 1u;
    unsigned captured = current->capture_count;
    return step | (mode != 0) << 4 | open << 5 |
        (mode == 2u && open) << 6 | (mode == 2u && !open && captured >= 2u * SLICE_RAMP / 4u) << 7;
}

void slap_fx_slicer_preview(unsigned track, SlapSlicePreview *preview)
{
    const volatile Fx *current = &fx[track];
    unsigned head = current->capture_head;
    unsigned count = current->mode == 2u ? current->capture_count : 0;
    unsigned phase = current->repeat_phase;
    unsigned sample = current->gate_sample;
    unsigned boundary = current->gate_boundary;
    unsigned clock = current->clock_step;
    unsigned remaining = boundary >= sample ? boundary - sample : clock;
    preview->slice_position = clock && remaining <= clock ? (clock - remaining) * 13u / clock : 0;
    preview->captured = count * SLAP_SLICE_PREVIEW / SLICE_CAPTURE;
    preview->replay = count > 1u ? (phase / 4u % count) * SLAP_SLICE_PREVIEW / SLICE_CAPTURE : 0;
    const volatile int16_t *ring = slice_line[track];
    unsigned start = head + SLICE_CAPTURE - count;
    for (unsigned point = 0; point < SLAP_SLICE_PREVIEW; ++point) {
        unsigned offset = point * SLICE_CAPTURE / SLAP_SLICE_PREVIEW;
        unsigned index = start + offset;
        if (index >= SLICE_CAPTURE) index -= SLICE_CAPTURE;
        preview->wave[point] = offset < count ? (int8_t)(ring[index] / 256) : 0;
    }
}

unsigned slap_fx_slicer_cell(unsigned track, unsigned step)
{
    return (slicer_patterns[slap_fx_value(track, SLICER, 1)] >> (15u - (step & 15u))) & 1u;
}

int slap_fx_response(unsigned track, unsigned point)
{
    /* Evaluate the discrete state-variable filter at z = exp(jw). */
    float low = unit(slap_fx_value(track,FILTER,0)), band = unit(slap_fx_value(track,FILTER,1));
    float high = unit(slap_fx_value(track,FILTER,2)), res = unit(slap_fx_value(track,FILTER,3));
    float total = low + band + high;
    if (!total) return 61;
    float k = 0.025f + 0.34f*(0.15f + low + band*0.55f + high*0.33f)/2.1f;
    float damping = 1.55f - 1.34f * res;
    float w = 0.004f * fm_exp2f(point * (9.26f / (SLAP_FX_PLOT - 1u)));
    float zr = fm_cosf(w) - 1.0f, zi = fm_sinf(w);
    float z2r = zr*zr - zi*zi, z2i = 2.0f*zr*zi;
    float lp_re = k*k*(zr + 1.0f), lp_im = k*k*zi;
    float b_re = k*zr, b_im = k*zi;
    float den_re = z2r + damping*b_re + lp_re;
    float den_im = z2i + damping*b_im + lp_im;
    float num_re = low*lp_re + band*b_re + high*z2r;
    float num_im = low*lp_im + band*b_im + high*z2i;
    float mag2 = (num_re*num_re + num_im*num_im) /
                 ((den_re*den_re + den_im*den_im) * total * total + 0.00000000001f);
    return clamp((int)(61.0f - 5.7f*fm_log2f(fm_maxf(0.0001f, mag2))), 7, 102);
}

uint32_t slap_fx_config(unsigned track, unsigned page) { return settings[track][page]; }

int slap_fx_config_valid(unsigned page, uint32_t word)
{
    if (word & 0x80808080u) return 0;
    if (page == SLICER && ((word & 255u) > 2u ||
        ((word >> 8) & 255u) > 19u || ((word >> 16) & 255u) > 3u)) return 0;
    if (page == INSERT && (word & 255u) > 7u) return 0;
    if (page == DELAY && ((word >> 8) & 255u) > 5u) return 0;
    return 1;
}

void slap_fx_restore(unsigned track, unsigned page, uint32_t word)
{
    if (settings[track][page] != word) {
        settings[track][page] = word;
        ++settings_revision[track];
    }
}
