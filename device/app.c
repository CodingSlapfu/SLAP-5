/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "fastmath.h"
#include "synthesis.h"
#include "effects.h"

enum { TRACKS = 5, STEPS = 64, VOICES = 16, RATE = 44100 };
enum { HOME, SEQ, GLOBAL, PATCH, ENV, FX, VISUAL, SAVE, HARMONY, LFO, ARP, MIXER, RECORD, GEN_DRUMS };
enum { NOTES = 0x7f, ACTIVE = 0x80, DRUMS = 4, DRUM_LANES = 6 };

typedef struct {
    volatile uint32_t steps[STEPS]; /* pitch, active flag, velocity, length, probability */
    volatile uint32_t patch, envelope, harmony, lfo;
    volatile uint32_t mix;          /* LSB first: length, pan, level, mute */
} Track;

typedef struct {
    float phase, inc, env, velocity, fall, bend;
    float attack, decay, sustain, release, drum_tune, drum_tail;
    float lfo_phase, lfo_inc, lfo_fade, lfo_gain, lfo_hold;
    uint32_t delay, rng;
    uint8_t lfo_wave, origin, kit, drum_family;
    uint32_t remaining, age;
    uint8_t track, pitch, manual, releasing, struck;
} Voice;

static Track tracks[TRACKS];
static volatile uint32_t extra_note[DRUMS][STEPS];
static volatile uint32_t drum_lane[DRUM_LANES][STEPS];
static const uint8_t lane_pitch[DRUM_LANES] = {36,38,42,37,39,49};
#include "drum_patterns.h"
static unsigned gen_style, gen_hit = 76, gen_groove = 35, gen_var = 28;
static unsigned gen_enabled = 1, gen_confirm, gen_count;
static uint32_t gen_seed = 0xd14a1175u;
static volatile uint32_t drum_filters;
static volatile uint32_t scheduled_events;
static volatile uint32_t recording, recorded_events;
static volatile uint32_t rec_running, rec_countdown;
static unsigned rec_bars, rec_count_in, rec_unquantized, rec_replace;
static unsigned rec_track, rec_length, rec_start_mode, rec_raw, rec_overwrite;
static uint32_t record_clock;
static struct {
    volatile uint32_t *cell;
    uint32_t start, step_ticks, word;
    unsigned pitch;
} held_record[VOICES];
static uint8_t monitored_next_step[TRACKS];
static volatile uint32_t metronome;
static unsigned click_remaining, click_accent, click_phase;
enum { CLICK_SAMPLES = RATE / 40 };
enum { BOOT_MS = 6000 };
static volatile uint32_t mixer_solo, dsp_overruns, intro_skipped;
static volatile uint32_t dsp_avg_ticks, dsp_peak_ticks, dsp_budget_ticks;
static volatile uint32_t active_voices, voice_steals;
static volatile uint8_t mixer_auto[TRACKS], mixer_proc[TRACKS];
static uint32_t boot_suppressed_keys;
static float mixer_envelope[TRACKS], mixer_gain[TRACKS];
static volatile uint32_t track_peak[TRACKS], fx_cost_ticks[TRACKS];
static Voice voices[VOICES];
static PLAT_POOL SlapSynth synths[VOICES];
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
static int host_discard_worker_result;
#endif
enum { SPECTRUM_BANDS = 7, VISUAL_COUNT = 8 };
static volatile float visual_rms[2], visual_peak[2], visual_band[SPECTRUM_BANDS];
static float meter_power[2], analysis_lp[SPECTRUM_BANDS - 1], analysis_power[SPECTRUM_BANDS];
static float analysis_mono;
static unsigned analysis_phase;

static void analyze_audio(float left, float right)
{
    analysis_mono += (left + right) * 0.125f;
    if (++analysis_phase == 4) {
        static const float coefficient[] = {0.0336f, 0.0819f, 0.1809f, 0.3662f, 0.6412f, 0.8975f};
        float previous = 0.0f;
        for (unsigned band = 0; band < SPECTRUM_BANDS; ++band) {
            float next = analysis_mono;
            if (band < SPECTRUM_BANDS - 1) {
                analysis_lp[band] += (analysis_mono - analysis_lp[band]) * coefficient[band];
                next = analysis_lp[band];
            }
            float signal = next - previous;
            analysis_power[band] += (signal * signal - analysis_power[band]) * 0.0023f;
            previous = next;
        }
        analysis_mono = 0.0f;
        analysis_phase = 0;
    }
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

static void track_setting(unsigned track, unsigned shift, unsigned value)
{
    uint32_t mask = 255u << shift;
    tracks[track].mix = (tracks[track].mix & ~mask) | (value << shift);
}

static void load_patch(unsigned track, unsigned number)
{
    const SlapPatch *patch = &slap_patches[number];
    tracks[track].envelope = patch->attack | (uint32_t)patch->decay << 8 |
                             (uint32_t)patch->sustain << 16 | (uint32_t)patch->release << 24;
    tracks[track].patch = number;
}

void app_init(void)
{
    startup_sample = 0;
    startup_phase = 0.0f;
    slap_fx_init();
#if FM1B_CPU2 && !defined(PLAT_WEB)
#ifdef PLAT_HOST
    host_discard_worker_result = getenv("SLAP_TEST_DISCARD_WORKER") != 0;
    plat_cpu2_set(getenv("SLAP_TEST_SINGLE_CORE") == 0);
#else
    plat_cpu2_set(1);
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

}

static float slap_sine(float phase) { return fm_sinf(phase * 6.2831853f); }

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

#include "drum_kits.h"

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
    if (voice->age) ++voice_steals;
    *voice = (Voice) {
        .inc = slap_pitch_increment(pitch),
        .velocity = (float)velocity / 127.0f, .fall = 1.0f, .bend = 1.0f,
        .remaining = duration, .age = ++ages,
        .track = (uint8_t)track, .pitch = (uint8_t)pitch, .manual = (uint8_t)manual,
        .origin = (uint8_t)origin, .delay = delay
    };
    if (track < DRUMS) {
        const SlapPatch *patch = &slap_patches[tracks[track].patch];
        uint32_t env = tracks[track].envelope;
        float a = env & 255u, d = (env >> 8) & 255u, r = env >> 24;
        voice->attack = 1.0f / (25.0f + 1.3f*a*a);
        voice->decay = 1.0f / (38.0f + 1.9f*d*d);
        voice->sustain = ((env >> 16) & 255u) * (1.0f / 127.0f);
        voice->release = 1.0f / (48.0f + 0.85f*r*r);
        uint32_t lfo = tracks[track].lfo;
        voice->lfo_wave = lfo & 255u;
        if (voice->lfo_wave) {
            const float hz = 0.1f * fm_exp2f(((lfo >> 8) & 255u) * (8.643856f / 127.0f));
            voice->lfo_inc = hz * (1.0f / RATE);
            voice->lfo_phase = ((lfo >> 16) & 255u) * (1.0f / 128.0f);
            voice->lfo_fade = 1.0f / (1.0f + (lfo >> 24) * 690.0f);
            voice->lfo_hold = (int16_t)(random_word() & 65535u) * (1.0f / 32768.0f);
        }
        slap_synth_start(&synths[voice - voices], patch->engine, pitch,
                         patch->color, patch->motion, random_word());
    } else {
        voice->rng = random_word();
        voice->kit = (uint8_t)tracks[track].patch;
        voice->drum_family = (uint8_t)drum_family(pitch);
        const struct DrumKit *kit = &drum_kits[voice->kit];
        unsigned tune = voice->drum_family == 0 ? kit->kick_pitch :
            voice->drum_family == 1 || voice->drum_family == 3 ? kit->snare_pitch :
            voice->drum_family == 4 ? kit->clap_rate : kit->metal_pitch;
        voice->drum_tune = tune * 0.01f;
        voice->drum_tail = 100.0f / kit->decay;
        voice->attack = 0.013f;
        voice->sustain = 1.0f;
        voice->release = 0.00065f;
    }
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

static void schedule_event(unsigned track, uint32_t word)
{
    unsigned probability = (word >> 24) & 127u;
    if (!(word & ACTIVE) || probability <= random_word() % 100u) return;
    unsigned gate = until_step * ((word >> 16) & 255u) / 192u;
    unsigned delay = until_step * timing_of(word) / 8u;
    trigger_delayed(track, word & NOTES, (word >> 8) & 127u, gate, 0, delay);
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
        *cell = (*cell & ~(255u << 16)) | (gate << 16);
    }
    held_record[slot].cell = 0;
}

static void end_recording(uint32_t now)
{
    for (unsigned slot = 0; slot < VOICES; ++slot) finish_record_note(slot, now);
    rec_running = rec_countdown = 0;
}

static void begin_recording(void)
{
    unsigned track = rec_track;
    if (rec_overwrite) {
        for (unsigned step = 0; step < rec_length; ++step) {
            tracks[track].steps[step] = 0;
            if (track < DRUMS) extra_note[track][step] = 0;
            else for (unsigned lane = 0; lane < DRUM_LANES; ++lane) drum_lane[lane][step] = 0;
        }
    }
    track_setting(track, 0, rec_length);
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
        if (rec_raw) {
            unsigned offset = (progress * 8u + step_duration / 2u) / step_duration;
            if (offset <= 5u) {
                position = positions;
                timing = offset > 3u ? 3u : offset;
            }
        } else if (until_step * 2u >= step_duration) position = positions;
    }
    if (position >= rec_length) return;
    unsigned step = position;
    unsigned source_pitch = pitch;
    unsigned lane = 0;
    volatile uint32_t *cell = &tracks[track].steps[step];
    if (track == DRUMS) {
        lane = drum_family(pitch) + 1;
        cell = &drum_lane[lane - 1][step];
        pitch = lane_pitch[lane - 1];
    } else if ((*cell & (ACTIVE | NOTES)) != (ACTIVE | pitch)) {
        volatile uint32_t *second = &extra_note[track][step];
        if ((*second & (ACTIVE | NOTES)) == (ACTIVE | pitch) || (*cell & ACTIVE)) {
            if ((*second & ACTIVE) && (*second & NOTES) != pitch) return;
            lane = 1;
            cell = second;
        }
    }
    record_note_off(track, source_pitch);
    for (unsigned slot = 0; slot < VOICES; ++slot)
        if (held_record[slot].cell == cell) held_record[slot].cell = 0;
    *cell = cell_word(pitch, velocity, 144u, 100u) | timing_bits(timing);
    for (unsigned slot = 0; slot < VOICES; ++slot) {
        if (held_record[slot].cell) continue;
        held_record[slot].cell = cell;
        held_record[slot].start = record_clock;
        held_record[slot].step_ticks = step_duration && until_step ? step_duration : RATE * 15u / tempo;
        held_record[slot].word = *cell;
        held_record[slot].pitch = source_pitch;
        break;
    }
    /* The live input has already sounded. Skip its first sequencer pass only. */
    if (position == step_number) monitored_next_step[track] |= 1u << lane;
    ++recorded_events;
}

static void read_notes(void)
{
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
    if (metronome && !(positions & 3u)) {
        click_remaining = CLICK_SAMPLES;
        click_accent = !(positions & 15u);
        click_phase = 0;
    }
    for (unsigned t = 0; t < TRACKS; ++t) {
        unsigned step = positions % (mix[t] & 255u);
        uint32_t word = tracks[t].steps[step];
        unsigned monitored = monitored_next_step[t];
        monitored_next_step[t] = 0;
        if (t < DRUMS) {
            if (!(monitored & 1u)) schedule_event(t, word);
            if (!(monitored & 2u)) schedule_event(t, extra_note[t][step]);
        } else {
            unsigned muted = muted_drums();
            if (!(monitored & 1u) && !(muted & (1u << drum_family(word & NOTES)))) schedule_event(t, word);
            for (unsigned lane = 0; lane < DRUM_LANES; ++lane)
                if (!(muted & (1u << lane)) && !(monitored & (2u << lane)))
                    schedule_event(t, drum_lane[lane][step]);
        }
    }
}

static float render_voice(Voice *v, SlapSynth *s)
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
        static const float tone[] = {0.0045f, 0.0028f, 0.0052f, 0.0072f};
        static const float pitch_mul[] = {1.0f, 0.81f, 1.33f, 1.85f};
        float hiss = (float)(next_random(&v->rng) & 65535u) / 32768.0f - 1.0f;
        unsigned family = v->drum_family;
        unsigned kit = drum_kits[v->kit].circuit;
        float decay;
        if (family == 0) {
            v->bend *= 0.9988f;
            inc = 0.00108f + tone[kit] * v->bend;
            decay = 0.99966f - kit * 0.00008f;
            wave = slap_sine(v->phase) + (kit == 3 ? 0.19f * hiss * v->bend : 0.0f);
        } else if (family == 1 || family == 3) {
            inc = tone[kit] * (family == 3 ? 3.17f : 1.0f);
            v->bend *= 0.9972f;
            decay = family == 3 ? 0.9977f : 0.99906f;
            float skin = kit == 2 ? slap_sine(v->phase + 0.25f*v->bend*slap_sine(v->phase*3.371f))
                                  : slap_sine(v->phase);
            float noise_mix = kit == 1 ? 0.91f : family == 3 ? 0.34f : 0.68f;
            wave = noise_mix * hiss + (1.0f - noise_mix) * skin * v->bend;
        } else if (family == 4) {
            decay = 0.99902f;
            v->bend *= 0.999f;
            inc = 0.008f * pitch_mul[kit];
            wave = hiss * (v->phase < 0.18f ? 0.95f : 0.23f);
        } else {
            inc = 0.052f * pitch_mul[kit];
            decay = family == 5 ? 0.99968f : kit == 1 ? 0.998f : 0.9969f;
            float metal = slap_sine(v->phase) * slap_sine(v->phase*3.79f);
            wave = 0.65f * hiss + (kit > 1 ? 0.4f : 0.13f) * metal;
        }
        if (v->kit >= 4u) {
            inc *= v->drum_tune;
            decay = 1.0f - (1.0f - decay) * v->drum_tail;
        }
        v->fall *= decay;
    }
    v->phase += inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    if (v->fall < 0.00005f) { v->age = 0; return 0.0f; }
    return wave * v->env * v->velocity * v->fall * 0.15f;
}

#if FM1B_CPU2 && !defined(PLAT_WEB)
enum { VOICE_BATCH = 64, MIN_VOICE_BATCH = 32 };
static struct {
    Voice voice[VOICES];
    unsigned slot[VOICES];
    unsigned count, muted;
    float output[VOICE_BATCH];
    uint32_t ticks;
} drum_worker __attribute__((aligned(32)));
static float voice_bus[VOICE_BATCH][TRACKS];
static volatile uint32_t worker_jobs, worker_fallbacks, worker_ticks, worker_wait_ticks;

static void render_drum_bus(uint32_t frames)
{
    if (frames > VOICE_BATCH) return;
    uint32_t started = plat_ticks24();
    for (unsigned i = 0; i < frames; ++i) {
        float dry = 0.0f;
        for (unsigned j = 0; j < drum_worker.count; ++j) {
            Voice *voice = &drum_worker.voice[j];
            if (!voice->age) continue;
            float sample = render_voice(voice, 0);
            if (!(drum_worker.muted & (1u << voice->drum_family))) dry += sample;
        }
        drum_worker.output[i] = dry;
    }
    drum_worker.ticks = plat_ticks24() - started;
}

static unsigned render_voice_batch(unsigned frames, unsigned muted,
                                   uint32_t callback_start, uint32_t budget)
{
    if (frames < MIN_VOICE_BATCH || !plat_cpu2_on() ||
        plat_ticks24() - callback_start >= budget / 2u) return 0;
    unsigned melodic = 0;
    drum_worker.count = 0;
    drum_worker.muted = muted;
    for (unsigned j = 0; j < VOICES; ++j) {
        if (!voices[j].age) continue;
        if (voices[j].track == DRUMS) {
            unsigned index = drum_worker.count++;
            drum_worker.slot[index] = j;
            drum_worker.voice[index] = voices[j];
        } else ++melodic;
    }
    if (!melodic || !drum_worker.count || !plat_cpu2_run(render_drum_bus, frames)) return 0;

    for (unsigned i = 0; i < frames; ++i) {
        for (unsigned t = 0; t < DRUMS; ++t) voice_bus[i][t] = 0.0f;
        for (unsigned j = 0; j < VOICES; ++j) {
            Voice *voice = &voices[j];
            if (voice->age && voice->track < DRUMS)
                voice_bus[i][voice->track] += render_voice(voice, &synths[j]);
        }
    }

    uint32_t wait_start = plat_ticks24();
    int busy;
    while ((busy = plat_cpu2_busy()) &&
           plat_ticks24() - wait_start < 60u * 24u &&
           plat_ticks24() - callback_start < budget * 3u / 4u) {}
    worker_wait_ticks += plat_ticks24() - wait_start;
#ifdef PLAT_HOST
    // Exercise the recovery path after the simulated worker has changed its copy.
    busy |= host_discard_worker_result;
#endif
    if (busy || plat_cpu2_wait() != 0) {
        plat_cpu2_set(0);
        ++worker_fallbacks;
        for (unsigned j = 0; j < drum_worker.count; ++j)
            drum_worker.voice[j] = voices[drum_worker.slot[j]];
        render_drum_bus(frames);
    } else {
        ++worker_jobs;
        worker_ticks += drum_worker.ticks;
    }
    for (unsigned j = 0; j < drum_worker.count; ++j)
        voices[drum_worker.slot[j]] = drum_worker.voice[j];
    for (unsigned i = 0; i < frames; ++i)
        voice_bus[i][DRUMS] = drum_worker.output[i];
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
    uint32_t mix[TRACKS], solo = mixer_solo;
    float left[TRACKS], right[TRACKS], level_at[TRACKS], wet[TRACKS];
    unsigned automatic[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t) {
        mix[t] = tracks[t].mix;
        float pan = ((mix[t] >> 8) & 255u) / 255.0f;
        float level = (mix[t] >> 24 || (solo && !(solo & (1u << t)))) ? 0.0f :
                      ((mix[t] >> 16) & 255u) / 255.0f;
        level_at[t] = level;
        wet[t] = mixer_proc[t] * (1.0f / 255.0f);
        automatic[t] = mixer_auto[t];
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
    read_notes();
    if (rec_running) mix[rec_track] = tracks[rec_track].mix;
    if (!playing || (!metronome && !rec_countdown)) click_remaining = 0;
    slap_fx_begin(tempo);
    const unsigned muted = muted_drums();
    unsigned measure_fx = slap_ui.page == FX && slap_ui.fx_page == 4;
    const unsigned analyze = slap_ui.page == VISUAL;
    float meter_sum[2] = {0}, meter_peak[2] = {0};
    float target = master_q12 * (1.0f / 4096.0f);
#if FM1B_CPU2 && !defined(PLAT_WEB)
    unsigned batch_start = 0, batch_end = 0, next_batch = 0;
    uint32_t callback_budget = (24000000u / RATE) * frames;
#endif
    for (uint32_t i = 0; i < frames; ++i) {
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
            if (rec_running && step_number >= rec_length) {
                end_recording(record_clock + i);
                recording = 0;
            }
            if (!recording || rec_running) next_step(mix);
        }
        if (until_step) --until_step;
        arp_render_clock();
        float dry[TRACKS] = {0};
#if FM1B_CPU2 && !defined(PLAT_WEB)
        if (i == next_batch) {
            unsigned amount = frames - i;
            if (amount > VOICE_BATCH) amount = VOICE_BATCH;
            if (playing && until_step && amount > until_step + 1u) amount = until_step + 1u;
            // A held arpeggio can create a voice before the next sequencer step.
            if (old_keys && input_track < DRUMS && (arp_setup[input_track] & 255u)) amount = 0;
            batch_start = i;
            batch_end = i + render_voice_batch(amount, muted, audio_started, callback_budget);
            next_batch = batch_end > i ? batch_end : i + VOICE_BATCH;
        }
        if (i < batch_end)
            for (unsigned t = 0; t < TRACKS; ++t) dry[t] = voice_bus[i - batch_start][t];
        else
#endif
        for (unsigned j = 0; j < VOICES; ++j) {
            Voice *v = &voices[j];
            if (!v->age) continue;
            float sample = render_voice(v, &synths[j]);
            if (v->track != DRUMS || !(muted & (1u << v->drum_family)))
                dry[v->track] += sample;
        }
        float l = 0.0f, r = 0.0f;
        for (unsigned t = 0; t < TRACKS; ++t) {
            unsigned sample_cost = measure_fx && i == 0u;
            uint32_t started = sample_cost ? plat_ticks24() : 0u;
            float processed = slap_fx_sample(t, dry[t]);
            if (sample_cost) {
                uint32_t ticks = plat_ticks24() - started;
                fx_cost_ticks[t] = (7u * fx_cost_ticks[t] + ticks + 4u) / 8u;
            }
            float sample = dry[t] + wet[t] * (processed - dry[t]);
            float amplitude = fm_fabsf(sample);
            mixer_envelope[t] += (amplitude - mixer_envelope[t]) *
                (amplitude > mixer_envelope[t] ? 0.003f : 0.00018f);
            float goal = automatic[t] && mixer_envelope[t] > 0.003f ?
                fm_maxf(0.45f, fm_minf(2.0f, 0.025f / mixer_envelope[t])) : 1.0f;
            mixer_gain[t] += (goal - mixer_gain[t]) * 0.0008f;
            sample *= mixer_gain[t];
            energy_smooth[t] += (fm_fabsf(sample) * level_at[t] - energy_smooth[t]) * 0.0015f;
            l += sample * left[t];
            r += sample * right[t];
        }
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
        l *= master_gain;
        r *= master_gain;
        lr[i * 2] = (int32_t)(l * 8300000.0f);
        lr[i * 2 + 1] = (int32_t)(r * 8300000.0f);
        if (analyze) {
            meter_sum[0] += l * l;
            meter_sum[1] += r * r;
            meter_peak[0] = fm_maxf(meter_peak[0], fm_fabsf(l));
            meter_peak[1] = fm_maxf(meter_peak[1], fm_fabsf(r));
            analyze_audio(l, r);
        }
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
        for (unsigned band = 0; band < SPECTRUM_BANDS; ++band)
            visual_band[band] = analysis_power[band];
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
        if (elapsed > budget) ++dsp_overruns;
        if (elapsed > dsp_peak_ticks) dsp_peak_ticks = elapsed;
        dsp_avg_ticks = dsp_avg_ticks - dsp_avg_ticks / 8u + elapsed / 8u;
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
        uint32_t steps[STEPS], fx[SLAP_FX_PAGES];
    } track[TRACKS];
    uint32_t extra[DRUMS][STEPS];
    uint8_t drums[DRUM_LANES][STEPS][3];
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

static int read_song(unsigned slot)
{
    int bytes = plat_store_load(OBJ_SONG1 + slot, &song_io, sizeof(song_io));
    int v2bytes = (int)((uint8_t *)&song_io.extra - (uint8_t *)&song_io);
    int v3bytes = (int)((uint8_t *)&song_io.arp - (uint8_t *)&song_io);
    if (song_io.magic != SONG_MAGIC ||
        !((song_io.version == 2 && bytes == v2bytes) ||
          (song_io.version == 3 && bytes == v3bytes) ||
          (song_io.version == 4 && bytes == (int)sizeof(song_io))) ||
        song_io.bpm < 40 || song_io.bpm > 240 || song_io.swing > 70) return 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        uint32_t mix = song_io.track[t].mix;
        if (!(mix & 255u) || (mix & 255u) > STEPS || (mix >> 24) > 1 ||
            song_io.track[t].patch >= (t == DRUMS ? DRUM_KITS : SLAP_PATCHES) || (song_io.track[t].envelope & 0x80808080u)) return 0;
        uint32_t h = song_io.track[t].harmony, l = song_io.track[t].lfo;
        if ((h & 255u) >= HARMONY_KEYS || ((h >> 8) & 255u) >= HARMONY_SCALES ||
            ((h >> 16) & 255u) >= HARMONY_CHORDS || (h >> 24) > 101u ||
            (l & 0x80808080u) || (l & 255u) > 5u) return 0;
        for (unsigned i = 0; i < STEPS; ++i)
            if (!step_valid(song_io.track[t].steps[i])) return 0;
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            if (!slap_fx_config_valid(i, song_io.track[t].fx[i])) return 0;
    }
    if (song_io.version >= 3) {
        if (song_io.filters & ~0x3f3fu) return 0;
        for (unsigned t = 0; t < DRUMS; ++t)
            for (unsigned i = 0; i < STEPS; ++i)
                if (!step_valid(song_io.extra[t][i])) return 0;
        for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
            for (unsigned i = 0; i < STEPS; ++i) {
                const uint8_t *d = song_io.drums[lane][i];
                uint32_t data = (uint32_t)d[0] << 8 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 24;
                if (data && !step_valid(data | ACTIVE | lane_pitch[lane])) return 0;
            }
        }
    }
    if (song_io.version == 4) {
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
        for (unsigned i = 0; i < STEPS; ++i)
            tracks[t].steps[i] = song_io.track[t].steps[i];
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            slap_fx_restore(t, i, song_io.track[t].fx[i]);
    }
    for (unsigned t = 0; t < DRUMS; ++t)
        for (unsigned i = 0; i < STEPS; ++i)
            extra_note[t][i] = song_io.version >= 3 ? song_io.extra[t][i] : 0u;
    for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < STEPS; ++i) {
            const uint8_t *d = song_io.drums[lane][i];
            uint32_t data = song_io.version >= 3 ?
                (uint32_t)d[0] << 8 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 24 : 0;
            drum_lane[lane][i] = data ? data | ACTIVE | lane_pitch[lane] : 0;
        }
    }
    drum_filters = song_io.version >= 3 ? song_io.filters : 0;
    for (unsigned t = 0; t < DRUMS; ++t)
        arp_setup[t] = song_io.version == 4 ? song_io.arp[t] : ARP_DEFAULT;
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
        for (unsigned i = 0; i < STEPS; ++i)
            song_io.track[t].steps[i] = tracks[t].steps[i];
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            song_io.track[t].fx[i] = slap_fx_config(t, i);
    }
    for (unsigned t = 0; t < DRUMS; ++t)
        for (unsigned i = 0; i < STEPS; ++i) song_io.extra[t][i] = extra_note[t][i];
    for (unsigned lane = 0; lane < DRUM_LANES; ++lane) {
        for (unsigned i = 0; i < STEPS; ++i) {
            uint32_t word = drum_lane[lane][i];
            for (unsigned byte = 0; byte < 3; ++byte)
                song_io.drums[lane][i][byte] = (uint8_t)(word >> ((byte + 1u) * 8u));
        }
    }
    song_io.filters = drum_filters;
    for (unsigned t = 0; t < DRUMS; ++t) song_io.arp[t] = arp_setup[t];
    int result = plat_store_save(OBJ_SONG1 + slot, &song_io, sizeof(song_io));
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
    return track == DRUMS ? &drum_lane[lane - 1u][step] : &extra_note[track][step];
}

static unsigned lane_count(unsigned track)
{
    return track == DRUMS ? DRUM_LANES + 1u : 2u;
}

static void generate_drums(void)
{
    if (!gen_enabled || playing) return;
    unsigned occupied = 0;
    unsigned length = tracks[DRUMS].mix & 255u;
    for (unsigned step = 0; step < length; ++step)
        for (unsigned lane = 0; lane < lane_count(DRUMS); ++lane)
            occupied |= *event_cell(DRUMS, step, lane) & ACTIVE;
    if (occupied && !gen_confirm) {
        gen_confirm = 1;
        return;
    }
    gen_confirm = 0;
    ++gen_count;
    const struct DrumStyle *style = &drum_style[gen_style];
    for (unsigned step = 0; step < length; ++step) {
        unsigned beat = step & 15u;
        unsigned bit = 1u << beat;
        unsigned fill = (step / 16u % 4u == 3u) && beat >= 13u;
        tracks[DRUMS].steps[step] = 0;
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
    if ((pressed & (1u << B_REC)) && !(slap_ui.page == SEQ &&
          slap_ui.track == DRUMS && slap_ui.lane && (buttons & (1u << B_SEL)))) {
        if (slap_ui.page == RECORD) metronome ^= 1u;
        else slap_ui.page = RECORD;
    }
    if ((pressed & (1u << B_SEL)) && slap_ui.page == RECORD && !(buttons & (1u << B_GLO))) {
        if (recording) {
            recording = 0;
            if (rec_countdown) ++sequence_epoch;
        } else if (!playing) {
            rec_track = (unsigned)slap_ui.track;
            rec_length = 16u << rec_bars;
            rec_start_mode = rec_count_in;
            rec_raw = rec_unquantized;
            rec_overwrite = rec_replace;
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
    } else if (pressed & (1u << B_GLO)) slap_ui.page = GLOBAL;
    if (pressed & (1u << B_SAVE)) {
        slap_ui.page = SAVE;
        slap_ui.save_armed = 0;
        slap_ui.notice = 0;
    }
    if (pressed & (1u << B_FX)) slap_ui.page = FX;
    if (pressed & (1u << B_ARP) && slap_ui.track < DRUMS && slap_ui.page != MIXER) slap_ui.page = ARP;
    if (pressed & (1u << B_SEL) && slap_ui.page == HOME && slap_ui.track < DRUMS) slap_ui.page = HARMONY;
    if (pressed & (1u << B_LFO)) slap_ui.page = LFO;
    if (pressed & (1u << B_EDIT)) slap_ui.page = PATCH;
    if (pressed & (1u << B_ENV)) slap_ui.page = ENV;
    if (pressed & (1u << B_SEL) && slap_ui.page == MIXER) {
        unsigned mute = !(tracks[slap_ui.track].mix >> 24);
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
        slap_ui.step = clamp(slap_ui.step + delta, 0, (int)(tracks[slap_ui.track].mix & 255u) - 1);
    else if (slap_ui.page == VISUAL)
        slap_ui.visual = clamp(slap_ui.visual + delta, 0, VISUAL_COUNT - 1);
    else if (slap_ui.page == FX)
        slap_ui.fx_page = clamp(slap_ui.fx_page + delta, 0, SLAP_FX_PAGES - 1);
    else if (slap_ui.page == HOME || slap_ui.page == MIXER || (slap_ui.page == RECORD && !recording) ||
             slap_ui.page == PATCH || slap_ui.page == ENV || slap_ui.page == LFO)
        slap_ui.track = clamp(slap_ui.track + delta, 0, TRACKS - 1);
    tempo = (uint32_t)clamp((int)tempo + plat_enc(EN_ALGO), 40, 240);
    int patch_delta = plat_enc(EN_PRESET);

    if (patch_delta) {
        int old_patch = (int)tracks[slap_ui.track].patch;
        if (slap_ui.track == DRUMS)
            tracks[DRUMS].patch = (unsigned)clamp(old_patch + patch_delta, 0, DRUM_KITS - 1);
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
        rec_bars = (unsigned)clamp((int)rec_bars + knob[0], 0, 2);
        rec_count_in = (unsigned)clamp((int)rec_count_in + knob[1], 0, 1);
        rec_unquantized = (unsigned)clamp((int)rec_unquantized + knob[2], 0, 1);
        rec_replace = (unsigned)clamp((int)rec_replace + knob[3], 0, 1);
    } else if (slap_ui.page == MIXER) {
        unsigned t = (unsigned)slap_ui.track;
        track_setting(t, 16, (unsigned)clamp((int)((mix >> 16) & 255u) + knob[0] * 4, 0, 255));
        track_setting(t, 8, (unsigned)clamp((int)((mix >> 8) & 255u) + knob[1] * 4, 0, 255));
        mixer_auto[t] = (uint8_t)clamp((int)mixer_auto[t] + knob[2], 0, 1);
        mixer_proc[t] = (uint8_t)clamp((int)mixer_proc[t] + knob[3] * 4, 0, 255);
    } else if (slap_ui.page == PATCH) {
        unsigned t = (unsigned)slap_ui.track;
        if (t == DRUMS) tracks[t].patch = (unsigned)clamp((int)tracks[t].patch + knob[1], 0, DRUM_KITS - 1);
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
    } else if (slap_ui.page == FX) {
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(slap_ui.track, slap_ui.fx_page, k, knob[k]);
    } else if (slap_ui.page == SEQ) {
        volatile uint32_t *cell = event_cell(slap_ui.track, slap_ui.step, slap_ui.lane);
        int fixed = slap_ui.track == DRUMS && slap_ui.lane;
        int editing_timing = buttons & (1u << B_SEL);
        uint32_t word = *cell;
        if (notes) {
            unsigned k = 0;
            while (!(notes & (1u << k))) ++k;
            unsigned pitch = fixed ? lane_pitch[slap_ui.lane - 1] : 29u + k + (unsigned)slap_ui.octave * 12u;
            word = (word & 255u) == (pitch | ACTIVE) ? 0u : cell_word(pitch, 100, 140, 100);
        }
        if (word & ACTIVE) {
            unsigned timing = (unsigned)clamp((int)timing_of(word) + (editing_timing ? knob[3] : 0), 0, 3);
            word = cell_word((unsigned)clamp((int)(word & NOTES) + (fixed ? 0 : knob[0]), 0, 127),
                (unsigned)clamp((int)((word >> 8) & 127u) + knob[1] * 3, 1, 127),
                (unsigned)clamp((int)((word >> 16) & 255u) + knob[2] * 5, 1, 255),
                (unsigned)clamp((int)((word >> 24) & 127u) + (editing_timing ? 0 : knob[3] * 5), 0, 100)) |
                timing_bits(timing);
        }
        *cell = word;
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
        track_setting(slap_ui.track, 0, (unsigned)clamp((int)(mix & 255u) + knob[1], 1, STEPS));
        track_setting(slap_ui.track, 16, (unsigned)clamp((int)((mix >> 16) & 255u) + knob[2] * 4, 0, 255));
        swing = (unsigned)clamp((int)swing + knob[3] * 2, 0, 70);
        slap_ui.step = clamp(slap_ui.step, 0, (int)(tracks[slap_ui.track].mix & 255u) - 1);
    }
    input_track = (unsigned)slap_ui.track;
    input_octave = (unsigned)slap_ui.octave;
    keys_audio = keys & ~boot_suppressed_keys;
    slap_ui.keys = keys;
    slap_ui.buttons = buttons;
    plat_leds(buttons | (playing << B_PLAY) | (recording << B_REC), keys);
}

static void decimal(char *out, unsigned value)
{
    if (value >= 1000) *out++ = (char)('0' + value / 1000);
    if (value >= 100) *out++ = (char)('0' + value / 100 % 10);
    if (value >= 10) *out++ = (char)('0' + value / 10 % 10);
    *out++ = (char)('0' + value % 10);
    *out = 0;
}

#include "visuals.h"

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
    char number[5];
    unsigned selected = (unsigned)slap_ui.track;
    uint32_t position = positions;
    uint32_t settings = tracks[selected].mix;
    unsigned length = settings & 255u, play_step = position % length;
    unsigned scroll = slap_ui.page == HOME && playing;
    unsigned substep = step_progress;
    unsigned bank = (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) / 16u * 16u;
    uint16_t c = track_color[selected], background = RGB(10,14,24);
    cv_begin(240, 48, background);
    if (slap_ui.page == HOME) block_text(8, 2, 3, "SLAP-5", C_WHITE);
    else cv_text(8, 1, &FONT_M, page_names[slap_ui.page], C_WHITE);
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
        cv_text(8, 25, &FONT_XS, slap_ui.page == MIXER ? "SEL MUTE  /  ARP SOLO" :
            slap_ui.page == RECORD ? (recording ? "SEL END TAKE  /  PLAY STOP" : playing ? "STOP TO ARM A TAKE" : "SEL ARM  /  PLAY TO START") :
            "SLAP-5 " APP_VERSION "  /  FM-1", C_WHITE);
        cv_blit(0, 48);
        for (int strip = 0; strip < 3; ++strip) {
            cv_begin(240, 36, background);
            if (slap_ui.page == MIXER) {
                for (unsigned t = 0; t < TRACKS; ++t) {
                    int x = 9 + (int)t * 46;
                    int h = (int)track_energy[t] * 92 / 160;
                    int peak = (int)track_peak[t] * 92 / 160;
                    cv_rect(x, 4-strip*36, 36, 96, RGB(24,36,54));
                    if (t == selected) cv_rect(x+1, 4-strip*36, 1, 96, C_WHITE);
                    cv_rect(x+4, 99-h-strip*36, 28, h, track_color[t]);
                    if (peak) cv_rect(x+4, 99-peak-strip*36, 28, 2, C_WHITE);
                    if (strip == 0) cv_text(x+6, 8, &FONT_XS,
                        (tracks[t].mix >> 24) ? "MUTE" : (mixer_solo & (1u << t)) ? "SOLO" :
                        mixer_auto[t] ? "AUTO" : "", C_WHITE);
                }
            } else if (slap_ui.page == GLOBAL) {
                static const char *const labels[3][2] = {
                    {"AVG DSP %", "PEAK US"}, {"AVG US", "BUDGET US"},
                    {"DEADLINE MISS", "VOICE STEALS"}
                };
                unsigned avg = dsp_avg_ticks, budget = dsp_budget_ticks;
                unsigned values[3][2] = {
                    {budget ? avg * 100u / budget : 0u, dsp_peak_ticks / 24u},
                    {avg / 24u, budget / 24u}, {dsp_overruns, voice_steals}
                };
                for (unsigned col = 0; col < 2; ++col) {
                    int x = 8 + (int)col * 116;
                    cv_text(x, 0, &FONT_XS, labels[strip][col], C_WHITE);
                    unsigned value = values[strip][col];
                    decimal(number, value > 9999 ? 9999 : value);
                    cv_text(x, 13, &FONT_M, number, c);
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
                if (strip == 1) cv_rect(8, 16, (int)(play_step + 1u) * 224 / (int)length, 9, c);
                if (strip == 2) {
                    cv_text(8, 8, &FONT_B, "CAPTURED", C_WHITE);
                    decimal(number, recorded_events > 999 ? 999 : recorded_events);
                    cv_text(174, 4, &FONT_M, number, c);
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
            decimal(number, worker_jobs > 9999 ? 9999 : worker_jobs);
            cv_text(55, 2, &FONT_XS, number, C_WHITE);
            cv_text(89, 2, &FONT_XS, "FALLBACKS", c);
            decimal(number, worker_fallbacks > 9999 ? 9999 : worker_fallbacks);
            cv_text(157, 2, &FONT_XS, number, C_WHITE);
#else
            cv_text(8, 2, &FONT_XS, "K1 TEMPO  /  K2 SWING", C_WHITE);
#endif
            cv_text(8, 20, &FONT_XS, "ACTIVE VOICES", c);
            decimal(number, active_voices);
            cv_text(113, 20, &FONT_XS, number, C_WHITE);
            cv_text(134, 20, &FONT_XS, "DMA XRUNS", c);
            unsigned xruns = plat_xruns();
            decimal(number, xruns > 9999 ? 9999 : xruns);
            cv_text(201, 20, &FONT_XS, number, C_WHITE);
        } else {
            static const char *const labels[] = {"LENGTH", "START", "QUANT", "MODE"};
            char bars[5];
            decimal(bars, 1u << rec_bars);
            const char *values[] = {bars, rec_count_in ? "COUNT-IN" : "1ST NOTE",
                rec_unquantized ? "OFF" : "ON", rec_replace ? "REPLACE" : "OVERDUB"};
            for (unsigned k = 0; k < 4; ++k) {
                int x = 8 + (int)k * 58;
                cv_text(x, 0, &FONT_XS, labels[k], c);
                cv_text(x, 15, &FONT_XS, values[k], C_WHITE);
            }
            cv_text(8, 30, &FONT_XS, metronome ? "REC: CLICK ON" : "REC: CLICK OFF", c);
        }
        cv_blit(0, 196);
        return;
    }
    cv_begin(240, 32, background);
    const SlapPatch *patch = &slap_patches[tracks[selected].patch];
    static const char *const drum_names[] = {"KICK","SNARE","HAT","RIM","CLAP","CYMBAL"};
    const char *lane_name = selected == DRUMS ?
        (slap_ui.lane ? drum_names[slap_ui.lane-1] : "DRUM NOTES") :
        (slap_ui.lane ? "NOTE B" : "NOTE A");
    const char *name = slap_ui.page == SEQ ? lane_name : slap_ui.page == SAVE ? "SONG SLOTS" : slap_ui.page == FX ? slap_fx_pages[slap_ui.fx_page] :
                       selected == DRUMS ? drum_kits[tracks[DRUMS].patch].name : patch->name;
    const felucca_font_t *name_font = text_w(&FONT_M, name) > 150 ? &FONT_B : &FONT_M;
    cv_text(8, 1, name_font, name, C_WHITE);
    decimal(number, position / 16u % 999u + 1u);
    cv_text(163, 5, &FONT_XS, "BAR", c);
    cv_text(190, 5, &FONT_XS, number, C_WHITE);
    cv_text(163, 19, &FONT_XS, playing && recording ? "REC" : recording ? "ARM" : playing ? "PLAY" : "STOP", c);
    decimal(number, (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) + 1u);
    cv_text(202, 19, &FONT_XS, number, C_WHITE);
    cv_blit(0, 48);
    if (slap_ui.page == SAVE) {
        for (int strip = 0; strip < 4; ++strip) {
            cv_begin(240, 40, background);
            if (strip < 2) for (int i = 0; i < 4; ++i) {
                int slot = strip * 4 + i;
                int x = 8 + 57 * i;
                decimal(number, (unsigned)slot + 1u);
                cv_rect(x, 4, 50, 30, slot == slap_ui.slot ? c : RGB(27,36,54));
                block_text(x + 18, 8, 3, number, slot == slap_ui.slot ? C_BLACK : C_WHITE);
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
        cv_text(8, 21, &FONT_XS, "SELECT TRACK  /  K1 GROUP  /  K2 PATCH", C_WHITE);
        cv_blit(0, 80);
        int current = (int)tracks[selected].patch;
        int count = selected == DRUMS ? DRUM_KITS : SLAP_PATCHES;
        int first = clamp(current-1, 0, count-3);
        for (int row = 0; row < 3; ++row) {
            int item = first + row;
            const char *name = selected == DRUMS ? drum_kits[item].name : slap_patches[item].name;
            cv_begin(240, 30, background);
            cv_rect(7, 2, 226, 26, item == current ? c : RGB(27,37,55));
            const felucca_font_t *font = text_w(&FONT_M, name) < 210 ? &FONT_M : &FONT_B;
            cv_text(14, 4, font, name, item == current ? C_BLACK : C_WHITE);
            cv_blit(0, 120+row*30);
        }
        cv_begin(240, 30, background);
        cv_rect(7, 14, 226, 1, RGB(45,58,83));
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
        cv_text(8, 3, &FONT_M, drum_style[gen_style].name, c);
        cv_text(8, 27, &FONT_XS, gen_confirm ? "SEL AGAIN TO REPLACE" :
                playing ? "STOP TO GENERATE" : gen_enabled ? "SEL GENERATE   /   ARP OFF" : "GEN OFF   /   ARP ON", C_WHITE);
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
            cv_rect(8, 18, 224, 1, RGB(39,50,71));
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
                cv_rect(x, 0, 12, 36, RGB(22,32,50));
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
                cv_rect(x, 22 - strip*36, 17, 66, active ? c : RGB(39,50,71));
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
                cv_rect(x, 0, 12, 36, index % 4 == 0 ? RGB(31,39,57) : RGB(19,25,39));
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
    cv_begin(240, 48, background);
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
        values[3] = slap_ui.buttons & (1u << B_SEL) ? timing_of(word) : (word >> 24) & 127u;
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
    uint32_t now = plat_ms();
    if (now - slap_ui.frame_at >= 45u) {
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
    else if (name_matches(name, "level")) *result = (int)((tracks[slap_ui.track].mix >> 16) & 255u);
    else if (name_matches(name, "visual_left")) *result = visual_rms[0] > 0.000001f;
    else if (name_matches(name, "visual_right")) *result = visual_rms[1] > 0.000001f;
    else if (name_matches(name, "visual_spectrum")) {
        float power = 0.0f;
        for (unsigned band = 0; band < SPECTRUM_BANDS; ++band) power += visual_band[band];
        *result = power > 0.000001f;
    }
    else if (name_matches(name, "metronome")) *result = (int)metronome;
#if FM1B_CPU2 && !defined(PLAT_WEB)
    else if (name_matches(name, "worker_jobs")) *result = (int)worker_jobs;
    else if (name_matches(name, "worker_fallbacks")) *result = (int)worker_fallbacks;
#endif
    else if (name_matches(name, "gen_count")) *result = (int)gen_count;
    else if (name_matches(name, "rec_running")) *result = (int)rec_running;
    else if (name_matches(name, "rec_countdown")) *result = (int)rec_countdown;
    else if (name_matches(name, "note_offset"))
        *result = (int)timing_of(*event_cell(slap_ui.track, slap_ui.step, slap_ui.lane));
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
    else if (name_matches(name, "length")) *result = tracks[slap_ui.track].mix & 255u;
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
    playing = !!run;
    ++sequence_epoch;
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
    *event_cell(track, step, lane) = word;
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
    tracks[track].steps[step] = word;
}

STUDIO_EXPORT void studio_set_track(uint32_t track, uint32_t mix, uint32_t patch)
{
    if (track >= TRACKS || !(mix & 255u) || (mix & 255u) > STEPS || (mix >> 24) > 1 ||
        patch >= (track == DRUMS ? DRUM_KITS : SLAP_PATCHES)) return;
    if (track == DRUMS) tracks[track].patch = patch;
    else if (patch != tracks[track].patch) load_patch(track, patch);
    tracks[track].mix = mix;
}

STUDIO_EXPORT void studio_set_clock(uint32_t bpm, uint32_t amount)
{
    if (bpm >= 40 && bpm <= 240) tempo = bpm;
    if (amount <= 70) swing = amount;
}

STUDIO_EXPORT const char *studio_patch_name(uint32_t patch)
{
    return patch < SLAP_PATCHES ? slap_patches[patch].name : "";
}

STUDIO_EXPORT void studio_audition(uint32_t track, uint32_t pitch)
{
    if (track < TRACKS && pitch < 128) trigger(track, pitch, 102, RATE * 2 / 5, 0);
}
#pragma pop_macro("__attribute__")
#endif
