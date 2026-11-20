/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "fastmath.h"
#include "synthesis.h"
#include "oscillator.h"
#include "effects.h"
#include "drum_kits.h"

#ifdef PLAT_HOST
#define dsp_fx_sample slap_fx_sample
#else
static float (*volatile dsp_fx_sample)(unsigned, float) = slap_fx_sample;
#endif

enum { TRACKS = 5, LEGACY_STEPS = 64, STEPS = 512, VOICES = 16, RATE = 44100 };
enum { HOME, SEQ, GLOBAL, PATCH, ENV, FX, VISUAL, SAVE, HARMONY, LFO, ARP, MIXER, RECORD, GEN_DRUMS };
enum { NOTES = 0x7f, ACTIVE = 0x80, DRUMS = 4, DRUM_LANES = 6, MODERN_SYNTH = 1u << 27 };

typedef struct {
    volatile uint32_t steps[STEPS]; /* pitch, active flag, velocity, length, probability */
    volatile uint32_t patch, envelope, harmony, lfo;
    volatile uint32_t mix;          /* LSB first: length, pan, level, mute */
} Track;

typedef struct {
    float phase, inc, env, velocity, fall, bend;
    float attack, decay, sustain, release, drum_tune, drum_decay;
    float lfo_phase, lfo_inc, lfo_fade, lfo_gain, lfo_hold;
    float last_sample, steal_tail;
    float drum_noise, drum_color, drum_body, drum_snap;
    float drum_texture_gain, drum_noise_mix, drum_tone;
    uint32_t delay, rng;
    uint8_t lfo_wave, origin, kit, drum_family;
    uint32_t remaining, age, sequence_note;
    uint8_t track, pitch, manual, releasing, struck, drum_circuit, drum_layers;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    uint8_t dispatch_cost;
#endif
} Voice;

static PLAT_POOL Track tracks[TRACKS];
static PLAT_POOL volatile uint32_t extra_note[DRUMS][STEPS];
static PLAT_POOL volatile uint32_t drum_lane[DRUM_LANES][STEPS];
enum { MELODIC_LANES = 4, SONG5_LANES = DRUMS * 2 + DRUM_LANES + 1,
       SONG_LANES = SONG5_LANES + DRUMS * (MELODIC_LANES - 2) };
static PLAT_POOL volatile uint32_t poly_note[DRUMS][MELODIC_LANES - 2][STEPS];
enum { NOTE_DURATION_UNIT = 128 };
static PLAT_POOL uint16_t melodic_duration[DRUMS][MELODIC_LANES][STEPS];
static unsigned triggered_sequence_note;
/* Zero means V2–V4 eighth-step timing. Groups 1–4 hold the upper two
 * bits of a sixteenth-step offset; the event word holds its lower bits. */
static PLAT_POOL uint8_t timing_group[SONG_LANES][STEPS];
static unsigned pattern_row(unsigned track, unsigned lane)
{
    if (track == DRUMS || lane < 2u) return track * 2u + lane;
    return SONG5_LANES + track * (MELODIC_LANES - 2u) + lane - 2u;
}

static const uint8_t lane_pitch[DRUM_LANES] = {36,38,42,37,39,49};
#include "drum_patterns.h"
static unsigned gen_style, gen_hit = 76, gen_groove = 35, gen_var = 28;
static unsigned gen_enabled = 1, gen_confirm, gen_count;
static uint32_t gen_seed = 0xd14a1175u;
static volatile uint32_t drum_filters;
static volatile uint32_t scheduled_events;
static volatile uint32_t recording, recorded_events, rec_begin_events, rec_rejected;
static volatile uint32_t rec_running, rec_countdown;
static unsigned rec_bars, rec_count_in, rec_replace;
// Zero is raw timing; each grid then has four strength settings.
enum { REC_FULL_16 = 8, REC_NATURAL_16 = 6, REC_QUANT_MAX = 12 };
static unsigned rec_quantize = REC_NATURAL_16;
static const char *const rec_quant_labels[] = {
    "OFF", "25% 1/8", "50% 1/8", "75% 1/8", "100% 1/8",
    "25% 1/16", "50% 1/16", "75% 1/16", "100% 1/16",
    "25% 1/32", "50% 1/32", "75% 1/32", "100% 1/32"
};
static unsigned rec_track, rec_length, rec_start_mode, rec_quant_setting, rec_overwrite, rec_loop;
static unsigned rec_pressed_at, rec_gesture;
static uint32_t record_clock;
static struct {
    volatile uint32_t *cell;
    uint16_t *duration;
    uint32_t start, step_ticks, word;
    unsigned pitch;
} held_record[VOICES];
static uint8_t monitored_next_step[TRACKS];
static volatile uint32_t metronome;
static uint32_t saved_metronome, metronome_changed_at;
enum { SETTINGS_TAG = 0x534c4d31u, SETTINGS_SETTLE_MS = 500u };
static unsigned click_remaining, click_accent, click_phase;
enum { CLICK_SAMPLES = RATE / 40 };
enum { BOOT_MS = 6000 };
static volatile uint32_t mixer_solo, dsp_overruns, intro_skipped;
static volatile uint32_t dsp_avg_ticks, dsp_peak_ticks, dsp_budget_ticks;
enum { DSP_BINS = 128, DSP_BIN_TICKS = 2400 };
static volatile uint32_t dsp_histogram[DSP_BINS], dsp_callbacks;
static volatile uint32_t dsp_reset_request, dsp_xrun_baseline;
static volatile unsigned global_details;
enum { PROFILE_INPUT, PROFILE_EVENTS, PROFILE_VOICES, PROFILE_FX, PROFILE_MIX, PROFILE_OUTPUT, PROFILE_COUNT };
static volatile uint32_t serial_peak, serial_at_peak[PROFILE_COUNT];
static volatile uint32_t peak_voice_batch, peak_fx_batch, peak_worker_work, peak_worker_wait;
#if FM1B_CPU2 && !defined(PLAT_WEB)
static volatile uint32_t peak_worker_overlap, peak_voice_recovery, peak_fx_recovery;
#endif

static volatile uint32_t active_voices, voice_steals;
static volatile uint8_t mixer_auto[TRACKS], mixer_proc[TRACKS];
static uint32_t boot_suppressed_keys;
static float mixer_envelope[TRACKS], mixer_gain[TRACKS];
static volatile uint32_t track_peak[TRACKS], fx_cost_ticks[TRACKS];
static Voice voices[VOICES];
static PLAT_POOL SlapSynth synths[VOICES];
static PLAT_POOL int16_t synth_strings[VOICES][SLAP_STRING_CAPACITY];
static volatile uint32_t tempo = 112, swing, playing, positions, sequence_epoch;
static volatile uint32_t keys_audio, input_track, input_octave;
static uint32_t old_keys, key_pitches[27], key_tracks[27], ages, noise = 13579;
static uint32_t until_step, step_duration, step_number, running_epoch;
static volatile uint32_t step_progress;
static uint32_t startup_sample;
static float startup_phase;
#define ARP_DEFAULT (2u << 8 | 1u << 16 | 64u << 24)
static volatile uint32_t arp_setup[DRUMS];
static volatile uint8_t arp_history[16];
static volatile uint32_t arp_cursor;
static uint32_t arp_pulse, arp_wait, arp_last_track = TRACKS, arp_stamp, arp_arrival[NKEYS];
static float master_gain, energy_smooth[TRACKS];
static volatile uint32_t track_energy[TRACKS];
#if FM1B_CPU2 && defined(PLAT_HOST) && !defined(PLAT_WEB)
static int host_discard_worker_result, host_discard_fx_result, host_late_voice;
#endif
enum { SPECTRUM_BANDS = 7, VISUAL_COUNT = 8 };
static volatile float visual_rms[2], visual_peak[2], visual_band[SPECTRUM_BANDS];
static float meter_power[2], analysis_lp[SPECTRUM_BANDS - 1], analysis_power[SPECTRUM_BANDS];
static float analysis_mono;
static unsigned analysis_phase;

enum { ANALYSIS_QUEUE = 512, ANALYSIS_CHUNK = 128 };
static float analysis_queue[ANALYSIS_QUEUE];
static volatile unsigned analysis_write, analysis_read;

static void capture_spectrum(float left, float right)
{
    analysis_mono += (left + right) * 0.125f;
    if (++analysis_phase != 4) return;
    unsigned write = analysis_write;
    if (write - analysis_read < ANALYSIS_QUEUE) {
        analysis_queue[write % ANALYSIS_QUEUE] = analysis_mono;
        __asm__ volatile("" ::: "memory");
        analysis_write = write + 1;
    }
    analysis_mono = 0.0f;
    analysis_phase = 0;
}

static void update_spectrum(void)
{
    static const float coefficient[] = {0.0336f, 0.0819f, 0.1809f, 0.3662f, 0.6412f, 0.8975f};
    unsigned read = analysis_read, available = analysis_write - read;
    if (available > ANALYSIS_CHUNK) available = ANALYSIS_CHUNK;
    __asm__ volatile("" ::: "memory");
    for (unsigned i = 0; i < available; ++i) {
        float mono = analysis_queue[(read + i) % ANALYSIS_QUEUE];
        float previous = 0.0f;
        for (unsigned band = 0; band < SPECTRUM_BANDS; ++band) {
            float next = mono;
            if (band < SPECTRUM_BANDS - 1) {
                analysis_lp[band] += (mono - analysis_lp[band]) * coefficient[band];
                next = analysis_lp[band];
            }
            float signal = next - previous;
            analysis_power[band] += (signal * signal - analysis_power[band]) * 0.0023f;
            previous = next;
        }
    }
    __asm__ volatile("" ::: "memory");
    analysis_read = read + available;
    for (unsigned band = 0; band < SPECTRUM_BANDS; ++band)
        visual_band[band] = analysis_power[band];
}
static struct {
    uint32_t buttons, keys, frame_at, boot_at, booting;
    int page, track, step, octave, fx_page, visual, slot, lane;
    uint32_t save_at;
    int save_armed;
    const char *notice;
} slap_ui;

static uint32_t cell_word(unsigned pitch, unsigned velocity, unsigned gate, unsigned probability)
{
    return ACTIVE | pitch | (velocity << 8) | (gate << 16) | (probability << 24);
}

enum { LENGTH_MASK = 0x060000ffu };
static volatile uint32_t pattern_revision[TRACKS];

static unsigned pattern_length(uint32_t mix)
{
    return (mix & 255u) | ((mix >> 17) & 768u);
}

static void track_setting(unsigned track, unsigned shift, unsigned value)
{
    if (shift == 0) {
        uint32_t old = tracks[track].mix;
        tracks[track].mix = (old & ~LENGTH_MASK) | (value & 255u) | ((value & 768u) << 17);
        if (pattern_length(old) != value) ++pattern_revision[track];
        return;
    }
    uint32_t mask = (shift == 24 ? 1u : 255u) << shift;
    tracks[track].mix = (tracks[track].mix & ~mask) | (value << shift);
}

static void load_patch(unsigned track, unsigned number)
{
    const SlapPatch *patch = &slap_patches[number];
    tracks[track].envelope = patch->attack | (uint32_t)patch->decay << 8 |
                             (uint32_t)patch->sustain << 16 | (uint32_t)patch->release << 24;
    tracks[track].patch = number;
    tracks[track].mix |= MODERN_SYNTH;
}

void app_init(void)
{
    uint32_t settings[2];
    saved_metronome = 0;
    if (plat_store_load(OBJ_SETTINGS, settings, sizeof settings) == (int)sizeof settings &&
        settings[0] == SETTINGS_TAG && settings[1] <= 2u)
        saved_metronome = settings[1];
    metronome = saved_metronome;
    startup_sample = 0;
    startup_phase = 0.0f;
    slap_fx_init();
#if FM1B_CPU2 && !defined(PLAT_WEB)
#ifdef PLAT_HOST
    host_discard_worker_result = getenv("SLAP_TEST_DISCARD_WORKER") != 0;
    host_discard_fx_result = getenv("SLAP_TEST_DISCARD_FX") != 0;
    const char *late_voice = getenv("SLAP_TEST_LATE_VOICE");
    host_late_voice = late_voice ? atoi(late_voice) : 0;
    plat_cpu2_set(getenv("SLAP_TEST_SINGLE_CORE") == 0);
#endif
#endif
    for (unsigned t = 0; t < DRUMS; ++t)
        arp_setup[t] = ARP_DEFAULT;
    arp_cursor = arp_wait = arp_pulse = 0;
    for (unsigned i = 0; i < 16; ++i) arp_history[i] = 255;
    for (unsigned t = 0; t < DRUMS; ++t) tracks[t].lfo = 31u << 8;
    for (unsigned t = 0; t < TRACKS; ++t) {
        tracks[t].mix = 16u | (128u << 8) | (170u << 16);
        mixer_proc[t] = 255;
        mixer_gain[t] = 1.0f;
    }
    static const uint8_t defaults[] = {67, 0, 78, 37};
    for (unsigned t = 0; t < DRUMS; ++t)
        load_patch(t, defaults[t]);
    tracks[DRUMS].patch = DRUM_LEGACY_KITS;
}

static float slap_sine(float phase) { return fm_sinf(phase * 6.2831853f); }

#if defined(PLAT_HOST)
#define lfo_sine slap_sine
#else
static float (*volatile lfo_sine)(float) = slap_sine;
#endif
#include "modulation.h"

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static uint32_t random_word(void) { return next_random(&noise); }

static unsigned drum_family(unsigned pitch)
{
    switch (pitch) {
    case 35: case 36: return 0;
    case 38: case 40: return 1;
    case 42: case 44: case 46: return 2;
    case 37: return 3;
    case 39: return 4;
    case 49: case 51: case 57: return 5;
    default: return (pitch + 2u) % 6u;
    }
}

/* Trim only factory level outliers from the three-register note sweep.
 * Unlisted patches retain their current balance and all patch IDs stay fixed. */
static const uint8_t factory_trim[SLAP_PATCHES] = {
    [0] = 92,                         /* FM Glass */
    [1] = 82,                         /* FM Bass */
    [24] = 116,                       /* Muted Guitar */
    [73] = 121, [75] = 125, [77] = 130, /* Acid Line, Throat Acid, Scream Filter */
    [103] = 80, [104] = 80,             /* Alto and Tenor Sax */
    [105] = 83, [106] = 82, [107] = 85, /* Clarinet, Oboe, Bassoon */
    [122] = 116                        /* Slow Granules */
};

static unsigned select_drum_kit(unsigned current, int delta)
{
    if (!delta) return current;
    if (current < DRUM_LEGACY_KITS)
        current = delta > 0 ? DRUM_LEGACY_KITS - 1u : DRUM_KITS;
    return (unsigned)clamp((int)current + delta, DRUM_LEGACY_KITS, DRUM_KITS - 1);
}

static float drum_decay(unsigned family, const struct DrumKit *kit, unsigned identity)
{
    float decay;
    if (family == 0)
        decay = kit->layers & DRUM_SUB ? 0.99976f : 0.99966f - kit->circuit * 0.00008f;
    else if (family == 1 || family == 3)
        decay = family == 3 ? 0.9977f : 0.99906f;
    else if (family == 4)
        decay = 0.99902f;
    else
        decay = family == 5 ? 0.99968f : kit->circuit == 1 ? 0.998f : 0.9969f;
    if (identity >= 4u)
        decay = 1.0f - (1.0f - decay) * (100.0f / kit->decay);
    return decay;
}

#if FM1B_CPU2 && !defined(PLAT_WEB)
enum { MAX_VOICE_COST = 14, VOICE_LOCAL_ONLY = 128 };

static unsigned voice_dispatch_cost(const Voice *voice, const SlapSynth *synth)
{
    static const uint8_t engine_cost[SLAP_ENGINES] = {
        7, 4, 7, 4, 4, 6, 3, 10, 4, 5, 4, 4, 8,
        4, 5, 6, 8, 9, 7, 6, 5, 4, 7, 6, 5
    };
    if (voice->track == DRUMS)
        return (voice->drum_family == 0 ? 5u : 4u) + (voice->drum_layers ? 2u : 0u);
    unsigned engine = synth->engine;
    unsigned cost = engine_cost[engine] + (voice->lfo_wave ? 2u : 0u);
    if (synth->modern && (engine == 0 || engine == 16 || engine == 20)) cost += 2u;
    // Shared sample-and-hold RNG and mutable string lines stay on Core 0.
    if (voice->lfo_wave == 5 || engine == 4 || engine == 6) cost |= VOICE_LOCAL_ONLY;
    return cost;
}
#endif

// These coefficients depend only on the track settings. The audio thread
// snapshots the packed settings at note-on and owns the cache.
static struct {
    uint32_t envelope, lfo;
    float attack, decay, sustain, release, lfo_inc, lfo_fade;
    int valid;
} note_coefficients[DRUMS];

static void trigger_voice(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration,
                          int manual, unsigned origin, unsigned delay)
{
    Voice *voice = &voices[0];
    for (unsigned i = 0; i < VOICES; ++i) {
        if (!voices[i].age) { voice = &voices[i]; break; }
        if (voices[i].releasing != voice->releasing) {
            if (voices[i].releasing) voice = &voices[i];
        } else if (voice->releasing ? voices[i].env * voices[i].velocity < voice->env * voice->velocity :
                   voices[i].age < voice->age) {
            voice = &voices[i];
        }
    }
    float tail = voice->age && voice->track == track ? voice->last_sample : 0.0f;
    if (voice->age) ++voice_steals;
    *voice = (Voice) {
        .velocity = (float)velocity / 127.0f, .fall = 1.0f, .bend = 1.0f,
        .remaining = duration, .age = ++ages, .sequence_note = triggered_sequence_note,
        .track = (uint8_t)track, .pitch = (uint8_t)pitch, .manual = (uint8_t)manual,
        .origin = (uint8_t)origin, .delay = delay, .steal_tail = tail
    };
    if (track < DRUMS) {
        unsigned patch_id = tracks[track].patch;
        const SlapPatch *patch = &slap_patches[patch_id];
        unsigned trim = factory_trim[patch_id];
        if (trim) voice->velocity *= trim * 0.01f;
        uint32_t env = tracks[track].envelope;
        uint32_t lfo = tracks[track].lfo;
        if (!note_coefficients[track].valid || note_coefficients[track].envelope != env ||
            note_coefficients[track].lfo != lfo) {
            float a = env & 255u, d = (env >> 8) & 255u, r = env >> 24;
            note_coefficients[track].attack = 1.0f / (25.0f + 1.3f*a*a);
            note_coefficients[track].decay = 1.0f / (38.0f + 1.9f*d*d);
            note_coefficients[track].sustain = ((env >> 16) & 255u) * (1.0f / 127.0f);
            note_coefficients[track].release = 1.0f / (48.0f + 0.85f*r*r);
            if (lfo & 255u) {
                const float hz = 0.1f * fm_exp2f(((lfo >> 8) & 255u) * (8.643856f / 127.0f));
                note_coefficients[track].lfo_inc = hz * (1.0f / RATE);
                note_coefficients[track].lfo_fade = 1.0f / (1.0f + (lfo >> 24) * 690.0f);
            }
            note_coefficients[track].envelope = env;
            note_coefficients[track].lfo = lfo;
            note_coefficients[track].valid = 1;
        }
        voice->attack = note_coefficients[track].attack;
        voice->decay = note_coefficients[track].decay;
        voice->sustain = note_coefficients[track].sustain;
        voice->release = note_coefficients[track].release;
        voice->lfo_wave = lfo & 255u;
        if (voice->lfo_wave) {
            voice->lfo_inc = note_coefficients[track].lfo_inc;
            voice->lfo_phase = ((lfo >> 16) & 255u) * (1.0f / 128.0f);
            voice->lfo_fade = note_coefficients[track].lfo_fade;
            voice->lfo_hold = (int16_t)(random_word() & 65535u) * (1.0f / 32768.0f);
        }
        unsigned slot = (unsigned)(voice - voices);
        slap_synth_start(&synths[slot], synth_strings[slot], patch->engine, pitch,
                         patch->color, patch->motion, random_word(),
                         tracks[track].mix & MODERN_SYNTH);
        voice->inc = synths[slot].inc;
    } else {
        voice->rng = random_word();
        voice->kit = (uint8_t)tracks[track].patch;
        voice->drum_family = (uint8_t)drum_family(pitch);
        const struct DrumKit *kit = &drum_kits[voice->kit];
        unsigned tune = voice->drum_family == 0 ? kit->kick_pitch :
            voice->drum_family == 1 || voice->drum_family == 3 ? kit->snare_pitch :
            voice->drum_family == 4 ? kit->clap_rate : kit->metal_pitch;
        voice->drum_tune = voice->kit >= 4u ? tune * 0.01f : 1.0f;
        voice->drum_circuit = kit->circuit;
        voice->drum_layers = kit->layers;
        static const float tone[] = {0.0045f, 0.0028f, 0.0052f, 0.0072f};
        static const float pitch_mul[] = {1.0f, 0.81f, 1.33f, 1.85f};
        voice->drum_tone = tone[kit->circuit];
        const unsigned family = voice->drum_family;
        if (family == 1 || family == 3) {
            voice->inc = voice->drum_tone * (family == 3 ? 3.17f : 1.0f);
        } else if (family != 0) {
            voice->inc = (family == 4 ? 0.008f : 0.052f) * pitch_mul[kit->circuit];
        }
        voice->inc *= voice->drum_tune;
        voice->drum_decay = drum_decay(voice->drum_family, kit, voice->kit);
        voice->drum_color = 0.04f + kit->noise_color * 0.0065f;
        voice->drum_body = kit->resonance * (1.0f / 127.0f);
        voice->drum_snap = kit->transient * (1.0f / 127.0f);
        voice->drum_texture_gain = 0.18f + 0.7f * voice->drum_snap;
        voice->drum_noise_mix = (kit->circuit == 1 ? 0.91f : family == 3 ? 0.34f : 0.68f) *
                                (1.0f - 0.28f * voice->drum_body);
        voice->attack = 0.013f;
        voice->sustain = 1.0f;
        voice->release = 0.00065f;
    }
#if FM1B_CPU2 && !defined(PLAT_WEB)
    voice->dispatch_cost = voice_dispatch_cost(voice, &synths[voice - voices]);
#endif
}

static void trigger_delayed(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration, int manual, unsigned offset)
{
    uint32_t harmony = tracks[track].harmony;
    unsigned root = harmony & 255u, scale = (harmony >> 8) & 255u;
    unsigned chord = (harmony >> 16) & 255u, strum = harmony >> 24;
    if (track == DRUMS || (!scale && !chord)) {
        trigger_voice(track, pitch, velocity, duration, manual, pitch, offset);
        return;
    }
    unsigned tonic = note_in_scale(pitch, root, scale);
    unsigned pitches[4] = {tonic, tonic, tonic, tonic};
    unsigned count = 1;
    if (chord) {
        unsigned third = scale ? scale_degree(tonic, 2, root, scale) : (unsigned)clamp((int)tonic + 4, 0, 127);
        unsigned fifth = scale ? scale_degree(tonic, 4, root, scale) : (unsigned)clamp((int)tonic + 7, 0, 127);
        pitches[1] = chord == 3 ? fifth : third;
        pitches[2] = fifth;
        pitches[3] = scale ? scale_degree(tonic, 6, root, scale) : (unsigned)clamp((int)tonic + 11, 0, 127);
        count = chord == 3 ? 2u : chord == 2 ? 4u : 3u;
    }
    unsigned previous = 255;
    for (unsigned i = 0; i < count; ++i) {
        if (pitches[i] == previous) continue;
        unsigned delay = strum && count > 1 ? RATE * (99u + strum) / 1000u * i / (count - 1u) : 0u;
        trigger_voice(track, pitches[i], velocity * (i ? 75u : 100u) / 100u,
                      duration, manual, pitch, offset + delay);
        previous = pitches[i];
    }
}

static void trigger(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration, int manual)
{
    trigger_delayed(track, pitch, velocity, duration, manual, 0);
}

static unsigned timing_of(uint32_t word)
{
    return ((word >> 15) & 1u) | ((word >> 30) & 2u);
}

static uint32_t timing_bits(unsigned offset)
{
    return ((offset & 1u) << 15) | ((offset & 2u) << 30);
}

static unsigned note_timing(unsigned row, unsigned step, uint32_t word)
{
    unsigned group = timing_group[row][step];
    return group ? (group - 1u) * 4u + timing_of(word) : timing_of(word) * 2u;
}

static unsigned melodic_lane_from_row(unsigned track, unsigned row)
{
    return row < SONG5_LANES ? row % 2u :
        2u + row - SONG5_LANES - track * (MELODIC_LANES - 2u);
}

static void schedule_event(unsigned track, unsigned row, unsigned step, uint32_t word)
{
    unsigned probability = (word >> 24) & 127u;
    if (!(word & ACTIVE) || probability <= random_word() % 100u) return;
    unsigned gate = until_step * ((word >> 16) & 255u) / 192u;
    unsigned delay = until_step * note_timing(row, step, word) / 16u;
    unsigned identity = 1u + row * STEPS + step;
    if (track < DRUMS) {
        unsigned lane = melodic_lane_from_row(track, row);
        unsigned duration = melodic_duration[track][lane][step];
        if (duration) {
            gate = duration * NOTE_DURATION_UNIT;
            for (unsigned slot = 0; slot < VOICES; ++slot)
                if (voices[slot].age && !voices[slot].releasing &&
                    voices[slot].sequence_note == identity &&
                    voices[slot].remaining + voices[slot].delay > delay) return;
        }
        if (rec_running && track == rec_track)
            for (unsigned slot = 0; slot < VOICES; ++slot)
                if (held_record[slot].duration == &melodic_duration[track][lane][step] &&
                    held_record[slot].cell) return;
    }
    triggered_sequence_note = identity;
    trigger_delayed(track, word & NOTES, (word >> 8) & 127u, gate ? gate : 1u, 0, delay);
    triggered_sequence_note = 0;
    ++scheduled_events;
}

static void release_note(unsigned track, unsigned pitch)
{
    for (unsigned i = 0; i < VOICES; ++i)
        if (voices[i].manual && voices[i].origin == pitch && voices[i].track == track)
            voices[i].releasing = 1;
}

static void finish_record_note(unsigned slot, uint32_t now)
{
    volatile uint32_t *cell = held_record[slot].cell;
    if (cell && *cell == held_record[slot].word) {
        unsigned elapsed = now - held_record[slot].start;
        unsigned span = held_record[slot].step_ticks;
        unsigned gate = elapsed >= span * 2u ? 255u :
            (unsigned)clamp((int)((elapsed * 192u + span / 2u) / span), 1, 255);
        if (held_record[slot].duration) {
            unsigned units = elapsed / NOTE_DURATION_UNIT +
                ((elapsed % NOTE_DURATION_UNIT) >= NOTE_DURATION_UNIT / 2u);
            *held_record[slot].duration = (uint16_t)clamp((int)units, 1, 65535);
        }
        *cell = (*cell & ~(255u << 16)) | (gate << 16);
    }
    held_record[slot].cell = 0;
}

static volatile uint32_t *event_cell(unsigned track, unsigned step, unsigned lane);
static unsigned lane_count(unsigned track);

static PLAT_POOL struct {
    uint32_t cells[DRUM_LANES + 1][STEPS];
    uint8_t timing[DRUM_LANES + 1][STEPS];
    uint16_t duration[MELODIC_LANES][STEPS];
    unsigned track, length, recorded_length, ready;
    uint32_t after;
} take_undo;

static void undo_recording(void)
{
    if (playing || recording || rec_running || !take_undo.ready) return;
    unsigned track = take_undo.track;
    take_undo.ready = 0;
    if (pattern_revision[track] != take_undo.after) return;
    for (unsigned lane = 0; lane < lane_count(track); ++lane)
        for (unsigned step = 0; step < take_undo.recorded_length; ++step) {
            *event_cell(track, step, lane) = take_undo.cells[lane][step];
            timing_group[pattern_row(track, lane)][step] = take_undo.timing[lane][step];
            if (track < DRUMS) melodic_duration[track][lane][step] = take_undo.duration[lane][step];
        }
    track_setting(track, 0, take_undo.length);
}

static void end_recording(uint32_t now)
{
    for (unsigned slot = 0; slot < VOICES; ++slot) finish_record_note(slot, now);
    take_undo.after = pattern_revision[rec_track];
    take_undo.ready = 1;
    rec_running = rec_countdown = 0;
}

static void begin_recording(void)
{
    unsigned track = rec_track;
    take_undo.ready = 0;
    take_undo.track = track;
    take_undo.length = pattern_length(tracks[track].mix);
    take_undo.recorded_length = rec_length;
    for (unsigned lane = 0; lane < lane_count(track); ++lane)
        for (unsigned step = 0; step < take_undo.recorded_length; ++step) {
            take_undo.cells[lane][step] = *event_cell(track, step, lane);
            take_undo.timing[lane][step] = timing_group[pattern_row(track, lane)][step];
            if (track < DRUMS) take_undo.duration[lane][step] = melodic_duration[track][lane][step];
        }
    if (rec_overwrite) {
        for (unsigned step = 0; step < rec_length; ++step) {
            for (unsigned lane = 0; lane < lane_count(track); ++lane)
                timing_group[pattern_row(track, lane)][step] = 0;
            if (track < DRUMS)
                for (unsigned lane = 0; lane < MELODIC_LANES; ++lane) melodic_duration[track][lane][step] = 0;
            tracks[track].steps[step] = 0;
            for (unsigned lane = 1; lane < lane_count(track); ++lane)
                *event_cell(track, step, lane) = 0;
        }
    }
    track_setting(track, 0, rec_length);
    rec_begin_events = recorded_events;
    rec_rejected = 0;
    rec_running = 1;
}

static void record_note_off(unsigned track, unsigned pitch)
{
    if (!rec_running || track != rec_track) return;
    for (unsigned slot = 0; slot < VOICES; ++slot)
        if (held_record[slot].cell && held_record[slot].pitch == pitch)
            finish_record_note(slot, record_clock);
}

/* Single audio-thread writer for live input. A filled two-note step is left intact. */
static void record_input(unsigned track, unsigned pitch, unsigned velocity)
{
    if (!recording || !playing || track != rec_track) return;
    if (!rec_running) {
        if (rec_start_mode) return;
        begin_recording();
    }
    unsigned position = step_number;
    unsigned timing = 0;
    if (until_step) {
        unsigned progress = step_duration - until_step;
        unsigned strength = rec_quant_setting ? (rec_quant_setting - 1u) % 4u + 1u : 0u;
        int grid = 0;
        if (rec_quant_setting > REC_FULL_16) {
            grid = progress * 4u < step_duration ? 0 :
                   progress * 4u < step_duration * 3u ? 8 : 16;
        } else if (rec_quant_setting && rec_quant_setting <= 4u) {
            if (positions & 1u) grid = 16;
        } else if (progress * 2u >= step_duration) {
            grid = 16;
        }
        int blended = (int)(progress * 16u * (4u - strength)) +
                      grid * (int)(step_duration * strength);
        unsigned offset = ((unsigned)blended + step_duration * 2u) / (step_duration * 4u);
        if (offset < 16u) {
            position = positions;
            timing = offset;
        }
    }
    if (!rec_loop && position >= rec_length) return;
    unsigned step = position % rec_length;
    unsigned source_pitch = pitch;
    unsigned lane = 0;
    volatile uint32_t *cell = &tracks[track].steps[step];
    if (track == DRUMS) {
        lane = drum_family(pitch) + 1;
        cell = &drum_lane[lane - 1][step];
        pitch = lane_pitch[lane - 1];
    } else {
        unsigned empty = MELODIC_LANES;
        for (lane = 0; lane < MELODIC_LANES; ++lane) {
            cell = event_cell(track, step, lane);
            uint32_t entry = *cell;
            if (!(entry & ACTIVE)) {
                if (empty == MELODIC_LANES) empty = lane;
                continue;
            }
            if ((entry & NOTES) == pitch &&
                note_timing(pattern_row(track, lane), step, entry) == timing) break;
        }
        if (lane == MELODIC_LANES) {
            if (empty == MELODIC_LANES) {
                ++rec_rejected;
                return;
            }
            lane = empty;
            cell = event_cell(track, step, lane);
        }
    }
    if (rec_loop && (*cell & ACTIVE)) return;
    record_note_off(track, source_pitch);
    unsigned available = VOICES;
    for (unsigned slot = 0; slot < VOICES; ++slot) {
        if (held_record[slot].cell == cell) {
            available = slot;
            break;
        }
        if (!held_record[slot].cell && available == VOICES) available = slot;
    }
    if (available == VOICES) {
        ++rec_rejected;
        return;
    }
    timing_group[pattern_row(track, lane)][step] = timing ? (uint8_t)(timing / 4u + 1u) : 0;
    *cell = cell_word(pitch, velocity, 144u, 100u) | timing_bits(timing);
    uint16_t *duration = track < DRUMS ? &melodic_duration[track][lane][step] : 0;
    if (duration) *duration = 0;
    held_record[available].cell = cell;
    held_record[available].duration = duration;
    held_record[available].start = record_clock;
    held_record[available].step_ticks = step_duration && until_step ? step_duration : RATE * 15u / tempo;
    held_record[available].word = *cell;
    held_record[available].pitch = source_pitch;
    /* The live input has already sounded. Skip its first sequencer pass only. */
    if (position == step_number) monitored_next_step[track] |= 1u << lane;
    ++recorded_events;
}

#ifdef PLAT_WEB
enum { STUDIO_NOTE_QUEUE = 64 };
static uint16_t studio_notes[STUDIO_NOTE_QUEUE];
static unsigned studio_notes_read, studio_notes_write;
#endif

static void read_notes(void)
{
#ifdef PLAT_WEB
    while (studio_notes_read != studio_notes_write) {
        unsigned command = studio_notes[studio_notes_read++ % STUDIO_NOTE_QUEUE];
        unsigned pitch = command & 127u, velocity = command >> 7;
        if (velocity) {
            release_note(rec_track, pitch);
            record_input(rec_track, pitch, velocity);
            trigger(rec_track, pitch, velocity, 0, 1);
        } else {
            record_note_off(rec_track, pitch);
            release_note(rec_track, pitch);
        }
    }
#endif
    uint32_t keys = keys_audio;
    for (unsigned k = 0; k < NKEYS; ++k) {
        unsigned bit = 1u << k;
        if ((keys ^ old_keys) & bit) {
            if (keys & bit) {
                unsigned pitch = 29 + k + input_octave * 12;
                unsigned track = input_track;
                key_pitches[k] = pitch;
                key_tracks[k] = track;
                arp_arrival[k] = ++arp_stamp;
                record_input(track, pitch, 100);
                if (track == DRUMS || !(arp_setup[track] & 255u))
                    trigger(track, pitch, 100, 0, 1);
                plat_midi_send(0x09u | (0x90u | track) << 8 | pitch << 16 | 100u << 24, TO_USB);
            } else {
                record_note_off(key_tracks[k], key_pitches[k]);
                release_note(key_tracks[k], key_pitches[k]);
                plat_midi_send(0x08u | (0x80u | key_tracks[k]) << 8 | key_pitches[k] << 16, TO_USB);
            }
        }
    }
    old_keys = keys;
    uint32_t packet;
    int port;
    for (unsigned count = 0; count < 32 && plat_midi_in(&packet, &port); ++count) {
        unsigned status = (packet >> 8) & 255u;
        unsigned track = status & 15u;
        unsigned pitch = (packet >> 16) & 127u, velocity = (packet >> 24) & 127u;
        if (track >= TRACKS) continue;
        if ((status & 0xf0u) == 0x90u && velocity) {
            release_note(track, pitch);
            record_input(track, pitch, velocity);
            trigger(track, pitch, velocity, 0, 1);
        } else if ((status & 0xf0u) == 0x80u || (status & 0xf0u) == 0x90u) {
            record_note_off(track, pitch);
            release_note(track, pitch);
        }
    }
}

static void arp_render_clock(void)
{
    static const uint8_t ratios[] = {4, 8, 16, 32, 12, 24};
    unsigned track = input_track;
    uint32_t config = track < DRUMS ? arp_setup[track] : 0u;
    unsigned mode = config & 255u;
    if (!mode || !old_keys || arp_last_track != track) {
        arp_wait = arp_pulse = 0;
        arp_last_track = track;
        if (!mode || !old_keys) return;
    }
    if (arp_wait) { --arp_wait; return; }
    unsigned keys[NKEYS], count = 0;
    for (unsigned k = 0; k < NKEYS; ++k) {
        if (!(old_keys & (1u << k)) || key_tracks[k] != track) continue;
        unsigned at = count++;
        if (mode == 6) {
            while (at && arp_arrival[keys[at - 1]] > arp_arrival[k]) {
                keys[at] = keys[at - 1];
                --at;
            }
        }
        keys[at] = k;
    }
    unsigned period = RATE * 240u / (tempo * ratios[(config >> 8) & 255u]);
    arp_wait = period - 1u;
    if (!count) return;
    unsigned total = count * ((config >> 16) & 255u);
    unsigned index = arp_pulse % total;
    if (mode == 2) index = total - index - 1;
    if (mode == 3 && total > 1) {
        index = arp_pulse % (2u * total - 2u);
        if (index >= total) index = 2u * total - 2u - index;
    }
    if (mode == 4) index = random_word() % total;
    if (mode == 5) index = (arp_pulse % (total / count)) * count;
    uint32_t harmony = tracks[track].harmony;
    unsigned gate = period * (4u + (config >> 24)) / 104u;
    unsigned first = 0;
    for (unsigned i = 0; i < (mode == 5 ? count : 1u); ++i) {
        unsigned n = index + i;
        unsigned pitch = (unsigned)clamp((int)key_pitches[keys[n % count]] + (int)(n / count) * 12, 0, 127);
        pitch = note_in_scale(pitch, harmony & 255u, (harmony >> 8) & 255u);
        if (!i) first = pitch;
        trigger_voice(track, pitch, 100, gate, 0, pitch, 0);
    }
    arp_history[arp_cursor & 15u] = (uint8_t)first;
    ++arp_cursor;
    ++arp_pulse;
}

static unsigned muted_drums(void)
{
    unsigned filter = drum_filters, solo = (filter >> 8) & 63u;
    return solo ? 63u & ~solo : filter & 63u;
}

static void next_step(const uint32_t *mix)
{
    unsigned base = RATE * 15u / tempo;
    unsigned offset = base * swing / 200u;
    until_step = step_number & 1u ? base - offset : base + offset;
    step_duration = until_step;
    positions = step_number++;
    if ((metronome == 2u || (metronome == 1u && recording)) && !(positions & 3u)) {
        click_remaining = CLICK_SAMPLES;
        click_accent = !(positions & 15u);
        click_phase = 0;
    }
    for (unsigned t = 0; t < TRACKS; ++t) {
        unsigned step = positions % pattern_length(mix[t]);
        uint32_t word = tracks[t].steps[step];
        unsigned monitored = monitored_next_step[t];
        monitored_next_step[t] = 0;
        if (t < DRUMS) {
            if (!(monitored & 1u)) schedule_event(t, pattern_row(t, 0), step, word);
            for (unsigned lane = 1; lane < MELODIC_LANES; ++lane)
                if (!(monitored & (1u << lane)))
                    schedule_event(t, pattern_row(t, lane), step, *event_cell(t, step, lane));
        } else {
            unsigned muted = muted_drums();
            if (!(monitored & 1u) && !(muted & (1u << drum_family(word & NOTES)))) schedule_event(t, pattern_row(t, 0), step, word);
            for (unsigned lane = 0; lane < DRUM_LANES; ++lane)
                if (!(muted & (1u << lane)) && !(monitored & (2u << lane)))
                    schedule_event(t, pattern_row(t, lane + 1), step, drum_lane[lane][step]);
        }
    }
}

static inline __attribute__((always_inline)) float render_voice_signal(Voice *v, SlapSynth *s)
{
    if (v->delay) {
        if (v->releasing) v->age = 0;
        else --v->delay;
        return 0.0f;
    }
    if (!v->manual && v->remaining && !--v->remaining) v->releasing = 1;
    float target = v->releasing ? 0.0f : v->struck ? v->sustain : 1.0f;
    float rate = v->releasing ? v->release : v->struck ? v->decay : v->attack;
    v->env += (target - v->env) * rate;
    if (v->env > 0.94f && !v->releasing) v->struck = 1;
    if (v->releasing && v->env < 0.0001f) { v->age = 0; return 0.0f; }

    if (v->track < DRUMS) {
        if (v->lfo_wave) {
            v->lfo_gain = fm_minf(1.0f, v->lfo_gain + v->lfo_fade);
            v->lfo_phase += v->lfo_inc;
            if (v->lfo_phase >= 1.0f) {
                v->lfo_phase -= 1.0f;
                if (v->lfo_wave == 5)
                    v->lfo_hold = (int16_t)(random_word() & 65535u) * (1.0f / 32768.0f);
            }
            float value = lfo_shape(v->lfo_wave, v->lfo_phase, v->lfo_hold);
            s->inc = v->inc * (1.0f + 0.027f * v->lfo_gain * value);
        }
        return slap_synth_sample(s) * v->env * v->velocity * 0.18f;
    }
    float wave;
    float inc = v->inc;
    {
        float hiss = (float)(next_random(&v->rng) & 65535u) / 32768.0f - 1.0f;
        v->drum_noise += (hiss - v->drum_noise) * v->drum_color;
        float air = hiss - v->drum_noise;
        float texture = v->drum_noise + air * v->drum_texture_gain;
        unsigned family = v->drum_family;
        unsigned kit = v->drum_circuit;
        unsigned layers = v->drum_layers;
        if (family == 0) {
            v->bend *= 0.9988f;
            inc = (0.00108f + v->drum_tone * v->bend) * v->drum_tune;
            float body = slap_oscillator(v->phase);
            wave = body * (1.0f - 0.24f * v->drum_body) +
                0.24f * v->drum_body * slap_oscillator(v->phase * 2.0f) * v->bend +
                0.24f * v->drum_snap * air * v->bend * v->bend;
            if (layers & DRUM_SUB) {
                // Saturated long sine body, with a rounded attack from the sweep.
                float sub = body * (1.45f - 0.45f * body * body);
                wave = 0.76f * sub + 0.24f * wave;
            }
            if (layers & DRUM_BEATER)
                wave += 0.20f * air * v->bend * v->bend * v->bend;
        } else if (family == 1 || family == 3) {
            v->bend *= 0.9972f;
            float skin = kit == 2 ? slap_oscillator(v->phase + 0.25f*v->bend*slap_oscillator(v->phase*3.371f))
                                  : slap_oscillator(v->phase);
            float noise_mix = v->drum_noise_mix;
            wave = noise_mix * texture + (1.0f - noise_mix) * skin * v->bend;
            if (layers & DRUM_MEMBRANE) {
                float head = slap_oscillator(v->phase * 2.13f) + 0.38f * slap_oscillator(v->phase * 3.91f);
                wave = 0.65f * wave + 0.25f * head * v->bend;
            }
            if (layers & DRUM_BRUSH) {
                float brush = kit == 2 ? slap_oscillator(v->phase) : skin;
                wave = 0.52f * wave + 0.48f * v->drum_noise * (1.0f + brush);
            }
        } else if (family == 4) {
            v->bend *= 0.999f;
            float burst_width = 0.07f + 0.23f * v->drum_snap;
            wave = texture * (v->phase < burst_width ? 0.95f : 0.16f + 0.24f * v->drum_body);
            if (layers & DRUM_CLUSTER) {
                // Three decaying hand-clap transients ahead of the diffuse tail.
                float cluster = v->phase < 0.12f || (v->phase > 0.31f && v->phase < 0.43f) ||
                                (v->phase > 0.70f && v->phase < 0.79f) ? v->bend : 0.0f;
                wave = texture * (0.23f + 0.73f * cluster);
            }
        } else {
            float metal = slap_oscillator(v->phase) * slap_oscillator(v->phase*3.79f);
            wave = (0.72f - 0.26f * v->drum_body) * air +
                (0.09f + 0.46f * v->drum_body) * metal;
            if (layers & DRUM_PLATE) {
                // Inharmonic beating modes give these hats and cymbals a pitched edge.
                float plate = slap_oscillator(v->phase * 2.71f) * slap_oscillator(v->phase * 5.42f);
                wave = 0.57f * wave + 0.37f * plate;
            }
        }
        if (layers & DRUM_TAPE) {
            float rounded = wave * (1.34f - 0.34f * wave * wave);
            wave = 0.78f * rounded + 0.22f * v->drum_noise * v->bend;
        }
        if (layers & DRUM_FOLD) {
            float driven = 2.1f * wave;
            wave = 0.53f * wave + 0.47f * (fm_clampf(driven, -1.0f, 1.0f) * 2.0f - driven);
        }
        v->fall *= v->drum_decay;
    }
    v->phase += inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    if (v->fall < 0.00005f) { v->age = 0; return 0.0f; }
    return wave * v->env * v->velocity * v->fall * 0.15f;
}

#ifndef PLAT_HOST
__attribute__((section(".c1_text.slap_voice"), noinline))
#endif
static float render_voice(Voice *v, SlapSynth *s)
{
    float sample = render_voice_signal(v, s);
    if (v->steal_tail != 0.0f) {
        sample += v->steal_tail;
        v->steal_tail *= 0.94f;
        if (fm_fabsf(v->steal_tail) < 0.00001f) v->steal_tail = 0.0f;
    }
    v->last_sample = sample;
    return sample;
}

#ifdef PLAT_HOST
#define dsp_render_voice render_voice
#else
static float (*volatile dsp_render_voice)(Voice *, SlapSynth *) = render_voice;
#endif

enum { VOICE_BATCH = 64 };
static float voice_bus[TRACKS][VOICE_BATCH];

static void clear_voice_bus(float bus[][VOICE_BATCH], unsigned frames)
{
    if (frames == VOICE_BATCH) {
        __builtin_memset(bus, 0, TRACKS * sizeof(*bus));
    } else {
        for (unsigned track = 0; track < TRACKS; ++track)
            __builtin_memset(bus[track], 0, frames * sizeof(float));
    }
}

#ifndef PLAT_HOST
__attribute__((section(".c1_text.slap_voice"), noinline))
#endif
static void render_voice_into_bus(Voice *voice, SlapSynth *synth, unsigned muted,
                                  float bus[][VOICE_BATCH], unsigned frames)
{
    unsigned track = voice->track;
    int audible = track != DRUMS || !(muted & (1u << voice->drum_family));
    float *output = bus[track];
    unsigned i = 0;
    // Between events a sustained, unmodulated synth voice only changes its
    // oscillator, envelope and gate countdown. Keep the envelope in registers
    // and hand the exact release sample back to the general renderer.
    if (frames && track < DRUMS && voice->age && voice->struck &&
        !voice->releasing && !voice->delay && !voice->lfo_wave && voice->steal_tail == 0.0f) {
        unsigned sustain_frames = frames;
        if (!voice->manual && voice->remaining && sustain_frames >= voice->remaining)
            sustain_frames = voice->remaining - 1u;
        float env = voice->env, sample = voice->last_sample;
        const float target = voice->sustain, rate = voice->decay, velocity = voice->velocity;
        for (; i < sustain_frames; ++i) {
            env += (target - env) * rate;
            sample = slap_synth_sample(synth) * env * velocity * 0.18f;
            output[i] += sample;
        }
        voice->env = env;
        voice->last_sample = sample;
        if (!voice->manual && voice->remaining) voice->remaining -= i;
    }
    for (; i < frames && voice->age; ++i) {
        float sample = render_voice(voice, synth);
        if (audible) output[i] += sample;
    }
}

#ifdef PLAT_HOST
#define dsp_render_voice_into_bus render_voice_into_bus
#else
static void (*volatile dsp_render_voice_into_bus)(Voice *, SlapSynth *, unsigned, float (*)[VOICE_BATCH], unsigned)
    = render_voice_into_bus;
#endif

static void render_local_voice_bus(const unsigned *slots, unsigned active, unsigned remote,
                                   int ordered_rng, unsigned muted, unsigned frames)
{
    clear_voice_bus(voice_bus, frames);
    if (ordered_rng) {
        // Sample-and-hold shares the scheduler RNG and needs sample/slot order.
        for (unsigned i = 0; i < frames; ++i) {
            for (unsigned n = 0; n < active; ++n) {
                unsigned slot = slots[n];
                Voice *voice = &voices[slot];
                if (!voice->age || (remote & (1u << slot))) continue;
                float sample = dsp_render_voice(voice, &synths[slot]);
                if (voice->track != DRUMS || !(muted & (1u << voice->drum_family)))
                    voice_bus[voice->track][i] += sample;
            }
        }
    } else {
        for (unsigned n = 0; n < active; ++n) {
            unsigned slot = slots[n];
            if (!(remote & (1u << slot)))
                dsp_render_voice_into_bus(&voices[slot], &synths[slot], muted, voice_bus, frames);
        }
    }
}

#if FM1B_CPU2 && !defined(PLAT_WEB)
enum { MIN_VOICE_BATCH = 32, REMOTE_VOICES = VOICES / 2 };
_Static_assert((unsigned)VOICE_BATCH <= (unsigned)SLAP_FX_BATCH, "FX checkpoint must cover each voice span");
static struct {
    Voice voice[REMOTE_VOICES];
    unsigned slot[REMOTE_VOICES];
    unsigned count, muted, track_mask;
    float bus[TRACKS][VOICE_BATCH];
    uint32_t start, ticks;
} voice_worker __attribute__((aligned(32)));
static PLAT_POOL SlapSynth worker_synth[REMOTE_VOICES];
static float fx_bus[TRACKS][VOICE_BATCH] __attribute__((aligned(32)));
static unsigned fx_worker_tracks[SLAP_FX_WORKER_TRACKS], fx_worker_count;
static volatile uint32_t fx_worker_jobs, fx_worker_fallbacks, fx_worker_ticks;
static uint32_t fx_job_start, fx_job_ticks, worker_overlap_ticks;
static volatile uint32_t worker_jobs, worker_fallbacks, worker_ticks, worker_wait_ticks;
static volatile uint32_t voice_late_jobs, voice_rejected_jobs, voice_busy_jobs, fx_late_jobs;
static volatile uint32_t voice_short_spans, voice_budget_skips, voice_no_split;
static volatile uint32_t voice_drained_jobs, peak_voice_job_ticks;
static uint32_t voice_pending_since;
static unsigned voice_pending, voice_jobs_since_late = 16;

static void service_late_voice(uint32_t callback_budget)
{
    if (!voice_pending) return;
    if (plat_cpu2_busy()) {
        if (plat_ticks24() - voice_pending_since >= callback_budget) {
            plat_cpu2_retire_late();
            voice_pending = 0;
        }
        return;
    }
    // After an acknowledged completion the scratch belongs to Core 0 again.
    // Its contents are obsolete because Core 0 already advanced the live state.
    if (plat_cpu2_wait() == 0) {
        ++voice_drained_jobs;
        if (voice_worker.ticks > peak_voice_job_ticks) peak_voice_job_ticks = voice_worker.ticks;
    }
    voice_pending = 0;
}

static uint32_t worker_overlap(uint32_t local, uint32_t local_ticks,
                               uint32_t remote, uint32_t remote_ticks)
{
    // Differences are at most one audio callback, including across timer wrap.
    int32_t remote_offset = (int32_t)(remote - local);
    int32_t start = remote_offset > 0 ? remote_offset : 0;
    int32_t end = remote_offset + (int32_t)remote_ticks;
    if (end > (int32_t)local_ticks) end = (int32_t)local_ticks;
    return end > start ? (uint32_t)(end - start) : 0u;
}

static void render_worker_voices(uint32_t frames)
{
    if (frames > VOICE_BATCH) return;
    uint32_t started = plat_ticks24();
    for (unsigned track = 0; track < TRACKS; ++track)
        if (voice_worker.track_mask & (1u << track))
            __builtin_memset(voice_worker.bus[track], 0, frames * sizeof(float));
    for (unsigned i = 0; i < voice_worker.count; ++i)
        dsp_render_voice_into_bus(&voice_worker.voice[i], &worker_synth[i], voice_worker.muted,
                              voice_worker.bus, frames);
    voice_worker.start = started;
    voice_worker.ticks = plat_ticks24() - started;
}

static unsigned render_voice_batch(unsigned frames, unsigned muted,
                                   uint32_t callback_start, uint32_t budget)
{
    unsigned active_slots[VOICES], active = 0, total_cost = 0;
    unsigned eligible_by_cost[MAX_VOICE_COST + 1];
    int ordered_rng = 0;
    service_late_voice(budget);
    int worker_on = plat_cpu2_on();
    int mailbox_busy = worker_on && plat_cpu2_busy();
    if (mailbox_busy) ++voice_busy_jobs;
    int dispatch = 0;
    if (worker_on && !mailbox_busy && !voice_pending) {
        if (frames < MIN_VOICE_BATCH) ++voice_short_spans;
        else if (plat_ticks24() - callback_start >= budget * 2u / 3u) ++voice_budget_skips;
        else dispatch = 1;
    }
    if (dispatch) __builtin_memset(eligible_by_cost, 0, sizeof eligible_by_cost);
    for (unsigned j = 0; j < VOICES; ++j) {
        const Voice *voice = &voices[j];
        if (!voice->age) continue;
        active_slots[active++] = j;
        ordered_rng |= voice->track != DRUMS && voice->lfo_wave == 5;
        if (!dispatch) continue;
        unsigned hint = voice->dispatch_cost;
        unsigned cost = hint & (VOICE_LOCAL_ONLY - 1u);
        if (hint <= MAX_VOICE_COST) eligible_by_cost[cost] |= 1u << j;
        total_cost += cost;
    }
    // A callback already in single-core mode needs only one live mix bus.
    // A launched job always keeps both groups until it commits or recovers.
    unsigned remote = 0, remote_count = 0;
    if (dispatch && active > 1) {
        unsigned remaining = (total_cost * 7u + 8u) / 16u;
        // Keep two-voice chords eligible without offloading the entire workload.
        if (active == 2) remaining = (total_cost + 1u) / 2u;
        for (unsigned cost = MAX_VOICE_COST; cost && remote_count < REMOTE_VOICES; --cost) {
            unsigned slots = eligible_by_cost[cost];
            while (slots && cost <= remaining && remote_count < REMOTE_VOICES) {
                unsigned slot_bit = slots & (0u - slots);
                slots ^= slot_bit;
                remote |= slot_bit;
                remaining -= cost;
                ++remote_count;
            }
        }
    }

    if (dispatch && active > 1 && !remote) ++voice_no_split;
    int launched = 0;
    if (remote) {
        voice_worker.count = 0;
        voice_worker.muted = muted;
        voice_worker.track_mask = 0;
        for (unsigned n = 0; n < active; ++n) {
            unsigned slot = active_slots[n];
            if (!(remote & (1u << slot))) continue;
            unsigned i = voice_worker.count++;
            voice_worker.slot[i] = slot;
            voice_worker.voice[i] = voices[slot];
            voice_worker.track_mask |= 1u << voices[slot].track;
            if (voices[slot].track != DRUMS) {
                worker_synth[i] = synths[slot];
                worker_synth[i].string = 0;
            }
        }
        launched = plat_cpu2_run(render_worker_voices, frames);
        if (!launched) ++voice_rejected_jobs;
    }

    uint32_t local_start = launched ? plat_ticks24() : 0;
    render_local_voice_bus(active_slots, active, remote, ordered_rng, muted, frames);
    if (!remote) return frames;

    int completed = 0;
    uint32_t recovery_start = 0;
    if (launched) {
        uint32_t wait_start = plat_ticks24();
        int busy;
        while ((busy = plat_cpu2_busy()) &&
               plat_ticks24() - wait_start < 240u * 24u &&
               plat_ticks24() - callback_start < budget * 3u / 4u) {}
        worker_wait_ticks += plat_ticks24() - wait_start;
#ifdef PLAT_HOST
        if (host_late_voice > 0 && worker_jobs >= 4u) {
            --host_late_voice;
            busy = 1;
        }
        int discard = host_discard_worker_result;
#else
        int discard = 0;
#endif
        completed = !busy && !discard && plat_cpu2_wait() == 0;
        if (completed) {
            worker_overlap_ticks += worker_overlap(local_start, wait_start - local_start,
                                                   voice_worker.start, voice_worker.ticks);
        } else {
            recovery_start = plat_ticks24();
            if (busy) {
                ++voice_late_jobs;
                if (voice_jobs_since_late < 16) plat_cpu2_retire_late();
                else {
                    voice_pending = 1;
                    voice_pending_since = recovery_start;
                }
                voice_jobs_since_late = 0;
            } else {
                ++voice_rejected_jobs;
                if (plat_cpu2_on()) plat_cpu2_set(0);
            }
            ++worker_fallbacks;
        }
    }
    if (completed) {
        ++worker_jobs;
        if (voice_jobs_since_late < 16) ++voice_jobs_since_late;
        if (voice_worker.ticks > peak_voice_job_ticks) peak_voice_job_ticks = voice_worker.ticks;
        worker_ticks += voice_worker.ticks;
        for (unsigned i = 0; i < voice_worker.count; ++i) {
            unsigned slot = voice_worker.slot[i];
            voices[slot] = voice_worker.voice[i];
            if (voices[slot].track != DRUMS) {
                synths[slot] = worker_synth[i];
                synths[slot].string = synth_strings[slot];
            }
        }
    } else {
        // The worker may still own its private scratch. Restore its share
        // from the original live state and write only to the local bus.
        for (unsigned n = 0; n < active; ++n) {
            unsigned slot = active_slots[n];
            if (remote & (1u << slot))
                dsp_render_voice_into_bus(&voices[slot], &synths[slot], muted, voice_bus, frames);
        }
        if (launched) {
            uint32_t recovery = plat_ticks24() - recovery_start;
            if (recovery > peak_voice_recovery) peak_voice_recovery = recovery;
        }
    }
    if (completed) {
        for (unsigned t = 0; t < TRACKS; ++t) {
            if (!(voice_worker.track_mask & (1u << t))) continue;
            for (unsigned i = 0; i < frames; ++i)
                voice_bus[t][i] += voice_worker.bus[t][i];
        }
    }
    return frames;
}

static unsigned select_fx_worker_tracks(void)
{
    unsigned cost[TRACKS], total = 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        cost[t] = slap_fx_work(t);
        total += cost[t];
    }
    unsigned best = 0, difference = total;
    // Keep track zero on Core 0 as the sole owner of the FX display clock.
    for (unsigned a = 1; a < TRACKS; ++a) {
        if (!cost[a]) continue;
        for (unsigned b = a; b < TRACKS; ++b) {
            unsigned remote = cost[a] + (a == b ? 0 : cost[b]);
            if (remote < 2u || total - remote < 2u) continue;
            unsigned delta = remote > total - remote ? 2u * remote - total : total - 2u * remote;
            if (delta < difference) {
                difference = delta;
                best = (1u << a) | (1u << b);
            }
        }
    }
    return best;
}

static void render_track_fx(unsigned track, unsigned frames)
{
    for (unsigned i = 0; i < frames; ++i)
        fx_bus[track][i] = dsp_fx_sample(track, voice_bus[track][i]);
}

static void render_worker_fx(uint32_t frames)
{
    uint32_t start = plat_ticks24();
    for (unsigned slot = 0; slot < fx_worker_count; ++slot)
        render_track_fx(fx_worker_tracks[slot], frames);
    fx_job_start = start;
    fx_job_ticks = plat_ticks24() - start;
}

static unsigned render_fx_batch(unsigned frames, unsigned mask, unsigned active, uint32_t start, uint32_t budget)
{
    if (!mask || voice_pending || !plat_cpu2_on() || plat_cpu2_busy() || plat_ticks24() - start >= budget * 3u / 4u) return 0;
    fx_worker_count = 0;
    for (unsigned t = 1; t < TRACKS; ++t) {
        if (!(mask & (1u << t))) continue;
        unsigned slot = fx_worker_count++;
        fx_worker_tracks[slot] = t;
        slap_fx_checkpoint(slot, t, frames);
    }
    if (!plat_cpu2_run(render_worker_fx, frames)) return 0;
    uint32_t local_start = plat_ticks24();
    for (unsigned t = 0; t < TRACKS; ++t)
        if ((active & ~mask) & (1u << t)) render_track_fx(t, frames);

    uint32_t wait_start = plat_ticks24();
    int busy;
    while ((busy = plat_cpu2_busy()) &&
           plat_ticks24() - wait_start < 240u * 24u &&
           plat_ticks24() - start < budget * 3u / 4u) {}
    worker_wait_ticks += plat_ticks24() - wait_start;
#ifdef PLAT_HOST
    busy |= host_discard_fx_result && fx_worker_jobs == 128u;
#endif
    if (busy || plat_cpu2_wait() != 0) {
        // Stop the worker before restoring any state or reusing its output bus.
        uint32_t recovery_start = plat_ticks24();
        if (busy) plat_cpu2_retire_late();
        else if (plat_cpu2_on()) plat_cpu2_set(0);
        ++fx_worker_fallbacks;
        if (busy) ++fx_late_jobs;
        for (unsigned slot = 0; slot < fx_worker_count; ++slot)
            slap_fx_rollback(slot);
        render_worker_fx(frames);
        uint32_t recovery = plat_ticks24() - recovery_start;
        if (recovery > peak_fx_recovery) peak_fx_recovery = recovery;
    } else {
        worker_overlap_ticks += worker_overlap(local_start, wait_start - local_start,
                                               fx_job_start, fx_job_ticks);
        ++fx_worker_jobs;
        fx_worker_ticks += fx_job_ticks;
    }
    return frames;
}

#else

static unsigned render_voice_batch(unsigned frames, unsigned muted,
                                   uint32_t callback_start, uint32_t budget)
{
    (void)callback_start;
    (void)budget;
    unsigned slots[VOICES], active = 0;
    int ordered_rng = 0;
    for (unsigned j = 0; j < VOICES; ++j) {
        if (!voices[j].age) continue;
        slots[active++] = j;
        ordered_rng |= voices[j].track != DRUMS && voices[j].lfo_wave == 5;
    }
    render_local_voice_bus(slots, active, 0, ordered_rng, muted, frames);
    return frames;
}

#endif

/* Two rising tones with independent smooth envelopes, mixed just above silence. */
static float startup_sound(void)
{
    enum { TOTAL = RATE * 42 / 100, SECOND = RATE * 15 / 100 };
    uint32_t frame = startup_sample++;
    unsigned local = frame >= SECOND ? frame - SECOND : frame;
    unsigned duration = frame >= SECOND ? TOTAL - SECOND : SECOND;
    float position = (float)local / duration;
    float envelope = 16.0f * position * position * (1.0f-position) * (1.0f-position);
    float frequency = frame >= SECOND ? 440.0f : 293.665f;
    startup_phase += (frequency + 18.0f*position) / RATE;
    if (startup_phase >= 1.0f) startup_phase -= 1.0f;
    return 0.036f * envelope * (slap_sine(startup_phase) +
           0.19f * slap_sine(startup_phase * 2.0f));
}

void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    uint32_t audio_started = plat_ticks24();
    unsigned profile = slap_ui.page == GLOBAL && global_details == 2;
    uint32_t serial[PROFILE_COUNT] = {0};
    if (dsp_reset_request) {
        dsp_peak_ticks = dsp_avg_ticks = dsp_overruns = dsp_callbacks = 0;
        peak_voice_batch = peak_fx_batch = peak_worker_work = peak_worker_wait = 0;
        serial_peak = 0;
        for (unsigned part = 0; part < PROFILE_COUNT; ++part) serial_at_peak[part] = 0;
        dsp_xrun_baseline = plat_xruns();
#if FM1B_CPU2 && !defined(PLAT_WEB)
        worker_jobs = worker_fallbacks = worker_ticks = worker_wait_ticks = 0;
        fx_worker_jobs = fx_worker_fallbacks = fx_worker_ticks = 0;
        worker_overlap_ticks = peak_worker_overlap = peak_voice_recovery = peak_fx_recovery = 0;
        voice_late_jobs = voice_rejected_jobs = voice_busy_jobs = fx_late_jobs = 0;
        voice_short_spans = voice_budget_skips = voice_no_split = 0;
        voice_drained_jobs = peak_voice_job_ticks = 0;
#endif
        for (unsigned bin = 0; bin < DSP_BINS; ++bin) dsp_histogram[bin] = 0;
        dsp_reset_request = 0;
    }
    uint32_t mix[TRACKS], solo = mixer_solo;
    float left[TRACKS], right[TRACKS], level_at[TRACKS], wet[TRACKS];
    unsigned automatic[TRACKS], gain_needed[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t) {
        mix[t] = tracks[t].mix;
        float pan = ((mix[t] >> 8) & 255u) / 255.0f;
        float level = ((mix[t] & (1u << 24)) || (solo && !(solo & (1u << t)))) ? 0.0f :
                      ((mix[t] >> 16) & 255u) / 255.0f;
        level_at[t] = level;
        wet[t] = mixer_proc[t] * (1.0f / 255.0f);
        automatic[t] = mixer_auto[t];
        gain_needed[t] = automatic[t] || mixer_gain[t] != 1.0f;
        left[t] = level * (1.0f - pan);
        right[t] = level * pan;
    }
    uint32_t epoch = sequence_epoch;
    if (running_epoch != epoch) {
        running_epoch = epoch;
        for (unsigned t = 0; t < TRACKS; ++t) monitored_next_step[t] = 0;
        until_step = step_number = arp_wait = arp_pulse = 0;
        for (unsigned i = 0; i < VOICES; ++i)
            if (!voices[i].manual) voices[i].releasing = 1;
    }
    if (!recording && rec_running) end_recording(record_clock);
    if (!recording) rec_countdown = 0;
    if (recording && playing && rec_loop && !rec_start_mode && !rec_running) begin_recording();
    read_notes();
    if (rec_running) mix[rec_track] = tracks[rec_track].mix;
    if (!playing) click_remaining = 0;
    slap_fx_begin(tempo);
    const unsigned muted = muted_drums();
    unsigned measure_fx = slap_ui.page == FX && slap_ui.fx_page == 4;
    const unsigned analyze = slap_ui.page == VISUAL;
    const unsigned inspect_fx = slap_ui.page == FX || slap_ui.page == PATCH;
    const unsigned effect_mask = inspect_fx ? (1u << TRACKS) - 1u : slap_fx_active_mask();
    float meter_sum[2] = {0}, meter_peak[2] = {0};
    float target = master_q12 * (1.0f / 4096.0f);
    unsigned batch_start = 0, batch_end = 0;
    uint32_t callback_budget = (24000000u / RATE) * frames;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    unsigned fx_batch_end = 0;
    unsigned fx_mask = measure_fx || !plat_cpu2_on() ? 0 : select_fx_worker_tracks();
    uint32_t voice_batch_ticks = 0, fx_batch_ticks = 0;
    uint32_t remote_start = worker_ticks + fx_worker_ticks, wait_start = worker_wait_ticks;
    uint32_t overlap_start = worker_overlap_ticks;
#endif
    if (profile) serial[PROFILE_INPUT] = plat_ticks24() - audio_started;
    for (uint32_t i = 0; i < frames; ++i) {
        uint32_t section = profile ? plat_ticks24() : 0;
        if (playing && until_step == 0) {
            if (recording && !rec_running && rec_start_mode) {
                if (step_number < 4) {
                    click_remaining = CLICK_SAMPLES;
                    click_accent = step_number == 0;
                    click_phase = 0;
                    rec_countdown = 4u - step_number++;
                    until_step = RATE * 60u / tempo;
                } else {
                    step_number = rec_countdown = 0;
                    begin_recording();
                    mix[rec_track] = tracks[rec_track].mix;
                }
            }
            if (rec_running && !rec_loop && step_number >= rec_length) {
                end_recording(record_clock + i);
                recording = 0;
            }
            if (!recording || rec_running) next_step(mix);
        }
        if (until_step) --until_step;
        arp_render_clock();
        if (profile) serial[PROFILE_EVENTS] += plat_ticks24() - section;
        if (i == batch_end) {
            unsigned amount = frames - i;
            if (amount > VOICE_BATCH) amount = VOICE_BATCH;
            if (playing && amount > until_step + 1u) amount = until_step + 1u;
            // The current sample's arpeggio tick has already run. End this
            // batch before the next tick can allocate or steal a voice.
            if (old_keys && input_track < DRUMS && (arp_setup[input_track] & 255u) &&
                amount > arp_wait + 1u)
                amount = arp_wait + 1u;
            batch_start = i;
#if FM1B_CPU2 && !defined(PLAT_WEB)
            uint32_t stage_start = plat_ticks24();
#else
            uint32_t stage_start = profile ? plat_ticks24() : 0;
#endif
            batch_end = i + render_voice_batch(amount, muted, audio_started, callback_budget);
#if FM1B_CPU2 && !defined(PLAT_WEB)
            uint32_t voice_end = plat_ticks24();
            voice_batch_ticks += voice_end - stage_start;
            if (profile) serial[PROFILE_VOICES] += voice_end - stage_start;
            fx_batch_end = i;
            if (fx_mask) {
                fx_batch_end += render_fx_batch(amount, fx_mask, effect_mask, audio_started, callback_budget);
                fx_batch_ticks += plat_ticks24() - voice_end;
            }
#else
            if (profile) serial[PROFILE_VOICES] += plat_ticks24() - stage_start;
#endif
        }
        float l = 0.0f, r = 0.0f;
        for (unsigned t = 0; t < TRACKS; ++t) {
            unsigned sample_cost = measure_fx && i == 0u;
            float sample = voice_bus[t][i - batch_start];
            if (effect_mask & (1u << t)) {
                float processed;
#if FM1B_CPU2 && !defined(PLAT_WEB)
                if (i < fx_batch_end) processed = fx_bus[t][i - batch_start];
                else
#endif
                {
                    uint32_t started = sample_cost || profile ? plat_ticks24() : 0u;
                    processed = dsp_fx_sample(t, sample);
                    if (profile || sample_cost) {
                        uint32_t ticks = plat_ticks24() - started;
                        if (profile) serial[PROFILE_FX] += ticks;
                        if (sample_cost) fx_cost_ticks[t] = (7u * fx_cost_ticks[t] + ticks + 4u) / 8u;
                    }
                }
                sample += wet[t] * (processed - sample);
            }
            if (profile) section = plat_ticks24();
            float amplitude = fm_fabsf(sample);
            mixer_envelope[t] += (amplitude - mixer_envelope[t]) *
                (amplitude > mixer_envelope[t] ? 0.003f : 0.00018f);
            if (gain_needed[t]) {
                float goal = automatic[t] && mixer_envelope[t] > 0.003f ?
                    fm_maxf(0.45f, fm_minf(2.0f, 0.025f / mixer_envelope[t])) : 1.0f;
                mixer_gain[t] += (goal - mixer_gain[t]) * 0.0008f;
                if (!automatic[t] && fm_fabsf(1.0f - mixer_gain[t]) < 0.00005f)
                    mixer_gain[t] = 1.0f;
                sample *= mixer_gain[t];
                amplitude = fm_fabsf(sample);
            }
            energy_smooth[t] += (amplitude * level_at[t] - energy_smooth[t]) * 0.0015f;
            l += sample * left[t];
            r += sample * right[t];
            if (profile) serial[PROFILE_MIX] += plat_ticks24() - section;
        }
        if (profile) section = plat_ticks24();
        if (!intro_skipped && !playing && startup_sample < RATE * 42u / 100u) {
            float tone = startup_sound();
            l += tone;
            r += tone;
        } else if (playing) {
            startup_sample = RATE;
        }
        if (click_remaining) {
            unsigned half_cycle = click_accent ? 11u : 17u;
            if (++click_phase >= 2u * half_cycle) click_phase = 0;
            float click = (click_accent ? 0.13f : 0.085f) * (click_remaining-- / (float)CLICK_SAMPLES);
            if (click_phase >= half_cycle) click = -click;
            l += click;
            r += click;
        }
        master_gain += (target - master_gain) * 0.003f;
        l /= 1.0f + fm_fabsf(l);
        r /= 1.0f + fm_fabsf(r);
        // The visual bus represents the same limited mix at full master gain.
        // Track mute, solo and level have already been applied.
        if (analyze) {
            meter_sum[0] += l * l;
            meter_sum[1] += r * r;
            meter_peak[0] = fm_maxf(meter_peak[0], fm_fabsf(l));
            meter_peak[1] = fm_maxf(meter_peak[1], fm_fabsf(r));
            capture_spectrum(l, r);
        }
        l *= master_gain;
        r *= master_gain;
        lr[i * 2] = (int32_t)(l * 8300000.0f);
        lr[i * 2 + 1] = (int32_t)(r * 8300000.0f);
        if (profile) serial[PROFILE_OUTPUT] += plat_ticks24() - section;
    }
    record_clock += frames;
    if (analyze && frames) {
        for (unsigned channel = 0; channel < 2; ++channel) {
            float alpha = fm_minf(1.0f, frames * (1.0f / (RATE * 0.08f)));
            meter_power[channel] += (meter_sum[channel] / frames - meter_power[channel]) * alpha;
            visual_rms[channel] = meter_power[channel];
            float decay = fm_maxf(0.0f, 1.0f - frames * (1.0f / (RATE * 0.40f)));
            visual_peak[channel] = fm_maxf(visual_peak[channel] * decay, meter_peak[channel]);
        }
    }
    step_progress = playing && step_duration && until_step <= step_duration ? 14u - until_step * 14u / step_duration : 0u;
    for (unsigned t = 0; t < TRACKS; ++t) {
        track_energy[t] = (uint32_t)fm_minf(160.0f, energy_smooth[t] * 1850.0f);
        uint32_t peak = track_peak[t];
        if (peak) --peak;
        track_peak[t] = track_energy[t] > peak ? track_energy[t] : peak;
    }
    if (frames) {
        uint32_t elapsed = plat_ticks24() - audio_started;
        uint32_t budget = (24000000u / RATE) * frames + (24000000u % RATE) * frames / RATE;
        dsp_budget_ticks = budget;
        unsigned bin = elapsed / DSP_BIN_TICKS;
        if (bin >= DSP_BINS) bin = DSP_BINS - 1u;
        if (!profile) {
            ++dsp_histogram[bin];
            ++dsp_callbacks;
            if (elapsed > budget) ++dsp_overruns;
        }
        if (profile && elapsed > serial_peak) {
            serial_peak = elapsed;
            for (unsigned part = 0; part < PROFILE_COUNT; ++part) serial_at_peak[part] = serial[part];
        }
        if (!profile && elapsed > dsp_peak_ticks) {
#if FM1B_CPU2 && !defined(PLAT_WEB)
            peak_voice_batch = voice_batch_ticks;
            peak_fx_batch = fx_batch_ticks;
            peak_worker_work = worker_ticks + fx_worker_ticks - remote_start;
            peak_worker_wait = worker_wait_ticks - wait_start;
            peak_worker_overlap = worker_overlap_ticks - overlap_start;
#endif
            dsp_peak_ticks = elapsed;
        }
        if (!profile) dsp_avg_ticks = dsp_avg_ticks - dsp_avg_ticks / 8u + elapsed / 8u;
        unsigned active = 0;
        for (unsigned v = 0; v < VOICES; ++v) active += voices[v].age != 0;
        active_voices = active;
    }
}

void app_panic(void)
{
    playing = 0;
    startup_sample = RATE;
    for (unsigned i = 0; i < VOICES; ++i) voices[i].age = 0;
    master_gain = 0.0f;
}

#define SONG_MAGIC 0x35504c53u
typedef struct {
    uint32_t magic, version, bpm, swing;
    struct {
        uint32_t mix, patch, envelope, harmony, lfo;
        uint32_t steps[LEGACY_STEPS], fx[SLAP_FX_PAGES];
    } track[TRACKS];
    uint32_t extra[DRUMS][LEGACY_STEPS];
    uint8_t drums[DRUM_LANES][LEGACY_STEPS][3];
    uint32_t filters;
    uint32_t arp[DRUMS];
} SongRecord;

typedef char SongRecordFitsFlash[(sizeof(SongRecord) <= 3840) ? 1 : -1];

static SongRecord song_io;
static const uint8_t save_keys[16] = PLAT_WHITE_KEYS;

static int step_valid(uint32_t word)
{
    return !word || ((word & ACTIVE) && ((word >> 8) & 127u) &&
                      ((word >> 16) & 255u) && ((word >> 24) & 127u) <= 100);
}

/* V5 is indexed and sparse. V2–V4 retain their exact byte layout. */
enum { SONG_MAX = 3840, SONG5_HEADER = 316, SONG5_EVENT = 6 };
static uint8_t song_bytes[SONG_MAX];

static uint32_t song_get32(unsigned at)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)song_bytes[at + i] << (8u * i);
    return value;
}

static void song_put32(unsigned at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) song_bytes[at + i] = (uint8_t)(value >> (8u * i));
}

static volatile uint32_t *song_cell(unsigned row, unsigned step)
{
    if (row < 8u) return event_cell(row / 2u, step, row % 2u);
    if (row < SONG5_LANES) return event_cell(DRUMS, step, row - 8u);
    unsigned extra = row - SONG5_LANES;
    return event_cell(extra / (MELODIC_LANES - 2u), step, 2u + extra % (MELODIC_LANES - 2u));
}

static unsigned song5_fields(int writing)
{
    unsigned at = 16;
#define FIELD(word) do { if (writing) song_put32(at, word); else word = song_get32(at); at += 4; } while (0)
    for (unsigned t = 0; t < TRACKS; ++t) {
        FIELD(song_io.track[t].mix);
        FIELD(song_io.track[t].patch);
        FIELD(song_io.track[t].envelope);
        FIELD(song_io.track[t].harmony);
        FIELD(song_io.track[t].lfo);
        for (unsigned page = 0; page < SLAP_FX_PAGES; ++page) FIELD(song_io.track[t].fx[page]);
    }
    FIELD(song_io.filters);
    for (unsigned t = 0; t < DRUMS; ++t) FIELD(song_io.arp[t]);
#undef FIELD
    return at;
}

static unsigned song5_bytes;

static int prepare_sparse_song(unsigned bytes, unsigned version)
{
    unsigned stride = version == 7u ? 8u : SONG5_EVENT;
    if (bytes < SONG5_HEADER || (bytes - SONG5_HEADER) % stride) return 0;
    unsigned previous = 0;
    for (unsigned at = SONG5_HEADER; at < bytes; at += stride) {
        unsigned packed = song_bytes[at] | (unsigned)song_bytes[at + 1] << 8;
        unsigned bits = version >= 6u ? 14u : 13u;
        unsigned index = packed & ((1u << bits) - 1u);
        uint32_t word = song_get32(at + 2);
        unsigned row = index / STEPS;
        if ((packed >> bits) > 4u || row >= (version >= 6u ? SONG_LANES : SONG5_LANES) ||
            (at != SONG5_HEADER && index <= previous) || !word || !step_valid(word))
            return 0;
        if (row > 8u && row < SONG5_LANES && (word & NOTES) != lane_pitch[row - 9u]) return 0;
        if (version == 7u && row >= 8u && row < SONG5_LANES &&
            (song_bytes[at + 6] || song_bytes[at + 7])) return 0;
        previous = index;
    }
    song_io.magic = song_get32(0);
    song_io.version = version;
    song_io.bpm = song_get32(8);
    song_io.swing = song_get32(12);
    if (song5_fields(0) != SONG5_HEADER) return 0;
    song5_bytes = bytes;
    return 1;
}

static unsigned encode_sparse_song(unsigned version)
{
    unsigned stride = version == 7u ? 8u : SONG5_EVENT;
    song_put32(0, SONG_MAGIC);
    song_put32(4, version);
    song_put32(8, song_io.bpm);
    song_put32(12, song_io.swing);
    unsigned at = song5_fields(1);
    for (unsigned row = 0; row < (version >= 6u ? SONG_LANES : SONG5_LANES); ++row) {
        for (unsigned step = 0; step < STEPS; ++step) {
            uint32_t word = *song_cell(row, step);
            if (!word) continue;
            if (at > SONG_MAX - stride) return 0;
            unsigned index = row * STEPS + step;
            if (version >= 6u) {
                unsigned offset = note_timing(row, step, word);
                index |= (offset / 4u) << 14;
                word = (word & ~timing_bits(3u)) | timing_bits(offset);
            } else index |= (unsigned)timing_group[row][step] << 13;
            song_bytes[at] = (uint8_t)index;
            song_bytes[at + 1] = (uint8_t)(index >> 8);
            song_put32(at + 2, word);
            if (version == 7u) {
                unsigned duration = 0;
                if (row < 8u || row >= SONG5_LANES) {
                    unsigned track = row < 8u ? row / 2u : (row - SONG5_LANES) / 2u;
                    duration = melodic_duration[track][melodic_lane_from_row(track, row)][step];
                }
                song_bytes[at + 6] = (uint8_t)duration;
                song_bytes[at + 7] = (uint8_t)(duration >> 8);
            }
            at += stride;
        }
    }
    return at;
}

static int read_song(unsigned slot)
{
    int bytes = plat_store_load(OBJ_SONG1 + slot, song_bytes, sizeof(song_bytes));
    if (bytes < 8) return 0;
    unsigned version = song_get32(4);
    if (version >= 5u && version <= 7u) {
        if (!prepare_sparse_song((unsigned)bytes, version)) return 0;
    } else {
        if ((unsigned)bytes > sizeof(song_io)) return 0;
        for (int i = 0; i < bytes; ++i) ((uint8_t *)&song_io)[i] = song_bytes[i];
    }
    int v2bytes = (int)((uint8_t *)&song_io.extra - (uint8_t *)&song_io);
    int v3bytes = (int)((uint8_t *)&song_io.arp - (uint8_t *)&song_io);
    if (song_io.magic != SONG_MAGIC ||
        !((song_io.version == 2 && bytes == v2bytes) ||
          (song_io.version == 3 && bytes == v3bytes) ||
          (song_io.version == 4 && bytes == (int)sizeof(song_io)) || (song_io.version >= 5u && song_io.version <= 7u)) ||
        song_io.bpm < 40 || song_io.bpm > 240 || song_io.swing > 70) return 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        uint32_t mix = song_io.track[t].mix;
        if (!pattern_length(mix) || pattern_length(mix) > (song_io.version >= 5u ? STEPS : LEGACY_STEPS) || (mix >> (t == DRUMS ? 27 : 28)) ||
            song_io.track[t].patch >= (t == DRUMS ? DRUM_KITS : SLAP_PATCHES) || (song_io.track[t].envelope & 0x80808080u)) return 0;
        uint32_t h = song_io.track[t].harmony, l = song_io.track[t].lfo;
        if ((h & 255u) >= HARMONY_KEYS || ((h >> 8) & 255u) >= HARMONY_SCALES ||
            ((h >> 16) & 255u) >= HARMONY_CHORDS || (h >> 24) > 101u ||
            (l & 0x80808080u) || (l & 255u) > 5u) return 0;
        if (song_io.version < 5u)
            for (unsigned i = 0; i < LEGACY_STEPS; ++i)
                if (!step_valid(song_io.track[t].steps[i])) return 0;
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            if (!slap_fx_config_valid(i, song_io.track[t].fx[i])) return 0;
    }
    if (song_io.version >= 3 && (song_io.filters & ~0x3f3fu)) return 0;
    if (song_io.version == 3 || song_io.version == 4) {
        for (unsigned t = 0; t < DRUMS; ++t)
            for (unsigned i = 0; i < LEGACY_STEPS; ++i)
                if (!step_valid(song_io.extra[t][i])) return 0;
        for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
            for (unsigned i = 0; i < LEGACY_STEPS; ++i) {
                const uint8_t *d = song_io.drums[lane][i];
                uint32_t data = (uint32_t)d[0] << 8 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 24;
                if (data && !step_valid(data | ACTIVE | lane_pitch[lane])) return 0;
            }
        }
    }
    if (song_io.version >= 4) {
        for (unsigned t = 0; t < DRUMS; ++t) {
            uint32_t a = song_io.arp[t];
            unsigned octaves = (a >> 16) & 255u;
            if ((a & 255u) > 6u || ((a >> 8) & 255u) > 5u ||
                octaves < 1u || octaves > 4u || (a >> 24) > 100u) return 0;
        }
    }
    return 1;
}

static void load_song(unsigned slot)
{
    if (!read_song(slot)) { slap_ui.notice = "EMPTY OR INVALID"; return; }
    tempo = song_io.bpm;
    swing = song_io.swing;
    for (unsigned t = 0; t < TRACKS; ++t) {
        tracks[t].mix = song_io.track[t].mix;
        tracks[t].patch = song_io.track[t].patch;
        tracks[t].envelope = song_io.track[t].envelope;
        tracks[t].harmony = song_io.track[t].harmony;
        tracks[t].lfo = song_io.track[t].lfo;
        for (unsigned i = 0; i < LEGACY_STEPS; ++i)
            tracks[t].steps[i] = song_io.version >= 5u ? 0u : song_io.track[t].steps[i];
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            slap_fx_restore(t, i, song_io.track[t].fx[i]);
    }
    for (unsigned t = 0; t < DRUMS; ++t)
        for (unsigned i = 0; i < LEGACY_STEPS; ++i)
            extra_note[t][i] = song_io.version == 3 || song_io.version == 4 ? song_io.extra[t][i] : 0u;
    for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < LEGACY_STEPS; ++i) {
            const uint8_t *d = song_io.drums[lane][i];
            uint32_t data = song_io.version == 3 || song_io.version == 4 ?
                (uint32_t)d[0] << 8 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 24 : 0;
            drum_lane[lane][i] = data ? data | ACTIVE | lane_pitch[lane] : 0;
        }
    }
    drum_filters = song_io.version >= 3 ? song_io.filters : 0;
    for (unsigned t = 0; t < DRUMS; ++t)
        arp_setup[t] = song_io.version >= 4 ? song_io.arp[t] : ARP_DEFAULT;
    for (unsigned row = 0; row < SONG_LANES; ++row)
        for (unsigned step = 0; step < STEPS; ++step) {
            timing_group[row][step] = 0;
            if (step >= LEGACY_STEPS || row >= SONG5_LANES) *song_cell(row, step) = 0;
        }
    for (unsigned track = 0; track < DRUMS; ++track)
        for (unsigned lane = 0; lane < MELODIC_LANES; ++lane)
            for (unsigned step = 0; step < STEPS; ++step)
                melodic_duration[track][lane][step] = 0;
    if (song_io.version >= 5u) {
        unsigned stride = song_io.version == 7u ? 8u : SONG5_EVENT;
        for (unsigned at = SONG5_HEADER; at < song5_bytes; at += stride) {
            unsigned packed = song_bytes[at] | (unsigned)song_bytes[at + 1] << 8;
            unsigned bits = song_io.version >= 6u ? 14u : 13u;
            unsigned index = packed & ((1u << bits) - 1u);
            timing_group[index / STEPS][index % STEPS] =
                (uint8_t)((packed >> bits) + (song_io.version >= 6u));
            unsigned row = index / STEPS, step = index % STEPS;
            *song_cell(row, step) = song_get32(at + 2);
            if (song_io.version == 7u && (row < 8u || row >= SONG5_LANES)) {
                unsigned track = row < 8u ? row / 2u : (row - SONG5_LANES) / 2u;
                melodic_duration[track][melodic_lane_from_row(track, row)][step] =
                    (uint16_t)(song_bytes[at + 6] | (unsigned)song_bytes[at + 7] << 8);
            }
        }
    }
    take_undo.ready = 0;
    slap_ui.step = 0;
    ++sequence_epoch;
    slap_ui.notice = "LOADED";
}

static void write_song(unsigned slot)
{
    song_io.magic = SONG_MAGIC;
    song_io.version = 4;
    song_io.bpm = tempo;
    song_io.swing = swing;
    for (unsigned t = 0; t < TRACKS; ++t) {
        song_io.track[t].mix = tracks[t].mix;
        song_io.track[t].patch = tracks[t].patch;
        song_io.track[t].envelope = tracks[t].envelope;
        song_io.track[t].harmony = tracks[t].harmony;
        song_io.track[t].lfo = tracks[t].lfo;
        for (unsigned i = 0; i < LEGACY_STEPS; ++i)
            song_io.track[t].steps[i] = tracks[t].steps[i];
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            song_io.track[t].fx[i] = slap_fx_config(t, i);
    }
    for (unsigned t = 0; t < DRUMS; ++t)
        for (unsigned i = 0; i < LEGACY_STEPS; ++i) song_io.extra[t][i] = extra_note[t][i];
    for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < LEGACY_STEPS; ++i) {
            uint32_t word = drum_lane[lane][i];
            for (unsigned byte = 0; byte < 3; ++byte)
                song_io.drums[lane][i][byte] = (uint8_t)(word >> ((byte + 1u) * 8u));
        }
    }
    song_io.filters = drum_filters;
    for (unsigned t = 0; t < DRUMS; ++t) song_io.arp[t] = arp_setup[t];
    unsigned version = 4u;
    for (unsigned t = 0; t < TRACKS; ++t)
        if (pattern_length(tracks[t].mix) > LEGACY_STEPS) version = 5u;
    for (unsigned row = 0; row < SONG_LANES && version < 6u; ++row)
        for (unsigned step = 0; step < STEPS; ++step) {
            if (!*song_cell(row, step)) continue;
            if (row >= SONG5_LANES) {
                version = 6u;
                break;
            }
            if (step >= LEGACY_STEPS || timing_group[row][step]) version = 5u;
        }
    for (unsigned track = 0; track < DRUMS && version < 7u; ++track)
        for (unsigned lane = 0; lane < MELODIC_LANES && version < 7u; ++lane)
            for (unsigned step = 0; step < STEPS; ++step)
                if (melodic_duration[track][lane][step] && (*event_cell(track, step, lane) & ACTIVE)) {
                    version = 7u;
                    break;
                }
    unsigned bytes = version > 4u ? encode_sparse_song(version) : sizeof(song_io);
    if (!bytes) { slap_ui.notice = "BANK TOO DENSE"; return; }
    const void *payload = version > 4u ? (const void *)song_bytes : (const void *)&song_io;
    int result = plat_store_save(OBJ_SONG1 + slot, payload, bytes);
    slap_ui.notice = result == 0 && read_song(slot) ? "SAVED" : "SAVE FAILED";
}

static void save_input(uint32_t buttons, uint32_t pressed, uint32_t keys, uint32_t notes, int delta)
{
    unsigned now = plat_ms();
    if (delta) {
        slap_ui.slot = clamp(slap_ui.slot + delta, 0, 7);
        slap_ui.save_armed = 0;
        slap_ui.notice = 0;
    }
    unsigned target = 1u << save_keys[slap_ui.slot];
    if (pressed & (1u << B_SEL)) {
        if (playing) slap_ui.notice = "STOP TO LOAD";
        else load_song((unsigned)slap_ui.slot);
    }
    if (!delta && notes == target) {
        slap_ui.notice = playing ? "STOP TO SAVE" : "KEEP HOLDING";
        slap_ui.save_at = now;
        slap_ui.save_armed = !playing;
    } else if (keys != target || playing) {
        slap_ui.save_armed = 0;
    }
    if (slap_ui.save_armed && now - slap_ui.save_at >= 4000u) {
        slap_ui.save_armed = 0;
        write_song((unsigned)slap_ui.slot);
    }
    uint32_t dim = 0;
    for (unsigned i = 0; i < 8; ++i) dim |= 1u << save_keys[i];
    plat_lights(buttons | (playing << B_PLAY), now / 220u & 1u ? target : 0,
                0, dim & ~target, 0);
    keys_audio = 0;
}

static volatile uint32_t *event_cell(unsigned track, unsigned step, unsigned lane)
{
    if (!lane) return &tracks[track].steps[step];
    if (track == DRUMS) return &drum_lane[lane - 1u][step];
    return lane == 1u ? &extra_note[track][step] : &poly_note[track][lane - 2u][step];
}

static unsigned lane_count(unsigned track)
{
    return track == DRUMS ? DRUM_LANES + 1u : MELODIC_LANES;
}

static void generate_drums(void)
{
    if (!gen_enabled || playing) return;
    unsigned occupied = 0;
    unsigned length = pattern_length(tracks[DRUMS].mix);
    for (unsigned step = 0; step < length; ++step)
        for (unsigned lane = 0; lane < lane_count(DRUMS); ++lane)
            occupied |= *event_cell(DRUMS, step, lane) & ACTIVE;
    if (occupied && !gen_confirm) {
        gen_confirm = 1;
        return;
    }
    gen_confirm = 0;
    ++gen_count;
    if (take_undo.track == DRUMS) take_undo.ready = 0;
    const struct DrumStyle *style = &drum_style[gen_style];
    for (unsigned step = 0; step < length; ++step) {
        unsigned beat = step & 15u;
        unsigned bit = 1u << beat;
        unsigned fill = (step / 16u % 4u == 3u) && beat >= 13u;
        tracks[DRUMS].steps[step] = 0;
        for (unsigned lane = 0; lane < lane_count(DRUMS); ++lane)
            timing_group[pattern_row(DRUMS, lane)][step] = 0;
        for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
            unsigned anchor = (lane < 2) && (style->lanes[lane] & bit);
            unsigned chance = lane == 2 ? 30 + gen_hit * 70 / 100 : gen_hit;
            unsigned hit = anchor || ((style->lanes[lane] & bit) && next_random(&gen_seed) % 100u < chance);
            unsigned ghost = !hit && lane == 1 && ((style->ghost_snare & bit) || fill)
                             && next_random(&gen_seed) % 100u < gen_var / 2u;
            unsigned velocity = ghost ? 48u : lane == 0 ? 112u : lane == 1 ? 99u :
                64u + (!(beat & 3u) ? 20u : 0u);
            velocity += next_random(&gen_seed) % (1u + gen_var / 7u);
            unsigned offset = (beat & 1u) ? (3u * gen_groove + 50u) / 100u : 0u;
            drum_lane[lane][step] = hit || ghost ?
                cell_word(lane_pitch[lane], velocity, lane == 5 ? 154u : 105u, 100u) | timing_bits(offset) : 0u;
        }
    }
}

void app_input_only(void)
{
    uint32_t buttons = plat_buttons(), keys = plat_keys();
    uint32_t pressed = buttons & ~slap_ui.buttons, notes = keys & ~slap_ui.keys;
    if (slap_ui.booting) {
        if (plat_ms() - slap_ui.boot_at >= BOOT_MS) slap_ui.booting = 0;
        else {
            unsigned moved = 0;
            for (int role = 0; role < NE; ++role) {
                int v = plat_enc(role);
                if (v) moved = 1;
            }
            if (pressed || notes || (moved && plat_ms() - slap_ui.boot_at >= 95u)) {
                slap_ui.booting = 0;
                intro_skipped = 1;
                boot_suppressed_keys = keys;
                slap_ui.page = HOME;
                slap_ui.frame_at = plat_ms() - 45u;
            }
            slap_ui.buttons = buttons;
            slap_ui.keys = keys;
            return;
        }
    }
    boot_suppressed_keys &= keys;
    if (pressed & (1u << B_PLAY)) {
        playing ^= 1u;
        if (!playing) recording = 0;
        ++sequence_epoch;
    }
    const unsigned rec_button = 1u << B_REC;
    if (pressed & rec_button) {
        rec_gesture = !(slap_ui.page == SEQ && slap_ui.track == DRUMS &&
                        slap_ui.lane && (buttons & (1u << B_SEL)));
        rec_pressed_at = plat_ms();
    }
    if (rec_gesture && plat_ms() - rec_pressed_at >= 2000u) {
        rec_gesture = 0;
        slap_ui.page = RECORD;
    } else if (rec_gesture && !(buttons & rec_button)) {
        rec_gesture = 0;
        if (recording) {
            recording = 0;
            if (!rec_running) {
                playing = 0;
                ++sequence_epoch;
            }
        } else if (slap_ui.page == HOME) {
            rec_track = (unsigned)slap_ui.track;
            rec_length = pattern_length(tracks[rec_track].mix);
            rec_start_mode = !playing;
            rec_quant_setting = rec_quantize;
            rec_overwrite = 0;
            rec_loop = 1;
            if (!playing) {
                ++sequence_epoch;
                recording = 1;
                playing = 1;
            } else recording = 1;
        }
    }
    if ((pressed & (1u << B_SEL)) && slap_ui.page == RECORD && !(buttons & (1u << B_GLO))) {
        if (recording) {
            recording = 0;
            if (rec_countdown) ++sequence_epoch;
        } else if (!playing) {
            rec_track = (unsigned)slap_ui.track;
            rec_length = 16u << rec_bars;
            rec_start_mode = rec_count_in;
            rec_quant_setting = rec_quantize;
            rec_overwrite = rec_replace;
            rec_loop = 0;
            recording = 1;
        }
    }
    if (pressed & (1u << B_HOME))
        slap_ui.page = slap_ui.page == HOME ? VISUAL : HOME;
    if (pressed & (1u << B_SEQ)) {
        if (slap_ui.page == SEQ && slap_ui.track == DRUMS) {
            slap_ui.page = GEN_DRUMS;
            gen_confirm = 0;
        } else {
            slap_ui.page = SEQ;
            slap_ui.lane = 0;
        }
    }
    if (slap_ui.page == SEQ && (buttons & (1u << B_SEL))) {
        if (slap_ui.track == DRUMS && slap_ui.lane &&
            (pressed & ((1u << B_ARP) | (1u << B_REC)))) {
            unsigned bit = 1u << (slap_ui.lane - 1u);
            if (pressed & (1u << B_ARP)) drum_filters ^= bit;
            if (pressed & (1u << B_REC)) drum_filters ^= bit << 8;
        }
    }
    if ((buttons & (1u << B_GLO)) && (pressed & (1u << B_SEL))) {
        slap_ui.page = MIXER;
        pressed &= ~(1u << B_SEL);
    } else if (pressed & (1u << B_GLO)) {
        global_details = slap_ui.page == GLOBAL ? (global_details + 1u) % 7u : 0u;
        slap_ui.page = GLOBAL;
    }
    if (pressed & (1u << B_SAVE)) {
        slap_ui.page = SAVE;
        slap_ui.save_armed = 0;
        slap_ui.notice = 0;
    }
    if (pressed & (1u << B_FX)) slap_ui.page = FX;
    if (pressed & (1u << B_ARP)) {
        if (slap_ui.page == RECORD) undo_recording();
        else if (slap_ui.track < DRUMS && slap_ui.page != MIXER) slap_ui.page = ARP;
    }
    if (pressed & (1u << B_SEL) && slap_ui.page == HOME && slap_ui.track < DRUMS) slap_ui.page = HARMONY;
    if (pressed & (1u << B_LFO)) slap_ui.page = LFO;
    if (pressed & (1u << B_EDIT)) slap_ui.page = PATCH;
    if (pressed & (1u << B_ENV)) slap_ui.page = ENV;
    if (pressed & (1u << B_SEL) && slap_ui.page == MIXER) {
        unsigned mute = !(tracks[slap_ui.track].mix & (1u << 24));
        track_setting(slap_ui.track, 24, mute);
    }
    if ((pressed & (1u << B_ARP)) && slap_ui.page == MIXER)
        mixer_solo ^= 1u << slap_ui.track;
    if (pressed & (1u << B_OCTDN)) slap_ui.octave = clamp(slap_ui.octave - 1, 0, 4);
    if (pressed & (1u << B_OCTUP)) slap_ui.octave = clamp(slap_ui.octave + 1, 0, 4);
    int delta = plat_enc(EN_SELECT);
    if (slap_ui.page == SAVE) {
        for (int k = EN_ALGO; k < NE; ++k) (void)plat_enc(k);
        save_input(buttons, pressed, keys, notes, delta);
        slap_ui.keys = keys;
        slap_ui.buttons = buttons;
        return;
    }
    if (slap_ui.page == SEQ && (buttons & (1u << B_SEL)))
        slap_ui.lane = clamp(slap_ui.lane + delta, 0, (int)lane_count(slap_ui.track)-1);
    else if (slap_ui.page == SEQ)
        slap_ui.step = clamp(slap_ui.step + delta, 0, (int)(pattern_length(tracks[slap_ui.track].mix)) - 1);
    else if (slap_ui.page == VISUAL)
        slap_ui.visual = clamp(slap_ui.visual + delta, 0, VISUAL_COUNT - 1);
    else if (slap_ui.page == HOME || slap_ui.page == MIXER || (slap_ui.page == RECORD && !recording) ||
             slap_ui.page == PATCH || slap_ui.page == ENV || slap_ui.page == LFO || slap_ui.page == FX)
        slap_ui.track = clamp(slap_ui.track + delta, 0, TRACKS - 1);
    int algorithm = plat_enc(EN_ALGO);
    if (slap_ui.page == FX) slap_ui.fx_page = clamp(slap_ui.fx_page + algorithm, 0, SLAP_FX_PAGES - 1);
    else tempo = (uint32_t)clamp((int)tempo + algorithm, 40, 240);
    int patch_delta = plat_enc(EN_PRESET);

    if (patch_delta) {
        int old_patch = (int)tracks[slap_ui.track].patch;
        if (slap_ui.track == DRUMS)
            tracks[DRUMS].patch = select_drum_kit((unsigned)old_patch, patch_delta);
        else load_patch(slap_ui.track, (unsigned)clamp(old_patch + patch_delta, 0, SLAP_PATCHES-1));
    }
    int knob[4];
    for (unsigned k = 0; k < 4; ++k) knob[k] = plat_enc(EN_K1 + k);
    uint32_t mix = tracks[slap_ui.track].mix;
    if (slap_ui.page == GEN_DRUMS) {
        if (knob[0] || knob[1] || knob[2] || knob[3] || (pressed & (1u << B_ARP))) gen_confirm = 0;
        gen_style = (unsigned)clamp((int)gen_style + knob[0], 0, sizeof(drum_style) / sizeof(drum_style[0]) - 1);
        gen_hit = (unsigned)clamp((int)gen_hit + knob[1], 0, 100);
        gen_groove = (unsigned)clamp((int)gen_groove + knob[2], 0, 100);
        gen_var = (unsigned)clamp((int)gen_var + knob[3], 0, 100);
        if (pressed & (1u << B_ARP)) gen_enabled ^= 1u;
        if (pressed & (1u << B_SEL)) generate_drums();
    } else if (slap_ui.page == RECORD && !recording) {
        rec_bars = (unsigned)clamp((int)rec_bars + knob[0], 0, 5);
        rec_count_in = (unsigned)clamp((int)rec_count_in + knob[1], 0, 1);
        rec_quantize = (unsigned)clamp((int)rec_quantize - knob[2], 0, REC_QUANT_MAX);
        rec_replace = (unsigned)clamp((int)rec_replace + knob[3], 0, 1);
    } else if (slap_ui.page == MIXER) {
        unsigned t = (unsigned)slap_ui.track;
        track_setting(t, 16, (unsigned)clamp((int)((mix >> 16) & 255u) + knob[0] * 4, 0, 255));
        track_setting(t, 8, (unsigned)clamp((int)((mix >> 8) & 255u) + knob[1] * 4, 0, 255));
        mixer_auto[t] = (uint8_t)clamp((int)mixer_auto[t] + knob[2], 0, 1);
        mixer_proc[t] = (uint8_t)clamp((int)mixer_proc[t] + knob[3] * 4, 0, 255);
    } else if (slap_ui.page == PATCH) {
        unsigned t = (unsigned)slap_ui.track;
        if (t == DRUMS) tracks[t].patch = select_drum_kit(tracks[t].patch, knob[1]);
        else if (knob[0] || knob[1]) {
            int patch = (int)tracks[t].patch, engine = slap_patches[patch].engine;
            engine = clamp(engine + knob[0], 0, SLAP_ENGINES-1);
            int first = 0;
            while (first < SLAP_PATCHES-1 && slap_patches[first].engine != engine) ++first;
            int last = first;
            while (last < SLAP_PATCHES-1 && slap_patches[last+1].engine == engine) ++last;
            load_patch(t, (unsigned)clamp(clamp(patch, first, last) + knob[1], first, last));
        }
    } else if (slap_ui.page == GLOBAL) {
        tempo = (unsigned)clamp((int)tempo + knob[0], 40, 240);
        swing = (unsigned)clamp((int)swing + knob[1] * 2, 0, 70);
        if (knob[2]) dsp_reset_request = 1;
        unsigned mode = (unsigned)clamp((int)metronome + knob[3], 0, 2);
        if (mode != metronome) {
            metronome = mode;
            metronome_changed_at = plat_ms();
        }
    } else if (slap_ui.page == FX) {
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(slap_ui.track, slap_ui.fx_page, k, knob[k]);
    } else if (slap_ui.page == SEQ) {
        volatile uint32_t *cell = event_cell(slap_ui.track, slap_ui.step, slap_ui.lane);
        int fixed = slap_ui.track == DRUMS && slap_ui.lane;
        int editing_timing = buttons & (1u << B_SEL);
        unsigned row = pattern_row(slap_ui.track, slap_ui.lane);
        uint32_t word = *cell;
        uint32_t original_word = word;
        uint8_t original_timing = timing_group[row][slap_ui.step];
        if (notes) {
            timing_group[row][slap_ui.step] = 0;
            unsigned k = 0;
            while (!(notes & (1u << k))) ++k;
            unsigned pitch = fixed ? lane_pitch[slap_ui.lane - 1] : 29u + k + (unsigned)slap_ui.octave * 12u;
            word = (word & 255u) == (pitch | ACTIVE) ? 0u : cell_word(pitch, 100, 140, 100);
        }
        if (word & ACTIVE) {
            unsigned timing = timing_of(word);
            if (editing_timing && knob[3]) {
                unsigned offset = (unsigned)clamp((int)note_timing(row, slap_ui.step, word) + knob[3], 0, 15);
                timing_group[row][slap_ui.step] = (uint8_t)(offset / 4u + 1u);
                timing = offset;
            }
            word = cell_word((unsigned)clamp((int)(word & NOTES) + (fixed ? 0 : knob[0]), 0, 127),
                (unsigned)clamp((int)((word >> 8) & 127u) + knob[1] * 3, 1, 127),
                (unsigned)clamp((int)((word >> 16) & 255u) + knob[2] * 5, 1, 255),
                (unsigned)clamp((int)((word >> 24) & 127u) + (editing_timing ? 0 : knob[3] * 5), 0, 100)) |
                timing_bits(timing);
        }
        *cell = word;
        if (slap_ui.track < DRUMS && (notes || knob[2]))
            melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step] = 0;
        if (word != original_word || timing_group[row][slap_ui.step] != original_timing)
            ++pattern_revision[slap_ui.track];
    } else if (slap_ui.page == ARP) {
        static const uint8_t min[] = {0, 0, 1, 0}, max[] = {6, 5, 4, 100};
        uint32_t word = arp_setup[slap_ui.track];
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k*8u;
            unsigned value = (unsigned)clamp((int)((word >> shift) & 255u) + knob[k], min[k], max[k]);
            word = (word & ~(255u << shift)) | value << shift;
        }
        arp_setup[slap_ui.track] = word;
    } else if (slap_ui.page == HARMONY || (slap_ui.page == LFO && slap_ui.track < DRUMS)) {
        const int harmony_max[] = {HARMONY_KEYS-1,HARMONY_SCALES-1,HARMONY_CHORDS-1,101};
        const int lfo_max[] = {5,127,127,127};
        int harmony = slap_ui.page == HARMONY;
        volatile uint32_t *setting = harmony ? &tracks[slap_ui.track].harmony : &tracks[slap_ui.track].lfo;
        uint32_t word = *setting;
        const int *limit = harmony ? harmony_max : lfo_max;
        for (unsigned k = 0; k < 4; ++k) {
            if (!knob[k]) continue;
            unsigned shift = 8u * k;
            int base = (word >> shift) & 255u;
            int value = clamp(base + knob[k] * (harmony && k == 3 ? 5 : 1), 0, limit[k]);
            word = (word & ~(255u << shift)) | (uint32_t)value << shift;
        }
        *setting = word;
    } else if (slap_ui.page == ENV && slap_ui.track < DRUMS) {
        uint32_t env = tracks[slap_ui.track].envelope;
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k * 8u;
            int v = clamp((int)((env >> shift) & 255u) + knob[k] * 2, 0, 127);
            env = (env & ~(255u << shift)) | (uint32_t)v << shift;
        }
        tracks[slap_ui.track].envelope = env;
    } else if (slap_ui.page == HOME) {
        track_setting(slap_ui.track, 8, (unsigned)clamp((int)((mix >> 8) & 255u) + knob[0] * 4, 0, 255));
        track_setting(slap_ui.track, 0, (unsigned)clamp((int)pattern_length(mix) + knob[1], 1, STEPS));
        track_setting(slap_ui.track, 16, (unsigned)clamp((int)((mix >> 16) & 255u) + knob[2] * 4, 0, 255));
        swing = (unsigned)clamp((int)swing + knob[3] * 2, 0, 70);
        slap_ui.step = clamp(slap_ui.step, 0, (int)(pattern_length(tracks[slap_ui.track].mix)) - 1);
    }
    input_track = (unsigned)slap_ui.track;
    input_octave = (unsigned)slap_ui.octave;
    keys_audio = keys & ~boot_suppressed_keys;
    slap_ui.keys = keys;
    slap_ui.buttons = buttons;
    plat_leds(buttons | (playing << B_PLAY) | (recording << B_REC), keys);
    if (!playing && !recording && !keys && metronome != saved_metronome) {
        uint32_t now = plat_ms();
        if (now - metronome_changed_at >= SETTINGS_SETTLE_MS) {
            uint32_t settings[] = {SETTINGS_TAG, metronome};
            if (plat_store_save(OBJ_SETTINGS, settings, sizeof settings) == 0)
                saved_metronome = settings[1];
            metronome_changed_at = now;
        }
    }
}

static void decimal(char *out, unsigned value)
{
    char reverse[10];
    unsigned count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (count) *out++ = reverse[--count];
    *out = 0;
}

#include "visuals.h"

static unsigned callback_percentile(unsigned percentile)
{
    unsigned callbacks = dsp_callbacks;
    if (!callbacks) return 0;
    unsigned needed = callbacks - (callbacks / 100u * (100u - percentile) +
                                   (callbacks % 100u) * (100u - percentile) / 100u);
    unsigned seen = 0;
    for (unsigned bin = 0; bin < DSP_BINS - 1u; ++bin) {
        seen += dsp_histogram[bin];
        if (seen >= needed) return (bin + 1u) * (DSP_BIN_TICKS / 24u);
    }
    return dsp_peak_ticks / 24u;
}

static void draw_panel(void)
{
    static const char *const page_names[] = {"SLAP-5", "SEQUENCE", "GLOBAL", "PATCH", "ENV", "FX", "VISUAL", "SAVE", "HARMONY", "LFO", "ARP", "MIXER", "RECORD", "GEN DRUMS"};
    static const char *const arp_labels[] = {"MODE", "RATE", "OCT", "GATE"};
    static const char *const arp_modes[] = {"OFF","UP","DOWN","UP/DN","RANDOM","CHORD","PLAYED"};
    static const char *const arp_rates[] = {"1/4","1/8","1/16","1/32","1/8T","1/16T"};
    static const char *const lfo_labels[] = {"WAVE", "RATE", "PHS", "FADE"};
    static const char *const harmony_labels[] = {"KEY", "SCALE", "CHORD", "STRUM"};
    static const char *const env_labels[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    static const char *const labels[] = {"PAN", "STEPS", "LEVEL", "SWING"};
    static const char *const edit_labels[] = {"NOTE", "VEL", "GATE", "HIT %"};
    char number[12];
    unsigned selected = (unsigned)slap_ui.track;
    uint32_t position = positions;
    uint32_t settings = tracks[selected].mix;
    unsigned length = pattern_length(settings), play_step = position % length;
    unsigned scroll = slap_ui.page == HOME && playing;
    unsigned substep = step_progress;
    unsigned bank = (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) / 16u * 16u;
    uint16_t c = track_color[selected], background = C_BLACK;
    cv_begin(240, 48, background);
    if (slap_ui.page == HOME) cv_text(8, 1, &FONT_M, "SLAP-5", C_WHITE);
    else cv_text(8, 1, &FONT_M, page_names[slap_ui.page], c);
    decimal(number, tempo);
    cv_text(164, 1, &FONT_M, number, C_WHITE);
    cv_text(210, 10, &FONT_XS, "BPM", C_WHITE);
    static const uint16_t track_idle[] = {
        RGB(0,165,188), RGB(128,63,218), RGB(224,36,131), RGB(221,136,0), RGB(18,178,84)
    };
    for (unsigned t = 0; t < TRACKS; ++t) {
        int x = 4 + (int)t * 47;
        decimal(number, t + 1u);
        if (t == selected) cv_rect(x - 1, 24, 46, 24, C_WHITE);
        cv_rect(x, 25, 44, 22, t == selected ? track_color[t] : track_idle[t]);
        if (slap_ui.page != HOME) {
            cv_text(x + 18, 28, &FONT_XS, number, C_BLACK);
            continue;
        }
        cv_text(x + 1, 25, &FONT_T, number, C_BLACK);
        const char *name = t == DRUMS ? drum_kits[tracks[t].patch].name :
            slap_patches[tracks[t].patch].name;
        for (unsigned row = 0; row < 2 && *name; ++row) {
            char label[24];
            unsigned n = 0;
            unsigned space = 0;
            int width = row ? 41 : 34;
            while (n < sizeof(label) - 2 && name[n]) {
                label[n] = name[n];
                label[n + 1] = 0;
                if (text_w(&FONT_T, label) > width) break;
                if (name[n] == ' ') space = n;
                ++n;
            }
            if (!row && name[n] && name[n] != ' ' && space) n = space;
            label[n] = 0;
            cv_text(x + (row ? 2 : 9), 25 + 11 * (int)row, &FONT_T, label, C_BLACK);
            name += n;
            while (*name == ' ') ++name;
        }
    }
    cv_blit(0, 0);
    if (slap_ui.page == GLOBAL || slap_ui.page == MIXER || slap_ui.page == RECORD) {
        static const char *const mixer_labels[] = {"LEVEL", "PAN", "AUTO", "PROC"};
        unsigned values[] = {(settings >> 16) & 255u, (settings >> 8) & 255u,
            mixer_auto[selected], mixer_proc[selected]};
        const char *patch_name = selected == DRUMS ? drum_kits[tracks[selected].patch].name :
            slap_patches[tracks[selected].patch].name;
        cv_begin(240, 40, background);
        cv_text(8, 4, &FONT_M, slap_ui.page == GLOBAL ? "SYSTEM" : patch_name, c);
        cv_text(8, 25, slap_ui.page == RECORD ? &FONT_S : &FONT_XS, slap_ui.page == MIXER ? "SEL MUTE  /  ARP SOLO" :
            slap_ui.page == RECORD ? (recording ? "SEL END TAKE  /  PLAY STOP" : playing ? "STOP TO ARM A TAKE" : "SEL ARM  /  PLAY TO START") :
            global_details == 1 ? "PEAK BREAKDOWN  /  GLO NEXT" :
            global_details == 2 ? "SERIAL PROFILE  /  TIMING OVERHEAD" :
            global_details == 3 ? "WORKER COUNTERS  /  GLO NEXT" :
            global_details == 4 ? "CORE TIMING  /  GLO NEXT" :
            global_details == 5 ? "CORE 1 STATUS  /  GLO NEXT" :
            global_details == 6 ? "DISPATCH  /  GLO NEXT" : "SLAP-5 " APP_VERSION "  /  GLO FOR PEAK", C_WHITE);
        cv_blit(0, 48);
        for (int strip = 0; strip < 3; ++strip) {
            cv_begin(240, 36, background);
            if (slap_ui.page == MIXER) {
                for (unsigned t = 0; t < TRACKS; ++t) {
                    int x = 9 + (int)t * 46;
                    int h = (int)track_energy[t] * 92 / 160;
                    int peak = (int)track_peak[t] * 92 / 160;
                    cv_rect(x, 4-strip*36, 36, 96, track_surface[t]);
                    if (t == selected) cv_rect(x+1, 4-strip*36, 1, 96, C_WHITE);
                    cv_rect(x+4, 99-h-strip*36, 28, h, track_color[t]);
                    if (peak) cv_rect(x+4, 99-peak-strip*36, 28, 2, C_WHITE);
                    if (strip == 0) cv_text(x+6, 8, &FONT_XS,
                        (tracks[t].mix & (1u << 24)) ? "MUTE" : (mixer_solo & (1u << t)) ? "SOLO" :
                        mixer_auto[t] ? "AUTO" : "", C_WHITE);
                }
            } else if (slap_ui.page == GLOBAL) {
                static const char *const labels[3][2] = {
                    {"AVG DSP %", "PEAK US"}, {"P95 US", "P99 US"},
                    {"DEADLINE MISS", "BUDGET US"}
                };
                static const char *const detail_labels[3][2] = {
                    {"PEAK CALLBACK US", "OTHER US"}, {"VOICE BATCH US", "FX BATCH US"},
                    {"C1 WORK US", "WORKER WAIT US"}
                };
                unsigned avg = dsp_avg_ticks, budget = dsp_budget_ticks;
                unsigned peak = dsp_peak_ticks, voice_ticks = peak_voice_batch, fx_ticks = peak_fx_batch;
                unsigned other = peak > voice_ticks + fx_ticks ? peak - voice_ticks - fx_ticks : 0;
                unsigned values[3][2] = {
                    {budget ? avg * 100u / budget : 0u, peak / 24u},
                    {callback_percentile(95), callback_percentile(99)}, {dsp_overruns, budget / 24u}
                };
                unsigned detail_values[3][2] = {
                    {peak / 24u, other / 24u}, {voice_ticks / 24u, fx_ticks / 24u},
                    {peak_worker_work / 24u, peak_worker_wait / 24u}
                };
                static const char *const serial_labels[PROFILE_COUNT] = {
                    "INPUT / SETUP US", "EVENTS US", "VOICE SPANS US", "SERIAL FX US", "TRACK MIX US", "OUTPUT / METER US"
                };
                static const char *const worker_labels[3][2] = {
                    {"VOICE LATE", "FX LATE"}, {"VOICE BUSY", "VOICE REJECT"}, {"VOICE RERENDER", "FX ROLLBACK"}
                };
                static const char *const timing_labels[3][2] = {
                    {"C1 OVERLAP US", "C1 WAIT US"}, {"VOICE RECOV US", "FX RECOV US"},
                    {"PEAK LOAD %", "HEADROOM US"}
                };
                unsigned timing_values[3][2] = {
                    {0, 0}, {0, 0}, {budget ? peak * 100u / budget : 0, budget > peak ? (budget - peak) / 24u : 0}
                };
                unsigned worker_values[3][2] = {{0}};
#if FM1B_CPU2 && !defined(PLAT_WEB)
                timing_values[0][0] = peak_worker_overlap / 24u;
                timing_values[0][1] = peak_worker_wait / 24u;
                timing_values[1][0] = peak_voice_recovery / 24u;
                timing_values[1][1] = peak_fx_recovery / 24u;
                worker_values[0][0] = voice_late_jobs;
                worker_values[0][1] = fx_late_jobs;
                worker_values[1][0] = voice_busy_jobs;
                worker_values[1][1] = voice_rejected_jobs;
                worker_values[2][0] = worker_fallbacks;
                worker_values[2][1] = fx_worker_fallbacks;
#endif
                for (unsigned col = 0; col < 2; ++col) {
                    int x = 8 + (int)col * 116;
                    unsigned stage = (unsigned)strip * 2u + col;
                    if (global_details == 6) {
                        static const char *const labels_dispatch[] = {
                            "SHORT SPANS", "BUDGET SKIPS", "NO VOICE SPLIT", "MAILBOX BAD", "LATE DRAINED", "VOICE JOB MAX US"
                        };
                        unsigned value = 0;
#if FM1B_CPU2 && !defined(PLAT_WEB)
                        const unsigned dispatch_values[] = {voice_short_spans, voice_budget_skips,
                            voice_no_split, plat_cpu2_bad(), voice_drained_jobs, peak_voice_job_ticks / 24u};
                        value = dispatch_values[stage];
#endif
                        cv_text(x, 0, &FONT_XS, labels_dispatch[stage], C_WHITE);
                        decimal(number, value);
                        cv_text(x, 13, text_w(&FONT_M, number) > 110 ? &FONT_S : &FONT_M, number, c);
                        continue;
                    }
                    if (global_details == 5) {
                        static const char *const labels_c1[] = {
                            "START RESULT", "RUNTIME", "MAILBOX SENT", "MAILBOX DONE", "STACK USED B", "RETIRE REASON"
                        };
                        const char *state = "UNAVAILABLE";
                        unsigned value = 0;
#if FM1B_CPU2 && !defined(PLAT_WEB)
                        static const char *const startup[] = {
                            "NOT TRIED", "PASSED", "BOOT GUARD", "BARRED", "NO REPLY", "PROBE FAIL"
                        };
                        static const char *const retired[] = {
                            "NONE", "MISREAD", "LATE", "BARRED", "FAULT", "NO REPLY"
                        };
                        unsigned boot = plat_cpu2_start_state();
                        unsigned reason = (unsigned)plat_cpu2_gave_up();
                        if (stage == 0 && boot < sizeof startup / sizeof *startup) state = startup[boot];
                        if (stage == 1) state = voice_pending ? "DRAINING" : plat_cpu2_on() ? "ONLINE" : plat_cpu2_barred() ? "BARRED" : "OFFLINE";
                        if (stage == 2) value = plat_cpu2_mailbox_requests();
                        if (stage == 3) value = plat_cpu2_mailbox_jobs();
                        if (stage == 4 && boot == PLAT_CPU2_STARTED) value = plat_cpu2_stack();
                        if (stage == 5 && reason < sizeof retired / sizeof *retired) state = retired[reason];
#endif
                        cv_text(x, 0, &FONT_XS, labels_c1[stage], C_WHITE);
                        if (stage >= 2 && stage <= 4) {
                            decimal(number, value);
                            cv_text(x, 13, text_w(&FONT_M, number) > 110 ? &FONT_S : &FONT_M, number, c);
                        } else {
                            cv_text(x, 13, &FONT_S, state, c);
                        }
                        continue;
                    }
                    const char *label = global_details == 4 ? timing_labels[strip][col] : global_details == 3 ? worker_labels[strip][col] :
                        global_details == 2 ? serial_labels[stage] :
                        global_details == 1 ? detail_labels[strip][col] : labels[strip][col];
                    cv_text(x, 0, &FONT_XS, label, C_WHITE);
                    unsigned value = global_details == 4 ? timing_values[strip][col] : global_details == 3 ? worker_values[strip][col] :
                        global_details == 2 ? serial_at_peak[stage] / 24u :
                        global_details == 1 ? detail_values[strip][col] : values[strip][col];
                    decimal(number, value);
                    cv_text(x, 13, text_w(&FONT_M, number) > 110 ? &FONT_S : &FONT_M, number, c);
                }
            } else {
                if (strip == 0) {
                    cv_text(8, 8, &FONT_M, rec_running ? "RECORDING" : rec_countdown ? "COUNT-IN" :
                        recording ? playing ? "PLAY A NOTE" : "ARMED" : "READY", c);
                    if (rec_countdown) {
                        decimal(number, rec_countdown);
                        cv_text(188, 8, &FONT_M, number, C_WHITE);
                    }
                }
                if (strip == 1) {
                    decimal(number, play_step / 16u + 1u);
                    cv_text(8, 0, &FONT_XS, "BAR", c);
                    cv_text(34, 0, &FONT_XS, number, C_WHITE);
                    decimal(number, play_step % 16u + 1u);
                    cv_text(77, 0, &FONT_XS, "STEP", c);
                    cv_text(110, 0, &FONT_XS, number, C_WHITE);
                    for (unsigned step = 0; step < 16; ++step) {
                        int x = 8 + 14 * (int)step;
                        cv_rect(x, 18, 12, 11, step <= play_step % 16u && playing ? c : track_surface[selected]);
                        if (step == play_step % 16u && playing) cv_rect(x, 18, 12, 2, C_WHITE);
                    }
                }
                if (strip == 2) {
                    cv_text(8, 0, &FONT_XS, rec_rejected ? "STEP CAPACITY REACHED" : "NOTES THIS TAKE", C_WHITE);
                    unsigned count = recorded_events - rec_begin_events;
                    decimal(number, count > 999 ? 999 : count);
                    cv_text(174, 1, &FONT_B, number, c);
                    cv_rect(8, 23, 224, 7, track_surface[selected]);
                    cv_rect(8, 23, (int)track_energy[selected] * 224 / 160, 7, c);
                }
            }
            cv_blit(0, 88 + strip*36);
        }
        cv_begin(240, 44, background);
        if (slap_ui.page == MIXER) {
            for (unsigned k = 0; k < 4; ++k) {
                int x = 8 + (int)k*58;
                cv_text(x, 0, &FONT_XS, mixer_labels[k], C_WHITE);
                decimal(number, values[k]);
                cv_text(x, 16, &FONT_M, number, c);
            }
        } else if (slap_ui.page == GLOBAL) {
#if FM1B_CPU2 && !defined(PLAT_WEB)
            cv_text(8, 2, &FONT_XS, "C1 JOBS", c);
            uint32_t total_jobs = worker_jobs + fx_worker_jobs;
            decimal(number, total_jobs);
            cv_text(55, 2, &FONT_XS, number, C_WHITE);
            cv_text(126, 2, &FONT_XS, "FALLBACKS", c);
            uint32_t total_fallbacks = worker_fallbacks + fx_worker_fallbacks;
            decimal(number, total_fallbacks);
            cv_text(238 - text_w(&FONT_XS, number), 2, &FONT_XS, number, C_WHITE);
#else
            cv_text(8, 2, &FONT_XS, "K1 TEMPO  /  K2 SWING", C_WHITE);
#endif
            cv_text(8, 31, &FONT_XS, metronome == 2 ? "K4 METRO ON" : metronome == 1 ? "K4 METRO REC" : "K4 METRO OFF", c);
            cv_text(115, 31, &FONT_XS, "K3 RESET STATS", c);
            cv_text(8, 18, &FONT_XS, "ACTIVE VOICES", c);
            decimal(number, active_voices);
            cv_text(113, 20, &FONT_XS, number, C_WHITE);
            cv_text(134, 20, &FONT_XS, "DMA XRUNS", c);
            unsigned xruns = plat_xruns() - dsp_xrun_baseline;
            decimal(number, xruns);
            cv_text(238 - text_w(&FONT_XS, number), 20, &FONT_XS, number, C_WHITE);
        } else {
            static const char *const labels[] = {"LENGTH", "START", "QUANT", "MODE"};
            char bars[5];
            decimal(bars, 1u << rec_bars);
            const char *values[] = {bars, rec_count_in ? "COUNT-IN" : "1ST NOTE",
                rec_quant_labels[rec_quantize], rec_replace ? "REPLACE" : "OVERDUB"};
            for (unsigned k = 0; k < 4; ++k) {
                int x = 8 + (int)k * 58;
                cv_text(x, 0, &FONT_XS, labels[k], c);
                cv_text(x, 15, &FONT_XS, values[k], C_WHITE);
            }
            cv_text(8, 30, &FONT_XS, metronome == 2 ? "METRO ON" : metronome == 1 ? "METRO REC" : "METRO OFF", c);
            if (take_undo.ready && !playing && !recording)
                cv_text(132, 30, &FONT_XS, "ARP UNDO TAKE", C_WHITE);
        }
        cv_blit(0, 196);
        return;
    }
    cv_begin(240, 32, background);
    const SlapPatch *patch = &slap_patches[tracks[selected].patch];
    static const char *const drum_names[] = {"KICK","SNARE","HAT","RIM","CLAP","CYMBAL"};
    static const char *const melodic_names[] = {"NOTE A", "NOTE B", "NOTE C", "NOTE D"};
    const char *lane_name = selected == DRUMS ?
        (slap_ui.lane ? drum_names[slap_ui.lane-1] : "DRUM NOTES") :
        melodic_names[slap_ui.lane];
    const char *name = slap_ui.page == SEQ ? lane_name : slap_ui.page == SAVE ? "SONG SLOTS" : slap_ui.page == FX ? slap_fx_pages[slap_ui.fx_page] :
                       selected == DRUMS ? drum_kits[tracks[DRUMS].patch].name : patch->name;
    unsigned drum_sequence = selected == DRUMS && slap_ui.page == SEQ;
    const felucca_font_t *name_font = drum_sequence || text_w(&FONT_M, name) > 150 ? &FONT_B : &FONT_M;
    cv_text(8, 1, name_font, name, C_WHITE);
    if (drum_sequence) cv_text(8, 17, &FONT_XS, "SEQ: GEN DRUMS", c);
    decimal(number, position / 16u % 999u + 1u);
    cv_text(163, 5, &FONT_XS, "BAR", c);
    cv_text(190, 5, &FONT_XS, number, C_WHITE);
    cv_text(163, 19, &FONT_XS, rec_countdown ? "COUNT" : recording && playing ? "REC" : recording ? "ARM" : playing ? "PLAY" : "STOP", c);
    decimal(number, rec_countdown ? rec_countdown : (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) + 1u);
    cv_text(202, 19, &FONT_XS, number, C_WHITE);
    cv_blit(0, 48);
    if (slap_ui.page == SAVE) {
        for (int strip = 0; strip < 4; ++strip) {
            cv_begin(240, 40, background);
            if (strip < 2) for (int i = 0; i < 4; ++i) {
                int slot = strip * 4 + i;
                int x = 8 + 57 * i;
                decimal(number, (unsigned)slot + 1u);
                cv_rect(x, 4, 50, 30, slot == slap_ui.slot ? c : track_surface[selected]);
                cv_text(x + 18, 8, &FONT_M, number, slot == slap_ui.slot ? C_BLACK : C_WHITE);
            }
            if (strip == 2) {
                cv_text(8, 7, &FONT_B, slap_ui.notice ? slap_ui.notice : "HOLD WHITE KEY 4S", C_WHITE);
                if (slap_ui.save_armed)
                    cv_rect(8, 27, (int)((plat_ms() - slap_ui.save_at) * 224u / 4000u), 4, c);
            }
            if (strip == 3) cv_text(8, 7, &FONT_XS, "SEL LOADS   /   SELECT CHOOSES", c);
            cv_blit(0, 80 + strip * 40);
        }
        return;
    }
    if (slap_ui.page == PATCH) {
        cv_begin(240, 40, background);
        cv_text(8, 2, &FONT_B, selected == DRUMS ? "DRUM KITS" : slap_engine_names[patch->engine], c);
        cv_text(8, 21, &FONT_XS, selected == DRUMS ? "SELECT TRACK  /  K2 KIT" :
                "SELECT TRACK  /  K1 GROUP  /  K2 PATCH", C_WHITE);
        cv_blit(0, 80);
        int current = (int)tracks[selected].patch;
        int drums = selected == DRUMS;
        int legacy_kit = drums && current < DRUM_LEGACY_KITS;
        int minimum = drums ? DRUM_LEGACY_KITS : 0;
        int count = drums ? DRUM_KITS : SLAP_PATCHES;
        int first = clamp(current - 1, minimum, count - 3);
        for (int row = 0; row < 3; ++row) {
            int item = legacy_kit ? (row == 0 ? current : minimum + row - 1) : first + row;
            const char *name = selected == DRUMS ? drum_kits[item].name : slap_patches[item].name;
            cv_begin(240, 30, background);
            cv_rect(7, 2, 226, 26, item == current ? c : track_surface[selected]);
            const felucca_font_t *font = &FONT_S;
            cv_text(14, 4, font, name, item == current ? C_BLACK : C_WHITE);
            cv_blit(0, 120+row*30);
        }
        cv_begin(240, 30, background);
        cv_rect(7, 14, 226, 1, track_rule[selected]);
        for (unsigned k = 0; k+1u < SLAP_FX_PLOT; ++k)
            cv_line(8+k*2u, 14+slap_fx_trace(selected,k)/5, 10+k*2u, 14+slap_fx_trace(selected,k+1u)/5, c);
        cv_blit(0, 210);
        return;
    }
    if (selected == DRUMS && (slap_ui.page == ENV || slap_ui.page == LFO)) {
        cv_begin(240, 40, background);
        cv_text(8, 8, &FONT_B, slap_ui.page == ENV ? "KIT ENVELOPE IS FIXED" :
                "NO PITCH LFO FOR KITS", c);
        cv_blit(0, 80);
        for (unsigned y = 120; y < 240; y += 40) {
            cv_begin(240, 40, background);
            cv_blit(0, y);
        }
        return;
    }
    if (slap_ui.page == GEN_DRUMS) {
        cv_begin(240, 40, background);
        cv_text(8, 3, &FONT_B, drum_style[gen_style].name, c);
        cv_text(8, 27, &FONT_XS, gen_confirm ? "SEL AGAIN TO REPLACE" :
                playing ? "STOP TO GENERATE" : gen_enabled ? "SEL GENERATE   /   ARP OFF" : "GEN OFF   /   ARP ON", C_WHITE);
        cv_text(178, 6, &FONT_XS, "SEQ BACK", c);
        cv_blit(0, 80);
        for (unsigned strip = 0; strip < 2; ++strip) {
            cv_begin(240, 36, background);
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned step = 0; step < 16; ++step) {
                    unsigned at = bank + step;
                    unsigned hit = at < length && (drum_lane[strip * 3 + row][at] & ACTIVE);
                    cv_rect(8 + step * 14, row * 11, 12, 8,
                            hit ? c : RGB(12,54,36));
                    if (playing && at == play_step) cv_rect(8 + step * 14, row * 11, 2, 8, C_WHITE);
                }
            cv_blit(0, 120 + strip * 36);
        }
        cv_begin(240, 48, background);
        static const char *const label[] = {"STYLE", "HIT %", "GROOVE", "VAR %"};
        unsigned value[] = {gen_style + 1, gen_hit, gen_groove, gen_var};
        for (unsigned k = 0; k < 4; ++k) {
            cv_text(8 + k * 58, 2, &FONT_XS, label[k], c);
            decimal(number, value[k]);
            cv_text(8 + k * 58, 17, &FONT_M, number, C_WHITE);
        }
        cv_blit(0, 192);
        return;
    }
    uint32_t env = tracks[selected].envelope;
    uint32_t modulation = slap_ui.page == LFO ? tracks[selected].lfo : tracks[selected].harmony;
    for (int strip = 0; strip < 3; ++strip) {
        cv_begin(240, 36, background);
        if (slap_ui.page == FX) {
            const unsigned fpage = (unsigned)slap_ui.fx_page;
            cv_rect(8, 18, 224, 1, track_rule[selected]);
            if (fpage == 2) {
                for (int i = 0; i < 16; ++i) {
                    unsigned hit = slap_fx_slicer_cell(selected, (unsigned)i);
                    int y = (hit ? 17 : 75) - strip * 36;
                    cv_rect(8 + i * 14, y, 11, 13, c);
                    if (i == (int)(play_step & 15u) && playing)
                        cv_rect(8 + i * 14, 0, 1, 36, C_WHITE);
                }
            } else {
                for (unsigned i = 0; i < SLAP_FX_PLOT; ++i) {
                    int y = fpage == 1 ? slap_fx_response(selected, i) : 53 + slap_fx_trace(selected, i);
                    cv_rect(8 + (int)i * 3, y - strip * 36, 3, 3, c);
                }
            }
        } else if (slap_ui.page == ARP) {
            unsigned cursor = arp_cursor;
            for (unsigned i = 0; i < 16; ++i) {
                int x = 8 + (int)i*14;
                unsigned pitch = arp_history[(cursor + i) & 15u];
                cv_rect(x, 0, 12, 36, track_surface[selected]);
                if (pitch < 128u)
                    cv_rect(x, 98 - (int)(pitch % 24u)*4 - strip*36, 11, 5, c);
            }
        } else if (slap_ui.page == LFO) {
            unsigned wave = modulation & 255u;
            float phase = ((modulation >> 16) & 255u) / 128.0f;
            for (unsigned i = 0; i < 224; ++i) {
                float cycles = phase + i * (1.0f/112.0f) + (plat_ms() & 1023u) / 1024.0f;
                float at = cycles - fm_floorf(cycles);
                float held = (float)((unsigned)(cycles * 7.0f) * 173u & 255u) / 128.0f - 1.0f;
                cv_rect(8 + (int)i, 51 - (int)(lfo_shape(wave, at, held) * 31.0f) - strip*36, 1, 3, c);
            }
        } else if (slap_ui.page == HARMONY) {
            unsigned root = modulation & 255u, scale = (modulation >> 8) & 255u;
            for (unsigned i = 0; i < 12; ++i) {
                int x = 8 + (int)i * 19;
                int active = scale_tones[scale] & (1u << ((i + 12u - root) % 12u));
                cv_rect(x, 22 - strip*36, 17, 66, active ? c : track_rule[selected]);
                if (strip == 1) cv_text(x + 2, 18, &FONT_XS, harmony_keys[i], active ? C_BLACK : C_WHITE);
            }
        } else if (slap_ui.page == ENV) {
            const int attack = 12 + (env & 255u) / 3;
            const int decay = attack + 12 + ((env >> 8) & 255u) / 3;
            const int level = 101 - ((env >> 16) & 255u) * 94 / 127;
            const int release = 224 - 12 - (env >> 24) / 3;
            for (int x = 0; x < 224; ++x) {
                int y;
                if (x < attack) y = 101 - x * 94 / attack;
                else if (x < decay) y = 7 + (x - attack) * (level - 7) / (decay - attack);
                else if (x < release) y = level;
                else y = level + (x - release) * (101 - level) / (224 - release);
                cv_rect(8 + x, y - strip * 36, 1, 3, c);
            }
        } else {
            for (unsigned s = 0; s < (scroll ? 19u : 16u); ++s) {
                if (!scroll && bank + s >= length) break;
                unsigned index = scroll ? (play_step + length * 3u - 3u + s) % length : bank + s;
                int x = 8 + (int)s * 14 - (scroll ? (int)substep : 0);
                uint32_t word = tracks[selected].steps[index];
                cv_rect(x, 0, 12, 36, index % 4 == 0 ? track_rule[selected] : track_surface[selected]);
                if (!scroll && index == play_step && playing) cv_rect(x, 0, 2, 36, C_WHITE);
                if (slap_ui.page == SEQ && index == (unsigned)slap_ui.step) cv_rect(x + 9, 0, 3, 36, c);
                for (unsigned lane = 0; lane < lane_count(selected); ++lane) {
                    word = *event_cell(selected, index, lane);
                    if (!(word & ACTIVE)) continue;
                    int y = 100 - (int)((word & NOTES) % 24u) * 4 - strip * 36;
                    int width = 2 + (int)((word >> 16) & 255u) * 13 / 192;
                    cv_rect(x + 2 + (int)timing_of(word), y, width, 5, c);
                }
            }
            if (scroll) cv_rect(50, 0, 1, 36, C_WHITE);
        }
        cv_blit(0, 80 + strip * 36);
    }
    cv_begin(240, 52, background);
    if (slap_ui.page == FX && slap_ui.fx_page == 4) {
        cv_text(8, 1, &FONT_XS, "FX TICKS / SAMPLE", C_WHITE);
        unsigned v = fx_cost_ticks[selected];
        decimal(number, v > 999 ? 999 : v);
        cv_text(176, 1, &FONT_XS, number, c);
    }
    uint32_t word = *event_cell(selected, slap_ui.step, slap_ui.page == SEQ ? slap_ui.lane : 0);
    unsigned values[] = { (settings >> 8) & 255u, length, ((settings >> 16) & 255u) * 100u / 255u, swing };
    if (slap_ui.page == SEQ) {
        values[0] = word & NOTES;
        values[1] = (word >> 8) & 127u;
        values[2] = (word >> 16) & 255u;
        values[3] = slap_ui.buttons & (1u << B_SEL) ? note_timing(pattern_row(selected, slap_ui.lane), slap_ui.step, word) : (word >> 24) & 127u;
    }
    if (slap_ui.page == ENV)
        for (unsigned k = 0; k < 4; ++k) values[k] = (env >> (k*8u)) & 255u;
    for (unsigned k = 0; k < 4; ++k) {
        int x = 8 + (int)k * 58;
        const char *label = slap_ui.page == FX ? slap_fx_labels[slap_ui.fx_page][k] :
                            slap_ui.page == ARP ? arp_labels[k] : slap_ui.page == LFO ? lfo_labels[k] : slap_ui.page == HARMONY ? harmony_labels[k] :
                            slap_ui.page == SEQ ? (k == 3 && (slap_ui.buttons & (1u << B_SEL)) ? "OFFSET" : edit_labels[k]) : slap_ui.page == ENV ? env_labels[k] : labels[k];
        if (!label) continue;
        cv_text(x, slap_ui.page == FX && slap_ui.fx_page == 4 ? 16 : 2, &FONT_XS, label, c);
        if (slap_ui.page == ARP) {
            unsigned value = (arp_setup[selected] >> (8u*k)) & 255u;
            if (k < 2u) {
                cv_text(x, 15, &FONT_XS, k == 0 ? arp_modes[value] : arp_rates[value], C_WHITE);
                continue;
            }
            values[k] = value;
        } else if (slap_ui.page == LFO || slap_ui.page == HARMONY) {
            unsigned v = (modulation >> (8u*k)) & 255u;
            const char *tag = slap_ui.page == LFO ? (k == 0 ? lfo_waves[v] : 0) :
                k == 0 ? harmony_keys[v] : k == 1 ? harmony_scales[v] : k == 2 ? harmony_chords[v] : !v ? "OFF" : 0;
            if (tag) { cv_text(x, 15, &FONT_XS, tag, C_WHITE); continue; }
            if (slap_ui.page == LFO && k == 1) {
                unsigned hundredths = (unsigned)(10.0f * fm_exp2f(v * (8.643856f / 127.0f)) + 0.5f);
                decimal(number, hundredths / 100u);
                char rate[8];
                unsigned j = 0;
                while (number[j]) { rate[j] = number[j]; ++j; }
                rate[j++] = '.';
                rate[j++] = (char)('0' + (hundredths / 10u) % 10u);
                rate[j++] = (char)('0' + hundredths % 10u);
                rate[j] = 0;
                cv_text(x, 15, &FONT_B, rate, C_WHITE);
                continue;
            }
            values[k] = slap_ui.page == HARMONY ? 99u + v : v;
        } else if (slap_ui.page == FX) {
            unsigned page = (unsigned)slap_ui.fx_page;
            values[k] = slap_fx_value(selected, page, k);
            if (page == 3 && k == 0) {
                cv_text(x, 18, &FONT_XS, slap_insert_names[values[k]], C_WHITE);
                continue;
            }
            if (page == 2 && k == 0) {
                static const char *const modes[] = {"OFF", "GATE", "STUTTER"};
                cv_text(x, 18, &FONT_XS, modes[values[k]], C_WHITE);
                continue;
            }
        }
        decimal(number, values[k]);
        cv_text(x, slap_ui.page == FX && slap_ui.fx_page == 4 ? 26 : 15, &FONT_M, number, C_WHITE);
    }
    cv_blit(0, 188);
}

void app_frame(void)
{
    app_input_only();
    if (slap_ui.page == VISUAL) update_spectrum();
    else analysis_read = analysis_write;
    uint32_t now = plat_ms();
    static uint32_t last_xruns, last_pressure;
    static unsigned under_pressure;
    unsigned frame_interval = 45u;
    if (slap_ui.page == VISUAL) {
        uint32_t xruns = plat_xruns();
        if (xruns != last_xruns || (dsp_budget_ticks &&
            dsp_avg_ticks > dsp_budget_ticks * 3u / 4u)) {
            last_pressure = now;
            under_pressure = 1;
        }
        last_xruns = xruns;
        if (under_pressure && now - last_pressure >= 1000u) under_pressure = 0;
        if (under_pressure) frame_interval = 85u;
    } else {
        last_xruns = plat_xruns();
    }
    if (now - slap_ui.frame_at >= frame_interval) {
        slap_ui.frame_at = now;
        if (slap_ui.booting && now - slap_ui.boot_at < BOOT_MS)
            draw_boot(now - slap_ui.boot_at);
        else {
            slap_ui.booting = 0;
            if (slap_ui.page == VISUAL) visualizer_draw((unsigned)slap_ui.visual, now / 4u);
            else draw_panel();
        }
    }
}

void app_ui_init(void)
{
    slap_ui.octave = 2;
    input_octave = 2;
    slap_ui.boot_at = plat_ms();
    slap_ui.booting = 1;
    intro_skipped = 0;
    draw_boot(0);
}

void app_say(const char *title, const char *message)
{
    (void)title;
    (void)message;
}

static int name_matches(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

int app_expect(const char *name, int *result)
{
    if (name_matches(name, "track")) *result = slap_ui.track + 1;
    else if (name_matches(name, "tempo")) *result = (int)tempo;
    else if (name_matches(name, "boot")) *result = (int)slap_ui.booting;
    else if (name_matches(name, "recording")) *result = (int)recording;
    else if (name_matches(name, "rec_rejected")) *result = (int)rec_rejected;
    else if (name_matches(name, "note_ms"))
        *result = slap_ui.track < DRUMS && (unsigned)slap_ui.lane < MELODIC_LANES ?
            (int)(melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step] * NOTE_DURATION_UNIT * 10u / (RATE / 100u)) : 0;
    else if (name_matches(name, "level")) *result = (int)((tracks[slap_ui.track].mix >> 16) & 255u);
    else if (name_matches(name, "visual_left")) *result = visual_rms[0] > 0.000001f;
    else if (name_matches(name, "visual_right")) *result = visual_rms[1] > 0.000001f;
    else if (name_matches(name, "visual_spectrum")) {
        float power = 0.0f;
        for (unsigned band = 0; band < SPECTRUM_BANDS; ++band) power += visual_band[band];
        *result = power > 0.000001f;
    }
    else if (name_matches(name, "held_sequenced")) {
        *result = 0;
        for (unsigned slot = 0; slot < VOICES; ++slot)
            *result += voices[slot].age && !voices[slot].releasing && voices[slot].sequence_note;
    }
    else if (name_matches(name, "chord_mode")) *result = (int)((tracks[slap_ui.track].harmony >> 16) & 255u);
    else if (name_matches(name, "metronome")) *result = (int)metronome;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    else if (name_matches(name, "worker_overlap_us")) *result = (int)(worker_overlap_ticks / 24u);
    else if (name_matches(name, "worker_jobs")) *result = (int)worker_jobs;
    else if (name_matches(name, "worker_fallbacks")) *result = (int)worker_fallbacks;
    else if (name_matches(name, "voice_drained_jobs")) *result = (int)voice_drained_jobs;
    else if (name_matches(name, "voice_late_jobs")) *result = (int)voice_late_jobs;
    else if (name_matches(name, "fx_worker_jobs")) *result = (int)fx_worker_jobs;
    else if (name_matches(name, "fx_worker_fallbacks")) *result = (int)fx_worker_fallbacks;
#endif
    else if (name_matches(name, "gen_count")) *result = (int)gen_count;
    else if (name_matches(name, "rec_running")) *result = (int)rec_running;
    else if (name_matches(name, "rec_countdown")) *result = (int)rec_countdown;
    else if (name_matches(name, "note_offset"))
        *result = (int)note_timing(pattern_row(slap_ui.track, slap_ui.lane), slap_ui.step,
            *event_cell(slap_ui.track, slap_ui.step, slap_ui.lane));
    else if (name_matches(name, "note_gate"))
        *result = (int)((*event_cell(slap_ui.track, slap_ui.step, slap_ui.lane) >> 16) & 255u);
    else if (name_matches(name, "recorded")) *result = (int)recorded_events;
    else if (name_matches(name, "playing")) *result = (int)playing;
    else if (name_matches(name, "lane")) *result = slap_ui.lane;
    else if (name_matches(name, "events")) *result = (int)scheduled_events;
    else if (name_matches(name, "step")) *result = (int)positions;
    else if (name_matches(name, "arp_events")) *result = (int)arp_cursor;
    else if (name_matches(name, "arp_mode")) *result = (int)(arp_setup[0] & 255u);
    else if (name_matches(name, "arp_note")) *result = (int)arp_history[(arp_cursor - 1u) & 15u];
    else if (name_matches(name, "slot")) *result = slap_ui.slot + 1;
    else if (name_matches(name, "fx_page")) *result = slap_ui.fx_page + 1;
    else if (name_matches(name, "fx_k1")) *result = slap_fx_value(slap_ui.track, slap_ui.fx_page, 0);
    else if (name_matches(name, "patch")) *result = (int)tracks[slap_ui.track].patch;
    else if (name_matches(name, "env_attack")) *result = tracks[slap_ui.track].envelope & 255u;
    else if (name_matches(name, "lfo_rate")) *result = (tracks[slap_ui.track].lfo >> 8) & 255u;
    else if (name_matches(name, "length")) *result = pattern_length(tracks[slap_ui.track].mix);
    else if (name_matches(name, "pitch")) {
        uint32_t word = *event_cell(slap_ui.track, slap_ui.step, slap_ui.lane);
        *result = word & ACTIVE ? (int)(word & NOTES) : -1;
    }
    else if (name_matches(name, "page")) *result = slap_ui.page;
    else return 0;
    return 1;
}

#ifdef PLAT_WEB
#pragma push_macro("__attribute__")
#undef __attribute__
#define STUDIO_EXPORT __attribute__((used, visibility("default")))
STUDIO_EXPORT void studio_transport(uint32_t run)
{
    if (!run) {
        if (rec_running) end_recording(record_clock);
        recording = 0;
        rec_countdown = 0;
        studio_notes_read = studio_notes_write;
        for (unsigned i = 0; i < VOICES; ++i)
            if (voices[i].manual) voices[i].releasing = 1;
    }
    playing = !!run;
    ++sequence_epoch;
}

STUDIO_EXPORT uint32_t studio_begin_take(uint32_t track, uint32_t bars,
                                        uint32_t count_in, uint32_t unquantized, uint32_t replace)
{
    if (track >= TRACKS || bars > 5u || playing || recording || rec_running) return 0;
    rec_track = track;
    rec_length = 16u << bars;
    rec_start_mode = !!count_in;
    rec_quant_setting = unquantized ? 0u : REC_FULL_16;
    rec_overwrite = !!replace;
    rec_loop = 0;
    rec_countdown = 0;
    recording = playing = 1;
    ++sequence_epoch;
    return 1;
}

STUDIO_EXPORT uint32_t studio_quick_take(uint32_t track, uint32_t unquantized)
{
    if (track >= TRACKS || recording || rec_running) return 0;
    rec_track = track;
    rec_length = pattern_length(tracks[track].mix);
    rec_start_mode = !playing;
    rec_quant_setting = unquantized ? 0u : REC_FULL_16;
    rec_overwrite = 0;
    rec_loop = 1;
    rec_countdown = 0;
    if (!playing) ++sequence_epoch;
    recording = playing = 1;
    return 1;
}

STUDIO_EXPORT void studio_take_quantization(uint32_t setting)
{
    if (recording && setting <= REC_QUANT_MAX) rec_quant_setting = setting;
}

STUDIO_EXPORT uint32_t studio_is_playing(void) { return playing; }

STUDIO_EXPORT uint32_t studio_record_state(void)
{
    return recording ? (rec_running ? 5u : rec_countdown ? rec_countdown : 6u) : 0u;
}

STUDIO_EXPORT void studio_end_take(void)
{
    if (!recording) return;
    if (rec_running) end_recording(record_clock);
    else {
        playing = 0;
        rec_countdown = 0;
        ++sequence_epoch;
    }
    recording = 0;
    studio_notes_read = studio_notes_write;
    for (unsigned i = 0; i < VOICES; ++i)
        if (voices[i].manual && voices[i].track == rec_track) voices[i].releasing = 1;
}

STUDIO_EXPORT void studio_take_note(uint32_t pitch, uint32_t velocity)
{
    if (pitch > 127u || velocity > 127u ||
        studio_notes_write - studio_notes_read >= STUDIO_NOTE_QUEUE) return;
    studio_notes[studio_notes_write++ % STUDIO_NOTE_QUEUE] = (uint16_t)(pitch | velocity << 7);
}

STUDIO_EXPORT uint32_t studio_step(uint32_t track, uint32_t step)
{
    return track < TRACKS && step < STEPS ? tracks[track].steps[step] : 0;
}

STUDIO_EXPORT uint32_t studio_event(uint32_t track, uint32_t step, uint32_t lane)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track)) return 0;
    return *event_cell(track, step, lane);
}

STUDIO_EXPORT void studio_set_event(uint32_t track, uint32_t step, uint32_t lane, uint32_t word)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track) || !step_valid(word)) return;
    if (word && track == DRUMS && lane && (word & NOTES) != lane_pitch[lane-1u]) return;
    timing_group[pattern_row(track, lane)][step] = 0;
    *event_cell(track, step, lane) = word;
    if (track < DRUMS) melodic_duration[track][lane][step] = 0;
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_note_offset(uint32_t track, uint32_t step, uint32_t lane)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track)) return 0;
    return note_timing(pattern_row(track, lane), step, *event_cell(track, step, lane));
}

STUDIO_EXPORT void studio_set_note_offset(uint32_t track, uint32_t step, uint32_t lane, uint32_t offset)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track) || offset > 15u) return;
    volatile uint32_t *cell = event_cell(track, step, lane);
    if (!*cell) return;
    timing_group[pattern_row(track, lane)][step] = offset ? (uint8_t)(offset / 4u + 1u) : 0;
    *cell = (*cell & ~timing_bits(3u)) | timing_bits(offset);
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_note_duration(uint32_t track, uint32_t lane, uint32_t step)
{
    if (track >= DRUMS || lane >= MELODIC_LANES || step >= STEPS) return 0;
    return melodic_duration[track][lane][step];
}

STUDIO_EXPORT void studio_set_note_duration(uint32_t track, uint32_t lane, uint32_t step, uint32_t units)
{
    if (track >= DRUMS || lane >= MELODIC_LANES || step >= STEPS || units > 65535 ||
        !(*event_cell(track, step, lane) & ACTIVE)) return;
    melodic_duration[track][lane][step] = (uint16_t)units;
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_drum_filters(void) { return drum_filters; }

STUDIO_EXPORT void studio_set_drum_filters(uint32_t mask)
{
    if (!(mask & ~0x3f3fu)) drum_filters = mask;
}

STUDIO_EXPORT uint32_t studio_mix(uint32_t track)
{
    return track < TRACKS ? tracks[track].mix : 0;
}

STUDIO_EXPORT uint32_t studio_bpm(void) { return tempo; }
STUDIO_EXPORT uint32_t studio_swing(void) { return swing; }

STUDIO_EXPORT uint32_t studio_patch(uint32_t track)
{
    return track < TRACKS ? tracks[track].patch : 0;
}

STUDIO_EXPORT void studio_set_step(uint32_t track, uint32_t step, uint32_t word)
{
    if (track >= TRACKS || step >= STEPS) return;
    if (!step_valid(word)) return;
    timing_group[pattern_row(track, 0)][step] = 0;
    tracks[track].steps[step] = word;
    if (track < DRUMS) melodic_duration[track][0][step] = 0;
    ++pattern_revision[track];
}

STUDIO_EXPORT void studio_set_track(uint32_t track, uint32_t mix, uint32_t patch)
{
    if (track >= TRACKS || !pattern_length(mix) || pattern_length(mix) > STEPS || (mix >> (track == DRUMS ? 27 : 28)) ||
        patch >= (track == DRUMS ? DRUM_KITS : SLAP_PATCHES)) return;
    if (track == DRUMS) tracks[track].patch = patch;
    else if (patch != tracks[track].patch) load_patch(track, patch);
    if (pattern_length(tracks[track].mix) != pattern_length(mix)) ++pattern_revision[track];
    tracks[track].mix = mix;
}

STUDIO_EXPORT void studio_set_clock(uint32_t bpm, uint32_t amount)
{
    if (bpm >= 40 && bpm <= 240) tempo = bpm;
    if (amount <= 70) swing = amount;
}

STUDIO_EXPORT uint32_t studio_kit_count(void) { return DRUM_KITS; }

STUDIO_EXPORT const char *studio_kit_name(uint32_t kit)
{
    return kit < DRUM_KITS ? drum_kits[kit].name : "";
}

STUDIO_EXPORT const char *studio_patch_name(uint32_t patch)
{
    return patch < SLAP_PATCHES ? slap_patches[patch].name : "";
}

STUDIO_EXPORT uint32_t studio_generate_drums(uint32_t style, uint32_t hit,
                                           uint32_t groove, uint32_t variation)
{
    if (playing || recording || style >= sizeof drum_style / sizeof drum_style[0] ||
        hit > 100u || groove > 100u || variation > 100u) return 0;
    gen_style = style;
    gen_hit = hit;
    gen_groove = groove;
    gen_var = variation;
    unsigned enabled = gen_enabled;
    gen_enabled = gen_confirm = 1;
    generate_drums();
    gen_enabled = enabled;
    return 1;
}

STUDIO_EXPORT uint32_t studio_fx(uint32_t track, uint32_t page)
{
    return track < TRACKS && page < SLAP_FX_PAGES ? slap_fx_config(track, page) : 0;
}

STUDIO_EXPORT void studio_set_fx(uint32_t track, uint32_t page, uint32_t word)
{
    if (track < TRACKS && page < SLAP_FX_PAGES && slap_fx_config_valid(page, word))
        slap_fx_restore(track, page, word);
}

STUDIO_EXPORT uint32_t studio_metronome(void) { return metronome; }

STUDIO_EXPORT void studio_set_metronome(uint32_t mode)
{
    if (mode <= 2u) metronome = mode;
}

STUDIO_EXPORT void studio_audition(uint32_t track, uint32_t pitch)
{
    if (track < TRACKS && pitch < 128) trigger(track, pitch, 102, RATE * 2 / 5, 0);
}
#pragma pop_macro("__attribute__")
#endif
