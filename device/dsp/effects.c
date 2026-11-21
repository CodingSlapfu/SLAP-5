/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "effects.h"
#include "fastmath.h"
#include "plat.h"

enum { TRACK_DELAY = 12288, MIX_DELAY = 3072, CHORUS = 1024, ROOM = 2048 };
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
    float allpass[3];
    float coef, damp, low, band, high, dp[4], ch[4], rv[4], dr[4];
    float depth, mix, amount, tone, speed, filter_scale, phase_step;
    uint32_t time, mode, insert, clock_step, gate_sample, block_index;
    uint32_t gate_boundary;
    unsigned gate_step;
    uint32_t delay_pos, chorus_pos, room_pos, delay_div, crush_count;
    uint32_t revision, bpm, room_short, room_long, live, assigned;
    unsigned tape_mode;
    uint32_t tape_controls;
    float tape_age, tape_phase, tape_wet, tape_tone;
    float previous_sample, transition;
    unsigned transition_pending;
    uint16_t pattern;
    int16_t slice[256];
} Fx;
static Fx fx[SLAP_FX_TRACKS + 1];

// These coefficients stay fixed throughout a callback, including while the
// worker renders or Core 0 rolls back an FX job.
typedef struct {
    float room_damping, room_feedback, echo_damping, echo_feedback;
    float drive_gain, drive_damping, chorus_damping;
} FxCoefficients;
static FxCoefficients coefficients[SLAP_FX_TRACKS + 1];
static volatile uint32_t settings_revision[SLAP_FX_TRACKS];
static PLAT_POOL int16_t echo_line[SLAP_FX_TRACKS][TRACK_DELAY];
static PLAT_POOL int16_t chorus_line[SLAP_FX_TRACKS][CHORUS];
static PLAT_POOL int16_t room_line[SLAP_FX_TRACKS][ROOM];
/* Core 0 owns the post-mixer bus. Its short echo never touches track buffers. */
static PLAT_POOL int16_t mix_echo[MIX_DELAY], mix_chorus[CHORUS], mix_room[ROOM];
static float mix_wet;
static unsigned mix_engaged;
static uint32_t previous_live_key;
static uint8_t smooth_controls[4];


#if !defined(PLAT_WEB)
typedef struct {
    Fx state;
    unsigned track, delay_count, chorus_count, room_count;
    int16_t delay[SLAP_FX_BATCH / 4], chorus[SLAP_FX_BATCH], room[SLAP_FX_BATCH];
    int8_t trace[SLAP_FX_PLOT];
} FxCheckpoint;

static PLAT_POOL FxCheckpoint checkpoints[SLAP_FX_WORKER_TRACKS];

unsigned slap_fx_work(unsigned track)
{
    const Fx *f = &fx[track];
    return (f->tape_mode != 0) * 6u + (f->filter_scale > 0.0f) * 2u + (f->dr[0] > 0.0f) * 3u +
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
}

void slap_fx_checkpoint(unsigned slot, unsigned track, unsigned frames)
{
    FxCheckpoint *checkpoint = &checkpoints[slot];
    const Fx *f = &fx[track];
    checkpoint->track = track;
    checkpoint->state = *f;
    checkpoint->delay_count = f->dp[0] > 0.0f || f->tape_mode ? (frames + 3u - ((4u - f->delay_div) & 3u)) / 4u : 0u;
    checkpoint->chorus_count = f->ch[0] > 0.0f || f->insert == 7 ? frames : 0u;
    checkpoint->room_count = f->rv[0] > 0.0f ? frames : 0u;
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
    if (id < 4u) return gesture[id];
    unsigned slot = id - 4u;
    if (slot >= SLAP_FX_PAGES * 4u || slot / 4u == MAIN_FX) return 0;
    return slap_fx_labels[slot / 4u][slot % 4u];
}

const char *slap_live_parameter_effect(unsigned id)
{
    if (id < 4u) return "LIVE KEY";
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
    int cap = 127, stride = 2;
    if (page == INSERT && !knob) { cap = 7; stride = 1; }
    if (page == SLICER && knob < 3u) {
        static const uint8_t max[] = {2, 19, 3};
        cap = max[knob];
        stride = 1;
    }
    if (page == DELAY && knob == 1) { cap = 5; stride = 1; }
    unsigned shift = knob * 8u;
    uint32_t word = settings[track][page];
    unsigned v = (unsigned)clamp((int)((word >> shift) & 255u) + delta * stride, 0, cap);
    uint32_t changed = (word & ~(255u << shift)) | v << shift;
    if (changed != word) {
        settings[track][page] = changed;
        ++settings_revision[track];
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
        if (master && !mix_engaged) continue; /* Keep the current effect during release. */
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
        if (mode == 10u || mode == 11u) {
            if (f->tape_mode != mode || (previous_action & 15u) != mode) {
                f->tape_mode = mode;
                f->tape_age = f->tape_phase = 0.0f;
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
            case 3: slice = fx_word(2, 4, speed >> 5, amount); break;
            case 4: filter = fx_word(5 + tone / 2, amount, speed / 3, space); break;
            case 5: delay = fx_word(amount, 1 + speed / 32, space / 2, tone); break;
            case 6: room = fx_word(amount, 40 + space / 2, speed, tone); break;
            case 7: insert = fx_word(5, amount, tone, speed); mix = 127; break;
            case 8: chorus = fx_word(amount, space, speed, tone); break;
            case 9:
                insert = fx_word(5, amount, tone, speed);
                slice = fx_word(2, 12, speed >> 5, amount);
                mix = 127;
                break;
            }
        }
        if (mode) {
            uint32_t *words[SLAP_FX_PAGES] = {0, &filter, &slice, &insert, &mix, &room, &delay, &chorus, &drive};
            for (unsigned k = 0; k < 4u; ++k) {
                unsigned id = assigned >> (8u * k) & 255u;
                if (id < 4u || !slap_live_parameter_name(id)) continue;
                unsigned slot = id - 4u;
                unsigned shift = 8u * (slot % 4u);
                unsigned value = slap_live_parameter_value(id, live >> (7u + 6u * k) & 63u);
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
        f->mode = slice & 255u;
        f->pattern = slicer_patterns[(slice >> 8) & 255u];
        unsigned clock_step = 44100u * 60u / bpm * slice_lengths[(slice >> 16) & 255u] / 16u;
        if (f->clock_step != clock_step) {
            f->clock_step = clock_step;
            f->gate_step = (f->gate_sample / clock_step) & 15u;
            f->gate_boundary = (f->gate_step + 1u) * clock_step;
        }
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
            .chorus_damping = 0.025f + 0.7f * f->ch[3]
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

static inline __attribute__((always_inline)) float tape_sample(Fx *f, int16_t *echo, unsigned capacity, float dry)
{
    unsigned mode = f->live & 15u;
    int held = mode == f->tape_mode;
    float amount = (f->tape_controls >> 4 & 63u) * (1.0f / 63.0f);
    float rate = (f->tape_controls >> 10 & 63u) * (1.0f / 63.0f);
    float tone = (f->tape_controls >> 16 & 63u) * (1.0f / 63.0f);
    float space = (f->tape_controls >> 22 & 63u) * (1.0f / 63.0f);
    f->tape_wet += ((held ? (mode == 10u ? 1.0f : amount) : 0.0f) - f->tape_wet) * 0.009f;
    unsigned pos = f->delay_pos;
    if ((f->delay_div++ & 3u) == 0) {
        echo[pos] = pack(dry);
        if (++pos == capacity) pos = 0;
        f->delay_pos = pos;
    }
    float wet;
    if (f->tape_mode == 10u) {
        if (held) {
            f->tape_age = fm_minf(1.0f, f->tape_age + (0.00007f + 0.00031f * rate));
            f->tape_phase = fm_minf(10000.0f, f->tape_phase + 0.25f * f->tape_age);
        }
        float delay = fm_minf(f->tape_phase + 2.0f, (float)(capacity - 2u));
        wet = tape_read(echo, capacity, pos, delay) * (1.0f - f->tape_age);
    } else {
        float length = 256.0f + 512.0f * space;
        if (held) {
            f->tape_phase += (0.025f + 0.16f * amount) / length;
            if (f->tape_phase >= 1.0f) f->tape_phase -= 1.0f;
        }
        float phase = f->tape_phase;
        float second = phase < 0.5f ? phase + 0.5f : phase - 0.5f;
        float weight = 1.0f - fm_fabsf(2.0f * phase - 1.0f);
        wet = weight * tape_read(echo, capacity, pos, 2.0f + length * phase) +
              (1.0f - weight) * tape_read(echo, capacity, pos, 2.0f + length * second);
    }
    f->tape_tone += (wet - f->tape_tone) * (0.05f + 0.80f * tone);
    if (!held && f->tape_wet < 0.00005f) f->tape_mode = 0;
    return dry + (f->tape_tone - dry) * f->tape_wet;
}

static inline __attribute__((always_inline)) float process_sample(
    unsigned track, float dry, int16_t *echo, unsigned capacity, int16_t *chorus, int16_t *room)
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
        driven = 0.68f * soft(driven) + 0.32f * f->dr[3] * fold(driven);
        f->drive_lp += (driven - f->drive_lp) * c->drive_damping;
        x += f->dr[0] * (f->drive_lp - x);
    }
    if (f->ch[0] > 0.0f || f->insert) {
        f->phase += f->phase_step;
        if (f->phase >= 1.0f) f->phase -= 1.0f;
        float lfo = 1.0f - 2.0f * fm_fabsf(f->phase - 0.5f);
        if (f->ch[0] > 0.0f || f->insert == 7) {
            unsigned pos = f->chorus_pos;
            unsigned tap = 12u + (unsigned)(lfo * (f->insert == 7 ? 105.0f : 500.0f) *
                            (0.1f + 0.9f*f->ch[1]));
            float ch = unpack(chorus[preceding(pos, tap, CHORUS)]);
            chorus[pos] = pack(x);
            f->chorus_pos = (pos + 1u) & (CHORUS - 1u);
            if (f->insert) x = insert_sample(f, x, lfo, ch);
            f->chorus_lp += (ch - f->chorus_lp) * c->chorus_damping;
            x += 0.7f * f->ch[0] * f->chorus_lp;
        } else {
            x = insert_sample(f, x, lfo, 0.0f);
        }
    }
    if (f->mode) {
        unsigned in_slice = f->gate_sample & 255u;
        int open = (f->pattern >> (15u - f->gate_step)) & 1u;
        ++f->gate_sample;
        if (f->gate_sample >= f->clock_step * 16u) {
            f->gate_sample = 0;
            f->gate_step = 0;
            f->gate_boundary = f->clock_step;
        } else if (f->gate_sample == f->gate_boundary) {
            ++f->gate_step;
            f->gate_boundary += f->clock_step;
        }
        f->gate += ((open ? 1.0f : 1.0f - f->depth) - f->gate) * 0.032f;
        if (f->mode == 2) {
            if (open) f->slice[in_slice] = pack(x);
            else x += (unpack(f->slice[in_slice]) - x) * f->depth;
        } else x *= f->gate;
    }
    if (f->rv[0] > 0.0f) {
        unsigned idx = f->room_pos;
        float a = unpack(room[preceding(idx, f->room_long, ROOM)]);
        float b = unpack(room[preceding(idx, f->room_short, ROOM)]);
        f->reverb_lp += ((a * 0.61f + b * 0.39f) - f->reverb_lp) * c->room_damping;
        room[idx] = pack(x * 0.58f + f->reverb_lp * c->room_feedback);
        f->room_pos = (idx + 1u) & (ROOM - 1u);
        x += f->rv[0] * f->reverb_lp;
    }
    if (f->tape_mode) x = tape_sample(f, echo, capacity, x);
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
    if (track < SLAP_FX_TRACKS && (f->block_index++ & 63u) == 0) {
        unsigned point = (f->block_index >> 6) % SLAP_FX_PLOT;
        traces[track][point] = (int8_t)(clamp_signal(x * 2.0f) * 55.0f);
        if (!track) trace_head = point;
    }
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
                                  chorus_line[track], room_line[track]);
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
    float wet = process_sample(SLAP_FX_TRACKS, middle, mix_echo, MIX_DELAY, mix_chorus, mix_room);
    // Use a centered wet signal and crossfade from the original stereo mix.
    *left += (wet - *left) * mix_wet;
    *right += (wet - *right) * mix_wet;
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
