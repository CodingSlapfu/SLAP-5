/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "fastmath.h"
#include "synthesis.h"
#include "oscillator.h"
#include "effects.h"
#include "drum_samples.h"
#include "drum_kits.h"

#ifdef PLAT_HOST
#define dsp_fx_sample slap_fx_sample
#else
static float (*volatile dsp_fx_sample)(unsigned, float) = slap_fx_sample;
#endif

enum { TRACKS = 5, LEGACY_STEPS = 64, STEPS = 512, VOICES = 16, RATE = 44100 };
enum { HOME, SEQ, GLOBAL, PATCH, ENV, FX, VISUAL, SAVE, HARMONY, LFO, ARP, MIXER, RECORD, GEN_DRUMS, LIVE_FX, SETTINGS };
enum { NOTES = 0x7f, ACTIVE = 0x80, DRUMS = 4, OLD_DRUM_LANES = 6, DRUM_LANES = 16, MODERN_SYNTH = 1u << 27, ADVANCED_SYNTH = 1u << 28 };

typedef struct {
    volatile uint32_t steps[STEPS]; /* pitch, active flag, velocity, length, probability */
    volatile uint32_t patch, envelope, harmony, lfo;
    volatile uint32_t mix;          /* LSB first: length, pan, level, mute */
} Track;

typedef struct {
    float phase, inc, env, velocity, fall, bend;
    float attack, decay, sustain, release, drum_tune, drum_decay, glide_inc;
    float lfo_phase, lfo_inc, lfo_fade, lfo_gain, lfo_hold;
    float last_sample, steal_tail;
    float drum_noise, drum_color, drum_body, drum_snap;
    float drum_texture_gain, drum_noise_mix, drum_tone;
    uint32_t delay, rng;
    uint8_t lfo_wave, origin, kit, drum_family;
    uint32_t remaining, age, sequence_note;
    DrumSampleVoice sample;
    uint8_t track, pitch, manual, releasing, struck, drum_circuit, drum_layers, audition;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    uint8_t dispatch_cost;
#endif
} Voice;

static PLAT_POOL Track tracks[TRACKS];
static PLAT_POOL volatile uint32_t extra_note[DRUMS][STEPS];
typedef struct {
    uint8_t velocity_timing, gate, probability_timing;
} DrumCell;
static volatile DrumCell drum_lane[DRUM_LANES][STEPS];
enum { MELODIC_LANES = 4, SONG5_LANES = DRUMS * 2 + OLD_DRUM_LANES + 1,
       SONG7_LANES = SONG5_LANES + DRUMS * (MELODIC_LANES - 2),
       SONG_LANES = SONG7_LANES + DRUM_LANES - OLD_DRUM_LANES };
static PLAT_POOL volatile uint32_t poly_note[DRUMS][MELODIC_LANES - 2][STEPS];
enum { NOTE_DURATION_UNIT = 128 };
static PLAT_POOL uint16_t melodic_duration[DRUMS][MELODIC_LANES][STEPS];
static unsigned triggered_sequence_note, ratchet_tail;
/* Zero means V2–V4 eighth-step timing. Groups 1–4 hold the upper two
 * bits of a sixteenth-step offset; the event word holds its lower bits. */
static PLAT_POOL volatile uint8_t timing_group[SONG_LANES][STEPS / 2];

static inline unsigned timing_group_get(unsigned row, unsigned step)
{
    return (timing_group[row][step / 2u] >> ((step & 1u) * 4u)) & 15u;
}

static inline void timing_group_set(unsigned row, unsigned step, unsigned group)
{
    unsigned shift = (step & 1u) * 4u;
    volatile uint8_t *pair = &timing_group[row][step / 2u];
#ifndef PLAT_HOST
    uint32_t flags = irq_save();
#endif
    *pair = (uint8_t)((*pair & ~(15u << shift)) | (group << shift));
#ifndef PLAT_HOST
    irq_restore(flags);
#endif
}
static unsigned pattern_row(unsigned track, unsigned lane)
{
    if (track == DRUMS && lane > OLD_DRUM_LANES) return SONG7_LANES + lane - OLD_DRUM_LANES - 1u;
    if (track == DRUMS || lane < 2u) return track * 2u + lane;
    return SONG5_LANES + track * (MELODIC_LANES - 2u) + lane - 2u;
}

static const uint8_t lane_pitch[DRUM_LANES] = {36,38,42,37,39,49,35,40,46,44,45,47,50,51,70,81};
#include "drum_patterns.h"
static unsigned gen_style, gen_hit = 76, gen_groove = 35, gen_var = 28;
static unsigned gen_bars;
static unsigned gen_enabled = 1, gen_confirm;
static volatile unsigned gen_count, gen_queued;
static volatile uint32_t gen_applied_at;
static uint32_t gen_seed = 0xd14a1175u;
static volatile uint32_t drum_filters;
static volatile uint32_t live_fx_word;
static uint32_t fx_pressed_at, live_fx_suppressed;
static unsigned live_fx_armed, live_fx_full_mix;
static volatile uint32_t live_fx_assignments = 0x03020100u;
static uint32_t saved_assignments, pending_assignments;
static const unsigned live_fx_knobs[4] = {52, 33, 39, 41};
static const uint8_t live_black_keys[11] = PLAT_BLACK_KEYS;
static const char *const live_fx_names[] = {
    "GATE", "STUTTER", "BEAT REPEAT", "FILTER", "DELAY THROW",
    "HALO ROOM", "BITCRUSH", "PITCH MOD", "GLITCH", "VINYL STOP", "REWIND"
};

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
static volatile unsigned rec_follow_track, quick_wait_note;
static uint32_t edit_pressed_at, glo_pressed_at;
static unsigned edit_pending, glo_pending;
static uint32_t record_clock;
static struct {
    uint16_t *duration;
    uint32_t start, step_ticks, word;
    uint16_t cell;
    uint8_t pitch;
} held_record[VOICES];
static uint8_t monitored_next_step[TRACKS];
static volatile uint32_t metronome;
static uint32_t saved_preferences, pending_preferences, preferences_changed_at;
static uint32_t saved_mixer_options, pending_mixer_options, saved_duck, pending_duck;
enum { SETTINGS_TAG = 0x534c4d31u, SETTINGS_SETTLE_MS = 500u };
static unsigned click_remaining, click_accent, click_phase;
enum { CLICK_SAMPLES = RATE / 40 };
enum { BOOT_MS = 1400 };
static volatile uint32_t mixer_solo, dsp_overruns, intro_skipped;
static volatile uint32_t dsp_avg_ticks, dsp_peak_ticks, dsp_budget_ticks;
enum { DSP_BINS = 128, DSP_BIN_TICKS = 2400 };
static volatile uint32_t dsp_histogram[DSP_BINS], dsp_callbacks;
static volatile uint32_t dsp_reset_request, dsp_xrun_baseline;
static volatile unsigned global_details;
enum { PROFILE_INPUT, PROFILE_EVENTS, PROFILE_VOICES, PROFILE_FX, PROFILE_MIX, PROFILE_OUTPUT, PROFILE_COUNT };
static volatile uint32_t serial_peak, serial_at_peak[PROFILE_COUNT];
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
static uint32_t host_serial_fx_batch;
#endif
enum { SPIKE_NOTE_ON, SPIKE_LOCAL, SPIKE_DISPATCH, SPIKE_SPAN, SPIKE_FX, SPIKE_COUNT };
static volatile uint32_t spike_values[SPIKE_COUNT], spike_callback;
static uint32_t spike_current[SPIKE_COUNT];
static int spike_enabled, engine_profile;
volatile unsigned app_audio_half_profile_enabled;
enum { HALF_TRACE_SIZE = 8 };
typedef struct {
    uint32_t observed, half, app, note_on, span, wait, overlap, nested, timer5, xrun;
    uint32_t local, dispatch, voice, fx_batch, remote, jobs, ack, buffer, deadline;
} HalfTrace;
static volatile HalfTrace half_trace[HALF_TRACE_SIZE];
static volatile unsigned half_trace_written, half_trace_frozen, half_trace_faults;
static uint32_t half_app_ticks, half_wait_ticks, half_overlap_ticks;
static uint32_t half_voice_ticks, half_fx_ticks, half_remote_ticks, half_jobs;
static volatile uint32_t longest_half_ticks;
static unsigned dma_view;
enum { TRACE_DRUM_ENGINE = SLAP_ENGINES, TRACE_ENGINES = SLAP_ENGINES + 1 };
static volatile uint32_t local_engine_peaks[TRACE_ENGINES], remote_engine_peaks[TRACE_ENGINES];

void app_audio_half_profile(uint32_t ticks, uint32_t nested, uint32_t timer5_ticks, uint32_t ack_ticks, unsigned half, unsigned xrun)
{
    if (!spike_enabled || half_trace_frozen) return;
    const uint32_t budget = (24000000u / RATE) * APP_BLOCK + (24000000u % RATE) * APP_BLOCK / RATE;
    unsigned late = ticks > budget;
    if (!xrun && !late && (half_trace_faults || ticks <= longest_half_ticks)) return;
    if (xrun || late) ++half_trace_faults;
    if (ticks > longest_half_ticks) longest_half_ticks = ticks;
    unsigned index = half_trace_written & (HALF_TRACE_SIZE - 1u);
    half_trace[index].observed = plat_ticks24();
    half_trace[index].half = ticks;
    half_trace[index].app = half_app_ticks;
    half_trace[index].note_on = spike_current[SPIKE_NOTE_ON];
    half_trace[index].span = spike_current[SPIKE_SPAN];
    half_trace[index].wait = half_wait_ticks;
    half_trace[index].overlap = half_overlap_ticks;
    half_trace[index].nested = nested;
    half_trace[index].timer5 = timer5_ticks;
    half_trace[index].xrun = xrun;
    half_trace[index].local = spike_current[SPIKE_LOCAL];
    half_trace[index].dispatch = spike_current[SPIKE_DISPATCH];
    half_trace[index].voice = half_voice_ticks;
    half_trace[index].fx_batch = half_fx_ticks;
    half_trace[index].remote = half_remote_ticks;
    half_trace[index].jobs = half_jobs;
    half_trace[index].ack = ack_ticks;
    half_trace[index].buffer = half;
    half_trace[index].deadline = late;
    ++half_trace_written;
}


static volatile uint32_t peak_voice_batch, peak_fx_batch, peak_worker_work, peak_worker_wait;
#if FM1B_CPU2 && !defined(PLAT_WEB)
static volatile uint32_t peak_worker_overlap, peak_voice_recovery, peak_fx_recovery;
#endif

static volatile uint32_t active_voices, voice_steals;
#if defined(PLAT_HOST)
static unsigned host_peak_voices;
#endif
static volatile uint8_t mixer_proc[TRACKS], mixer_duck[DRUMS];
static float drum_duck_envelope, duck_amount[DRUMS], mix_share_gain[TRACKS];
static uint32_t boot_suppressed_keys;
static float mixer_envelope[TRACKS], mixer_gain[TRACKS];
static volatile uint32_t track_peak[TRACKS], fx_cost_ticks[TRACKS];
static Voice voices[VOICES];
static PLAT_POOL SlapSynth synths[VOICES];
static uint32_t track_timbre[DRUMS], track_fm_envelopes[DRUMS][7];
static unsigned edit_page, timbre_operator, patch_group[TRACKS];
static PLAT_POOL int16_t synth_strings[VOICES][SLAP_STRING_CAPACITY];
static volatile uint32_t tempo = 112, swing, playing, positions, sequence_epoch;
static volatile uint32_t perfect_note_off;
static volatile uint32_t keys_audio, input_track, input_octave;
static uint32_t old_keys, key_pitches[27], key_tracks[27], ages, noise = 13579;
static uint32_t until_step, step_duration, step_number, running_epoch;
static volatile uint32_t step_progress;
static volatile uint32_t step_audition_request;
static unsigned auditioning_step;
static uint32_t startup_sample;
static float startup_phase;
#define ARP_DEFAULT (2u << 8 | 1u << 16 | 64u << 24)
static volatile uint32_t arp_setup[DRUMS];
static volatile uint8_t arp_history[16];
static volatile uint32_t arp_cursor;
static uint32_t arp_pulse, arp_wait, arp_last_track = TRACKS, arp_stamp, arp_arrival[NKEYS];
static float master_gain, energy_smooth[TRACKS];
static volatile uint32_t track_energy[TRACKS], visual_notes[TRACKS];
static unsigned note_activity[TRACKS], dancer_pattern, dancer_outline = 1, dancer_lighting = 1, dancer_reactivity = 1;
#if FM1B_CPU2 && defined(PLAT_HOST) && !defined(PLAT_WEB)
static int host_discard_worker_result, host_discard_fx_result, host_late_voice;
enum { HOST_COST_SPLIT, HOST_SLOT_SPLIT, HOST_TRACK_SPLIT };
static unsigned host_voice_split;
#endif
enum { SPECTRUM_BANDS = 7 };
static const uint8_t visual_modes[] = {0, 1, 2, 3, 5, 7, 6};

static unsigned visual_index(unsigned mode)
{
    if (mode == 4u) mode = 5u;
    for (unsigned i = 0; i < sizeof visual_modes; ++i)
        if (visual_modes[i] == mode) return i;
    return sizeof visual_modes - 1u;
}

static int select_visual(unsigned mode, int delta)
{
    int index = (int)visual_index(mode) + delta;
    if (index < 0) index = 0;
    if (index >= (int)sizeof visual_modes) index = sizeof visual_modes - 1;
    return visual_modes[index];
}
static volatile float visual_rms[2], visual_peak[2], visual_band[SPECTRUM_BANDS];
static float meter_power[2], analysis_lp[SPECTRUM_BANDS - 1], analysis_power[SPECTRUM_BANDS];
static float analysis_mono;
static unsigned analysis_phase;

enum { ANALYSIS_QUEUE = 512, ANALYSIS_CHUNK = 128 };
static PLAT_POOL int16_t analysis_queue[ANALYSIS_QUEUE];
static volatile unsigned analysis_write, analysis_read;

static void capture_spectrum(float left, float right)
{
    analysis_mono += (left + right) * 0.125f;
    if (++analysis_phase != 4) return;
    unsigned write = analysis_write;
    if (write - analysis_read < ANALYSIS_QUEUE) {
        analysis_queue[write % ANALYSIS_QUEUE] = (int16_t)(analysis_mono * 32767.0f);
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
        float mono = analysis_queue[(read + i) % ANALYSIS_QUEUE] * (1.0f / 32767.0f);
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

/* Preserve the saved byte representation and quantize only on edits. */
static unsigned percent_from_byte(unsigned value)
{
    return (value * 100u + 127u) / 255u;
}

static unsigned level_after_turn(unsigned byte, int delta)
{
    unsigned percent = (unsigned)clamp((int)percent_from_byte(byte) + delta, 0, 100);
    return (percent * 255u + 50u) / 100u;
}

static int pan_percent(unsigned byte)
{
    if (byte < 128u) return -(int)(((128u - byte) * 100u + 64u) / 128u);
    return (int)(((byte - 128u) * 100u + 63u) / 127u);
}

static unsigned pan_after_turn(unsigned byte, int delta)
{
    int pan = clamp(pan_percent(byte) + delta, -100, 100);
    if (pan < 0) return 128u - ((unsigned)-pan * 128u + 50u) / 100u;
    return 128u + ((unsigned)pan * 127u + 50u) / 100u;
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
    track_timbre[track] = 0;
    __builtin_memset(track_fm_envelopes[track], 0, sizeof(track_fm_envelopes[track]));
    tracks[track].mix = (tracks[track].mix & ~ADVANCED_SYNTH) | MODERN_SYNTH |
        (patch->engine == 0u || patch->engine == 2u ? ADVANCED_SYNTH : 0u);
}

enum { PREF_AUDIO, PREF_METRO, PREF_RECORD, PREF_TRANSPORT, PREF_KEYS, PREF_DISPLAY, PREF_DIAG, PREF_LEDS, PREF_SECTIONS };
static unsigned settings_section, settings_item;
static const uint8_t settings_count[PREF_SECTIONS] = {4, 3, 4, 1, 1, 2, 1, 2};
static const char *const settings_names[] = {
    "AUDIO", "METRONOME", "RECORDING", "TRANSPORT", "KEYBOARD", "DISPLAY", "SYSTEM", "LEDS"
};
static volatile uint32_t output_trim, speaker_mode, click_trim, resume_playback, auto_level_off, auto_mix;
static unsigned visual_quality, led_style, led_keys_on_hit;
static volatile unsigned split_keys;

void app_init(void)
{
    uint32_t settings[5];
    int setting_bytes = plat_store_load(OBJ_SETTINGS, settings, sizeof settings);
    saved_preferences = 0;
    live_fx_assignments = 0x03020100u;
    if ((setting_bytes == 8 || setting_bytes == 12 || setting_bytes == 16 || setting_bytes == 20) && settings[0] == SETTINGS_TAG &&
        (settings[1] & 3u) <= 2u && (settings[1] >> 6 & 7u) <= 5u &&
        (settings[1] >> 11 & 15u) <= REC_QUANT_MAX)
        saved_preferences = settings[1];
    if ((setting_bytes == 12 || setting_bytes == 16 || setting_bytes == 20) && settings[0] == SETTINGS_TAG) {
        unsigned valid = 1;
        for (unsigned k = 0; k < 4u; ++k)
            if (!slap_live_parameter_name(settings[2] >> (8u * k) & 255u)) valid = 0;
        if (valid) live_fx_assignments = settings[2];
    }
    uint32_t options = setting_bytes >= 16 && settings[0] == SETTINGS_TAG ? settings[3] : 0u;
    uint32_t duck = setting_bytes == 20 && settings[0] == SETTINGS_TAG ? settings[4] : 0u;
    auto_mix = options & 1u;
    dancer_pattern = (unsigned)clamp((options >> 1) & 3u, 0, 2);
    dancer_outline = !(options & 8u);
    dancer_lighting = !(options & 16u);
    dancer_reactivity = !(options & 32u);
    for (unsigned t = 0; t < DRUMS; ++t)
        mixer_duck[t] = (uint8_t)clamp((int)((duck >> (8u * t)) & 255u), 0, 100);
    drum_duck_envelope = 0.0f;
    saved_mixer_options = pending_mixer_options = options | 128u |
        ((!(options & 128u) && (saved_preferences >> 30) == 3u) ? 64u : 0u);
    saved_duck = pending_duck = duck;
    saved_assignments = pending_assignments = live_fx_assignments;
    pending_preferences = saved_preferences;
    metronome = saved_preferences & 3u;
    perfect_note_off = saved_preferences >> 2 & 15u;
    rec_bars = saved_preferences >> 6 & 7u;
    if (rec_bars > 2u) rec_bars = 2u;
    rec_count_in = saved_preferences >> 9 & 1u;
    rec_replace = saved_preferences >> 10 & 1u;
    rec_quantize = saved_preferences >> 11 & 15u;
    output_trim = saved_preferences >> 15 & 3u;
    speaker_mode = saved_preferences >> 17 & 1u;
    click_trim = saved_preferences >> 18 & 3u;
    resume_playback = saved_preferences >> 20 & 1u;
    visual_quality = saved_preferences >> 21 & 1u;
    auto_level_off = saved_preferences >> 25 & 1u;
    split_keys = saved_preferences >> 26 & 15u;
    led_style = saved_preferences >> 30;
    led_keys_on_hit = !!(options & 64u) || (!(options & 128u) && led_style == 3u);
    slap_ui.visual = select_visual(saved_preferences >> 22 & 7u, 0);
    startup_sample = 0;
    startup_phase = 0.0f;
    slap_fx_init();
#if FM1B_CPU2 && !defined(PLAT_WEB)
#ifdef PLAT_HOST
    const char *split = getenv("SLAP_TEST_VOICE_SPLIT");
    host_voice_split = split && strcmp(split, "slots") == 0 ? HOST_SLOT_SPLIT :
        split && strcmp(split, "tracks") == 0 ? HOST_TRACK_SPLIT : HOST_COST_SPLIT;
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
        tracks[t].mix = 16u | (128u << 8) | ((t == DRUMS ? 196u : 170u) << 16);
        mixer_proc[t] = 255;
        mixer_gain[t] = mix_share_gain[t] = 1.0f;
        if (t < DRUMS) duck_amount[t] = 0.0f;
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

/* Musical groups are a view over stable factory IDs. */
static const uint8_t patch_family[] = {
    8, 1, 4, 4, 5, 5, 2, 2, 2, 3, 2, 2, 8, 4, 2, 8,
    8, 6, 1, 4, 8, 2, 5, 5, 5, 5, 4, 6, 6, 3, 6, 6,
    5, 5, 1, 5, 5, 8, 8, 8, 1, 2, 8, 8, 8, 8, 5, 5,
    4, 1, 8, 4, 8, 1, 8, 8, 2, 8, 8, 8, 8, 8, 2, 2,
    2, 2, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4,
    4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 6, 6, 6,
    6, 6, 7, 7, 7, 1, 7, 7, 7, 7, 7, 1, 7, 7, 7, 7,
    7, 5, 5, 5, 5, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    2, 3, 3, 2, 8, 2, 2, 1,
};
_Static_assert(sizeof patch_family == SLAP_PATCHES, "group every patch");

static const char *const patch_groups[] = {
    "ALL PATCHES", "BASS", "LEADS", "PADS", "KEYS / PIANOS", "PLUCKS / BELLS",
    "STRINGS / CHOIRS", "ACOUSTIC / WINDS", "EXPERIMENTAL / FX"
};
static const char *const kit_groups[] = {"FACTORY KITS", "LEGACY KITS", "ALL KITS"};

static int patch_in_group(unsigned track, unsigned id)
{
    unsigned group = patch_group[track];
    if (track != DRUMS) return !group || patch_family[id] == group;
    return group == 2u || (id < DRUM_LEGACY_KITS) == (group == 1u);
}

static unsigned browse_patch(unsigned track, unsigned current, int delta)
{
    int limit = track == DRUMS ? DRUM_KITS : SLAP_PATCHES;
    int dir = delta < 0 ? -1 : 1;
    if (!patch_in_group(track, current)) {
        int id = dir > 0 ? 0 : limit - 1;
        while (!patch_in_group(track, (unsigned)id)) id += dir;
        current = (unsigned)id;
        delta -= dir;
    }
    while (delta) {
        int id = (int)current + dir;
        while (id >= 0 && id < limit && !patch_in_group(track, (unsigned)id)) id += dir;
        if (id < 0 || id >= limit) break;
        current = (unsigned)id;
        delta -= dir;
    }
    return current;
}

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
    if (voice->track == DRUMS && voice->kit >= DRUM_LEGACY_KITS)
        return 4u | VOICE_LOCAL_ONLY; /* Serialized XIP reads until measured on FM-1. */
    if (voice->track == DRUMS)
        return (voice->drum_family == 0 ? 5u : 4u) + (voice->drum_layers ? 2u : 0u);
    unsigned engine = synth->engine;
    unsigned cost = engine_cost[engine] + (voice->lfo_wave ? 2u : 0u);
    if (synth->modern && (engine == 0 || engine == 13 || engine == 16 || engine == 18 || engine == 20)) cost += 2u;
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
    uint32_t started = spike_enabled ? plat_ticks24() : 0u;
    Voice *voice = &voices[0];
    float glide_start = 0.0f;
    if (manual && track < DRUMS && !delay && !(tracks[track].harmony >> 16 & 255u)) {
        unsigned patch = tracks[track].patch;
        if ((patch >= 73u && patch <= 77u) || patch == 131u) {
            unsigned held = 0;
            for (unsigned i = 0; i < VOICES; ++i) {
                const Voice *prior = &voices[i];
                if (!prior->age || prior->track != track || !prior->manual ||
                    prior->releasing || prior->delay || prior->pitch == pitch) continue;
                if (++held > 1u) { glide_start = 0.0f; break; }
                glide_start = prior->glide_inc > 0.0f ? prior->glide_inc : prior->inc;
            }
        }
    }
    float quietest_release = 0.0f;
    for (unsigned i = 0; i < VOICES; ++i) {
        Voice *candidate = &voices[i];
        if (!candidate->age) { voice = candidate; break; }
        if (candidate->audition && !auditioning_step) {
            if (!voice->audition) voice = candidate;
            continue;
        }
        if (voice->audition && !auditioning_step) continue;
        if (candidate->releasing) {
            float loudness = candidate->env * candidate->velocity;
            if (!voice->releasing || i == 0 || loudness < quietest_release) {
                voice = candidate;
                quietest_release = loudness;
            }
        } else if (!voice->releasing && candidate->age < voice->age) {
            voice = candidate;
        }
    }
    if (auditioning_step && voice->age) return;
    float tail = voice->age && voice->track == track ? voice->last_sample : 0.0f;
    if (voice->age && !voice->audition) ++voice_steals;
    *voice = (Voice) {
        .velocity = (float)velocity / 127.0f, .fall = 1.0f, .bend = 1.0f,
        .remaining = duration, .age = ++ages, .sequence_note = triggered_sequence_note,
        .track = (uint8_t)track, .pitch = (uint8_t)pitch, .manual = (uint8_t)manual,
        .origin = (uint8_t)origin, .delay = delay, .steal_tail = tail, .audition = (uint8_t)auditioning_step
    };
    ++visual_notes[track];
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
                         tracks[track].mix & ADVANCED_SYNTH ? 2 : !!(tracks[track].mix & MODERN_SYNTH));
        if (tracks[track].mix & ADVANCED_SYNTH) {
            slap_synth_timbre(&synths[slot], track_timbre[track]);
            if (patch->engine == 0u)
                slap_synth_fm_envelopes(&synths[slot], track_fm_envelopes[track], track_fm_envelopes[track][6]);
        }
        voice->inc = synths[slot].inc;
        voice->glide_inc = glide_start;
    } else if (tracks[DRUMS].patch >= DRUM_LEGACY_KITS) {
        voice->kit = (uint8_t)tracks[DRUMS].patch;
        unsigned hit = drum_sample_hit(pitch);
        voice->sample.id = (uint16_t)((voice->kit - DRUM_LEGACY_KITS) * SAMPLE_HITS + hit);
        voice->env = 1.0f;
        voice->remaining = 0;
        if (hit >= 4u && hit <= 6u && !ratchet_tail)
            for (unsigned slot = 0; slot < VOICES; ++slot) {
                Voice *other = &voices[slot];
                unsigned other_hit = other->sample.id % SAMPLE_HITS;
                if (other != voice && other->age && other->track == DRUMS &&
                    other->kit >= DRUM_LEGACY_KITS && other_hit >= 4u && other_hit <= 6u &&
                    (!auditioning_step || other->audition))
                    other->releasing = 1;
            }
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
    if (spike_enabled) spike_current[SPIKE_NOTE_ON] += plat_ticks24() - started;
}

static void record_input_origin(unsigned track, unsigned pitch, unsigned velocity, unsigned origin, int finish_previous);

static void trigger_delayed(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration,
                            int manual, unsigned offset, int split_key)
{
    uint32_t harmony = tracks[track].harmony;
    unsigned root = harmony & 255u, scale = (harmony >> 8) & 255u;
    unsigned chord = (harmony >> 16) & 255u, strum = harmony >> 24;
    int correct = scale && !(perfect_note_off & (1u << track));
    if (split_key == 0) chord = 0;
    if (split_key < 0 && !manual && (split_keys & (1u << track))) {
        chord = 0;
        correct = 0;
    }
    if (track == DRUMS || (!correct && !chord)) {
        if (split_key >= 0) record_input_origin(track, pitch, velocity, pitch, 1);
        trigger_voice(track, pitch, velocity, duration, manual, pitch, offset);
        return;
    }
    unsigned anchor = note_in_scale(pitch, root, scale);
    unsigned tonic = correct ? anchor : pitch;
    unsigned pitches[4] = {tonic, tonic, tonic, tonic};
    unsigned count = 1;
    if (chord) {
        unsigned third = scale ? scale_degree(anchor, 2, root, scale) - anchor : 4u;
        unsigned fifth = scale ? scale_degree(anchor, 4, root, scale) - anchor : 7u;
        unsigned seventh = scale ? scale_degree(anchor, 6, root, scale) - anchor : 11u;
        third = (unsigned)clamp((int)(tonic + third), 0, 127);
        fifth = (unsigned)clamp((int)(tonic + fifth), 0, 127);
        pitches[1] = chord == 3 ? fifth : third;
        pitches[2] = fifth;
        pitches[3] = (unsigned)clamp((int)(tonic + seventh), 0, 127);
        count = chord == 3 ? 2u : chord == 2 ? 4u : 3u;
    }
    unsigned previous = 255;
    for (unsigned i = 0; i < count; ++i) {
        if (pitches[i] == previous) continue;
        unsigned delay = strum && count > 1 ? RATE * (99u + strum) / 1000u * i / (count - 1u) : 0u;
        unsigned note_velocity = velocity * (i ? 75u : 100u) / 100u;
        if (split_key >= 0) record_input_origin(track, pitches[i], note_velocity, pitch, i == 0);
        trigger_voice(track, pitches[i], note_velocity, duration, manual, pitch, offset + delay);
        previous = pitches[i];
    }
}

static void trigger(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration, int manual)
{
    trigger_delayed(track, pitch, velocity, duration, manual, 0, -1);
}

static unsigned timing_of(uint32_t word)
{
    return ((word >> 15) & 1u) | ((word >> 30) & 2u);
}

/* Five timing groups times three step modes fit the undo snapshot's nibble. */
enum { TIMING_GROUPS = 5 };
enum { STEP_NORMAL, STEP_RATCHET, STEP_FILL };
static volatile unsigned fill_active;

static uint32_t timing_bits(unsigned offset)
{
    return ((offset & 1u) << 15) | ((offset & 2u) << 30);
}

static unsigned note_timing(unsigned row, unsigned step, uint32_t word)
{
    unsigned group = timing_group_get(row, step) % TIMING_GROUPS;
    return group ? (group - 1u) * 4u + timing_of(word) : timing_of(word) * 2u;
}

static unsigned melodic_lane_from_row(unsigned track, unsigned row)
{
    return row < SONG5_LANES ? row % 2u :
        2u + row - SONG5_LANES - track * (MELODIC_LANES - 2u);
}

static void schedule_event(unsigned track, unsigned row, unsigned step, uint32_t word)
{
    if (!(word & ACTIVE)) return;
    unsigned mode = timing_group_get(row, step) / TIMING_GROUPS;
    if (mode == STEP_FILL && !fill_active) return;
    unsigned probability = (word >> 24) & 127u;
    if (probability <= random_word() % 100u) return;
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
    if (mode == STEP_RATCHET) {
        unsigned interval = until_step / 2u;
        if (delay + interval < until_step) {
            if (gate > interval) gate = interval;
            trigger_delayed(track, word & NOTES, (word >> 8) & 127u, gate ? gate : 1u, 0, delay, -1);
            delay += interval;
            ratchet_tail = 1;
        }
    }
    trigger_delayed(track, word & NOTES, (word >> 8) & 127u, gate ? gate : 1u, 0, delay, -1);
    triggered_sequence_note = 0;
    ratchet_tail = 0;
    ++scheduled_events;
}

static void release_note(unsigned track, unsigned pitch)
{
    for (unsigned i = 0; i < VOICES; ++i)
        if (voices[i].manual && voices[i].origin == pitch && voices[i].track == track &&
            (track != DRUMS || voices[i].kit < DRUM_LEGACY_KITS))
            voices[i].releasing = 1;
}

static inline uint32_t drum_cell_get(unsigned lane, unsigned step)
{
    const volatile DrumCell *cell = &drum_lane[lane][step];
    unsigned velocity = cell->velocity_timing;
    return velocity & 127u ? ACTIVE | lane_pitch[lane] | velocity << 8 |
        (uint32_t)cell->gate << 16 | (uint32_t)cell->probability_timing << 24 : 0u;
}

static inline void drum_cell_set(unsigned lane, unsigned step, uint32_t word)
{
    volatile DrumCell *cell = &drum_lane[lane][step];
    // Core 0 schedules and edits these cells. Keep all three bytes coherent
    // when an audio interrupt lands during a foreground edit.
#ifndef PLAT_HOST
    uint32_t flags = irq_save();
#endif
    cell->velocity_timing = (uint8_t)(word >> 8);
    cell->gate = (uint8_t)(word >> 16);
    cell->probability_timing = (uint8_t)(word >> 24);
#ifndef PLAT_HOST
    irq_restore(flags);
#endif
}

static volatile uint32_t *melodic_cell(unsigned track, unsigned step, unsigned lane)
{
    if (!lane) return &tracks[track].steps[step];
    return lane == 1u ? &extra_note[track][step] : &poly_note[track][lane - 2u][step];
}

static inline uint32_t event_get(unsigned track, unsigned step, unsigned lane)
{
    return track == DRUMS && lane ? drum_cell_get(lane - 1u, step) : *melodic_cell(track, step, lane);
}

static inline void event_set(unsigned track, unsigned step, unsigned lane, uint32_t word)
{
    if (track == DRUMS && lane) drum_cell_set(lane - 1u, step, word);
    else *melodic_cell(track, step, lane) = word;
}

static void finish_record_note(unsigned slot, uint32_t now)
{
    unsigned cell = held_record[slot].cell;
    if (!cell) return;
    unsigned index = cell - 1u;
    unsigned track_lane = index / STEPS;
    unsigned track = track_lane / (DRUM_LANES + 1u);
    unsigned lane = track_lane % (DRUM_LANES + 1u);
    unsigned step = index % STEPS;
    uint32_t word = event_get(track, step, lane);
    if (word == held_record[slot].word) {
        unsigned elapsed = now - held_record[slot].start;
        unsigned span = held_record[slot].step_ticks;
        unsigned gate = elapsed >= span * 2u ? 255u :
            (unsigned)clamp((int)((elapsed * 192u + span / 2u) / span), 1, 255);
        if (held_record[slot].duration) {
            unsigned units = elapsed / NOTE_DURATION_UNIT +
                ((elapsed % NOTE_DURATION_UNIT) >= NOTE_DURATION_UNIT / 2u);
            *held_record[slot].duration = (uint16_t)clamp((int)units, 1, 65535);
        }
        event_set(track, step, lane, (word & ~(255u << 16)) | (gate << 16));
    }
    held_record[slot].cell = 0;
}

static unsigned lane_count(unsigned track);

enum { LIVE_LOOP_MAX = 64 };
static PLAT_POOL uint8_t queued_drums[DRUM_LANES][LIVE_LOOP_MAX];
static uint32_t drum_step_locks[DRUM_LANES + 1][LIVE_LOOP_MAX / 32];
static unsigned queued_drum_length, queued_drum_previous_length, queued_drum_revision, queued_drum_kit, queued_drum_swing;

typedef struct {
    uint32_t cells[DRUM_LANES + 1][LIVE_LOOP_MAX];
    uint8_t timing[DRUM_LANES + 1][LIVE_LOOP_MAX / 2];
    uint16_t duration[MELODIC_LANES][LIVE_LOOP_MAX];
} LoopSnapshot;

/* Short loops share the same pool allocation as one maximum-length legacy
 * snapshot. Long legacy edits keep their original whole-track undo behavior. */
static PLAT_POOL union {
    LoopSnapshot loops[TRACKS];
    union {
        struct {
            uint32_t free_notes[STEPS];
            DrumCell fixed[DRUM_LANES][STEPS];
            uint8_t timing[DRUM_LANES + 1][STEPS / 2];
        } drums;
        struct {
            uint32_t cells[MELODIC_LANES][STEPS];
            uint8_t timing[MELODIC_LANES][STEPS / 2];
            uint16_t duration[MELODIC_LANES][STEPS];
        } melody;
    } legacy;
} snapshot;

typedef struct {
    unsigned length, recorded_length, ready;
    uint32_t after;
} UndoState;
static UndoState take_undo[TRACKS];
static unsigned undo_latest, snapshot_legacy;

typedef struct {
    uint32_t *cells;
    uint8_t *timing;
    uint16_t *duration;
    DrumCell *fixed;
} SnapshotLane;

static SnapshotLane snapshot_lane(unsigned track, unsigned lane)
{
    if (!snapshot_legacy) {
        LoopSnapshot *loop = &snapshot.loops[track];
        return (SnapshotLane){.cells = loop->cells[lane], .timing = loop->timing[lane],
                              .duration = track < DRUMS ? loop->duration[lane] : 0};
    }
    if (track == DRUMS)
        return (SnapshotLane){.cells = lane ? 0 : snapshot.legacy.drums.free_notes,
            .timing = snapshot.legacy.drums.timing[lane],
            .fixed = lane ? snapshot.legacy.drums.fixed[lane - 1u] : 0};
    return (SnapshotLane){.cells = snapshot.legacy.melody.cells[lane],
                          .timing = snapshot.legacy.melody.timing[lane], .duration = snapshot.legacy.melody.duration[lane]};
}

static void undo_recording(unsigned track)
{
    UndoState *undo = &take_undo[track];
    if (recording || rec_running || !undo->ready) return;
    undo->ready = 0;
    if (pattern_revision[track] != undo->after) return;
    monitored_next_step[track] = 0;
    unsigned length = undo->recorded_length;
    unsigned lanes = lane_count(track);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        SnapshotLane saved = snapshot_lane(track, lane);
        volatile uint8_t *groups = timing_group[pattern_row(track, lane)];
        if (saved.fixed) {
            for (unsigned step = 0; step < length; ++step) drum_lane[lane - 1u][step] = saved.fixed[step];
        } else {
            for (unsigned step = 0; step < length; ++step) event_set(track, step, lane, saved.cells[step]);
        }
        for (unsigned pair = 0; pair < length / 2u; ++pair) groups[pair] = saved.timing[pair];
        if (length & 1u) timing_group_set(pattern_row(track, lane), length - 1u, saved.timing[length / 2u] & 15u);
        if (saved.duration)
            for (unsigned step = 0; step < length; ++step)
                melodic_duration[track][lane][step] = saved.duration[step];
    }
    track_setting(track, 0, undo->length);
}

static void finish_snapshot(unsigned track)
{
    take_undo[track].after = pattern_revision[track];
    take_undo[track].ready = 1;
    undo_latest = track;
}

static void end_recording(uint32_t now)
{
    for (unsigned slot = 0; slot < VOICES; ++slot) finish_record_note(slot, now);
    finish_snapshot(rec_track);
    rec_running = rec_countdown = 0;
}

static void snapshot_take(unsigned track, unsigned length)
{
    unsigned legacy = length > LIVE_LOOP_MAX;
    if (legacy || snapshot_legacy)
        for (unsigned t = 0; t < TRACKS; ++t) take_undo[t].ready = 0;
    snapshot_legacy = legacy;
    take_undo[track] = (UndoState){
        .length = pattern_length(tracks[track].mix), .recorded_length = length
    };
    unsigned lanes = lane_count(track);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        SnapshotLane saved = snapshot_lane(track, lane);
        const volatile uint8_t *groups = timing_group[pattern_row(track, lane)];
        if (saved.fixed) {
            for (unsigned step = 0; step < length; ++step) saved.fixed[step] = drum_lane[lane - 1u][step];
        } else {
            for (unsigned step = 0; step < length; ++step) saved.cells[step] = event_get(track, step, lane);
        }
        for (unsigned pair = 0; pair < length / 2u; ++pair)
            saved.timing[pair] = groups[pair];
        if (length & 1u) saved.timing[length / 2u] = groups[length / 2u] & 15u;
        if (saved.duration)
            for (unsigned step = 0; step < length; ++step)
                saved.duration[step] = melodic_duration[track][lane][step];
    }
}

static void clear_events(unsigned track, unsigned length)
{
    for (unsigned lane = 0; lane < lane_count(track); ++lane) {
        volatile uint8_t *groups = timing_group[pattern_row(track, lane)];
        if (track == DRUMS && lane) {
            for (unsigned step = 0; step < length; ++step) drum_lane[lane - 1u][step] = (DrumCell){0};
        } else {
            volatile uint32_t *cells = melodic_cell(track, 0, lane);
            for (unsigned step = 0; step < length; ++step) cells[step] = 0;
        }
        for (unsigned pair = 0; pair < length / 2u; ++pair) groups[pair] = 0;
        if (length & 1u) timing_group_set(pattern_row(track, lane), length - 1u, 0);
        if (track < DRUMS)
            for (unsigned step = 0; step < length; ++step) melodic_duration[track][lane][step] = 0;
    }
    ++pattern_revision[track];
}

static struct {
    uint32_t word;
    uint16_t duration;
    uint8_t timing, track, lane;
    unsigned valid;
} step_clipboard;

static unsigned clear_mode, clear_track;
static uint32_t clear_armed_at;
/* Foreground publishes one request. The audio callback owns pattern mutation. */
enum { PATTERN_UNDO = 1, PATTERN_CLEAR_LOOP, PATTERN_CLEAR_TRACK, PATTERN_CLEAR_LEGACY };
static volatile uint32_t pattern_action;

static unsigned drum_step_locked(unsigned lane, unsigned step)
{
    return step < LIVE_LOOP_MAX &&
        (drum_step_locks[lane][step / 32u] & (1u << (step & 31u)));
}

static void apply_generated_drums(void)
{
    if (!gen_queued) return;
    unsigned length = queued_drum_length;
    if (!recording && !rec_running && pattern_revision[DRUMS] == queued_drum_revision &&
        tracks[DRUMS].patch == queued_drum_kit && pattern_length(tracks[DRUMS].mix) == queued_drum_previous_length) {
        snapshot_take(DRUMS, length);
        for (unsigned lane = 0; lane <= DRUM_LANES; ++lane) {
            unsigned row = pattern_row(DRUMS, lane);
            for (unsigned step = 0; step < length; ++step) {
                if (drum_step_locked(lane, step)) continue;
                unsigned velocity = lane ? queued_drums[lane - 1u][step] : 0u;
                unsigned gate = lane == 6u ? 154u : lane > 6u ? 104u : 105u;
                unsigned offset = lane <= OLD_DRUM_LANES && (step & 1u) ? queued_drum_swing : 0u;
                event_set(DRUMS, step, lane, velocity ?
                    cell_word(lane_pitch[lane - 1u], velocity, gate, 100u) | timing_bits(offset) : 0u);
                timing_group_set(row, step, 0);
            }
        }
        track_setting(DRUMS, 0, length);
        ++pattern_revision[DRUMS];
        finish_snapshot(DRUMS);
        ++gen_count;
        gen_applied_at = plat_ms();
    }
    gen_queued = 0;
}

static void apply_pattern_action(void)
{
    uint32_t request = pattern_action;
    if (!request || recording || rec_running) return;
    unsigned track = request >> 8;
    unsigned action = request & 255u;
    if (action == PATTERN_UNDO) {
        undo_recording(track);
    } else {
        unsigned length = action == PATTERN_CLEAR_LEGACY ? STEPS :
            action == PATTERN_CLEAR_TRACK ? LIVE_LOOP_MAX : pattern_length(tracks[track].mix);
        snapshot_take(track, length);
        clear_events(track, length);
        monitored_next_step[track] = 0;
        finish_snapshot(track);
    }
    pattern_action = 0;
}

static int track_has_hidden_events(unsigned track)
{
    if (pattern_length(tracks[track].mix) > LIVE_LOOP_MAX) return 1;
    unsigned lanes = lane_count(track);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        for (unsigned step = LIVE_LOOP_MAX; step < STEPS; ++step)
            if (event_get(track, step, lane)) return 1;
    }
    return 0;
}

static void confirm_clear(unsigned track, unsigned all)
{
    unsigned action = 1u + all;
    if (clear_mode != action || clear_track != track) {
        clear_mode = action;
        clear_track = track;
        clear_armed_at = plat_ms();
        return;
    }
    clear_mode = 0;
    if (!pattern_action) {
        unsigned command = all ? PATTERN_CLEAR_TRACK : PATTERN_CLEAR_LOOP;
        if (all && track_has_hidden_events(track)) command = PATTERN_CLEAR_LEGACY;
        pattern_action = (track << 8) | command;
    }
}

static void begin_recording(void)
{
    unsigned track = rec_track;
    snapshot_take(track, rec_length);
    if (rec_overwrite) clear_events(track, rec_length);
    track_setting(track, 0, rec_length);
    rec_begin_events = recorded_events;
    rec_rejected = 0;
    rec_running = 1;
    quick_wait_note = 0;
}

static void record_note_off(unsigned track, unsigned pitch)
{
    if (!rec_running || track != rec_track) return;
    for (unsigned slot = 0; slot < VOICES; ++slot)
        if (held_record[slot].cell && held_record[slot].pitch == pitch)
            finish_record_note(slot, record_clock);
}

/* Single audio-thread writer for live input. A filled two-note step is left intact. */
static void record_input_origin(unsigned track, unsigned pitch, unsigned velocity, unsigned source_pitch, int finish_previous)
{
    if (!recording || track != rec_track) return;
    if (!playing) {
        if (!quick_wait_note) return;
        playing = 1;
    }
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
    unsigned lane = 0;
    if (track == DRUMS) {
        lane = drum_family(pitch) + 1;
        for (unsigned i = 0; i < DRUM_LANES; ++i)
            if (pitch == lane_pitch[i]) { lane = i + 1u; break; }
        pitch = lane_pitch[lane - 1];
    } else {
        unsigned empty = MELODIC_LANES;
        for (lane = 0; lane < MELODIC_LANES; ++lane) {
            uint32_t entry = event_get(track, step, lane);
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
        }
    }
    if (rec_loop && (event_get(track, step, lane) & ACTIVE)) return;
    unsigned cell = 1u + (track * (DRUM_LANES + 1u) + lane) * STEPS + step;
    if (finish_previous) record_note_off(track, source_pitch);
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
    timing_group_set(pattern_row(track, lane), step, timing ? (uint8_t)(timing / 4u + 1u) : 0);
    uint32_t word = cell_word(pitch, velocity, 144u, 100u) | timing_bits(timing);
    event_set(track, step, lane, word);
    uint16_t *duration = track < DRUMS ? &melodic_duration[track][lane][step] : 0;
    if (duration) *duration = 0;
    held_record[available].cell = cell;
    held_record[available].duration = duration;
    held_record[available].start = record_clock;
    held_record[available].step_ticks = step_duration && until_step ? step_duration : RATE * 15u / tempo;
    held_record[available].word = word;
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

static void record_input(unsigned track, unsigned pitch, unsigned velocity)
{
    record_input_origin(track, pitch, velocity, pitch, 1);
}

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
                if (track == DRUMS && tracks[DRUMS].patch >= DRUM_LEGACY_KITS) {
                    static const uint8_t hits[SAMPLE_HITS] = {
                        36,35,38,40,42,46,44,39,37,45,47,50,49,51,70,81
                    };
                    const uint8_t white[SAMPLE_HITS] = PLAT_WHITE_KEYS;
                    for (unsigned hit = 0; hit < SAMPLE_HITS; ++hit)
                        if (white[hit] == k) { pitch = hits[hit]; break; }
                }
                key_pitches[k] = pitch;
                key_tracks[k] = track;
                arp_arrival[k] = ++arp_stamp;
                if (track < DRUMS && (split_keys & (1u << track))) {
                    unsigned black = 0;
                    for (unsigned i = 0; i < sizeof live_black_keys; ++i)
                        black |= k == live_black_keys[i];
                    trigger_delayed(track, pitch, 100, 0, 1, 0, black);
                } else {
                    record_input(track, pitch, 100);
                    if (track == DRUMS || !(arp_setup[track] & 255u))
                        trigger(track, pitch, 100, 0, 1);
                }
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

static void arp_render_clock(unsigned track, uint32_t config)
{
    static const uint8_t ratios[] = {4, 8, 16, 32, 12, 24};
    unsigned mode = config & 255u;
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
        if (!(perfect_note_off & (1u << track)))
            pitch = note_in_scale(pitch, harmony & 255u, (harmony >> 8) & 255u);
        if (!i) first = pitch;
        trigger_voice(track, pitch, 100, gate, 0, pitch, 0);
    }
    arp_history[arp_cursor & 15u] = (uint8_t)first;
    ++arp_cursor;
    ++arp_pulse;
}

static void audition_selected_step(void)
{
    uint32_t request = step_audition_request;
    if (!request) return;
    step_audition_request = 0;
    if (recording || rec_running) return;
    unsigned track = (request >> 16) - 1u;
    unsigned step = request & 65535u;
    for (unsigned lane = 0; lane < lane_count(track); ++lane) {
        uint32_t word = event_get(track, step, lane);
        if (!(word & ACTIVE)) continue;
        unsigned pitch = word & NOTES;
        unsigned gate = (RATE * 15u / tempo) * ((word >> 16) & 255u) / 192u;
        if (track < DRUMS && melodic_duration[track][lane][step])
            gate = melodic_duration[track][lane][step] * NOTE_DURATION_UNIT;
        unsigned bounded_gate = (unsigned)clamp((int)gate, 1, RATE / 4);
        auditioning_step = 1;
        trigger_voice(track, pitch, (word >> 8) & 127u, bounded_gate, 0, pitch, 0);
        auditioning_step = 0;
    }
}

static unsigned muted_drums(void)
{
    unsigned filter = drum_filters, solo = filter >> 16;
    return solo ? 65535u & ~solo : filter & 65535u;
}

static void next_step(const uint32_t *mix)
{
    unsigned base = RATE * 15u / tempo;
    unsigned offset = base * swing / 200u;
    until_step = step_number & 1u ? base - offset : base + offset;
    step_duration = until_step;
    positions = step_number++;
    slap_fx_transport_step(positions, step_duration, base);
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
                    schedule_event(t, pattern_row(t, lane), step, event_get(t, step, lane));
        } else {
            unsigned muted = muted_drums();
            if (!(monitored & 1u) && !(muted & (1u << drum_family(word & NOTES)))) schedule_event(t, pattern_row(t, 0), step, word);
            for (unsigned lane = 0; lane < DRUM_LANES; ++lane)
                if (!(muted & (1u << lane)) && !(monitored & (2u << lane)))
                    schedule_event(t, pattern_row(t, lane + 1), step, drum_cell_get(lane, step));
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
    if (v->track == DRUMS && v->kit >= DRUM_LEGACY_KITS) {
        int finished = 0;
        int pcm = drum_sample_next(&v->sample, &finished);
        if (v->releasing) v->env *= 0.985f;
        if (finished || v->env < 0.0001f) {
            v->age = 0;
            return 0.0f;
        }
        return pcm * (0.22f / 32768.0f) * v->velocity * v->env;
    }
    if (!v->manual && v->remaining && !--v->remaining) v->releasing = 1;
    float target = v->releasing ? 0.0f : v->struck ? v->sustain : 1.0f;
    float rate = v->releasing ? v->release : v->struck ? v->decay : v->attack;
    v->env += (target - v->env) * rate;
    if (v->env > 0.94f && !v->releasing) v->struck = 1;
    if (v->releasing && v->env < 0.0001f) { v->age = 0; return 0.0f; }

    if (v->track < DRUMS) {
        float pitch_inc = v->inc;
        if (v->glide_inc > 0.0f) {
            pitch_inc = v->glide_inc + (v->inc - v->glide_inc) * 0.00135f;
            if (fm_fabsf(v->inc - pitch_inc) < v->inc * 0.0002f) {
                v->glide_inc = 0.0f;
                pitch_inc = v->inc;
            } else v->glide_inc = pitch_inc;
            s->inc = pitch_inc;
        }
        if (v->lfo_wave) {
            v->lfo_gain = fm_minf(1.0f, v->lfo_gain + v->lfo_fade);
            v->lfo_phase += v->lfo_inc;
            if (v->lfo_phase >= 1.0f) {
                v->lfo_phase -= 1.0f;
                if (v->lfo_wave == 5)
                    v->lfo_hold = (int16_t)(random_word() & 65535u) * (1.0f / 32768.0f);
            }
            float value = lfo_shape(v->lfo_wave, v->lfo_phase, v->lfo_hold);
            s->inc = pitch_inc * (1.0f + 0.027f * v->lfo_gain * value);
        }
        s->operator_release = v->releasing;
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
    // A delayed start with no steal tail contributes zero until its onset.
    // Advance its delay in one step while preserving the exact onset frame.
    if (frames && voice->delay && voice->steal_tail == 0.0f) {
        voice->last_sample = 0.0f;
        if (voice->releasing) {
            voice->age = 0;
            return;
        }
        i = frames < voice->delay ? frames : voice->delay;
        voice->delay -= i;
        if (i == frames) return;
    }
    // Unmodulated tonal attacks and sustains have no per-sample work
    // outside the oscillator and envelope. Stop before the gate-off sample.
    if (i < frames && track < DRUMS && voice->age && !voice->releasing &&
        !voice->delay && !voice->lfo_wave && !voice->glide_inc && voice->steal_tail == 0.0f) {
        const unsigned first = i;
        unsigned end = frames;
        if (!voice->manual && voice->remaining && frames - i >= voice->remaining)
            end = i + voice->remaining - 1u;
        float env = voice->env, sample = voice->last_sample;
        const float velocity = voice->velocity;
        if (!voice->struck) {
            const float attack = voice->attack;
            for (; i < end;) {
                env += (1.0f - env) * attack;
                sample = slap_synth_sample(synth) * env * velocity * 0.18f;
                output[i++] += sample;
                if (env > 0.94f) {
                    voice->struck = 1;
                    break;
                }
            }
        }
        const float target = voice->sustain, rate = voice->decay;
        for (; i < end; ++i) {
            env += (target - env) * rate;
            sample = slap_synth_sample(synth) * env * velocity * 0.18f;
            output[i] += sample;
        }
        voice->env = env;
        voice->last_sample = sample;
        if (!voice->manual && voice->remaining) voice->remaining -= i - first;
    }
    // A released tonal voice has a fixed envelope target until a new event.
    // Leave modulated voices, steal tails and pending starts on the general path.
    if (i < frames && track < DRUMS && voice->age && voice->releasing &&
        !voice->delay && !voice->lfo_wave && !voice->glide_inc && voice->steal_tail == 0.0f) {
        float env = voice->env;
        const float rate = voice->release, velocity = voice->velocity;
        float sample = voice->last_sample;
        const unsigned first = i;
        synth->operator_release = 1;
        for (; i < frames; ++i) {
            env += (0.0f - env) * rate;
            if (env < 0.0001f) {
                voice->age = 0;
                sample = 0.0f;
                break;
            }
            sample = slap_synth_sample(synth) * env * velocity * 0.18f;
            output[i] += sample;
        }
        voice->env = env;
        voice->last_sample = sample;
        if (!voice->manual && voice->remaining) {
            unsigned elapsed = i - first + (i < frames);
            voice->remaining = elapsed < voice->remaining ? voice->remaining - elapsed : 0u;
        }
        return;
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

static void profile_voice_span(Voice *voice, SlapSynth *synth, unsigned muted,
                               float bus[][VOICE_BATCH], unsigned frames, volatile uint32_t *peaks)
{
    unsigned engine = voice->track == DRUMS ? TRACE_DRUM_ENGINE : synth->engine;
    uint32_t started = plat_ticks24();
    dsp_render_voice_into_bus(voice, synth, muted, bus, frames);
    uint32_t elapsed = plat_ticks24() - started;
    if (elapsed > peaks[engine]) peaks[engine] = elapsed;
}

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
            if (remote & (1u << slot)) continue;
            if (engine_profile)
                profile_voice_span(&voices[slot], &synths[slot], muted, voice_bus, frames, local_engine_peaks);
            else
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
    unsigned count, muted, track_mask, profile;
    uint32_t engine_peaks[TRACE_ENGINES];
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
    for (unsigned i = 0; i < voice_worker.count; ++i) {
        if (voice_worker.profile)
            profile_voice_span(&voice_worker.voice[i], &worker_synth[i], voice_worker.muted,
                               voice_worker.bus, frames, voice_worker.engine_peaks);
        else
            dsp_render_voice_into_bus(&voice_worker.voice[i], &worker_synth[i], voice_worker.muted,
                                      voice_worker.bus, frames);
    }
    voice_worker.start = started;
    voice_worker.ticks = plat_ticks24() - started;
}

#ifdef PLAT_HOST
/* Compare alternatives with the production mailbox, snapshots and recovery.
 * The fixed slot experiment never moves a slot between cores to fill a gap.
 * The track experiment moves at most one complete eligible track per span. */
static unsigned experimental_voice_split(const unsigned *slots, unsigned active, unsigned budget)
{
    unsigned mask = 0, used = 0;
    if (host_voice_split == HOST_SLOT_SPLIT) {
        for (unsigned i = 0; i < active; ++i) {
            unsigned slot = slots[i], cost = voices[slot].dispatch_cost;
            if ((slot & 1u) && cost <= MAX_VOICE_COST) {
                mask |= 1u << slot;
                used += cost;
            }
        }
        // Keep the same conservative worker ceiling. A skewed split stays local.
        return used <= budget ? mask : 0u;
    }

    unsigned masks[TRACKS] = {0}, costs[TRACKS] = {0}, counts[TRACKS] = {0};
    unsigned blocked = 0;
    for (unsigned i = 0; i < active; ++i) {
        unsigned slot = slots[i], cost = voices[slot].dispatch_cost;
        unsigned track = voices[slot].track;
        if (cost > MAX_VOICE_COST) blocked |= 1u << track;
        masks[track] |= 1u << slot;
        costs[track] += cost;
        ++counts[track];
    }
    for (unsigned track = 0; track < TRACKS; ++track) {
        if (!(blocked & (1u << track)) && counts[track] <= REMOTE_VOICES &&
            costs[track] <= budget && costs[track] > used) {
            mask = masks[track];
            used = costs[track];
        }
    }
    return mask;
}
#endif

static unsigned render_voice_batch(unsigned frames, unsigned muted,
                                   uint32_t callback_start, uint32_t budget)
{
    uint32_t dispatch_started = spike_enabled ? plat_ticks24() : 0u;
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
#ifdef PLAT_HOST
        if (host_voice_split != HOST_COST_SPLIT)
            remote = experimental_voice_split(active_slots, active, remaining);
        else
#endif
        for (unsigned cost = remaining < MAX_VOICE_COST ? remaining : MAX_VOICE_COST;
             cost && remote_count < REMOTE_VOICES; --cost) {
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
        voice_worker.profile = (unsigned)engine_profile;
        if (voice_worker.profile)
            __builtin_memset(voice_worker.engine_peaks, 0, sizeof voice_worker.engine_peaks);
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

    uint32_t local_start = launched || spike_enabled ? plat_ticks24() : 0;
    if (spike_enabled) spike_current[SPIKE_DISPATCH] += local_start - dispatch_started;
    render_local_voice_bus(active_slots, active, remote, ordered_rng, muted, frames);
    if (spike_enabled) spike_current[SPIKE_LOCAL] += plat_ticks24() - local_start;
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
        if (voice_worker.profile) {
            for (unsigned engine = 0; engine < TRACE_ENGINES; ++engine)
                if (voice_worker.engine_peaks[engine] > remote_engine_peaks[engine])
                    remote_engine_peaks[engine] = voice_worker.engine_peaks[engine];
        }
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
    // Each track costs at most 19 units, so five bits encode its cost.
    // Runtime effect state does not influence the existing split policy.
    static unsigned previous_key, previous_mask;
    _Static_assert(TRACKS * 5u <= 32u, "FX cost key fits in one word");
    unsigned cost[TRACKS], total = 0, key = 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        cost[t] = slap_fx_work(t);
        key |= cost[t] << (t * 5u);
        total += cost[t];
    }
    if (key == previous_key) return previous_mask;
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
    previous_key = key;
    previous_mask = best;
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
    app_audio_half_profile_enabled = slap_ui.page == GLOBAL && global_details == 8 && !half_trace_frozen;
    engine_profile = slap_ui.page == GLOBAL && global_details == 9;
    spike_enabled = app_audio_half_profile_enabled || engine_profile ||
        (slap_ui.page == GLOBAL && global_details == 7);
    if (spike_enabled) {
        __builtin_memset(spike_current, 0, sizeof spike_current);
    }
    uint32_t serial[PROFILE_COUNT] = {0};
    if (dsp_reset_request) {
        dsp_peak_ticks = dsp_avg_ticks = dsp_overruns = dsp_callbacks = 0;
        peak_voice_batch = peak_fx_batch = peak_worker_work = peak_worker_wait = 0;
        serial_peak = spike_callback = 0;
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        host_serial_fx_batch = 0;
#endif
        half_trace_written = longest_half_ticks = half_trace_faults = 0;
        for (unsigned engine = 0; engine < TRACE_ENGINES; ++engine)
            local_engine_peaks[engine] = remote_engine_peaks[engine] = 0;
        for (unsigned part = 0; part < SPIKE_COUNT; ++part) spike_values[part] = 0;
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
    float left[TRACKS], right[TRACKS], level_at[TRACKS], wet[TRACKS], gain_goal[TRACKS];
    unsigned gain_needed[TRACKS];
    float duck_depth[DRUMS];
    unsigned duck_enabled = 0;
    for (unsigned t = 0; t < DRUMS; ++t) {
        duck_depth[t] = mixer_duck[t] * 0.008f;
        duck_enabled |= duck_depth[t] > 0.0f || duck_amount[t] > 0.0f;
    }
    const float duck_release = tempo * (1.0f / (15.0f * RATE));
    if (!duck_enabled) drum_duck_envelope = 0.0f;
    const unsigned automatic = !auto_level_off;
    const unsigned sharing = auto_mix;
    float activity[TRACKS], share_goal[TRACKS], activity_sum = 0.0f;
    for (unsigned t = 0; t < TRACKS; ++t) {
        mix[t] = tracks[t].mix;
        unsigned pan_byte = (mix[t] >> 8) & 255u;
        float pan = pan_byte <= 128u ? pan_byte / 256.0f : 0.5f + (pan_byte - 128u) / 254.0f;
        float level = ((mix[t] & (1u << 24)) || (solo && !(solo & (1u << t)))) ? 0.0f :
                      ((mix[t] >> 16) & 255u) / 255.0f;
        level_at[t] = level;
        wet[t] = mixer_proc[t] * (1.0f / 255.0f);
        float envelope = mixer_envelope[t];
        float confidence = fm_clampf((envelope - 0.002f) * 125.0f, 0.0f, 1.0f);
        float correction = fm_clampf(0.035f / fm_maxf(envelope, 0.006f), 0.62f, 1.65f);
        gain_goal[t] = automatic ? 1.0f + confidence * (correction - 1.0f) : 1.0f;
        activity[t] = sharing ? fm_clampf(envelope * level * 28.0f, 0.0f, 1.0f) : 0.0f;
        activity_sum += activity[t];
        gain_needed[t] = automatic || mixer_gain[t] != 1.0f;
        share_goal[t] = 1.0f;
        left[t] = level * (1.0f - pan);
        right[t] = level * pan;
    }
    if (sharing && activity_sum > 1.0f) {
        float crowding = fm_minf((activity_sum - 1.0f) * 0.13f, 0.30f);
        for (unsigned t = 0; t < TRACKS; ++t)
            share_goal[t] -= crowding * (1.0f - activity[t] / activity_sum);
    }
    for (unsigned t = 0; t < TRACKS; ++t) {
        float share = mix_share_gain[t];
        share += (share_goal[t] - share) * fm_minf(0.15f, frames * (1.0f / (RATE * 0.15f)));
        mix_share_gain[t] = fm_fabsf(share - 1.0f) < 0.00005f ? 1.0f : share;
        left[t] *= share;
        right[t] *= share;
    }
    uint32_t epoch = sequence_epoch;
    if (running_epoch != epoch) {
        running_epoch = epoch;
        for (unsigned t = 0; t < TRACKS; ++t) monitored_next_step[t] = 0;
        positions = until_step = step_number = arp_wait = arp_pulse = 0;
        for (unsigned i = 0; i < VOICES; ++i)
            if (!voices[i].manual) voices[i].releasing = 1;
    }
    static unsigned was_playing;
    if (was_playing && !playing)
        for (unsigned i = 0; i < VOICES; ++i)
            if (!voices[i].manual) voices[i].releasing = 1;
    was_playing = playing;
    if (!recording && rec_running) end_recording(record_clock);
    unsigned follow = rec_follow_track;
    if (recording && rec_loop && follow && follow - 1u != rec_track) {
        unsigned running = rec_running;
        if (running) end_recording(record_clock);
        rec_track = follow - 1u;
        rec_length = pattern_length(tracks[rec_track].mix);
        if (running) {
            rec_start_mode = 0;
            begin_recording();
            mix[rec_track] = tracks[rec_track].mix;
        }
    }
    if (!recording) rec_countdown = 0;
    if (!playing) {
        apply_pattern_action();
        apply_generated_drums();
    }
    if (recording && playing && rec_loop && !rec_start_mode && !rec_running && !quick_wait_note)
        begin_recording();
    read_notes();
    audition_selected_step();
    // The UI publishes these settings; the callback consumes one snapshot.
    const unsigned arp_track = input_track;
    const uint32_t arp_config = arp_track < DRUMS ? arp_setup[arp_track] : 0u;
    const int arp_active = old_keys && (arp_config & 255u) && !(split_keys & (1u << arp_track));
    if (!arp_active || arp_last_track != arp_track) {
        arp_wait = arp_pulse = 0;
        arp_last_track = arp_track;
    }
    if (rec_running) mix[rec_track] = tracks[rec_track].mix;
    const unsigned home_count_in = recording && rec_loop && rec_start_mode && !rec_running;
    if (!playing && !home_count_in) click_remaining = 0;
    slap_fx_begin_live(tempo, live_fx_word);
    unsigned live_mix_active = slap_fx_mix_active();
    const unsigned muted = muted_drums();
    unsigned measure_fx = slap_ui.page == FX && slap_ui.fx_page == 4;
    const unsigned analyze = slap_ui.page == VISUAL;
    const unsigned inspect_fx = slap_ui.page == FX || slap_ui.page == PATCH;
    const unsigned active_effects = slap_fx_active_mask();
    const unsigned effect_mask = inspect_fx ? (1u << TRACKS) - 1u : active_effects;
    float meter_sum[2] = {0}, meter_peak[2] = {0};
    float target = master_q12 * (1.0f / 4096.0f) * (1.0f - 0.2f * output_trim);
    float click_gain = 1.0f - 0.25f * click_trim;
    const unsigned speaker = speaker_mode;
    static float speaker_dc_l, speaker_dc_r;
    unsigned batch_start = 0, batch_end = 0;
    uint32_t callback_budget = (24000000u / RATE) * frames;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    unsigned fx_batch_end = 0;
    unsigned fx_mask = !measure_fx && (active_effects & (active_effects - 1u)) && plat_cpu2_on()
        ? select_fx_worker_tracks() : 0;
    uint32_t voice_batch_ticks = 0, fx_batch_ticks = 0;
    uint32_t remote_start = worker_ticks + fx_worker_ticks, wait_start = worker_wait_ticks;
    uint32_t overlap_start = worker_overlap_ticks;
    uint32_t jobs_start = worker_jobs + fx_worker_jobs;
#endif
    if (profile) serial[PROFILE_INPUT] = plat_ticks24() - audio_started;
    for (uint32_t i = 0; i < frames; ++i) {
        uint32_t section = profile ? plat_ticks24() : 0;
        if ((playing || home_count_in) && until_step == 0) {
            if (recording && !rec_running && rec_start_mode) {
                if (step_number < 4) {
                    click_remaining = CLICK_SAMPLES;
                    click_accent = step_number == 0;
                    click_phase = 0;
                    rec_countdown = 4u - step_number++;
                    until_step = RATE * 60u / tempo;
                } else {
                    step_number = rec_countdown = 0;
                    playing = 1;
                    begin_recording();
                    mix[rec_track] = tracks[rec_track].mix;
                }
            }
            if (rec_running && !rec_loop && step_number >= rec_length) {
                end_recording(record_clock + i);
                recording = 0;
            }
            if (pattern_action && !recording && !rec_running) {
                apply_pattern_action();
                for (unsigned t = 0; t < TRACKS; ++t)
                    mix[t] = (mix[t] & ~LENGTH_MASK) | (tracks[t].mix & LENGTH_MASK);
            }
            if (!recording || rec_running) {
                if (!(step_number & 15u) && gen_queued) {
                    apply_generated_drums();
                    mix[DRUMS] = tracks[DRUMS].mix;
                }
                next_step(mix);
            }
        }
        if ((playing || home_count_in) && until_step) --until_step;
        if (arp_active) arp_render_clock(arp_track, arp_config);
        if (profile) serial[PROFILE_EVENTS] += plat_ticks24() - section;
        if (i == batch_end) {
            unsigned amount = frames - i;
            if (amount > VOICE_BATCH) amount = VOICE_BATCH;
            if (playing && amount > until_step + 1u) amount = until_step + 1u;
            // The current sample's arpeggio tick has already run. End this
            // batch before the next tick can allocate or steal a voice.
            if (arp_active && amount > arp_wait + 1u)
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
            uint32_t voice_ticks = voice_end - stage_start;
            voice_batch_ticks += voice_ticks;
            if (spike_enabled && voice_ticks > spike_current[SPIKE_SPAN])
                spike_current[SPIKE_SPAN] = voice_ticks;
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
        float duck = 0.0f;
        if (duck_enabled) {
            float trigger = fm_fabsf(voice_bus[DRUMS][i - batch_start]) * level_at[DRUMS];
            trigger = fm_clampf((trigger - 0.025f) * 8.0f, 0.0f, 1.0f);
            drum_duck_envelope += (trigger - drum_duck_envelope) *
                (trigger > drum_duck_envelope ? 0.018f : duck_release);
            duck = drum_duck_envelope;
        }
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
                float response = gain_goal[t] < mixer_gain[t] ? 0.00035f : 0.00009f;
                mixer_gain[t] += (gain_goal[t] - mixer_gain[t]) * response;
                if (gain_goal[t] == 1.0f && fm_fabsf(1.0f - mixer_gain[t]) < 0.00005f)
                    mixer_gain[t] = 1.0f;
                sample *= mixer_gain[t];
                amplitude = fm_fabsf(sample);
            }
            if (duck_enabled && t < DRUMS) {
                duck_amount[t] += (duck_depth[t] - duck_amount[t]) * 0.003f;
                if (duck_depth[t] == 0.0f && duck_amount[t] < 0.00005f) duck_amount[t] = 0.0f;
                float gain = 1.0f - duck * duck_amount[t];
                sample *= gain;
                amplitude *= gain;
            }
            energy_smooth[t] += (amplitude * level_at[t] - energy_smooth[t]) * 0.0015f;
            l += sample * left[t];
            r += sample * right[t];
            if (profile) serial[PROFILE_MIX] += plat_ticks24() - section;
        }
        if (profile) section = plat_ticks24();
        if (live_mix_active) slap_fx_mix_sample(&l, &r);
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
            l += click * click_gain;
            r += click * click_gain;
        }
        if (speaker) {
            speaker_dc_l += 0.010f * (l - speaker_dc_l);
            speaker_dc_r += 0.010f * (r - speaker_dc_r);
            l -= speaker_dc_l;
            r -= speaker_dc_r;
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
        if (spike_enabled) {
            half_app_ticks = elapsed;
#if FM1B_CPU2 && !defined(PLAT_WEB)
            half_wait_ticks = worker_wait_ticks - wait_start;
            half_overlap_ticks = worker_overlap_ticks - overlap_start;
            half_voice_ticks = voice_batch_ticks;
            half_fx_ticks = fx_batch_ticks;
            half_remote_ticks = worker_ticks + fx_worker_ticks - remote_start;
            half_jobs = worker_jobs + fx_worker_jobs - jobs_start;
#endif
        }
        if (spike_enabled && elapsed > spike_callback) {
#if FM1B_CPU2 && !defined(PLAT_WEB)
            spike_current[SPIKE_FX] = fx_batch_ticks;
#endif
            for (unsigned part = 0; part < SPIKE_COUNT; ++part)
                spike_values[part] = spike_current[part];
            spike_callback = elapsed;
        }
        uint32_t budget = (24000000u / RATE) * frames + (24000000u % RATE) * frames / RATE;
        dsp_budget_ticks = budget;
        unsigned bin = elapsed / DSP_BIN_TICKS;
        if (bin >= DSP_BINS) bin = DSP_BINS - 1u;
        if (!profile && !spike_enabled) {
            ++dsp_histogram[bin];
            ++dsp_callbacks;
            if (elapsed > budget) ++dsp_overruns;
        }
        if (profile && elapsed > serial_peak) {
            serial_peak = elapsed;
#if defined(PLAT_HOST) && FM1B_CPU2 && !defined(PLAT_WEB)
            host_serial_fx_batch = fx_batch_ticks;
#endif
            for (unsigned part = 0; part < PROFILE_COUNT; ++part) serial_at_peak[part] = serial[part];
        }
        if (!profile && !spike_enabled && elapsed > dsp_peak_ticks) {
#if FM1B_CPU2 && !defined(PLAT_WEB)
            peak_voice_batch = voice_batch_ticks;
            peak_fx_batch = fx_batch_ticks;
            peak_worker_work = worker_ticks + fx_worker_ticks - remote_start;
            peak_worker_wait = worker_wait_ticks - wait_start;
            peak_worker_overlap = worker_overlap_ticks - overlap_start;
#endif
            dsp_peak_ticks = elapsed;
        }
        if (!profile && !spike_enabled) dsp_avg_ticks = dsp_avg_ticks - dsp_avg_ticks / 8u + elapsed / 8u;
        unsigned active = 0;
        for (unsigned v = 0; v < VOICES; ++v) active += voices[v].age != 0;
        active_voices = active;
#if defined(PLAT_HOST)
        if (active > host_peak_voices) host_peak_voices = active;
#endif
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
    uint8_t drums[OLD_DRUM_LANES][LEGACY_STEPS][3];
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

static unsigned song_position(unsigned row, unsigned *lane)
{
    if (row < 8u) {
        *lane = row % 2u;
        return row / 2u;
    }
    if (row < SONG5_LANES || row >= SONG7_LANES) {
        *lane = row < SONG5_LANES ? row - 8u : OLD_DRUM_LANES + 1u + row - SONG7_LANES;
        return DRUMS;
    }
    unsigned extra = row - SONG5_LANES;
    *lane = 2u + extra % (MELODIC_LANES - 2u);
    return extra / (MELODIC_LANES - 2u);
}

static uint32_t song_event_get(unsigned row, unsigned step)
{
    unsigned lane;
    unsigned track = song_position(row, &lane);
    return event_get(track, step, lane);
}

static void song_event_set(unsigned row, unsigned step, uint32_t word)
{
    unsigned lane;
    unsigned track = song_position(row, &lane);
    event_set(track, step, lane, word);
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

static unsigned song_header_size(unsigned version)
{
    return SONG5_HEADER + (version >= 10u ? sizeof(track_timbre) : 0u) +
        (version >= 11u ? sizeof(track_fm_envelopes) : 0u);
}

static int prepare_sparse_song(unsigned bytes, unsigned version)
{
    unsigned stride = version >= 8u ? 9u : version == 7u ? 8u : SONG5_EVENT;
    unsigned header = song_header_size(version);
    if (bytes < header || (bytes - header) % stride) return 0;
    if (version >= 11u) {
        unsigned at = SONG5_HEADER + sizeof(track_timbre);
        for (unsigned track = 0; track < DRUMS; ++track)
            for (unsigned i = 0; i < 7u; ++i, at += 4u) {
                unsigned value = song_get32(at);
                if (i == 6u ? value > 16u : value && (value < 65536u || value > 131071u)) return 0;
            }
    }
    unsigned previous = 0;
    for (unsigned at = header; at < bytes; at += stride) {
        unsigned packed = song_bytes[at] | (unsigned)song_bytes[at + 1] << 8;
        unsigned bits = version >= 8u ? 16u : version >= 6u ? 14u : 13u;
        unsigned index = packed & ((1u << bits) - 1u);
        uint32_t word = song_get32(at + 2);
        unsigned row = index / STEPS;
        if ((packed >> bits) > 4u || row >= (version >= 8u ? SONG_LANES : version >= 6u ? SONG7_LANES : SONG5_LANES) ||
            (at != header && index <= previous) || !word || !step_valid(word))
            return 0;
        if (row > 8u && row < SONG5_LANES && (word & NOTES) != lane_pitch[row - 9u]) return 0;
        if (row >= SONG7_LANES && (word & NOTES) != lane_pitch[OLD_DRUM_LANES + row - SONG7_LANES]) return 0;
        if (version >= 8u && song_bytes[at + 8] > (version == 8u ? 3u : 11u)) return 0;
        if (version >= 7u && ((row >= 8u && row < SONG5_LANES) || row >= SONG7_LANES) &&
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
    unsigned stride = version >= 8u ? 9u : version == 7u ? 8u : SONG5_EVENT;
    song_put32(0, SONG_MAGIC);
    song_put32(4, version);
    song_put32(8, song_io.bpm);
    song_put32(12, song_io.swing);
    unsigned at = song5_fields(1);
    if (version >= 10u)
        for (unsigned track = 0; track < DRUMS; ++track, at += 4u) song_put32(at, track_timbre[track]);
    if (version >= 11u)
        for (unsigned track = 0; track < DRUMS; ++track)
            for (unsigned i = 0; i < 7u; ++i, at += 4u) song_put32(at, track_fm_envelopes[track][i]);
    for (unsigned row = 0; row < (version >= 8u ? SONG_LANES : version >= 6u ? SONG7_LANES : SONG5_LANES); ++row) {
        for (unsigned step = 0; step < STEPS; ++step) {
            uint32_t word = song_event_get(row, step);
            if (!word) continue;
            if (at > SONG_MAX - stride) return 0;
            unsigned index = row * STEPS + step;
            if (version >= 6u) {
                unsigned offset = note_timing(row, step, word);
                if (version >= 8u)
                    song_bytes[at + 8] = (uint8_t)(offset / 4u + 4u * (timing_group_get(row, step) / TIMING_GROUPS));
                else index |= (offset / 4u) << 14;
                word = (word & ~timing_bits(3u)) | timing_bits(offset);
            } else index |= (unsigned)timing_group_get(row, step) << 13;
            song_bytes[at] = (uint8_t)index;
            song_bytes[at + 1] = (uint8_t)(index >> 8);
            song_put32(at + 2, word);
            if (version >= 7u) {
                unsigned duration = 0;
                if (row < 8u || (row >= SONG5_LANES && row < SONG7_LANES)) {
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
    if (version >= 5u && version <= 11u) {
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
          (song_io.version == 4 && bytes == (int)sizeof(song_io)) || (song_io.version >= 5u && song_io.version <= 11u)) ||
        song_io.bpm < 40 || song_io.bpm > 240 || song_io.swing > 70) return 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        uint32_t mix = song_io.track[t].mix;
        if (!pattern_length(mix) || pattern_length(mix) > (song_io.version >= 5u ? STEPS : LEGACY_STEPS) || (mix >> (t == DRUMS ? 27 : 29)) ||
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
    if (song_io.version >= 3 && song_io.version < 8u && (song_io.filters & ~0x3f3fu)) return 0;
    if (song_io.version == 3 || song_io.version == 4) {
        for (unsigned t = 0; t < DRUMS; ++t)
            for (unsigned i = 0; i < LEGACY_STEPS; ++i)
                if (!step_valid(song_io.extra[t][i])) return 0;
        for (unsigned lane = 0; lane < OLD_DRUM_LANES; ++lane) {
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
    __builtin_memset(drum_step_locks, 0, sizeof drum_step_locks);
    for (unsigned t = 0; t < DRUMS; ++t) {
        track_timbre[t] = song_io.version >= 10u ? song_get32(SONG5_HEADER + t * 4u) : 0u;
        for (unsigned i = 0; i < 7u; ++i)
            track_fm_envelopes[t][i] = song_io.version >= 11u ?
                song_get32(SONG5_HEADER + sizeof(track_timbre) + (t * 7u + i) * 4u) : 0u;
    }
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
    for (unsigned lane = 0; lane < OLD_DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < LEGACY_STEPS; ++i) {
            const uint8_t *d = song_io.drums[lane][i];
            uint32_t data = song_io.version == 3 || song_io.version == 4 ?
                (uint32_t)d[0] << 8 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 24 : 0;
            drum_cell_set(lane, i, data ? data | ACTIVE | lane_pitch[lane] : 0);
        }
    }
    drum_filters = song_io.version >= 8 ? song_io.filters :
        song_io.version >= 3 ? (song_io.filters & 63u) | ((song_io.filters & 0x3f00u) << 8) : 0;
    for (unsigned t = 0; t < DRUMS; ++t)
        arp_setup[t] = song_io.version >= 4 ? song_io.arp[t] : ARP_DEFAULT;
    for (unsigned row = 0; row < SONG_LANES; ++row) {
        for (unsigned pair = 0; pair < STEPS / 2u; ++pair) timing_group[row][pair] = 0;
        for (unsigned step = row >= SONG5_LANES ? 0u : LEGACY_STEPS; step < STEPS; ++step)
            song_event_set(row, step, 0);
    }
    for (unsigned track = 0; track < DRUMS; ++track)
        for (unsigned lane = 0; lane < MELODIC_LANES; ++lane)
            for (unsigned step = 0; step < STEPS; ++step)
                melodic_duration[track][lane][step] = 0;
    if (song_io.version >= 5u) {
        unsigned stride = song_io.version >= 8u ? 9u : song_io.version == 7u ? 8u : SONG5_EVENT;
        for (unsigned at = song_header_size(song_io.version); at < song5_bytes; at += stride) {
            unsigned packed = song_bytes[at] | (unsigned)song_bytes[at + 1] << 8;
            unsigned bits = song_io.version >= 8u ? 16u : song_io.version >= 6u ? 14u : 13u;
            unsigned index = packed & ((1u << bits) - 1u);
            unsigned row = index / STEPS, step = index % STEPS;
            unsigned offset_group = song_io.version >= 8u ? song_bytes[at + 8] & 3u : packed >> bits;
            unsigned mode = song_io.version >= 9u ? song_bytes[at + 8] >> 2 : 0u;
            timing_group_set(row, step, (uint8_t)(offset_group + (song_io.version >= 6u) + mode * TIMING_GROUPS));
            song_event_set(row, step, song_get32(at + 2));
            if (song_io.version >= 7u && (row < 8u || (row >= SONG5_LANES && row < SONG7_LANES))) {
                unsigned track = row < 8u ? row / 2u : (row - SONG5_LANES) / 2u;
                melodic_duration[track][melodic_lane_from_row(track, row)][step] =
                    (uint16_t)(song_bytes[at + 6] | (unsigned)song_bytes[at + 7] << 8);
            }
        }
    }
    pattern_action = 0;
    for (unsigned t = 0; t < TRACKS; ++t) take_undo[t].ready = 0;
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
    for (unsigned lane = 0; lane < OLD_DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < LEGACY_STEPS; ++i) {
            uint32_t word = drum_cell_get(lane, i);
            for (unsigned byte = 0; byte < 3; ++byte)
                song_io.drums[lane][i][byte] = (uint8_t)(word >> ((byte + 1u) * 8u));
        }
    }
    song_io.filters = drum_filters;
    for (unsigned t = 0; t < DRUMS; ++t) song_io.arp[t] = arp_setup[t];
    unsigned version = 4u;
    for (unsigned t = 0; t < TRACKS; ++t)
        if (t < DRUMS && track_timbre[t]) version = 10u;
        else if (version < 9u && (tracks[t].mix & ADVANCED_SYNTH)) version = 9u;
        else if (version < 5u && pattern_length(tracks[t].mix) > LEGACY_STEPS) version = 5u;
    for (unsigned row = 0; row < SONG_LANES && version < 6u; ++row)
        for (unsigned step = 0; step < STEPS; ++step) {
            if (!song_event_get(row, step)) continue;
            if (row >= SONG5_LANES) {
                version = 6u;
                break;
            }
            if (step >= LEGACY_STEPS || timing_group_get(row, step)) version = 5u;
        }
    for (unsigned track = 0; track < DRUMS && version < 7u; ++track)
        for (unsigned lane = 0; lane < MELODIC_LANES && version < 7u; ++lane)
            for (unsigned step = 0; step < STEPS; ++step)
                if (melodic_duration[track][lane][step] && (event_get(track, step, lane) & ACTIVE)) {
                    version = 7u;
                    break;
                }
    for (unsigned row = SONG7_LANES; row < SONG_LANES && version < 8u; ++row)
        for (unsigned step = 0; step < STEPS; ++step)
            if (song_event_get(row, step)) version = 8u;
    if (version < 8u && ((drum_filters & 65535u) > 63u || (drum_filters >> 16) > 63u)) version = 8u;
    for (unsigned row = 0; row < SONG_LANES && version < 9u; ++row)
        for (unsigned step = 0; step < STEPS; ++step)
            if (timing_group_get(row, step) >= TIMING_GROUPS && (song_event_get(row, step) & ACTIVE)) {
                version = 9u;
                break;
            }
    for (unsigned t = 0; t < DRUMS; ++t)
        for (unsigned i = 0; i < 7u; ++i)
            if (track_fm_envelopes[t][i]) version = 11u;
    if (version < 8u) song_io.filters = (drum_filters & 63u) | ((drum_filters >> 8) & 0x3f00u);
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
    plat_lights((buttons & ~(1u << B_PLAY)) |
                (playing && !(positions & 3u) ? LED_PLAY_GREEN : 0u),
                now / 220u & 1u ? target : 0, 0, dim & ~target, 1);
    keys_audio = 0;
}

static unsigned lane_count(unsigned track)
{
    return track == DRUMS ? DRUM_LANES + 1u : MELODIC_LANES;
}

static void generate_drums(void)
{
    if (!gen_enabled || gen_queued || recording || rec_running || pattern_action) return;
    unsigned occupied = 0;
    unsigned previous_length = pattern_length(tracks[DRUMS].mix);
    unsigned length = gen_bars ? gen_bars * 16u : previous_length;
    unsigned queued = playing;
    if (queued && length > LIVE_LOOP_MAX) return;
    unsigned revision = pattern_revision[DRUMS];
    unsigned kit = tracks[DRUMS].patch;
    for (unsigned step = 0; step < length; ++step)
        for (unsigned lane = 0; lane < lane_count(DRUMS); ++lane)
            occupied |= event_get(DRUMS, step, lane) & ACTIVE;
    if (occupied && !gen_confirm) {
        gen_confirm = 1;
        return;
    }
    gen_confirm = 0;
    if (!queued) snapshot_take(DRUMS, length);
    const struct DrumStyle *style = &drum_style[gen_style];
    for (unsigned step = 0; step < length; ++step) {
        unsigned beat = step & 15u;
        unsigned bit = 1u << beat;
        unsigned fill = (step / 16u == length / 16u - 1u) && beat >= 13u;
        if (!queued) {
            if (!drum_step_locked(0, step)) tracks[DRUMS].steps[step] = 0;
            for (unsigned lane = 0; lane < lane_count(DRUMS); ++lane)
                if (!drum_step_locked(lane, step)) timing_group_set(pattern_row(DRUMS, lane), step, 0);
        }
        unsigned open_hat = 0;
        for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
            if (lane >= OLD_DRUM_LANES) {
                unsigned accent = lane == 6 ? 14u : lane == 7 ? 11u : lane == 8 ? 10u :
                    lane == 9 ? 6u : lane == 10 ? 13u : lane == 11 ? 14u :
                    lane == 12 ? 15u : lane == 13 ? 0u : lane == 14 ? 7u : 15u;
                unsigned is_fill = lane >= 10 && lane <= 12;
                unsigned family = kit >= DRUM_LEGACY_KITS ? (kit - DRUM_LEGACY_KITS) % 5u : 0u;
                if (lane == 8 || lane == 9 || lane >= 14) accent = (accent + 2u * (family & 1u)) & 15u;
                int hit = kit >= DRUM_LEGACY_KITS && beat == accent &&
                    (is_fill ? fill : (lane == 13 ? step % 64u == 0 : (gen_style + family + lane) % 3u != 0)) &&
                    next_random(&gen_seed) % 100u < (is_fill ? gen_var : gen_hit / 2u);
                unsigned velocity = (lane <= 7 ? 84u : 59u) + (next_random(&gen_seed) & 15u);
                uint32_t word = hit ? cell_word(lane_pitch[lane], velocity, 104, 100) : 0u;
                if (queued) queued_drums[lane][step] = (uint8_t)((word >> 8) & 127u);
                else if (!drum_step_locked(lane + 1u, step)) drum_cell_set(lane, step, word);
                open_hat |= lane == 8 && hit;
                continue;
            }
            unsigned anchor = (lane < 2) && (style->lanes[lane] & bit);
            unsigned chance = lane == 2 ? 30 + gen_hit * 70 / 100 : gen_hit;
            unsigned hit = anchor || ((style->lanes[lane] & bit) && next_random(&gen_seed) % 100u < chance);
            unsigned ghost = !hit && lane == 1 && ((style->ghost_snare & bit) || fill)
                             && next_random(&gen_seed) % 100u < gen_var / 2u;
            unsigned velocity = ghost ? 48u : lane == 0 ? 112u : lane == 1 ? 99u :
                64u + (!(beat & 3u) ? 20u : 0u);
            velocity += next_random(&gen_seed) % (1u + gen_var / 7u);
            unsigned offset = (beat & 1u) ? (3u * gen_groove + 50u) / 100u : 0u;
            uint32_t word = hit || ghost ?
                cell_word(lane_pitch[lane], velocity, lane == 5 ? 154u : 105u, 100u) | timing_bits(offset) : 0u;
            if (queued) queued_drums[lane][step] = (uint8_t)((word >> 8) & 127u);
            else if (!drum_step_locked(lane + 1u, step)) drum_cell_set(lane, step, word);
        }
        if (open_hat) {
            if (queued) queued_drums[2][step] = 0;
            else if (!drum_step_locked(3, step)) drum_cell_set(2, step, 0);
        }
    }
    if (queued) {
        queued_drum_length = length;
        queued_drum_previous_length = previous_length;
        queued_drum_revision = revision;
        queued_drum_kit = kit;
        queued_drum_swing = (3u * gen_groove + 50u) / 100u;
        __asm__ volatile ("" ::: "memory");
        gen_queued = 1;
    } else {
        track_setting(DRUMS, 0, length);
        ++pattern_revision[DRUMS];
        finish_snapshot(DRUMS);
        ++gen_count;
        gen_applied_at = plat_ms();
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
    int previous_page = slap_ui.page;
    if ((slap_ui.page != RECORD && slap_ui.page != HOME) || recording ||
        (clear_mode && plat_ms() - clear_armed_at > 2200u)) clear_mode = 0;
    if (slap_ui.page == SEQ && (buttons & (1u << B_SEL))) {
        if (slap_ui.track == DRUMS && (pressed & (1u << B_GLO))) {
            unsigned at = (unsigned)slap_ui.step;
            if (at < LIVE_LOOP_MAX && !recording && !rec_running) {
                drum_step_locks[slap_ui.lane][at / 32u] ^= 1u << (at & 31u);
                ++pattern_revision[DRUMS];
            }
            pressed &= ~(1u << B_GLO);
        }
        unsigned edit = pressed & ((1u << B_EDIT) | (1u << B_LFO) |
            (1u << B_ENV) | (1u << B_OCTDN) | (1u << B_OCTUP));
        if (edit) {
            unsigned track = (unsigned)slap_ui.track;
            unsigned step = (unsigned)slap_ui.step;
            unsigned lane = (unsigned)slap_ui.lane;
            unsigned row = pattern_row(track, lane);
            uint32_t current_word = event_get(track, step, lane);
            uint16_t *duration = track < DRUMS ? &melodic_duration[track][lane][step] : 0;
            pressed &= ~edit;
            if (!recording && !rec_running && !pattern_action) {
                if (edit & ((1u << B_EDIT) | (1u << B_LFO))) {
                    step_clipboard.word = current_word;
                    step_clipboard.duration = duration ? *duration : 0;
                    step_clipboard.timing = timing_group_get(row, step);
                    step_clipboard.track = (uint8_t)track;
                    step_clipboard.lane = (uint8_t)lane;
                    step_clipboard.valid = 1;
                }
                if (edit & ((1u << B_LFO) | (1u << B_OCTDN))) {
                    event_set(track, step, lane, 0);
                    timing_group_set(row, step, 0);
                    if (duration) *duration = 0;
                    ++pattern_revision[track];
                } else if ((edit & (1u << B_OCTUP)) && (current_word & ACTIVE)) {
                    unsigned group = timing_group_get(row, step);
                    unsigned mode = (group / TIMING_GROUPS + 1u) % 3u;
                    timing_group_set(row, step, (uint8_t)(group % TIMING_GROUPS + mode * TIMING_GROUPS));
                    ++pattern_revision[track];
                } else if ((edit & (1u << B_ENV)) && step_clipboard.valid &&
                           step_clipboard.track == track && step_clipboard.lane == lane) {
                    event_set(track, step, lane, step_clipboard.word);
                    timing_group_set(row, step, step_clipboard.timing);
                    if (duration) *duration = step_clipboard.duration;
                    ++pattern_revision[track];
                }
            }
        }
    }
    uint32_t gesture_now = plat_ms();
    if (pressed & (1u << B_EDIT)) {
        edit_pending = 1;
        edit_pressed_at = gesture_now;
        pressed &= ~(1u << B_EDIT);
    }
    if (edit_pending && (buttons & (1u << B_EDIT)) && gesture_now - edit_pressed_at >= 2000u) {
        edit_pending = 0;
        slap_ui.page = MIXER;
        slap_ui.frame_at = gesture_now - 45u;
    } else if (edit_pending && !(buttons & (1u << B_EDIT))) {
        edit_pending = 0;
        pressed |= 1u << B_EDIT;
    }
    if (pressed & (1u << B_GLO)) {
        glo_pending = 1;
        glo_pressed_at = gesture_now;
    }
    if (glo_pending && (buttons & (1u << B_GLO))) {
        pressed &= ~(1u << B_SEL);
        if (gesture_now - glo_pressed_at >= 5000u) {
            glo_pending = 0;
            slap_ui.page = GLOBAL;
            global_details = 0;
            slap_ui.frame_at = gesture_now - 45u;
        }
    } else if (glo_pending) {
        glo_pending = 0;
        if (slap_ui.page == GLOBAL)
            global_details = (global_details + 1u) % 10u;
        else {
            slap_ui.page = SETTINGS;
            settings_section = settings_item = 0;
        }
    }
    if (pressed & (1u << B_PLAY)) {
        if (playing || (recording && rec_loop && rec_start_mode)) {
            playing = recording = 0;
        } else {
            playing = 1;
        }
        if (!resume_playback || rec_countdown) ++sequence_epoch;
    }
    const unsigned rec_button = 1u << B_REC;
    if (pressed & rec_button) {
        rec_gesture = !(slap_ui.page == SEQ && slap_ui.track == DRUMS &&
                        slap_ui.lane && (buttons & (1u << B_SEL)));
        rec_pressed_at = plat_ms();
    }
    if (slap_ui.page == HOME && (buttons & rec_button)) {
        unsigned modifiers = pressed & ((1u << B_OCTDN) | (1u << B_OCTUP) | (1u << B_SEL) | (1u << B_ARP));
        if (modifiers) {
            rec_gesture = 0;
            pressed &= ~modifiers;
            if ((modifiers & (1u << B_ARP)) && recording) {
                if (rec_running) pattern_action = PATTERN_UNDO | rec_track << 8;
                else if (rec_countdown || quick_wait_note) {
                    playing = 0;
                    ++sequence_epoch;
                }
                recording = 0;
            } else if (!recording && !rec_running && !pattern_action) {
                if (modifiers & (1u << B_OCTDN)) {
                    if (take_undo[slap_ui.track].ready) pattern_action = PATTERN_UNDO | (unsigned)slap_ui.track << 8;
                    clear_mode = 0;
                } else if (modifiers & (1u << B_OCTUP)) confirm_clear(slap_ui.track, 0);
                else if (modifiers & (1u << B_SEL)) confirm_clear(slap_ui.track, 1);
            }
        }
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
        } else if (slap_ui.page == HOME && !pattern_action && !rec_running) {
            rec_track = (unsigned)slap_ui.track;
            rec_length = pattern_length(tracks[rec_track].mix);
            quick_wait_note = !playing && !rec_count_in;
            rec_start_mode = !playing && rec_count_in;
            rec_quant_setting = rec_quantize;
            rec_overwrite = rec_replace;
            rec_loop = 1;
            rec_follow_track = rec_track + 1u;
            if (!playing) ++sequence_epoch;
            recording = 1;
        }
    }
    if ((pressed & (1u << B_SEL)) && slap_ui.page == RECORD && !(buttons & (1u << B_GLO))) {
        if (recording) {
            recording = 0;
            if (rec_countdown) ++sequence_epoch;
        } else if (!playing && !pattern_action && !rec_running) {
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
            gen_bars = (unsigned)clamp((int)((pattern_length(tracks[DRUMS].mix) + 15u) / 16u), 1, 4);
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
            if (pressed & (1u << B_REC)) drum_filters ^= bit << 16;
        }
    }
    if ((pressed & (1u << B_SEL)) && slap_ui.page == SETTINGS && settings_section == PREF_DIAG) {
        slap_ui.page = GLOBAL;
        global_details = 0;
        pressed &= ~(1u << B_SEL);
    }
    if (pressed & (1u << B_SAVE)) {
        slap_ui.page = SAVE;
        slap_ui.save_armed = 0;
        slap_ui.notice = 0;
    }
    if (pressed & (1u << B_FX)) {
        uint32_t now = plat_ms();
        if (slap_ui.page == FX && live_fx_armed && now - fx_pressed_at <= 400u) {
            slap_ui.page = LIVE_FX;
            live_fx_armed = 0;
        } else {
            slap_ui.page = FX;
            live_fx_armed = 1;
            fx_pressed_at = now;
        }
    }
    if (slap_ui.page != LIVE_FX) live_fx_word = 0;
    else if (pressed & (1u << B_SEL)) live_fx_full_mix ^= 1u;
    if (slap_ui.page == RECORD) {
        if ((pressed & (1u << B_ARP)) && recording) {
            if (rec_running) pattern_action = PATTERN_UNDO | rec_track << 8;
            else if (rec_start_mode) {
                playing = 0;
                ++sequence_epoch;
            }
            recording = 0;
            pressed &= ~(1u << B_ARP);
        }
        if (!recording && !rec_running && !pattern_action) {
            if (pressed & (1u << B_OCTDN)) confirm_clear(slap_ui.track, 0);
            else if (pressed & (1u << B_OCTUP)) confirm_clear(slap_ui.track, 1);
            else if (pressed || clear_track != (unsigned)slap_ui.track) clear_mode = 0;
        }
    }
    if (pressed & (1u << B_ARP)) {
        if (slap_ui.page == RECORD) {
            if (!pattern_action && !rec_running && take_undo[undo_latest].ready)
                pattern_action = PATTERN_UNDO | undo_latest << 8;
        }
        else if (slap_ui.track < DRUMS && slap_ui.page != MIXER) slap_ui.page = ARP;
    }
    if (pressed & (1u << B_SEL) && slap_ui.page == HOME && slap_ui.track < DRUMS) slap_ui.page = HARMONY;
    if (pressed & (1u << B_LFO)) slap_ui.page = LFO;
    unsigned entering_edit = !!(pressed & (1u << B_EDIT));
    if (entering_edit) {
        slap_ui.page = PATCH;
        edit_page = 0;
    }
    if (pressed & (1u << B_ENV)) slap_ui.page = ENV;
    if (slap_ui.page == MIXER) {
        for (unsigned t = 0; t < TRACKS; ++t)
            if (notes & (1u << save_keys[t]))
                tracks[t].mix ^= 1u << 24;
    }
    if ((pressed & (1u << B_ARP)) && slap_ui.page == MIXER)
        mixer_solo ^= 1u << slap_ui.track;
    if (slap_ui.page == HARMONY && slap_ui.track < DRUMS) {
        if (pressed & (1u << B_OCTDN)) perfect_note_off ^= 1u << slap_ui.track;
        if (pressed & (1u << B_OCTUP)) {
            uint32_t harmony = tracks[slap_ui.track].harmony;
            unsigned next = (((harmony >> 16) & 255u) + 1u) % HARMONY_CHORDS;
            tracks[slap_ui.track].harmony = (harmony & ~0x00ff0000u) | (next << 16);
        }
    } else if (slap_ui.page != RECORD && slap_ui.page != GEN_DRUMS && slap_ui.page != PATCH) {
        if (pressed & (1u << B_OCTDN)) slap_ui.octave = clamp(slap_ui.octave - 1, 0, 4);
        if (pressed & (1u << B_OCTUP)) slap_ui.octave = clamp(slap_ui.octave + 1, 0, 4);
    }
    int previous_track = slap_ui.track;
    int delta = plat_enc(EN_SELECT);
    if (entering_edit) delta = 0;
    if (slap_ui.page == SAVE) {
        for (int k = EN_ALGO; k < NE; ++k) (void)plat_enc(k);
        save_input(buttons, pressed, keys, notes, delta);
        slap_ui.keys = keys;
        slap_ui.buttons = buttons;
        return;
    }
    if (slap_ui.page == SETTINGS)
        settings_item = (unsigned)clamp((int)settings_item + delta, 0, settings_count[settings_section] - 1);
    else if (slap_ui.page == SEQ && (buttons & (1u << B_SEL)))
        slap_ui.lane = clamp(slap_ui.lane + delta, 0, (int)lane_count(slap_ui.track)-1);
    else if (slap_ui.page == SEQ) {
        int step = clamp(slap_ui.step + delta, 0, (int)pattern_length(tracks[slap_ui.track].mix) - 1);
        if (step != slap_ui.step) {
            slap_ui.step = step;
            step_audition_request = ((unsigned)slap_ui.track + 1u) << 16 | (unsigned)step;
        }
    }
    else if (slap_ui.page == VISUAL)
        slap_ui.visual = select_visual(slap_ui.visual, delta);
    else if (slap_ui.page == HOME || slap_ui.page == MIXER || (slap_ui.page == RECORD && !recording) ||
             slap_ui.page == PATCH || slap_ui.page == ENV || slap_ui.page == LFO || slap_ui.page == FX || slap_ui.page == LIVE_FX)
        slap_ui.track = clamp(slap_ui.track + delta, 0, TRACKS - 1);
    if (slap_ui.track != previous_track) {
        clear_mode = 0;
        edit_page = 0;
    }
    if (recording && slap_ui.page == HOME && rec_loop &&
        (slap_ui.track != previous_track || previous_page != HOME))
        rec_follow_track = (unsigned)slap_ui.track + 1u;
    int algorithm = plat_enc(EN_ALGO);
    if (entering_edit) algorithm = 0;
    if (slap_ui.page == FX) slap_ui.fx_page = clamp(slap_ui.fx_page + algorithm, 0, SLAP_FX_PAGES - 1);
    else if (slap_ui.page == PATCH) {
        unsigned track = slap_ui.track;
        unsigned last = track < DRUMS && slap_patches[tracks[track].patch].engine == 0u ? 4u : 1u;
        edit_page = (unsigned)clamp((int)edit_page + algorithm, 0, last);
    } else if (slap_ui.page == SETTINGS) {
        unsigned next = (unsigned)clamp((int)settings_section + algorithm, 0, PREF_SECTIONS - 1);
        if (next != settings_section) settings_item = 0;
        settings_section = next;
    } else tempo = (uint32_t)clamp((int)tempo + algorithm, 40, 240);
    int patch_delta = plat_enc(EN_PRESET);

    if (patch_delta && !entering_edit) {
        patch_group[slap_ui.track] = 0;
        int old_patch = (int)tracks[slap_ui.track].patch;
        if (slap_ui.track == DRUMS)
            tracks[DRUMS].patch = select_drum_kit((unsigned)old_patch, patch_delta);
        else {
            unsigned next = (unsigned)clamp(old_patch + patch_delta, 0, SLAP_PATCHES - 1);
            if (next != (unsigned)old_patch) load_patch(slap_ui.track, next);
            if (slap_patches[next].engine != 0u && edit_page > 1u) edit_page = 1u;
        }
    }
    int knob[4];
    for (unsigned k = 0; k < 4; ++k) {
        int movement = plat_enc(EN_K1 + k);
        knob[k] = entering_edit || (slap_ui.page == PATCH && algorithm) ? 0 : movement;
    }
    uint32_t mix = tracks[slap_ui.track].mix;
    if (slap_ui.page == SETTINGS) {
        int change = knob[0];
        for (unsigned k = 0; k < 4u; ++k) knob[k] = k == settings_item ? change : 0;
        switch (settings_section) {
        case PREF_AUDIO:
            auto_mix = (unsigned)clamp((int)auto_mix + knob[3], 0, 1);
            auto_level_off = (unsigned)clamp((int)auto_level_off - knob[2], 0, 1);
            output_trim = (unsigned)clamp((int)output_trim - knob[0], 0, 3);
            speaker_mode = (unsigned)clamp((int)speaker_mode + knob[1], 0, 1);
            break;
        case PREF_METRO:
            metronome = (unsigned)clamp((int)metronome + knob[0], 0, 2);
            click_trim = (unsigned)clamp((int)click_trim - knob[1], 0, 3);
            rec_count_in = (unsigned)clamp((int)rec_count_in + knob[2], 0, 1);
            break;
        case PREF_RECORD:
            rec_quantize = (unsigned)clamp((int)rec_quantize + knob[0], 0, REC_QUANT_MAX);
            rec_bars = (unsigned)clamp((int)rec_bars + knob[1], 0, 2);
            rec_replace = (unsigned)clamp((int)rec_replace + knob[2], 0, 1);
            rec_count_in = (unsigned)clamp((int)rec_count_in + knob[3], 0, 1);
            break;
        case PREF_TRANSPORT:
            resume_playback = (unsigned)clamp((int)resume_playback + knob[0], 0, 1);
            break;
        case PREF_KEYS:
            if (knob[0]) perfect_note_off = knob[0] > 0 ? 0u : 15u;
            break;
        case PREF_LEDS:
            if (knob[0]) led_style = (unsigned)clamp((int)led_style + knob[0], 0, 2);
            if (knob[1]) led_keys_on_hit = knob[1] > 0;
            break;
        case PREF_DISPLAY:
            slap_ui.visual = select_visual(slap_ui.visual, knob[0]);
            visual_quality = (unsigned)clamp((int)visual_quality + knob[1], 0, 1);
            break;
        }
    } else if (slap_ui.page == VISUAL && slap_ui.visual == 6) {
        dancer_pattern = (unsigned)clamp((int)dancer_pattern + knob[0], 0, 2);
        dancer_outline = (unsigned)clamp((int)dancer_outline + knob[1], 0, 1);
        dancer_lighting = (unsigned)clamp((int)dancer_lighting + knob[2], 0, 1);
        dancer_reactivity = (unsigned)clamp((int)dancer_reactivity + knob[3], 0, 1);
    } else if (slap_ui.page == GEN_DRUMS) {
        if (knob[0] || knob[1] || knob[2] || knob[3] || (pressed & (1u << B_ARP))) gen_confirm = 0;
        gen_style = (unsigned)clamp((int)gen_style + knob[0], 0, sizeof(drum_style) / sizeof(drum_style[0]) - 1);
        gen_hit = (unsigned)clamp((int)gen_hit + knob[1], 0, 100);
        if (buttons & (1u << B_OCTUP))
            gen_groove = (unsigned)clamp((int)gen_groove + knob[2], 0, 100);
        else gen_bars = (unsigned)clamp((int)gen_bars + knob[2], 1, 4);
        gen_var = (unsigned)clamp((int)gen_var + knob[3], 0, 100);
        if (pressed & (1u << B_ARP)) gen_enabled ^= 1u;
        if ((pressed & (1u << B_SEL)) && !knob[2]) generate_drums();
    } else if (slap_ui.page == RECORD && !recording) {
        rec_bars = (unsigned)clamp((int)rec_bars + knob[0], 0, 2);
        rec_count_in = (unsigned)clamp((int)rec_count_in + knob[1], 0, 1);
        rec_quantize = (unsigned)clamp((int)rec_quantize - knob[2], 0, REC_QUANT_MAX);
        rec_replace = (unsigned)clamp((int)rec_replace + knob[3], 0, 1);
    } else if (slap_ui.page == MIXER) {
        unsigned t = (unsigned)slap_ui.track;
        if (knob[0]) track_setting(t, 16, level_after_turn((mix >> 16) & 255u, knob[0]));
        if (knob[1]) track_setting(t, 8, pan_after_turn((mix >> 8) & 255u, knob[1]));
        if (knob[2]) {
            if (knob[2] > 0) mixer_solo |= 1u << t;
            else mixer_solo &= ~(1u << t);
        }
        if (t < DRUMS)
            mixer_duck[t] = (uint8_t)clamp((int)mixer_duck[t] + knob[3], 0, 100);
    } else if (slap_ui.page == PATCH) {
        unsigned t = (unsigned)slap_ui.track;
        const SlapPatch *program = t < DRUMS ? &slap_patches[tracks[t].patch] : 0;
        if (!edit_page) {
            patch_group[t] = (unsigned)clamp((int)patch_group[t] + knob[0], 0, t == DRUMS ? 2 : 8);
            if (knob[1]) {
                unsigned next = browse_patch(t, tracks[t].patch, knob[1]);
                if (t == DRUMS) tracks[t].patch = next;
                else if (next != tracks[t].patch) load_patch(t, next);
            }
        } else if (program && (program->engine == 0u || program->engine == 2u) && (knob[0] || knob[1] || knob[2] || knob[3])) {
            uint32_t value = track_timbre[t];
            if (program->engine == 0u && edit_page == 4u) {
                track_fm_envelopes[t][6] = (unsigned)clamp((int)track_fm_envelopes[t][6] + knob[0], 0, 16);
            } else if (program->engine == 0u && edit_page == 3u) {
                uint32_t config = track_fm_envelopes[t][timbre_operator];
                if (!config) config = 0x14561u;
                if ((buttons & (1u << B_OCTUP)) && knob[3]) {
                    track_fm_envelopes[t][6] = (unsigned)clamp((int)track_fm_envelopes[t][6] + knob[3], 0, 16);
                } else {
                    for (unsigned k = 0; k < 4u; ++k) {
                        unsigned shift = k * 4u;
                        unsigned next = (unsigned)clamp((int)(config >> shift & 15u) + knob[k], 0, 15);
                        config = (config & ~(15u << shift)) | next << shift;
                    }
                    track_fm_envelopes[t][timbre_operator] = config;
                }
            } else if (program->engine == 0u) {
                timbre_operator = (unsigned)clamp((int)timbre_operator + knob[0], 0, 5);
                if (!value) value = 0x3ffda249u;
                unsigned rs = timbre_operator * 3u, ls = 18u + timbre_operator * 2u;
                unsigned ratio = (unsigned)clamp((int)(value >> rs & 7u) + knob[1], 0, 7);
                unsigned level = (unsigned)clamp((int)(value >> ls & 3u) + knob[2], 0, 3);
                unsigned algorithm = (unsigned)clamp((int)(value >> 30) + knob[3], 0, 2);
                track_timbre[t] = (value & ~((7u << rs) | (3u << ls) | (3u << 30))) |
                    ratio << rs | level << ls | algorithm << 30;
            } else {
                int morph = value ? value & 127u : program->color;
                int motion = value ? value >> 8 & 127u : program->motion;
                track_timbre[t] = 0x80000000u | (unsigned)clamp(morph + knob[0], 0, 127) |
                    (unsigned)clamp(motion + knob[1], 0, 127) << 8;
            }
            tracks[t].mix |= ADVANCED_SYNTH;
        } else if (program) {
            if (knob[0]) slap_fx_change(t, 1, 0, knob[0]);
            if (knob[1]) slap_fx_change(t, 8, 0, knob[1]);
        }
    } else if (slap_ui.page == GLOBAL && global_details == 8) {
        if (knob[0]) dma_view = (unsigned)clamp((int)dma_view + knob[0], 0, 3);
        if (knob[1]) half_trace_frozen = knob[1] > 0;
        if (knob[2]) dsp_reset_request = 1;
    } else if (slap_ui.page == GLOBAL) {
        tempo = (unsigned)clamp((int)tempo + knob[0], 40, 240);
        swing = (unsigned)clamp((int)swing + knob[1], 0, 70);
        if (knob[2]) dsp_reset_request = 1;
        unsigned mode = (unsigned)clamp((int)metronome + knob[3], 0, 2);
        metronome = mode;
    } else if (slap_ui.page == FX) {
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(slap_ui.track, slap_ui.fx_page, k, knob[k]);
        if (slap_ui.fx_page == 4 && knob[1]) {
            unsigned t = (unsigned)slap_ui.track;
            mixer_proc[t] = (uint8_t)level_after_turn(mixer_proc[t], knob[1]);
        }
    } else if (slap_ui.page == SEQ) {

        int fixed = slap_ui.track == DRUMS && slap_ui.lane;
        int editing_timing = buttons & (1u << B_SEL);
        unsigned row = pattern_row(slap_ui.track, slap_ui.lane);
        uint32_t word = event_get(slap_ui.track, slap_ui.step, slap_ui.lane);
        uint32_t original_word = word;
        uint8_t original_timing = timing_group_get(row, slap_ui.step);
        if (notes) {
            timing_group_set(row, slap_ui.step, 0);
            unsigned k = 0;
            while (!(notes & (1u << k))) ++k;
            unsigned pitch = fixed ? lane_pitch[slap_ui.lane - 1] : 29u + k + (unsigned)slap_ui.octave * 12u;
            word = (word & 255u) == (pitch | ACTIVE) ? 0u : cell_word(pitch, 100, 140, 100);
        }
        int melodic = slap_ui.track < DRUMS;
        if (melodic && !editing_timing && knob[2]) {
            if (knob[2] < 0) {
                word = 0;
                timing_group_set(row, slap_ui.step, 0);
            } else if (!(word & ACTIVE)) {
                unsigned pitch = 29u + (unsigned)slap_ui.octave * 12u;
                word = cell_word(pitch, 100, 140, 100);
            }
        }
        if (word & ACTIVE) {
            if (melodic && !editing_timing && knob[3]) {
                uint16_t *hold = &melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step];
                unsigned unit = (RATE * 15u / tempo) / NOTE_DURATION_UNIT;
                int length = clamp((int)((*hold + unit / 2u) / unit) + knob[3], 0, 65535 / (int)unit);
                *hold = (uint16_t)((unsigned)length * unit);
                ++pattern_revision[slap_ui.track];
            }
            if (editing_timing && knob[2] && slap_ui.track < DRUMS) {
                uint16_t *hold = &melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step];
                unsigned quarter_step = (RATE * 15u / tempo) / (NOTE_DURATION_UNIT * 4u);
                int quarters = clamp((int)((*hold + quarter_step / 2u) / quarter_step) + knob[2],
                    0, 65535 / (int)quarter_step);
                *hold = (uint16_t)((unsigned)quarters * quarter_step);
                knob[2] = 0;
                ++pattern_revision[slap_ui.track];
            }
            unsigned timing = timing_of(word);
            if (editing_timing && knob[3]) {
                unsigned offset = (unsigned)clamp((int)note_timing(row, slap_ui.step, word) + knob[3], 0, 15);
                timing_group_set(row, slap_ui.step, (uint8_t)(offset / 4u + 1u +
                    (timing_group_get(row, slap_ui.step) / TIMING_GROUPS) * TIMING_GROUPS));
                timing = offset;
            }
            word = cell_word((unsigned)clamp((int)(word & NOTES) + (fixed || (melodic && editing_timing) ? 0 : knob[0]), 0, 127),
                (unsigned)clamp((int)((word >> 8) & 127u) + (melodic && editing_timing ? 0 : knob[1]), 1, 127),
                (unsigned)clamp((int)((word >> 16) & 255u) + (melodic ? (editing_timing ? knob[1] : 0) : knob[2]), 1, 255),
                (unsigned)clamp((int)((word >> 24) & 127u) + (melodic ? (editing_timing ? knob[0] : 0) :
                    (editing_timing ? 0 : knob[3])), 0, 100)) |
                timing_bits(timing);
        }
        if (word != original_word) event_set(slap_ui.track, slap_ui.step, slap_ui.lane, word);
        if (melodic && !(word & ACTIVE))
            melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step] = 0;
        if (word != original_word || timing_group_get(row, slap_ui.step) != original_timing)
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
        if (harmony && knob[1] && !(buttons & (1u << B_SEL))) {
            unsigned bit = 1u << slap_ui.track;
            if (knob[1] > 0) split_keys |= bit;
            else split_keys &= ~bit;
            knob[1] = 0;
        }
        const int *limit = harmony ? harmony_max : lfo_max;
        for (unsigned k = 0; k < 4; ++k) {
            if (!knob[k]) continue;
            unsigned shift = 8u * k;
            int base = (word >> shift) & 255u;
            int value = clamp(base + knob[k], 0, limit[k]);
            word = (word & ~(255u << shift)) | (uint32_t)value << shift;
        }
        *setting = word;
    } else if (slap_ui.page == ENV && slap_ui.track < DRUMS) {
        uint32_t env = tracks[slap_ui.track].envelope;
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k * 8u;
            int v = clamp((int)((env >> shift) & 255u) + knob[k], 0, 127);
            env = (env & ~(255u << shift)) | (uint32_t)v << shift;
        }
        tracks[slap_ui.track].envelope = env;
    } else if (slap_ui.page == HOME) {
        if (knob[0]) track_setting(slap_ui.track, 8, pan_after_turn((mix >> 8) & 255u, knob[0]));
        if (knob[1] && !recording && !rec_running) {
            unsigned old_length = pattern_length(mix);
            int index = old_length <= 16u ? 0 : old_length <= 32u ? 1 : 2;
            if (old_length == 16u || old_length == 32u || old_length == 64u)
                index = clamp(index + knob[1], 0, 2);
            if (old_length <= 64u || knob[1] < 0)
                track_setting(slap_ui.track, 0, 16u << index);
        }
        if (knob[2]) track_setting(slap_ui.track, 16, level_after_turn((mix >> 16) & 255u, knob[2]));
        swing = (unsigned)clamp((int)swing + knob[3], 0, 70);
        slap_ui.step = clamp(slap_ui.step, 0, (int)(pattern_length(tracks[slap_ui.track].mix)) - 1);
    }
#ifndef PLAT_WEB
    fill_active = slap_ui.page == SEQ && (buttons & (1u << B_SEL));
#else
    if (slap_ui.page == SEQ) fill_active = buttons & (1u << B_SEL);
#endif
    input_track = (unsigned)slap_ui.track;
    input_octave = (unsigned)slap_ui.octave;
    live_fx_suppressed &= keys;
    if (slap_ui.page == LIVE_FX) {
        unsigned mode = 0;
        for (unsigned i = 0; i < 11; ++i) {
            uint32_t bit = 1u << live_black_keys[i];
            if (keys & bit) {
                live_fx_suppressed |= bit;
                if (!mode && i < sizeof(live_fx_names) / sizeof(*live_fx_names)) mode = i + 1u;
            }
        }
        live_fx_word = mode ? (live_fx_full_mix ? TRACKS : (unsigned)slap_ui.track) | mode << 3 |
            live_fx_knobs[0] << 7 | live_fx_knobs[1] << 13 |
            live_fx_knobs[2] << 19 | live_fx_knobs[3] << 25 : 0;
    }
    if (slap_ui.page == MIXER) live_fx_suppressed |= keys;
    keys_audio = keys & ~(boot_suppressed_keys | live_fx_suppressed);
    slap_ui.keys = keys;
    slap_ui.buttons = buttons;
    unsigned lights = (buttons & ~(1u << B_PLAY)) | (recording << B_REC);
    if (playing && !rec_countdown && !(positions & 3u))
        lights |= LED_PLAY_GREEN;
    unsigned context = 0;
    if (slap_ui.page == HOME) {
        context = (1u << B_EDIT) | (1u << B_SEQ) | (1u << B_FX);
        if (slap_ui.track < DRUMS)
            context |= (1u << B_SEL) | (1u << B_ENV) | (1u << B_LFO) | (1u << B_ARP);
    }
    unsigned key_lights = keys;
    if (slap_ui.page == MIXER) {
        key_lights = 0;
        for (unsigned t = 0; t < TRACKS; ++t)
            if (!(tracks[t].mix & (1u << 24))) key_lights |= 1u << save_keys[t];
    }
    unsigned priority_lights = context | (lights & (LED_PLAY_GREEN | (1u << B_REC)));
    unsigned ordinary_lights = lights & ~priority_lights;
    uint32_t bright_buttons = priority_lights | ordinary_lights;
    uint32_t dim_buttons = ((1u << NB) - 1u) & ~bright_buttons;
    uint32_t normal_keys = ((1u << NKEYS) - 1u) & ~key_lights;
    if (slap_ui.page == MIXER || led_keys_on_hit) normal_keys = 0;
    plat_lights(bright_buttons, key_lights, dim_buttons, 0, led_style == 2u ? 2 : led_style == 1u ? 0 : 1);
    plat_lights_mid(0, normal_keys);
    uint32_t preferences = metronome | (perfect_note_off & 15u) << 2 |
        rec_bars << 6 | rec_count_in << 9 | rec_replace << 10 | rec_quantize << 11 |
        output_trim << 15 | speaker_mode << 17 | click_trim << 18 |
        resume_playback << 20 | visual_quality << 21 | (unsigned)slap_ui.visual << 22 | auto_level_off << 25 | split_keys << 26 | led_style << 30;
    uint32_t options = auto_mix | dancer_pattern << 1 | (unsigned)!dancer_outline << 3 |
        (unsigned)!dancer_lighting << 4 | (unsigned)!dancer_reactivity << 5 |
        led_keys_on_hit << 6 | 128u;
    uint32_t duck = 0;
    for (unsigned t = 0; t < DRUMS; ++t) duck |= (uint32_t)mixer_duck[t] << (8u * t);
    uint32_t now = plat_ms();
    if (preferences != pending_preferences || pending_assignments != live_fx_assignments ||
        pending_mixer_options != options || pending_duck != duck) {
        pending_preferences = preferences;
        pending_assignments = live_fx_assignments;
        pending_mixer_options = options;
        pending_duck = duck;
        preferences_changed_at = now;
    }
    if (!playing && !recording && !keys && (preferences != saved_preferences || live_fx_assignments != saved_assignments ||
        options != saved_mixer_options || duck != saved_duck) &&
        now - preferences_changed_at >= SETTINGS_SETTLE_MS) {
        uint32_t settings[] = {SETTINGS_TAG, preferences, live_fx_assignments, options, duck};
        unsigned bytes = duck ? sizeof settings : options ? sizeof settings - 4u : sizeof settings - 8u;
        if (plat_store_save(OBJ_SETTINGS, settings, bytes) == 0) {
            saved_preferences = preferences;
            saved_assignments = live_fx_assignments;
            saved_mixer_options = options;
            saved_duck = duck;
        }
        preferences_changed_at = now;
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

static int note_sounding(unsigned row, unsigned step)
{
    unsigned id = 1u + row * STEPS + step;
    // Read-only foreground snapshot. Play state may advance one audio frame
    // during drawing, so the next strip refresh corrects stale highlights.
    for (unsigned i = 0; i < VOICES; ++i) {
        const volatile Voice *voice = &voices[i];
        if (voice->age && voice->sequence_note == id && !voice->delay && !voice->releasing)
            return 1;
    }
    return 0;
}

static unsigned header_valid, graph_valid, home_idle_valid;

typedef struct {
    unsigned page, track, step, bank, mix, tempo, positions, revision, envelope, modulation, split, tuning;
} GraphState;
static GraphState previous_graph;
static int battery_status = -1;
static unsigned low_battery;

static void sample_battery(uint32_t now)
{
    static uint32_t sampled_at, empty_since;
    static unsigned empty;
    if (now - sampled_at < 500u) return;
    sampled_at = now;
    int status = plat_battery();
    if (status == 0 && !empty) empty_since = now;
    empty = status == 0;
    unsigned warning = empty && now - empty_since >= 2500u;
    if (battery_status != status || low_battery != warning) header_valid = 0;
    battery_status = status;
    low_battery = warning;
}

static void home_track_card(unsigned track, unsigned selected, int y)
{
    char number[] = {(char)('1' + track), 0};
    const char *name = track == DRUMS ? drum_kit_name(tracks[track].patch) :
        slap_patches[tracks[track].patch].name;
    int x = 4 + (int)track * 47;
    if (track == selected) cv_rect(x - 1, y - 1, 46, 26, C_WHITE);
    cv_rect(x, y, 44, 25, track == selected ? track_color[track] : track_rule[track]);
    cv_text(x + 3, y - 1, &FONT_T, number, track == selected ? C_BLACK : C_WHITE);
    for (unsigned row = 0; row < 2u && *name; ++row) {
        char label[24];
        unsigned n = 0, space = 0;
        while (n < sizeof(label) - 2u && name[n]) {
            label[n] = name[n];
            label[n + 1u] = 0;
            if (text_w(&FONT_T, label) > (row ? 40 : 33)) break;
            if (name[n] == ' ') space = n;
            ++n;
        }
        if (!row && name[n] && name[n] != ' ' && space) n = space;
        label[n] = 0;
        cv_text(x + (row ? 2 : 9), y + 1 + 11 * (int)row, &FONT_T, label,
            track == selected ? C_BLACK : C_WHITE);
        name += n;
        while (*name == ' ') ++name;
    }
}

static int held_note_width(unsigned duration, unsigned step, unsigned base, unsigned delay)
{
    unsigned swing_offset = base * swing / 200u;
    unsigned span = step & 1u ? base - swing_offset : base + swing_offset;
    unsigned first = span - delay;
    if (duration <= first) return (int)(duration * 14u / span);
    unsigned pixels = first * 14u / span;
    duration -= first;
    pixels += duration / (2u * base) * 28u;
    duration %= 2u * base;
    span = 2u * base - span;
    if (duration >= span) {
        pixels += 14u;
        duration -= span;
        span = 2u * base - span;
    }
    return (int)(pixels + duration * 14u / span);
}

static void draw_home_sequences(void)
{
    unsigned position = positions, run = playing, fraction = step_progress;
    unsigned selection = slap_ui.track, bpm = tempo, solo = mixer_solo;
    static unsigned last_step, last_bpm, last_selection, last_running, last_record, last_swing;
    static int last_battery;
    static struct {
        unsigned cursor, first, patch, mix, revision, meter, peak, focused;
        uint32_t active;
    } last_lane[TRACKS];
    int battery = low_battery ? -2 : battery_status;
    unsigned rec = rec_running;
    unsigned full = !home_idle_valid;
    unsigned redraw_header = full || last_step != position || last_bpm != bpm || last_battery != battery;
    unsigned redraw_footer = full || last_selection != selection || last_running != run || last_record != rec;
    unsigned geometry = full || last_bpm != bpm || last_swing != swing;
    last_step = position; last_bpm = bpm; last_battery = battery;
    last_selection = selection; last_running = run; last_record = rec; last_swing = swing;
    unsigned active[VOICES], count = 0;
    for (unsigned v = 0; v < VOICES; ++v)
        if (voices[v].age && !voices[v].delay && !voices[v].releasing && voices[v].sequence_note)
            active[count++] = voices[v].sequence_note;
    char number[12];
    if (redraw_header) {
    cv_begin(240, 33, C_BLACK);
    cv_text(5, 0, &FONT_M, "SLAP-5", C_WHITE);
    decimal(number, bpm);
    cv_text(100, 0, &FONT_M, number, C_WHITE);
    cv_text(141, 5, &FONT_B, "BPM", C_WHITE);
    decimal(number, (position & 15u) + 1u);
    cv_text(183, 1, &FONT_B, "STEP", C_WHITE);
    cv_text(238 - text_w(&FONT_B, number), 1, &FONT_B, number, C_WHITE);
    if (low_battery) cv_text(5, 22, &FONT_T, "LOW BATTERY", track_color[3]);
    else if (battery_status == 4) cv_text(66, 22, &FONT_T, "USB", C_WHITE);
    else if (battery_status >= 0)
        for (int bar = 0; bar < 4; ++bar)
            cv_rect(66 + bar * 5, 25, 4, 5, bar < battery_status ? C_WHITE : track_surface[0]);
    cv_blit(0, 0);
    }
    for (unsigned t = 0; t < TRACKS; ++t) {
        unsigned mix = tracks[t].mix, length = pattern_length(mix), patch = tracks[t].patch;
        unsigned step = position % length, first = step & ~15u, lanes = lane_count(t);
        uint16_t color = track_color[t];
        const char *name = t == DRUMS ? drum_kit_name(patch) : slap_patches[patch].name;
        unsigned level = (mix & (1u << 24)) || (solo && !(solo & (1u << t))) ? 0u : track_energy[t];
        int meter = (int)(fm_sqrtf(level * (1.0f / 160.0f)) * 44.0f);
        unsigned peak = level ? track_peak[t] : 0u;
        unsigned cursor = run ? (step & 15u) * 12u + fraction * 11u / 14u : 192u;
        unsigned focused = t == selection, revision = pattern_revision[t];
        uint32_t sounding = 2166136261u;
        for (unsigned v = 0; v < count; ++v) {
            unsigned row = (active[v] - 1u) / STEPS;
            for (unsigned lane = 0; lane < lanes; ++lane) {
                if (row != pattern_row(t, lane)) continue;
                sounding = (sounding ^ active[v]) * 16777619u;
                break;
            }
        }
        if (!geometry && !recording && last_lane[t].cursor == cursor && last_lane[t].first == first &&
            last_lane[t].patch == patch && last_lane[t].mix == mix && last_lane[t].revision == revision &&
            last_lane[t].meter == (unsigned)meter && last_lane[t].peak == peak &&
            last_lane[t].focused == focused && last_lane[t].active == sounding) continue;
        last_lane[t] = (__typeof__(last_lane[t])){cursor, first, patch, mix, revision,
            (unsigned)meter, peak, focused, sounding};
        cv_begin(240, 36, C_BLACK);
        cv_rect(5, 0, 24, 34, t == selection ? color : track_rule[t]);
        decimal(number, t + 1u);
        cv_text(10, 4, &FONT_M, number, t == selection ? C_BLACK : C_WHITE);
        cv_text(34, 0, text_w(&FONT_B, name) <= 148 ? &FONT_B : &FONT_S, name, C_WHITE);
        cv_rect(190, 5, 44, 7, track_surface[t]);
        cv_rect(190, 5, meter, 7, color);
        if (peak) cv_rect(190 + (int)(fm_sqrtf(peak * (1.0f / 160.0f)) * 43.0f), 5, 1, 7, C_WHITE);
        for (unsigned col = 0; col < 16u && first + col < length; ++col) {
            int x = 34 + (int)col * 12;
            cv_rect(x, 17, 11, 15, track_surface[t]);
            if (!(col & 3u)) cv_rect(x, 17, col ? 1 : 2, 15, track_rule[t]);
        }
        unsigned base = RATE * 15u / bpm;
        for (unsigned col = 0; col < 16u && first + col < length; ++col) {
            unsigned index = first + col;
            for (unsigned lane = 0; lane < lanes; ++lane) {
                unsigned word = event_get(t, index, lane);
                if (!(word & ACTIVE)) continue;
                unsigned row = pattern_row(t, lane), timing = note_timing(row, index, word);
                unsigned duration = t < DRUMS ? melodic_duration[t][lane][index] : 0u;
                unsigned swing_samples = base * swing / 200u;
                unsigned span = index & 1u ? base - swing_samples : base + swing_samples;
                unsigned gate = duration ? duration * NOTE_DURATION_UNIT : span * ((word >> 16) & 255u) / 192u;
                int x = 34 + (int)col * 12 + (int)(timing * 12u / 16u);
                int y = 29 - (int)((word & NOTES) % 12u);
                int width = held_note_width(gate, index, base, span * timing / 16u) * 12 / 14;
                uint16_t ink = color;
                unsigned id = 1u + row * STEPS + index;
                for (unsigned v = 0; v < count; ++v)
                    if (active[v] == id) { ink = C_WHITE; break; }
                cv_rect(x, y, clamp(width, 2, 226 - x), 2, ink);
            }
        }
        if (run) cv_rect(34 + (int)cursor, 17, 1, 15, C_WHITE);
        cv_blit(0, 33 + (int)t * 36);
    }
    if (redraw_footer) {
    cv_begin(240, 27, C_BLACK);
    cv_text(5, 5, &FONT_B, rec_running ? "REC" : run ? "PLAY" : "STOP", C_WHITE);
    cv_text(85, 5, &FONT_B, "1/16", C_WHITE);
    cv_text(149, 5, &FONT_B, "SELECT TRK", track_color[selection]);
    cv_blit(0, 213);
    }
    home_idle_valid = 1;
}

static const volatile SlapSynth *graph_voice(unsigned track, unsigned engine)
{
    unsigned age = 0;
    const volatile SlapSynth *voice = 0;
    for (unsigned v = 0; v < VOICES; ++v) {
        const volatile Voice *candidate = &voices[v];
        if (candidate->age > age && candidate->track == track && !candidate->delay &&
            !candidate->releasing && synths[v].engine == engine) {
            age = candidate->age;
            voice = &synths[v];
        }
    }
    return voice;
}

static void draw_fm_routing(unsigned selected, unsigned algorithm, uint32_t config, uint16_t color)
{
    static const uint8_t serial[][2] = {{5, 4}, {4, 3}, {2, 1}, {1, 0}};
    static const uint8_t x[] = {25, 63, 101, 139, 177, 215};
    const volatile SlapSynth *live = graph_voice(selected, 0);
    unsigned energy = track_energy[selected];
    uint16_t route = live && energy > 6u ? color : track_rule[selected];
    cv_rect(9, 36, 216, 1, track_surface[selected]);
    for (unsigned edge = 0; edge < sizeof serial / sizeof serial[0]; ++edge)
        cv_line(x[serial[edge][0]], 16, x[serial[edge][1]], 16, route);
    const int modulation = algorithm == 1u ? 5 : 3;
    cv_line(x[modulation], 9, x[modulation], 2, route);
    cv_line(x[modulation], 2, x[2], 2, route);
    cv_line(x[2], 2, x[2], 9, route);
    if (algorithm == 2u) {
        cv_line(x[4], 25, x[4], 34, route);
        cv_line(x[4], 34, x[0], 34, route);
        cv_line(x[0], 34, x[0], 25, route);
    }
    for (unsigned op = 0; op < 6u; ++op) {
        unsigned level = config ? config >> (18u + 2u * op) & 3u : 3u;
        float envelope = live && (live->operator_mask & (1u << op)) ? live->operators[op].gain : 1.0f;
        unsigned height = (unsigned)fm_clampf(envelope * level * energy / 27.0f, 0.0f, 17.0f);
        int carrier = op == 0u || (algorithm == 1u && op == 3u) || (algorithm == 2u && op == 2u);
        cv_rect(x[op] - 12, 8, 24, 19, op == timbre_operator ? track_rule[selected] : track_surface[selected]);
        cv_rect(x[op] - 11, 26 - (int)height, 22, (int)height, carrier ? C_WHITE : color);
        char label[] = {(char)('1' + op), 0};
        cv_text(x[op] - 3, 7, &FONT_XS, label, height > 10u && carrier ? C_BLACK : C_WHITE);
        if (carrier) cv_rect(x[op] - 5, 28, 10, 2, color);
    }
}

static unsigned browser_valid, browser_track, browser_patch, browser_group, browser_tempo, browser_page;

static void draw_panel(void)
{
    unsigned selected = (unsigned)slap_ui.track;
    if (selected >= TRACKS) return;
    if (slap_ui.page == PATCH && (!edit_page || selected == DRUMS)) {
        unsigned patch = tracks[selected].patch, group = patch_group[selected];
        if (browser_valid && selected == browser_track && patch == browser_patch &&
            group == browser_group && tempo == browser_tempo && edit_page == browser_page) return;
        browser_valid = 1;
        browser_track = selected; browser_patch = patch; browser_group = group; browser_tempo = tempo; browser_page = edit_page;
    } else browser_valid = 0;

    static const char *const page_names[] = {"SLAP-5", "SEQUENCE", "GLOBAL", "SOUND LAB", "ENV", "FX", "VISUAL", "SAVE", "KEY & CHORDS", "LFO", "ARP", "MIXER", "RECORD", "GEN DRUMS", "LIVE FX", "SETTINGS"};
    static const char *const arp_labels[] = {"MODE", "RATE", "OCT", "GATE"};
    static const char *const arp_modes[] = {"OFF","UP","DOWN","UP/DN","RANDOM","CHORD","PLAYED"};
    static const char *const arp_rates[] = {"1/4","1/8","1/16","1/32","1/8T","1/16T"};
    static const char *const lfo_labels[] = {"WAVE", "RATE", "PHS", "FADE"};
    static const char *const harmony_labels[] = {"KEY", "SPLIT", "CHORD", "STRUM"};
    static const char *const env_labels[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    static const char *const labels[] = {"PAN", "STEPS", "LEVEL", "SWING"};
    static const char *const edit_labels[] = {"NOTE", "VEL", "GATE", "HIT %"};
    static const char *const melodic_edit_labels[] = {"NOTE", "ACCENT", "REST", "HOLD"};
    static const char *const melodic_detail_labels[] = {"HIT %", "GATE", "HOLD /4", "OFFSET"};
    char number[12];
    unsigned had_graph = graph_valid;
    graph_valid = 0;
    uint32_t position = positions;
    uint32_t settings = tracks[selected].mix;
    unsigned length = pattern_length(settings), play_step = position % length;
    unsigned scroll = slap_ui.page == HOME && playing;
    unsigned substep = step_progress;
    unsigned bank = (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) / 16u * 16u;
    uint16_t c = track_color[selected], background = C_BLACK;
    static unsigned header_track, header_tempo, header_patches[TRACKS];
    unsigned redraw_header = !header_valid || slap_ui.page != HOME ||
        header_track != selected || header_tempo != tempo;
    for (unsigned t = 0; t < TRACKS; ++t)
        if (header_patches[t] != tracks[t].patch) redraw_header = 1;
    header_valid = slap_ui.page == HOME;
    if (redraw_header) {
        header_track = selected;
        header_tempo = tempo;
        for (unsigned t = 0; t < TRACKS; ++t) header_patches[t] = tracks[t].patch;
        cv_begin(240, slap_ui.page == HOME ? 50 : 48, background);
        if (slap_ui.page == HOME) cv_text(8, 1, &FONT_M, "SLAP-5", C_WHITE);
        else cv_text(8, 1, slap_ui.page == HARMONY ? &FONT_B : &FONT_M, page_names[slap_ui.page], c);
        decimal(number, tempo);
        cv_text(164, 1, &FONT_M, number, C_WHITE);
        cv_text(210, 10, &FONT_XS, "BPM", C_WHITE);
        if (slap_ui.page == HOME) {
            for (unsigned t = 0; t < TRACKS; ++t) home_track_card(t, selected, 25);
        } else for (unsigned t = 0; t < TRACKS; ++t) {
            int x = 4 + (int)t * 47;
            decimal(number, t + 1u);
            if (t == selected) cv_rect(x - 1, 24, 46, 24, C_WHITE);
            cv_rect(x, 25, 44, 22, track_color[t]);
            cv_text(x + 18, 28, &FONT_XS, number, C_BLACK);
        }
        if (slap_ui.page == HOME && low_battery) cv_text(104, 4, &FONT_T, "LOW BATTERY", C_WHITE);
        else if (slap_ui.page == HOME && battery_status == 4) cv_text(139, 5, &FONT_T, "USB", C_WHITE);
        else if (slap_ui.page == HOME && battery_status >= 0) {
            cv_rect(138, 5, 17, 8, C_WHITE);
            cv_rect(139, 6, 15, 6, C_BLACK);
            cv_rect(155, 7, 2, 4, C_WHITE);
            for (int bar = 0; bar < battery_status; ++bar)
                cv_rect(140 + 4 * bar, 7, 3, 4, C_WHITE);
        }
        cv_blit(0, 0);
    }
    if (slap_ui.page == SETTINGS) {
        const char *labels[4] = {0}, *values[4] = {0};
        char level[12], other[12];
        switch (settings_section) {
        case PREF_AUDIO:
            labels[0] = "OUTPUT"; labels[1] = "SPEAKER"; labels[2] = "AUTO LEVEL"; labels[3] = "AUTO MIX";
            decimal(level, 100u - output_trim * 20u);
            values[0] = level; values[1] = speaker_mode ? "OPTIMIZED" : "FLAT";
            values[2] = auto_level_off ? "OFF" : "ON"; values[3] = auto_mix ? "ON" : "OFF";
            break;
        case PREF_METRO:
            labels[0] = "METRONOME"; labels[1] = "CLICK LEVEL"; labels[2] = "RECORD START";
            decimal(level, 100u - click_trim * 25u);
            values[0] = metronome == 2u ? "ON" : metronome == 1u ? "REC ONLY" : "OFF";
            values[1] = level; values[2] = rec_count_in ? "COUNT-IN" : "FIRST NOTE";
            break;
        case PREF_RECORD:
            labels[0] = "QUANTIZATION"; labels[1] = "BARS"; labels[2] = "MODE"; labels[3] = "RECORD START";
            decimal(level, rec_quantize); decimal(other, 1u << rec_bars);
            values[0] = level; values[1] = other;
            values[2] = rec_replace ? "REPLACE" : "OVERDUB"; values[3] = rec_count_in ? "COUNT-IN" : "FIRST NOTE";
            break;
        case PREF_TRANSPORT:
            labels[0] = "PLAY START"; values[0] = resume_playback ? "RESUME" : "RESTART"; break;
        case PREF_KEYS:
            labels[0] = "PERFECT NOTE";
            values[0] = perfect_note_off == 0u ? "ON" : perfect_note_off == 15u ? "OFF" : "MIXED"; break;
        case PREF_DISPLAY:
            labels[0] = "VISUALIZER"; labels[1] = "DISPLAY QUALITY";
            decimal(level, visual_index(slap_ui.visual) + 1u);
            values[0] = level; values[1] = visual_quality ? "ECO" : "ADAPTIVE"; break;
        case PREF_LEDS: {
            static const char *const profiles[] = {"SOFT", "OFF", "BRIGHTER", "SOFT"};
            labels[0] = "IDLE BUTTONS"; values[0] = profiles[led_style];
            labels[1] = "NORMAL KEYS"; values[1] = led_keys_on_hit ? "OFF" : "MEDIUM";
            break;
        }
        case PREF_DIAG:
            labels[0] = "DIAGNOSTICS"; values[0] = "PRESS SEL"; break;
        }
        cv_begin(240, 48, C_BLACK);
        cv_text(8, 4, &FONT_M, settings_names[settings_section], c);
        cv_text(8, 29, &FONT_XS, "ALGO PAGE / SELECT ITEM / K1 VALUE", C_WHITE);
        cv_blit(0, 48);
        for (unsigned row = 0; row < 4u; ++row) {
            cv_begin(240, 36, C_BLACK);
            if (labels[row]) {
                unsigned focused = settings_item == row;
                cv_rect(5, 2, 230, 32, focused ? c : track_surface[selected]);
                uint16_t ink = focused ? C_BLACK : C_WHITE;
                cv_text(12, 10, &FONT_B, labels[row], ink);
                const char *value = values[row];
                const felucca_font_t *font = text_w(&FONT_B, value) <= 94 ? &FONT_B : &FONT_XS;
                cv_text(229 - text_w(font, value), 10, font, value, ink);
            }
            cv_blit(0, 96 + row * 36);
        }
        return;
    }
    if (slap_ui.page == LIVE_FX) {
        unsigned mode = live_fx_word >> 3 & 15u;
        if (mode == 10u) {
            unsigned speed = slap_fx_brake_speed(live_fx_full_mix ? TRACKS : selected);
            static unsigned platter_phase;
            platter_phase += speed * 2u;
            for (int y = 0; y < 192; y += 48) {
                cv_begin(240, 48, C_BLACK);
                for (int radius = 18; radius <= 69; radius += 10)
                    for (unsigned a = 0; a < 1024u; a += 20u) {
                        int x = 120 + visual_wave(a + 256u, radius);
                        int row = 83 + visual_wave(a, radius) - y;
                        cv_rect(x, row, 2, 2, radius == 68 ? c : track_rule[selected]);
                    }
                cv_line(120, 83 - y, 120 + visual_wave(platter_phase + 256u, 63),
                    83 + visual_wave(platter_phase, 63) - y, C_WHITE);
                if (y == 144) {
                    cv_text(8, 20, &FONT_B, "VINYL STOP", c);
                    decimal(number, speed);
                    cv_text(188, 20, &FONT_B, number, C_WHITE);
                    cv_text(219, 20, &FONT_B, "%", C_WHITE);
                }
                cv_blit(0, 48 + y);
            }
            return;
        }
        for (unsigned row = 0; row < 4; ++row) {
            cv_begin(240, 48, C_BLACK);
            for (unsigned col = 0; col < 3; ++col) {
                unsigned index = row * 3 + col;
                if (index < sizeof(live_fx_names) / sizeof(*live_fx_names)) {
                    int x = 4 + (int)col * 79;
                    cv_rect(x, 6, 73, 30, mode == index + 1 ? c : track_rule[selected]);
                    cv_text(x + 3, 13, &FONT_T, live_fx_names[index], mode == index + 1 ? C_BLACK : C_WHITE);
                }
            }
            if (row == 3)
                cv_text(4, 37, &FONT_T, live_fx_full_mix ? "SEL: FULL MIX" : "SEL: SELECTED TRACK", C_WHITE);
            cv_blit(0, 48 + row * 48);
        }
        return;
    }
    if (slap_ui.page == GLOBAL || slap_ui.page == MIXER || slap_ui.page == RECORD) {
        static const char *const mixer_labels[] = {"LEVEL", "PAN", "SOLO", "DUCK"};
        unsigned values[] = {percent_from_byte((settings >> 16) & 255u), (settings >> 8) & 255u,
            (mixer_solo >> selected) & 1u, selected < DRUMS ? mixer_duck[selected] : 0};
        const char *patch_name = selected == DRUMS ? drum_kit_name(tracks[selected].patch) :
            slap_patches[tracks[selected].patch].name;
        cv_begin(240, 40, background);
        HalfTrace dma = {0};
        unsigned written = half_trace_written, fault_count = half_trace_faults;
        if (slap_ui.page == GLOBAL && global_details == 8 && written) {
            dma = half_trace[(written - 1u) & (HALF_TRACE_SIZE - 1u)];
            if (written != half_trace_written) dma = (HalfTrace){0};
        }
        cv_text(8, 4, &FONT_M, slap_ui.page == GLOBAL ? "SYSTEM" : patch_name, c);
        cv_text(8, 25, slap_ui.page == RECORD ? &FONT_S : &FONT_XS, slap_ui.page == MIXER ? "WHITE KEYS 1-5 MUTE / ARP SOLO" :
            slap_ui.page == RECORD ? (recording ? "SEL END TAKE  /  PLAY STOP" : playing ? "STOP TO ARM A TAKE" : "SEL ARM  /  PLAY TO START") :
            global_details == 1 ? "PEAK BREAKDOWN  /  GLO NEXT" :
            global_details == 2 ? "SERIAL PROFILE  /  TIMING OVERHEAD" :
            global_details == 3 ? "WORKER COUNTERS  /  GLO NEXT" :
            global_details == 4 ? "CORE TIMING  /  GLO NEXT" :
            global_details == 5 ? "CORE 1 STATUS  /  GLO NEXT" :
            global_details == 9 ? "ENGINE SPAN PEAKS  /  TIMING OVERHEAD" :
            global_details == 8 ? (half_trace_frozen ? "DMA FROZEN / K1 PAGE / K2 RESUME" : "DMA TRACE / K1 PAGE / K2 FREEZE") :
            global_details == 7 ? "SPIKE PROFILE  /  TIMING OVERHEAD" :
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
                        !auto_level_off ? "AUTO" : "", C_WHITE);
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
                    if (global_details == 9) {
                        const volatile uint32_t *peaks = col ? remote_engine_peaks : local_engine_peaks;
                        unsigned engine = 0;
                        for (unsigned j = 1; j < TRACE_ENGINES; ++j)
                            if (peaks[j] > peaks[engine]) engine = j;
                        const char *label = strip == 0 ? (col ? "C1 ENGINE" : "C0 ENGINE") :
                            strip == 1 ? "PEAK SPAN US" : "ENGINE ID";
                        cv_text(x, 0, &FONT_XS, label, C_WHITE);
                        if (strip == 0) {
                            const char *name = !peaks[engine] ? "NONE" :
                                engine == TRACE_DRUM_ENGINE ? "DRUMS" : slap_engine_names[engine];
                            cv_text(x, 13, &FONT_XS, name, c);
                        } else {
                            decimal(number, strip == 1 ? peaks[engine] / 24u : engine);
                            cv_text(x, 13, &FONT_M, number, c);
                        }
                        continue;
                    }
                    if (global_details == 8) {
                        static const char *const labels_dma[4][6] = {
                            {"DMA HALF US", "APP CALLBACK US", "ACK US", "NESTED T5 US", "HALF XRUN", "DEADLINE MISS"},
                            {"VOICE BATCH US", "LOCAL VOICE US", "NOTE-ON US", "DISPATCH US", "MAX SPAN US", "BATCH FX US"},
                            {"CORE 1 WORK US", "WORKER WAIT US", "CORE 1 OVERLAP US", "COMPLETED JOBS", "T5 INTERRUPTS", "DMA BUFFER"},
                            {"TRACE ID", "FAULT RECORDS", "OTHER APP US", "OUTSIDE APP US", "TICK STAMP", "HALF XRUN"}
                        };
                        uint32_t accounted = dma.voice + dma.fx_batch;
                        unsigned values[4][6] = {
                            {dma.half / 24u, dma.app / 24u, dma.ack / 24u, dma.timer5 / 24u, dma.xrun, dma.deadline},
                            {dma.voice / 24u, dma.local / 24u, dma.note_on / 24u, dma.dispatch / 24u, dma.span / 24u, dma.fx_batch / 24u},
                            {dma.remote / 24u, dma.wait / 24u, dma.overlap / 24u, dma.jobs, dma.nested, dma.buffer},
                            {written, fault_count, dma.app > accounted ? (dma.app - accounted) / 24u : 0u,
                             dma.half > dma.app ? (dma.half - dma.app) / 24u : 0u, dma.observed, dma.xrun}
                        };
                        cv_text(x, 0, &FONT_XS, labels_dma[dma_view][stage], C_WHITE);
                        decimal(number, values[dma_view][stage]);
                        cv_text(x, 13, text_w(&FONT_M, number) > 110 ? &FONT_S : &FONT_M, number, c);
                        continue;
                    }
                    if (global_details == 7) {
                        static const char *const labels_spike[] = {
                            "NOTE-ON US", "LOCAL VOICE US", "VOICE DISPATCH US",
                            "MAX VOICE SPAN US", "BATCH FX US", "CALLBACK US"
                        };
                        unsigned ticks = stage < SPIKE_COUNT ? spike_values[stage] : spike_callback;
                        cv_text(x, 0, &FONT_XS, labels_spike[stage], C_WHITE);
                        decimal(number, ticks / 24u);
                        cv_text(x, 13, text_w(&FONT_M, number) > 110 ? &FONT_S : &FONT_M, number, c);
                        continue;
                    }
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
                if (k == 1) {
                    int pan = pan_percent(values[k]);
                    if (pan) {
                        number[0] = pan < 0 ? 'L' : 'R';
                        decimal(number + 1, (unsigned)(pan < 0 ? -pan : pan));
                    } else decimal(number, 0);
                } else decimal(number, values[k]);
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
            const char *record_action = recording ? "ARP CANCEL TAKE" :
                clear_mode == 1 ? "OCT- AGAIN: CLEAR PATTERN" :
                clear_mode == 2 ? "OCT+ AGAIN: CLEAR TRACK" : pattern_action ? "EDIT QUEUED" :
                take_undo[undo_latest].ready ? "ARP UNDO  /  OCT- PATTERN  /  OCT+ TRACK" : "OCT- PATTERN  /  OCT+ TRACK";
            cv_text(8, 30, &FONT_T, record_action, c);
        }
        cv_blit(0, 196);
        return;
    }
    unsigned home = slap_ui.page == HOME;
    cv_begin(240, home ? 30 : 32, background);
    const SlapPatch *patch = &slap_patches[tracks[selected].patch];
    static const char *const drum_names[] = {"KICK","SNARE","CLOSED HAT","RIM","CLAP","CRASH",
        "SUB KICK","ALT SNARE","OPEN HAT","PEDAL HAT","LOW TOM","MID TOM",
        "HIGH TOM","RIDE","SHAKER","PERC FX"};
    static const char *const melodic_names[] = {"NOTE A", "NOTE B", "NOTE C", "NOTE D"};
    const char *lane_name = selected == DRUMS ?
        (slap_ui.lane ? drum_names[slap_ui.lane-1] : "DRUM NOTES") :
        melodic_names[slap_ui.lane];
    const char *name = slap_ui.page == SEQ ? lane_name : slap_ui.page == SAVE ? "SONG SLOTS" : slap_ui.page == FX ? slap_fx_pages[slap_ui.fx_page] :
                       slap_ui.page == HARMONY ?
                       ((perfect_note_off & (1u << selected)) ? "PERFECT NOTE OFF" : "PERFECT NOTE ON") : selected == DRUMS ? drum_kit_name(tracks[DRUMS].patch) : patch->name;
    unsigned drum_sequence = selected == DRUMS && slap_ui.page == SEQ;
    const felucca_font_t *name_font = drum_sequence || text_w(&FONT_M, name) > 150 ? &FONT_B : &FONT_M;
    const char *transport = rec_countdown ? "COUNT" : rec_running ? "REC STEP" :
        recording ? "REC READY" : playing ? "STEP" : resume_playback && position ? "PAUSED" : "STEP";
    if (home) {
        for (unsigned t = 0; t < TRACKS; ++t) {
            int x = 4 + (int)t * 47;
            if (t == selected) cv_rect(x - 1, 0, 46, 10, C_WHITE);
            cv_rect(x, 0, 44, 9, track_rule[t]);
            cv_rect(x + 2, 3, (int)(note_activity[t] * 40u / 80u), 3, track_color[t]);
        }
        cv_text(8, 14, &FONT_XS, "1/16", c);
        cv_text(65, 14, &FONT_XS, "BAR", c);
        decimal(number, position / 16u % 999u + 1u);
        cv_text(94, 14, &FONT_XS, number, C_WHITE);
        cv_text(154, 14, &FONT_XS, transport, c);
        decimal(number, rec_countdown ? rec_countdown : play_step + 1u);
        cv_text(237 - text_w(&FONT_XS, number), 14, &FONT_XS, number, C_WHITE);
    } else {
        cv_text(8, 1, name_font, name, C_WHITE);
        if (slap_ui.page == SEQ) cv_text(105, 17, &FONT_T, "1/16", c);
        if (drum_sequence) cv_text(8, 17, &FONT_T, drum_step_locked(slap_ui.lane, slap_ui.step) ?
            "LOCKED" : "SEL+GLO LOCK", c);
        decimal(number, position / 16u % 999u + 1u);
        cv_text(163, 5, &FONT_XS, "BAR", c);
        cv_text(190, 5, &FONT_XS, number, C_WHITE);
        cv_text(154, 19, &FONT_XS, transport, c);
        decimal(number, rec_countdown ? rec_countdown : (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) + 1u);
        cv_text(237 - text_w(&FONT_XS, number), 19, &FONT_XS, number, C_WHITE);
    }
    cv_blit(0, home ? 50 : 48);
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
        if (edit_page && selected < DRUMS && (patch->engine == 0u || patch->engine == 2u)) {
            uint32_t config = track_timbre[selected];
            if (patch->engine == 0u && edit_page == 4u) {
                cv_text(8, 5, &FONT_M, "5 FEEDBACK", c);
                cv_text(8, 27, &FONT_XS, "K1 FEEDBACK / ALGO PAGE", C_WHITE);
                cv_blit(0, 80);
                cv_begin(240, 40, background);
                unsigned feedback = track_fm_envelopes[selected][6];
                if (feedback) decimal(number, feedback - 1u);
                cv_text(8, 2, &FONT_L, feedback ? number : "AUTO", C_WHITE);
                cv_rect(8, 35, 14 * (int)feedback, 3, c);
                cv_blit(0, 120);
                for (int y = 160; y < 240; y += 40) {
                    cv_begin(240, 40, background);
                    cv_blit(0, y);
                }
                return;
            }
            if (patch->engine == 0u && edit_page == 3u) {
                unsigned settings = track_fm_envelopes[selected][timbre_operator];
                unsigned feedback = track_fm_envelopes[selected][6];
                if (!settings) settings = 0x14561u;
                cv_text(8, 2, &FONT_B, "OP ENVELOPE", c);
                decimal(number, timbre_operator + 1u);
                cv_text(160, 2, &FONT_B, number, C_WHITE);
                cv_text(8, 21, &FONT_XS, "K1 A   K2 D   K3 S   K4 R", C_WHITE);
                cv_blit(0, 80);
                cv_begin(240, 48, background);
                int attack = 15 + (int)(settings & 15u) * 3;
                int decay = attack + 10 + (int)(settings >> 4 & 15u) * 3;
                int sustain = 40 - (int)(settings >> 8 & 15u) * 2;
                int release = 218 - (int)(settings >> 12 & 15u) * 3;
                for (int y = 8; y <= 40; y += 8) cv_rect(8, y, 218, 1, track_surface[selected]);
                cv_line(8, 40, attack, 6, c);
                cv_line(attack, 6, decay, sustain, c);
                cv_line(decay, sustain, release, sustain, c);
                cv_line(release, sustain, 225, 40, c);
                cv_blit(0, 120);
                cv_begin(240, 32, background);
                cv_text(8, 4, &FONT_B, "FEEDBACK", c);
                if (feedback) decimal(number, feedback - 1u);
                cv_text(139, 4, &FONT_B, feedback ? number : "AUTO", C_WHITE);
                cv_blit(0, 168);
                cv_begin(240, 40, background);
                cv_text(8, 2, &FONT_XS, "OCT+ / K4 FEEDBACK", C_WHITE);
                cv_text(8, 20, &FONT_XS, "ALGO PAGE / SELECT TRACK", C_WHITE);
                cv_blit(0, 200);
                return;
            }
            if (patch->engine == 0u) {
                unsigned factory = !config;
                unsigned algorithm = config ? config >> 30 : patch->motion < 44u ? 0u : patch->motion < 87u ? 1u : 2u;
                cv_text(8, 2, &FONT_B, edit_page == 1u ? "2 FM6 ROUTING" : "3 FM6 OPERATORS", c);
                decimal(number, timbre_operator + 1u);
                cv_text(148, 2, &FONT_B, number, C_WHITE);
                cv_text(8, 22, &FONT_XS, "K1 OP  K2 RATIO  K3 LEVEL  K4 ALG", C_WHITE);
                cv_blit(0, 80);
                cv_begin(240, 40, background);
                draw_fm_routing(selected, algorithm, config, c);
                cv_blit(0, 120);
                cv_begin(240, 40, background);
                unsigned ratio = (config >> (timbre_operator * 3u) & 7u) + 1u;
                decimal(number, ratio / 2u);
                unsigned end = 0;
                while (number[end]) ++end;
                number[end++] = '.';
                number[end++] = ratio & 1u ? '5' : '0';
                number[end] = 0;
                cv_text(8, 2, &FONT_XS, "RATIO", c);
                cv_text(55, 2, &FONT_XS, factory ? "AUTO" : number, C_WHITE);
                cv_text(99, 2, &FONT_XS, "LEVEL", c);
                decimal(number, factory ? 3u : config >> (18u + timbre_operator * 2u) & 3u);
                cv_text(145, 2, &FONT_XS, number, C_WHITE);
                static const char *const routing[] = {"SERIAL", "BRANCH", "LAYER", "SERIAL"};
                cv_text(8, 21, &FONT_XS, routing[algorithm], C_WHITE);
            } else {
                cv_text(8, 2, &FONT_B, "WAVETABLE", c);
                cv_text(8, 22, &FONT_XS, "K1 MORPH  K2 MOVEMENT", C_WHITE);
                cv_blit(0, 80);
                cv_begin(240, 40, background);
                unsigned morph = config ? config & 127u : patch->color;
                cv_rect(8, 16, 222, 1, track_rule[selected]);
                unsigned movement = config ? config >> 8 & 127u : patch->motion;
                const volatile SlapSynth *live = graph_voice(selected, 2);
                float phase = live ? live->slow : (plat_ms() % 10000u) *
                    ((0.19f + movement * (2.2f / 127.0f)) / 1000.0f);
                phase -= fm_floorf(phase);
                float inc = live ? live->inc : 0.0f;
                unsigned band = inc < 0.059f ? 0u : inc < 0.137f ? 1u : 2u;
                float preview = slap_wave_scan(morph * (1.0f / 127.0f),
                    movement * (1.0f / 127.0f), phase);
                int previous = 16;
                for (unsigned point = 0; point <= 112u; ++point) {
                    int y = 16 - (int)(slap_wave_preview(preview, band, point * 128u / 112u) * 14.0f);
                    if (point) cv_line(8 + (int)(point - 1u) * 2, previous, 8 + (int)point * 2, y, c);
                    previous = y;
                }
                cv_rect(8, 37, 222, 2, track_surface[selected]);
                cv_rect(8, 37, (int)(preview * 74.0f), 2, c);
                cv_blit(0, 120);
                cv_begin(240, 40, background);
                cv_rect(8, 5, 222, 10, track_surface[selected]);
                cv_rect(8, 5, (int)(movement * 222u / 127u), 10, c);
                cv_text(8, 22, &FONT_XS, "SCAN MOVEMENT", C_WHITE);
            }
            cv_blit(0, 160);
            cv_begin(240, 40, background);
            cv_text(8, 15, &FONT_XS, patch->engine == 0u ? "ALGO PAGE / SELECT TRACK" : "PRESETS PATCH / HOME BACK", C_WHITE);
            cv_blit(0, 200);
            return;
        }
        if (edit_page && selected < DRUMS) {
            cv_text(8, 2, &FONT_B, slap_engine_names[patch->engine], c);
            cv_text(8, 22, &FONT_XS, "K1 FILTER  K2 DRIVE  ENV  LFO", C_WHITE);
            cv_blit(0, 80);
            for (int band = 0; band < 3; ++band) {
                cv_begin(240, 40, background);
                cv_rect(8, 60 - band * 40, 224, 1, track_rule[selected]);
                for (unsigned i = 1; i < SLAP_FX_PLOT; ++i)
                    cv_line(5 + (int)i * 3, 60 - slap_fx_trace(selected, i - 1u) - band * 40,
                        8 + (int)i * 3, 60 - slap_fx_trace(selected, i) - band * 40, c);
                cv_blit(0, 120 + band * 40);
            }
            return;
        }
        if (edit_page && selected == DRUMS) {
            cv_text(8, 2, &FONT_M, "2 KIT VOICES", c);
            cv_text(8, 27, &FONT_XS, "PRESETS KIT / ALGO BROWSER", C_WHITE);
            cv_blit(0, 80);
            static const char *const kit_voice[] = {
                "KICK","SNARE","HIHAT","RIM","CLAP","CRASH","SUB","ALT SN",
                "OPEN","PEDAL","LOW TOM","MID TOM","HI TOM","RIDE","SHAKER","PERC"
            };
            for (int row = 0; row < 4; ++row) {
                cv_begin(240, 30, background);
                for (int col = 0; col < 4; ++col) {
                    int x = 5 + col * 59;
                    cv_rect(x, 3, 55, 25, track_surface[selected]);
                    cv_text(x + 3, 8, &FONT_XS, kit_voice[row * 4 + col], C_WHITE);
                }
                cv_blit(0, 120 + row * 30);
            }
            return;
        }
        int drums = selected == DRUMS;
        unsigned current = tracks[selected].patch;
        const char *category = drums ? kit_groups[patch_group[selected]] : patch_groups[patch_group[selected]];
        cv_text(8, 1, &FONT_B, category, c);
        cv_text(8, 22, &FONT_XS, "K1 GROUP   K2 PATCH   ALGO PAGE", C_WHITE);
        cv_blit(0, 80);
        unsigned choices[] = {browse_patch(selected, current, -1), current, browse_patch(selected, current, 1)};
        for (unsigned row = 0; row < 3u; ++row) {
            unsigned id = choices[row];
            cv_begin(240, 30, background);
            if (row == 1u || id != current) {
                uint16_t ink = row == 1u ? C_BLACK : C_WHITE;
                cv_rect(7, 2, 226, 26, row == 1u ? c : track_surface[selected]);
                decimal(number, id + 1u);
                cv_text(12, 5, &FONT_XS, number, ink);
                const char *name = drums ? drum_kit_name(id) : slap_patches[id].name;
                cv_text(43, 4, text_w(&FONT_B, name) <= 180 ? &FONT_B : &FONT_S, name, ink);
            }
            cv_blit(0, 120 + row * 30);
        }
        cv_begin(240, 30, background);
        cv_text(8, 7, &FONT_B, drums ? "DRUM KIT" : slap_engine_names[patch->engine], C_WHITE);
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
                gen_queued ? "QUEUED FOR NEXT BAR" : gen_applied_at && plat_ms() - gen_applied_at < 1000u ?
                "DRUM PATTERN APPLIED" : playing && length > LIVE_LOOP_MAX ? "STOP FOR LEGACY LOOP" :
                gen_enabled ? "SEL GENERATE   /   ARP OFF" : "GEN OFF   /   ARP ON", C_WHITE);
        cv_text(178, 6, &FONT_XS, "SEQ BACK", c);
        cv_blit(0, 80);
        for (unsigned strip = 0; strip < 2; ++strip) {
            cv_begin(240, 36, background);
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned step = 0; step < 16; ++step) {
                    unsigned at = bank + step;
                    unsigned hit = at < length && (drum_cell_get(strip * 3 + row, at) & ACTIVE);
                    cv_rect(8 + step * 14, row * 11, 12, 8,
                            hit ? c : RGB(12,54,36));
                    if (playing && at == play_step) cv_rect(8 + step * 14, row * 11, 2, 8, C_WHITE);
                }
            cv_blit(0, 120 + strip * 36);
        }
        cv_begin(240, 48, background);
        static const char *const label[] = {"STYLE", "HIT %", "BARS", "VAR %"};
        unsigned value[] = {gen_style + 1, gen_hit, gen_bars, gen_var};
        for (unsigned k = 0; k < 4; ++k) {
            unsigned groove_edit = k == 2u && (slap_ui.buttons & (1u << B_OCTUP));
            cv_text(8 + k * 58, 2, &FONT_XS, groove_edit ? "GROOVE" : label[k], c);
            decimal(number, groove_edit ? gen_groove : value[k]);
            cv_text(8 + k * 58, 17, &FONT_M, number, C_WHITE);
        }
        cv_blit(0, 192);
        return;
    }
    uint32_t env = tracks[selected].envelope;
    uint32_t modulation = slap_ui.page == LFO ? tracks[selected].lfo : tracks[selected].harmony;
    GraphState graph = {
        .page = (unsigned)slap_ui.page, .track = selected, .step = (unsigned)slap_ui.step,
        .bank = bank, .mix = settings, .tempo = tempo | swing << 8, .positions = position,
        .revision = pattern_revision[selected], .envelope = env, .modulation = modulation,
        .split = split_keys, .tuning = perfect_note_off,
    };
    graph_valid = !playing && !recording && (slap_ui.page == HOME || slap_ui.page == SEQ ||
        slap_ui.page == ENV || slap_ui.page == HARMONY);
    unsigned draw_graph = !graph_valid || !had_graph ||
        __builtin_memcmp(&graph, &previous_graph, sizeof graph);
    if (graph_valid) previous_graph = graph;
    static uint32_t filter_signature = UINT32_MAX;
    static uint8_t filter_curve[SLAP_FX_PLOT];
    if (slap_ui.page == FX && slap_ui.fx_page == 1) {
        uint32_t signature = slap_fx_config(selected, 1);
        if (filter_signature != signature) {
            filter_signature = signature;
            for (unsigned i = 0; i < SLAP_FX_PLOT; ++i)
                filter_curve[i] = (uint8_t)slap_fx_response(selected, i);
        }
    }
    for (int strip = 0; draw_graph && strip < 3; ++strip) {
        cv_begin(240, 36, background);
        if (slap_ui.page == FX) {
            const unsigned fpage = (unsigned)slap_ui.fx_page;
            for (int line = 17; line < 108; line += 18)
                cv_rect(8, line - strip * 36, 224, 1, line == 53 ? track_rule[selected] : track_surface[selected]);
            if (fpage == 1) {
                for (unsigned octave = 0; octave < 8u; ++octave)
                    cv_rect(8 + (int)octave * 28, 0, 1, 36, track_surface[selected]);
                if (strip == 2) {
                    cv_text(9, 24, &FONT_T, "LOW", C_WHITE);
                    cv_text(98, 24, &FONT_T, "FREQUENCY", C_WHITE);
                    cv_text(207, 24, &FONT_T, "HIGH", C_WHITE);
                }
            }
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
                    int y = fpage == 1 ? filter_curve[i] : 53 + slap_fx_trace(selected, i);
                    int next = fpage == 1 ? filter_curve[i + (i + 1u < SLAP_FX_PLOT)] :
                        53 + slap_fx_trace(selected, i + (i + 1u < SLAP_FX_PLOT));
                    int x = 8 + (int)i * 3;
                    if (fpage == 1) {
                        int low = y < 61 ? y : 61;
                        int high = y < 61 ? 61 : y;
                        cv_rect(x, low - strip * 36, 3, high - low, track_surface[selected]);
                    }
                    cv_line(x, y - strip * 36, x + 3, next - strip * 36, c);
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
            unsigned rate = (modulation >> 8) & 255u;
            float hz = 0.1f * fm_exp2f(rate * (8.643856f / 127.0f));
            static float sweep;
            static unsigned last_lfo;
            unsigned now = plat_ms(), elapsed = now - last_lfo;
            if (strip == 0) {
                if (elapsed < 200u) sweep += hz * elapsed * 0.001f;
                sweep -= fm_floorf(sweep);
                last_lfo = now;
            }
            cv_rect(8, 51 - strip * 36, 224, 1, track_rule[selected]);
            int previous = 51;
            for (unsigned i = 0; i < 224u; ++i) {
                float cycles = phase + i * (1.0f / 112.0f);
                float at = cycles - fm_floorf(cycles);
                float held = (float)((unsigned)(cycles * 7.0f) * 173u & 255u) / 128.0f - 1.0f;
                int y = 51 - (wave ? (int)(lfo_shape(wave, at, held) * 31.0f) : 0);
                if (i) cv_line(7 + (int)i, previous - strip * 36, 8 + (int)i, y - strip * 36, c);
                previous = y;
            }
            if (wave) {
                int marker = 8 + (int)(sweep * 112.0f);
                cv_rect(marker, 0, 1, 36, C_WHITE);
                cv_rect(marker + 112, 0, 1, 36, C_WHITE);
            }
        } else if (slap_ui.page == HARMONY) {
            unsigned root = modulation & 255u, scale = (modulation >> 8) & 255u;
            if (strip == 0) cv_text(8, 4, &FONT_XS, harmony_scales[scale], C_WHITE);
            if (strip == 2) {
                cv_text(8, 3, &FONT_XS, "OCT- PERFECT NOTE / OCT+ CHORD", C_WHITE);
                cv_text(8, 17, &FONT_XS, "K2 SPLIT KEYS / SEL+K2 SCALE", C_WHITE);
            }
            for (unsigned i = 0; i < 12; ++i) {
                int x = 8 + (int)i * 19;
                int active = scale_tones[scale] & (1u << ((i + 12u - root) % 12u));
                if (strip != 2) cv_rect(x, 22 - strip*36, 17, 50, active ? c : track_rule[selected]);
                if (strip == 1) cv_text(x + 2, 18, &FONT_XS, harmony_keys[i], active ? C_BLACK : C_WHITE);
            }
        } else if (slap_ui.page == ENV) {
            const int a = env & 127u, d = (env >> 8) & 127u, r = (env >> 24) & 127u;
            const int attack = 7 + a * 56 / 127, decay = attack + 7 + d * 56 / 127;
            const int level = 101 - ((env >> 16) & 127u) * 94 / 127;
            const int release = 217 - r * 58 / 127;
            for (int y = 11; y < 108; y += 18) cv_rect(8, y - strip * 36, 224, 1, track_surface[selected]);
            for (int x = 0; x < 224; ++x) {
                float y;
                if (x < attack) {
                    float phase = (float)x / attack;
                    y = 101.0f - 94.0f * phase * (2.0f - phase);
                } else if (x < decay) {
                    float phase = 1.0f - (float)(x - attack) / (decay - attack);
                    y = level - (level - 7.0f) * phase * phase;
                } else if (x < release) y = level;
                else {
                    float phase = (float)(224 - x) / (224 - release);
                    y = 101.0f - (101.0f - level) * phase * phase;
                }
                int gy = (int)y - strip * 36;
                cv_rect(8 + x, gy, 1, 101 - (int)y, track_surface[selected]);
                cv_rect(8 + x, gy, 1, 2, c);
            }
            const int markers[] = {attack, decay, release};
            for (unsigned point = 0; point < 3u; ++point) {
                int y = (point ? level : 7) - strip * 36;
                cv_rect(6 + markers[point], y - 2, 5, 5, C_WHITE);
            }
        } else {
            int first_step = scroll ? (int)play_step - 3 : (int)bank;
            int left = 8 - (scroll ? (int)substep : 0);
            int columns = scroll ? 19 : clamp((int)length - first_step, 0, 16);
            unsigned grid_step = (unsigned)((first_step + (int)length) % (int)length);
            for (int column = 0, x = left; column < columns; ++column, x += 14) {
                unsigned index = grid_step;
                if (++grid_step == length) grid_step = 0;
                cv_rect(x, 0, 12, 36, index % 4 == 0 ? track_rule[selected] : track_surface[selected]);
                if (!(index % 16u)) cv_rect(x, 0, 2, 36, c);
                else if (!(index % 4u)) cv_rect(x, 0, 1, 36, track_color[selected]);

                if (!scroll && index == play_step && playing) cv_rect(x, 0, 2, 36, C_WHITE);
                if (slap_ui.page == SEQ && index == (unsigned)slap_ui.step) cv_rect(x + 9, 0, 3, 36, c);
            }
            unsigned step_samples = RATE * 15u / tempo;
            int graph_end = scroll ? 240 : clamp(left + ((int)length - first_step) * 14, 0, 240);
            const int limited = selected == DRUMS && length > 23u;
            const unsigned scanned = limited ? 23u : length;
            int start = limited ? first_step - 3 : 0;
            if (limited && !scroll && start < 0) start = 0;
            unsigned index = (unsigned)((start + (int)length) % (int)length);
            const unsigned lanes = lane_count(selected);
            int period = (int)length * 14;
            for (unsigned count = 0; count < scanned; ++count) {
                for (unsigned lane = 0; lane < lanes; ++lane) {
                    unsigned row = pattern_row(selected, lane);
                    uint32_t word = event_get(selected, index, lane);
                    if (!(word & ACTIVE)) continue;
                    int y = (selected == DRUMS ? 8 + (int)(lane ? lane - 1u : drum_family(word & NOTES)) * 6 :
                        100 - (int)((word & NOTES) % 24u) * 4) - strip * 36;
                    if (y <= -5 || y >= 36) continue;
                    unsigned duration = selected < DRUMS ? melodic_duration[selected][lane][index] : 0;
                    unsigned timing = note_timing(row, index, word);
                    unsigned swing_offset = step_samples * swing / 200u;
                    unsigned span = index & 1u ? step_samples - swing_offset : step_samples + swing_offset;
                    unsigned gate = duration ? duration * NOTE_DURATION_UNIT : span * ((word >> 16) & 255u) / 192u;
                    int width = held_note_width(gate, index, step_samples, span * timing / 16u);
                    if (width < 2) width = 2;
                    int offset = (int)(timing * 14u / 16u);
                    int x = left + ((int)index - first_step) * 14 + offset;
                    if (width >= period) {
                        cv_rect(0, y, graph_end, 5, c);
                        if (scroll && note_sounding(row, index)) cv_rect(0, y, 240, 1, C_WHITE);
                        continue;
                    }
                    // Start at the oldest repetition that still reaches the graph.
                    while (x + width <= 0) x += period;
                    while (x - period + width > 0) x -= period;
                    for (; x < graph_end; x += period) {
                        int end = x + width;
                        int from = x < 0 ? 0 : x;
                        int to = end > graph_end ? graph_end : end;
                        if (to <= from) continue;
                        cv_rect(from, y, to - from, 5, c);
                        if (scroll && x <= 50 && end > 50 && note_sounding(row, index)) {
                            cv_rect(from, y, to - from, 1, C_WHITE);
                            cv_rect(from, y + 4, to - from, 1, C_WHITE);
                            cv_rect(50, y, 2, 5, C_WHITE);
                        }
                    }
                }
                if (++index == length) index = 0;
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
    uint32_t word = event_get(selected, slap_ui.step, slap_ui.page == SEQ ? slap_ui.lane : 0);
    unsigned values[] = { (settings >> 8) & 255u, length, percent_from_byte((settings >> 16) & 255u), swing };
    unsigned melodic_edit = slap_ui.page == SEQ && selected < DRUMS;
    unsigned detail_edit = slap_ui.buttons & (1u << B_SEL);
    if (slap_ui.page == SEQ) {
        values[0] = word & NOTES;
        values[1] = (word >> 8) & 127u;
        values[2] = (word >> 16) & 255u;
        values[3] = slap_ui.buttons & (1u << B_SEL) ? note_timing(pattern_row(selected, slap_ui.lane), slap_ui.step, word) : (word >> 24) & 127u;
        if (melodic_edit && detail_edit) {
            unsigned quarter = (RATE * 15u / tempo) / (NOTE_DURATION_UNIT * 4u);
            values[2] = (melodic_duration[selected][slap_ui.lane][slap_ui.step] + quarter / 2u) / quarter;
            values[0] = (word >> 24) & 127u;
            values[1] = (word >> 16) & 255u;
        } else if (melodic_edit) {
            unsigned unit = (RATE * 15u / tempo) / NOTE_DURATION_UNIT;
            values[3] = (melodic_duration[selected][slap_ui.lane][slap_ui.step] + unit / 2u) / unit;
        }
    }
    if (slap_ui.page == ENV)
        for (unsigned k = 0; k < 4; ++k) values[k] = (env >> (k*8u)) & 255u;
    for (unsigned k = 0; k < 4; ++k) {
        int x = 8 + (int)k * 58;
        const char *label = slap_ui.page == FX ? slap_fx_labels[slap_ui.fx_page][k] :
                            slap_ui.page == ARP ? arp_labels[k] : slap_ui.page == LFO ? lfo_labels[k] : slap_ui.page == HARMONY ? harmony_labels[k] :
                            slap_ui.page == SEQ ? (melodic_edit ? (detail_edit ? melodic_detail_labels[k] : melodic_edit_labels[k]) :
                                detail_edit && k == 3 ? "OFFSET" : edit_labels[k]) : slap_ui.page == ENV ? env_labels[k] : labels[k];
        if (slap_ui.page == FX && slap_ui.fx_page == 4 && k == 1) label = "FX BLEND";
        if (!label) continue;
        cv_text(x, slap_ui.page == FX && slap_ui.fx_page == 4 ? 16 : 2, &FONT_XS, label, c);
        if (slap_ui.page == SEQ && k == 0 && !detail_edit) {
            if (selected < DRUMS) {
                const char *pitch = harmony_keys[values[0] % 12u];
                char note[8];
                unsigned n = 0;
                while (pitch[n]) { note[n] = pitch[n]; ++n; }
                int octave = (int)(values[0] / 12u) - 1;
                if (octave < 0) { note[n++] = '-'; octave = -octave; }
                note[n++] = (char)('0' + octave);
                note[n] = 0;
                cv_text(x, 16, &FONT_M, word & ACTIVE ? note : "REST", C_WHITE);
            } else {
                static const char *const hits[] = {"KICK","SNARE","CH HAT","RIM","CLAP","CRASH","SUB",
                    "ALT SN","OP HAT","PEDAL","LOW TM","MID TM","HI TOM","RIDE","SHAKE","PERC"};
                cv_text(x, 16, &FONT_S, word & ACTIVE ? hits[drum_family(values[0])] : "REST", C_WHITE);
            }
            continue;
        }
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
                k == 0 ? harmony_keys[v] : k == 1 ? ((split_keys & (1u << selected)) ? "ON" : "OFF") : k == 2 ? harmony_chords[v] : !v ? "OFF" : 0;
            if (tag) { cv_text(x, 15, &FONT_XS, tag, C_WHITE); continue; }
            values[k] = slap_ui.page == HARMONY ? 99u + v : v;
        } else if (slap_ui.page == FX) {
            unsigned page = (unsigned)slap_ui.fx_page;
            values[k] = page == 4 && k == 1 ? percent_from_byte(mixer_proc[selected]) :
                slap_fx_value(selected, page, k);
            if (page == 3 && k == 0) {
                cv_text(x, 18, &FONT_XS, slap_insert_names[values[k]], C_WHITE);
                continue;
            }
            if (page == 2 && k == 2) {
                static const char *const divisions[] = {"1/8", "1/16", "1/32", "1/64"};
                cv_text(x, 18, &FONT_XS, divisions[values[k]], C_WHITE);
                continue;
            }
            if (page == 2 && k == 3) {
                decimal(number, (values[k] * 100u + 63u) / 127u);
                unsigned len = 0;
                while (number[len]) ++len;
                number[len++] = '%';
                number[len] = 0;
                cv_text(x, 15, &FONT_B, number, C_WHITE);
                continue;
            }
            if (page == 2 && k == 0) {
                static const char *const modes[] = {"OFF", "GATE", "STUTTER"};
                cv_text(x, 18, &FONT_XS, modes[values[k]], C_WHITE);
                continue;
            }
        }
        if (melodic_edit && !detail_edit && k == 2) {
            cv_text(x, 16, &FONT_B, word & ACTIVE ? "PLAY" : "REST", C_WHITE);
            continue;
        }
        if (slap_ui.page == HOME && k == 0 && pan_percent(values[k])) {
            int pan = pan_percent(values[k]);
            number[0] = pan < 0 ? 'L' : 'R';
            decimal(number + 1, (unsigned)(pan < 0 ? -pan : pan));
        } else decimal(number, slap_ui.page == HOME && k == 0 ? 0 : values[k]);
        cv_text(x, slap_ui.page == FX && slap_ui.fx_page == 4 ? 26 : 15, &FONT_M, number, C_WHITE);
    }
    if (slap_ui.page == SEQ && (slap_ui.buttons & (1u << B_SEL))) {
        const char *modes[] = {"NORMAL", "RATCHET", "FILL"};
        unsigned row = pattern_row(selected, slap_ui.lane);
        cv_text(8, 32, &FONT_T, "EDIT COPY / LFO CUT / ENV PASTE / OCT- REST", C_WHITE);
        cv_text(8, 42, &FONT_T, "OCT+ MODE", c);
        cv_text(74, 42, &FONT_T, modes[timing_group_get(row, slap_ui.step) / TIMING_GROUPS], C_WHITE);
    }
    if (melodic_edit && !detail_edit)
        cv_text(8, 37, &FONT_T, "K3 LEFT REST / RIGHT PLAY   SEL FOR DETAIL", C_WHITE);
    if (slap_ui.page == HOME) {
        const char *hint = clear_mode == 1 ? "REC+OCT+ AGAIN: CLEAR LOOP" :
            clear_mode == 2 ? "REC+SEL AGAIN: CLEAR TRACK" :
            pattern_action ? "EDIT QUEUED" : "REC+OCT- UNDO   REC+ARP CANCEL";
        cv_text(8, 37, &FONT_T, hint, C_WHITE);
    }
    cv_blit(0, 188);
}

void app_frame(void)
{
    app_input_only();
    if (slap_ui.page == VISUAL) update_spectrum();
    else analysis_read = analysis_write;
    uint32_t now = plat_ms();
    sample_battery(now);
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
        if (under_pressure || visual_quality) frame_interval = 85u;
    } else {
        last_xruns = plat_xruns();
    }
    if (now - slap_ui.frame_at >= frame_interval) {
        slap_ui.frame_at = now;
        static uint32_t displayed_notes[TRACKS];
        for (unsigned t = 0; t < TRACKS; ++t) {
            uint32_t notes = visual_notes[t];
            unsigned hits = notes - displayed_notes[t];
            displayed_notes[t] = notes;
            note_activity[t] = (unsigned)clamp((int)(note_activity[t] * 3u / 4u + (hits > 8u ? 8u : hits) * 6u), 0, 80);
        }
        if (slap_ui.booting && now - slap_ui.boot_at < BOOT_MS) {
            header_valid = graph_valid = 0;
            draw_boot(now - slap_ui.boot_at);
        } else {
            slap_ui.booting = 0;
            if (slap_ui.page == VISUAL) {
                header_valid = graph_valid = 0;
                visualizer_draw((unsigned)slap_ui.visual, now / 4u, 0);
            } else if (slap_ui.page == HOME) {
                header_valid = graph_valid = 0;
                draw_home_sequences();
            } else draw_panel();
            if (slap_ui.page != HOME) home_idle_valid = 0;
            if (slap_ui.page != PATCH || slap_ui.booting) browser_valid = 0;
        }
    }
}

void app_ui_init(void)
{
    header_valid = graph_valid = home_idle_valid = browser_valid = 0;
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
#if defined(PLAT_HOST)
    else if (name_matches(name, "peak_voices")) *result = (int)host_peak_voices;
    else if (name_matches(name, "voice_steals")) *result = (int)voice_steals;
#endif
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    else if (name_matches(name, "profile_peak_us")) *result = (int)(serial_peak / 24u);
    else if (name_matches(name, "profile_fx_batch_us")) *result = (int)(host_serial_fx_batch / 24u);
    else if (name_matches(name, "profile_input_us")) *result = (int)(serial_at_peak[PROFILE_INPUT] / 24u);
    else if (name_matches(name, "profile_events_us")) *result = (int)(serial_at_peak[PROFILE_EVENTS] / 24u);
    else if (name_matches(name, "profile_voices_us")) *result = (int)(serial_at_peak[PROFILE_VOICES] / 24u);
    else if (name_matches(name, "profile_serial_fx_us")) *result = (int)(serial_at_peak[PROFILE_FX] / 24u);
    else if (name_matches(name, "profile_mix_us")) *result = (int)(serial_at_peak[PROFILE_MIX] / 24u);
    else if (name_matches(name, "profile_output_us")) *result = (int)(serial_at_peak[PROFILE_OUTPUT] / 24u);
#endif
    else if (name_matches(name, "timbre_dirty"))
        *result = slap_ui.track < DRUMS && track_timbre[slap_ui.track] != 0;
    else if (name_matches(name, "tempo")) *result = (int)tempo;
    else if (name_matches(name, "boot")) *result = (int)slap_ui.booting;
    else if (name_matches(name, "recording")) *result = (int)recording;
    else if (name_matches(name, "rec_rejected")) *result = (int)rec_rejected;
    else if (name_matches(name, "note_ms"))
        *result = slap_ui.track < DRUMS && (unsigned)slap_ui.lane < MELODIC_LANES ?
            (int)(melodic_duration[slap_ui.track][slap_ui.lane][slap_ui.step] * NOTE_DURATION_UNIT * 10u / (RATE / 100u)) : 0;
    else if (name_matches(name, "level")) *result = (int)percent_from_byte((tracks[slap_ui.track].mix >> 16) & 255u);
    else if (name_matches(name, "pan_percent")) *result = pan_percent((tracks[slap_ui.track].mix >> 8) & 255u);
    else if (name_matches(name, "duck")) *result = slap_ui.track < DRUMS ? mixer_duck[slap_ui.track] : 0;
    else if (name_matches(name, "auto_mix")) *result = (int)auto_mix;
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
    else if (name_matches(name, "perfect_note")) *result = !(perfect_note_off & (1u << slap_ui.track));
    else if (name_matches(name, "chord_mode")) *result = (int)((tracks[slap_ui.track].harmony >> 16) & 255u);
    else if (name_matches(name, "settings_section")) *result = (int)settings_section;
    else if (name_matches(name, "resume_playback")) *result = (int)resume_playback;
    else if (name_matches(name, "output_trim")) *result = (int)output_trim;
    else if (name_matches(name, "metronome")) *result = (int)metronome;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    else if (name_matches(name, "worker_wait_us")) *result = (int)(worker_wait_ticks / 24u);
    else if (name_matches(name, "worker_work_us")) *result = (int)((worker_ticks + fx_worker_ticks) / 24u);
    else if (name_matches(name, "worker_voice_peak_us")) *result = (int)(peak_voice_job_ticks / 24u);
    else if (name_matches(name, "worker_online")) *result = plat_cpu2_on();
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
            event_get(slap_ui.track, slap_ui.step, slap_ui.lane));
    else if (name_matches(name, "note_gate"))
        *result = (int)((event_get(slap_ui.track, slap_ui.step, slap_ui.lane) >> 16) & 255u);
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
    else if (name_matches(name, "live_fx")) *result = (live_fx_word >> 3) & 15u;
    else if (name_matches(name, "live_mix")) *result = (int)live_fx_full_mix;
    else if (name_matches(name, "live_assignment")) *result = (int)(live_fx_assignments & 255u);
    else if (name_matches(name, "split_keys")) *result = (split_keys >> slap_ui.track) & 1u;
    else if (name_matches(name, "mute")) *result = (tracks[slap_ui.track].mix >> 24) & 1u;
    else if (name_matches(name, "step_mode")) *result =
        timing_group_get(pattern_row(slap_ui.track, slap_ui.lane), slap_ui.step) / TIMING_GROUPS;
    else if (name_matches(name, "record_track")) *result = (int)rec_track + 1;
    else if (name_matches(name, "fx_k1")) *result = slap_fx_value(slap_ui.track, slap_ui.fx_page, 0);
    else if (name_matches(name, "patch")) *result = (int)tracks[slap_ui.track].patch;
    else if (name_matches(name, "env_attack")) *result = tracks[slap_ui.track].envelope & 255u;
    else if (name_matches(name, "lfo_rate")) *result = (tracks[slap_ui.track].lfo >> 8) & 255u;
    else if (name_matches(name, "length")) *result = pattern_length(tracks[slap_ui.track].mix);
    else if (name_matches(name, "pitch")) {
        uint32_t word = event_get(slap_ui.track, slap_ui.step, slap_ui.lane);
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
    if (track >= TRACKS || bars > 5u || playing || recording || rec_running || pattern_action) return 0;
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
    if (track >= TRACKS || recording || rec_running || pattern_action) return 0;
    rec_track = track;
    rec_length = pattern_length(tracks[track].mix);
    quick_wait_note = 0;
    rec_start_mode = !playing;
    rec_quant_setting = unquantized ? 0u : REC_FULL_16;
    rec_overwrite = 0;
    rec_loop = 1;
    rec_follow_track = track + 1u;
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
    else if (!rec_loop || rec_countdown) {
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
    return event_get(track, step, lane);
}

STUDIO_EXPORT void studio_set_event(uint32_t track, uint32_t step, uint32_t lane, uint32_t word)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track) || !step_valid(word)) return;
    if (word && track == DRUMS && lane && (word & NOTES) != lane_pitch[lane-1u]) return;
    timing_group_set(pattern_row(track, lane), step, 0);
    event_set(track, step, lane, word);
    if (track < DRUMS) melodic_duration[track][lane][step] = 0;
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_note_offset(uint32_t track, uint32_t step, uint32_t lane)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track)) return 0;
    return note_timing(pattern_row(track, lane), step, event_get(track, step, lane));
}

STUDIO_EXPORT void studio_set_note_offset(uint32_t track, uint32_t step, uint32_t lane, uint32_t offset)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track) || offset > 15u) return;
    uint32_t word = event_get(track, step, lane);
    if (!word) return;
    timing_group_set(pattern_row(track, lane), step, offset ? (uint8_t)(offset / 4u + 1u) : 0);
    event_set(track, step, lane, (word & ~timing_bits(3u)) | timing_bits(offset));
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_step_mode(uint32_t track, uint32_t step, uint32_t lane)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track)) return 0;
    return timing_group_get(pattern_row(track, lane), step) / TIMING_GROUPS;
}

STUDIO_EXPORT void studio_set_step_mode(uint32_t track, uint32_t step, uint32_t lane, uint32_t mode)
{
    if (track >= TRACKS || step >= STEPS || lane >= lane_count(track) || mode > STEP_FILL ||
        !(event_get(track, step, lane) & ACTIVE)) return;
    unsigned row = pattern_row(track, lane);
    timing_group_set(row, step, (uint8_t)(timing_group_get(row, step) % TIMING_GROUPS + mode * TIMING_GROUPS));
    ++pattern_revision[track];
}

STUDIO_EXPORT void studio_set_fill(uint32_t enabled) { fill_active = !!enabled; }

STUDIO_EXPORT uint32_t studio_note_duration(uint32_t track, uint32_t lane, uint32_t step)
{
    if (track >= DRUMS || lane >= MELODIC_LANES || step >= STEPS) return 0;
    return melodic_duration[track][lane][step];
}

STUDIO_EXPORT void studio_set_note_duration(uint32_t track, uint32_t lane, uint32_t step, uint32_t units)
{
    if (track >= DRUMS || lane >= MELODIC_LANES || step >= STEPS || units > 65535 ||
        !(event_get(track, step, lane) & ACTIVE)) return;
    melodic_duration[track][lane][step] = (uint16_t)units;
    ++pattern_revision[track];
}

STUDIO_EXPORT uint32_t studio_drum_filters(void) { return drum_filters; }

STUDIO_EXPORT void studio_set_drum_filters(uint32_t mask)
{
    drum_filters = mask;
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
    timing_group_set(pattern_row(track, 0), step, 0);
    tracks[track].steps[step] = word;
    if (track < DRUMS) melodic_duration[track][0][step] = 0;
    ++pattern_revision[track];
}

STUDIO_EXPORT void studio_set_track(uint32_t track, uint32_t mix, uint32_t patch)
{
    if (track >= TRACKS || !pattern_length(mix) || pattern_length(mix) > STEPS || (mix >> (track == DRUMS ? 27 : 29)) ||
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
    return drum_kit_name(kit);
}

STUDIO_EXPORT uint32_t studio_patch_count(void) { return SLAP_PATCHES; }

STUDIO_EXPORT const char *studio_patch_name(uint32_t patch)
{
    return patch < SLAP_PATCHES ? slap_patches[patch].name : "";
}

STUDIO_EXPORT void studio_automate_mix(uint32_t track, uint32_t level, uint32_t pan)
{
    if (track >= TRACKS) return;
    if (level <= 255u) track_setting(track, 16, level);
    if (pan <= 255u) track_setting(track, 8, pan);
}

STUDIO_EXPORT void studio_set_mixer(uint32_t solo, uint32_t ducks, uint32_t modes)
{
    mixer_solo = solo & 31u;
    for (unsigned t = 0; t < DRUMS; ++t)
        mixer_duck[t] = (uint8_t)clamp((int)(ducks >> (t * 8u) & 255u), 0, 100);
    auto_level_off = !(modes & 1u);
    auto_mix = !!(modes & 2u);
}

STUDIO_EXPORT uint32_t studio_track_energy(uint32_t track)
{
    return track < TRACKS ? track_energy[track] : 0;
}

STUDIO_EXPORT uint32_t studio_patch_engine(uint32_t patch)
{
    return patch < SLAP_PATCHES ? slap_patches[patch].engine : 255u;
}

STUDIO_EXPORT uint32_t studio_timbre(uint32_t track)
{
    return track < DRUMS ? track_timbre[track] : 0u;
}

STUDIO_EXPORT void studio_set_timbre(uint32_t track, uint32_t settings)
{
    if (track < DRUMS) track_timbre[track] = settings;
}

STUDIO_EXPORT uint32_t studio_fm_envelope(uint32_t track, uint32_t slot)
{
    return track < DRUMS && slot < 7u ? track_fm_envelopes[track][slot] : 0u;
}

STUDIO_EXPORT void studio_set_fm_envelope(uint32_t track, uint32_t slot, uint32_t value)
{
    if (track < DRUMS && slot < 7u &&
        (slot == 6u ? value <= 16u : !value || (value >= 65536u && value <= 131071u)))
        track_fm_envelopes[track][slot] = value;
}

STUDIO_EXPORT uint32_t studio_generate_drums(uint32_t style, uint32_t hit,
                                           uint32_t groove, uint32_t variation, uint32_t bars)
{
    if (recording || rec_running || gen_queued || pattern_action ||
        style >= sizeof drum_style / sizeof drum_style[0] ||
        hit > 100u || groove > 100u || variation > 100u || bars > 4u ||
        (playing && !bars)) return 0;
    gen_bars = bars;
    gen_style = style;
    gen_hit = hit;
    gen_groove = groove;
    gen_var = variation;
    unsigned enabled = gen_enabled;
    gen_enabled = gen_confirm = 1;
    unsigned before = gen_count;
    generate_drums();
    gen_enabled = enabled;
    return gen_queued ? 2u : gen_count != before;
}

STUDIO_EXPORT void studio_apply_queued_drums(void)
{
    if (gen_queued && !recording && !rec_running) apply_generated_drums();
}

STUDIO_EXPORT uint32_t studio_drum_generation(void) { return gen_count; }
STUDIO_EXPORT uint32_t studio_drum_generation_pending(void) { return gen_queued; }

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
