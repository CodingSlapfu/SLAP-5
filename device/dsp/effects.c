/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "effects.h"
#include "fastmath.h"
#include "plat.h"

enum { TRACK_DELAY = 12288, CHORUS = 1024, ROOM = 2048 };
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
    uint32_t revision, bpm, room_short, room_long;
    uint16_t pattern;
    int16_t slice[256];
} Fx;
static Fx fx[SLAP_FX_TRACKS];
static volatile uint32_t settings_revision[SLAP_FX_TRACKS];
static PLAT_POOL int16_t echo_line[SLAP_FX_TRACKS][TRACK_DELAY];
static PLAT_POOL int16_t chorus_line[SLAP_FX_TRACKS][CHORUS];
static PLAT_POOL int16_t room_line[SLAP_FX_TRACKS][ROOM];

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
    return (f->filter_scale > 0.0f) * 2u + (f->dr[0] > 0.0f) * 3u +
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
    checkpoint->delay_count = f->dp[0] > 0.0f ? (frames + 3u - ((4u - f->delay_div) & 3u)) / 4u : 0u;
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
static float clamp_signal(float x) { return fm_clampf(x, -0.999f, 0.999f); }
static int16_t pack(float x) { return (int16_t)(clamp_signal(x) * 32767.0f); }
static float unpack(int16_t x) { return x * (1.0f / 32768.0f); }
static float soft(float x) { return x / (1.0f + fm_fabsf(x)); }

void slap_fx_init(void)
{
    for (unsigned t = 0; t < SLAP_FX_TRACKS; ++t) {
        settings[t][FILTER] = bytes(0, 0, 0, 22);
        settings[t][SLICER] = bytes(0, 0, 1, 102);
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
    static const unsigned slice_lengths[4] = {8, 4, 2, 1};
    static const unsigned echo_eighths[6] = {1, 2, 3, 4, 6, 8};
    for (unsigned t = 0; t < SLAP_FX_TRACKS; ++t) {
        Fx *f = &fx[t];
        uint32_t revision = settings_revision[t];
        if (f->revision == revision && f->bpm == bpm) continue;
        f->revision = revision;
        f->bpm = bpm;
        const uint32_t filter = settings[t][FILTER];
        const uint32_t slice = settings[t][SLICER];
        const uint32_t insert = settings[t][INSERT];
        const uint32_t delay = settings[t][DELAY];
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
        f->mix = unit(settings[t][DISPLAY] & 255u);
        f->time = 11025u * 60u / bpm * echo_eighths[(delay >> 8) & 255u] / 8u;
        if (f->time >= TRACK_DELAY) f->time = TRACK_DELAY - 1;
        uint32_t room = settings[t][REVERB], chorus = settings[t][CHORUS_FX], drive = settings[t][DRIVE];
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k * 8u;
            f->dp[k] = unit((delay >> shift) & 255u);
            f->ch[k] = unit((chorus >> shift) & 255u);
            f->rv[k] = unit((room >> shift) & 255u);
            f->dr[k] = unit((drive >> shift) & 255u);
        }
        f->phase_step = (0.18f + 3.5f * fm_maxf(f->ch[2], f->speed)) / 44100.0f;
        f->room_long = 300u + (unsigned)(f->rv[2] * 1100.0f);
        f->room_short = f->room_long * 11u / 17u;
    }
}

static unsigned preceding(unsigned pos, unsigned n, unsigned length)
{
    return pos >= n ? pos - n : pos + length - n;
}

static float fold(float x)
{
    float y = (x + 1.0f) * 0.25f;
    y -= fm_floorf(y);
    return 1.0f - 4.0f * fm_fabsf(y - 0.5f);
}

static float insert_sample(Fx *f, float x, float lfo, float flange)
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
        if (f->filter_scale > 0.0f || f->dr[0] > 0.0f || f->ch[0] > 0.0f ||
            f->insert || f->mode || f->rv[0] > 0.0f || f->dp[0] > 0.0f)
            mask |= 1u << track;
    }
    return mask;
}

float slap_fx_sample(unsigned track, float dry)
{
    Fx *f = &fx[track];
    float x = dry;
    if (f->filter_scale > 0.0f) {
        float hp = x - f->lp - f->damp * f->bp;
        f->bp += f->coef * hp;
        f->lp += f->coef * f->bp;
        x += (f->low*(f->lp-x) + f->band*(f->bp-x) + f->high*(hp-x)) * f->filter_scale;
        x = fm_clampf(x, -0.9f, 0.9f);
    }
    if (f->dr[0] > 0.0f) {
        float driven = x * (1.0f + 16.0f*f->dr[1]);
        driven = 0.68f * soft(driven) + 0.32f * f->dr[3] * fold(driven);
        f->drive_lp += (driven - f->drive_lp) * (0.009f + f->dr[2]*0.9f);
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
            float ch = unpack(chorus_line[track][preceding(pos, tap, CHORUS)]);
            chorus_line[track][pos] = pack(x);
            f->chorus_pos = (pos + 1u) & (CHORUS - 1u);
            if (f->insert) x = insert_sample(f, x, lfo, ch);
            f->chorus_lp += (ch - f->chorus_lp) * (0.025f + 0.7f * f->ch[3]);
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
        float a = unpack(room_line[track][preceding(idx, f->room_long, ROOM)]);
        float b = unpack(room_line[track][preceding(idx, f->room_short, ROOM)]);
        f->reverb_lp += ((a * 0.61f + b * 0.39f) - f->reverb_lp) * (0.12f + 0.58f*(1.0f - f->rv[3]));
        room_line[track][idx] = pack(x * 0.58f + f->reverb_lp * (0.31f + 0.63f*f->rv[1]));
        f->room_pos = (idx + 1u) & (ROOM - 1u);
        x += f->rv[0] * f->reverb_lp;
    }
    if (f->dp[0] > 0.0f) {
        if ((f->delay_div++ & 3u) == 0) {
            unsigned pos = f->delay_pos;
            float wet = unpack(echo_line[track][preceding(pos, f->time, TRACK_DELAY)]);
            f->delay_lp += (wet - f->delay_lp) * (0.11f + f->dp[3]*0.78f);
            echo_line[track][pos] = pack(x + f->delay_lp * (0.12f + 0.80f*f->dp[2]));
            if (++pos == TRACK_DELAY) pos = 0;
            f->delay_pos = pos;
        }
        x += f->dp[0] * f->delay_lp;
    }
    if ((f->block_index++ & 63u) == 0) {
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
