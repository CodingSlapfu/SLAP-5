/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "fastmath.h"
#include "synthesis.h"
#include "effects.h"

enum { TRACKS = 5, STEPS = 64, VOICES = 16, RATE = 44100 };
enum { HOME, SEQ, GLOBAL, PATCH, ENV, FX, VISUAL, SAVE, HARMONY, LFO };
enum { NOTES = 0x7f, ACTIVE = 0x80, DRUMS = 4 };

typedef struct {
    volatile uint32_t steps[STEPS]; /* pitch, active flag, velocity, length, probability */
    volatile uint32_t patch, envelope, harmony, lfo;
    volatile uint32_t mix;          /* LSB first: length, pan, level, mute */
} Track;

typedef struct {
    float phase, inc, env, velocity, fall, bend;
    float attack, decay, sustain, release;
    float lfo_phase, lfo_inc, lfo_fade, lfo_gain, lfo_hold;
    uint32_t delay;
    uint8_t lfo_wave, origin, kit;
    uint32_t remaining, age;
    uint8_t track, pitch, manual, releasing, struck;
} Voice;

static Track tracks[TRACKS];
static Voice voices[VOICES];
static PLAT_POOL SlapSynth synths[VOICES];
static volatile uint32_t tempo = 112, swing, playing, positions, sequence_epoch;
static volatile uint32_t keys_audio, input_track, input_octave;
static uint32_t old_keys, key_pitches[27], key_tracks[27], ages, noise = 13579;
static uint32_t until_step, step_duration, step_number, running_epoch;
static volatile uint32_t step_progress;
static uint32_t startup_sample;
static float startup_phase;
static float master_gain, energy_smooth[TRACKS];
static volatile uint32_t track_energy[TRACKS];
static struct {
    uint32_t buttons, keys, frame_at, boot_at, booting;
    int page, track, step, octave, fx_page, visual, slot;
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
    for (unsigned t = 0; t < DRUMS; ++t) tracks[t].lfo = 31u << 8;
    static const uint8_t bass[] = {36, 36, 39, 43, 34, 34, 39, 41};
    static const uint8_t lead[] = {72, 67, 70, 75, 74, 67, 65, 70};
    for (unsigned t = 0; t < TRACKS; ++t)
        tracks[t].mix = 16u | (128u << 8) | (170u << 16);
    static const uint8_t defaults[] = {67, 0, 78, 37};
    for (unsigned t = 0; t < DRUMS; ++t)
        load_patch(t, defaults[t]);
    for (unsigned i = 0; i < 8; ++i) {
        tracks[0].steps[2*i] = cell_word(bass[i], 105, 175, 100);
        tracks[1].steps[(3*i) % 16] = cell_word(lead[i], 70, 112, 100);
    }
    tracks[2].steps[0] = cell_word(51, 65, 255, 100);
    tracks[2].steps[8] = cell_word(53, 58, 255, 100);
    tracks[3].steps[3] = cell_word(79, 68, 130, 100);
    tracks[3].steps[11] = cell_word(77, 73, 130, 100);
    for (unsigned i = 0; i < 16; i += 2)
        tracks[DRUMS].steps[i] = cell_word(i % 8 == 0 ? 36 : i % 8 == 4 ? 38 : 42, 110, 100, 100);
}

static float slap_sine(float phase) { return fm_sinf(phase * 6.2831853f); }

#include "modulation.h"

static uint32_t random_word(void)
{
    noise ^= noise << 13;
    noise ^= noise >> 17;
    noise ^= noise << 5;
    return noise;
}

static const char *const drum_kits[] = {"STUDIO KIT","BRUSH KIT","FM KIT","INDUSTRIAL"};

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
        if (voices[i].age < voice->age) voice = &voices[i];
    }
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
        voice->kit = (uint8_t)tracks[track].patch;
        voice->attack = 0.013f;
        voice->sustain = 1.0f;
        voice->release = 0.00065f;
    }
}

static void trigger(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration, int manual)
{
    uint32_t harmony = tracks[track].harmony;
    unsigned root = harmony & 255u, scale = (harmony >> 8) & 255u;
    unsigned chord = (harmony >> 16) & 255u, strum = harmony >> 24;
    if (track == DRUMS || (!scale && !chord)) {
        trigger_voice(track, pitch, velocity, duration, manual, pitch, 0);
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
                      duration, manual, pitch, delay);
        previous = pitches[i];
    }
}

static void release_note(unsigned track, unsigned pitch)
{
    for (unsigned i = 0; i < VOICES; ++i)
        if (voices[i].manual && voices[i].origin == pitch && voices[i].track == track)
            voices[i].releasing = 1;
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
                trigger(track, pitch, 100, 0, 1);
                plat_midi_send(0x09u | (0x90u | track) << 8 | pitch << 16 | 100u << 24, TO_USB);
            } else {
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
        if ((status & 0xf0u) == 0x90u && velocity)
            trigger(track, pitch, velocity, 0, 1);
        else if ((status & 0xf0u) == 0x80u || (status & 0xf0u) == 0x90u)
            release_note(track, pitch);
    }
}

static void next_step(const uint32_t *mix)
{
    unsigned base = RATE * 15u / tempo;
    unsigned offset = base * swing / 200u;
    until_step = step_number & 1u ? base - offset : base + offset;
    step_duration = until_step;
    positions = step_number++;
    for (unsigned t = 0; t < TRACKS; ++t) {
        uint32_t word = tracks[t].steps[positions % (mix[t] & 255u)];
        if ((word & ACTIVE) && (word >> 24) > random_word() % 100u)
            trigger(t, word & NOTES, (word >> 8) & 127u,
                    until_step * ((word >> 16) & 255u) / 192u, 0);
    }
}

static float render_voice(Voice *v)
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
        SlapSynth *s = &synths[v - voices];
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
        float hiss = (float)(random_word() & 65535u) / 32768.0f - 1.0f;
        unsigned family = drum_family(v->pitch);
        unsigned kit = v->kit;
        if (family == 0) {
            v->bend *= 0.9988f;
            inc = 0.00108f + tone[kit] * v->bend;
            v->fall *= 0.99966f - kit * 0.00008f;
            wave = slap_sine(v->phase) + (kit == 3 ? 0.19f * hiss * v->bend : 0.0f);
        } else if (family == 1 || family == 3) {
            inc = tone[kit] * (family == 3 ? 3.17f : 1.0f);
            v->bend *= 0.9972f;
            v->fall *= family == 3 ? 0.9977f : 0.99906f;
            float skin = kit == 2 ? slap_sine(v->phase + 0.25f*v->bend*slap_sine(v->phase*3.371f))
                                  : slap_sine(v->phase);
            float noise_mix = kit == 1 ? 0.91f : family == 3 ? 0.34f : 0.68f;
            wave = noise_mix * hiss + (1.0f - noise_mix) * skin * v->bend;
        } else if (family == 4) {
            v->fall *= 0.99902f;
            v->bend *= 0.999f;
            inc = 0.008f * pitch_mul[kit];
            wave = hiss * (v->phase < 0.18f ? 0.95f : 0.23f);
        } else {
            inc = 0.052f * pitch_mul[kit];
            v->fall *= family == 5 ? 0.99968f : kit == 1 ? 0.998f : 0.9969f;
            float metal = slap_sine(v->phase) * slap_sine(v->phase*3.79f);
            wave = 0.65f * hiss + (kit > 1 ? 0.4f : 0.13f) * metal;
        }
    }
    v->phase += inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    if (v->fall < 0.00005f) { v->age = 0; return 0.0f; }
    return wave * v->env * v->velocity * v->fall * 0.15f;
}

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
    uint32_t mix[TRACKS];
    float left[TRACKS], right[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t) {
        mix[t] = tracks[t].mix;
        float pan = ((mix[t] >> 8) & 255u) / 255.0f;
        float level = mix[t] >> 24 ? 0.0f : ((mix[t] >> 16) & 255u) / 255.0f;
        left[t] = level * (1.0f - pan);
        right[t] = level * pan;
    }
    uint32_t epoch = sequence_epoch;
    if (running_epoch != epoch) {
        running_epoch = epoch;
        until_step = step_number = 0;
        for (unsigned i = 0; i < VOICES; ++i)
            if (!voices[i].manual) voices[i].releasing = 1;
    }
    read_notes();
    slap_fx_begin(tempo);
    float target = master_q12 * (1.0f / 4096.0f);
    for (uint32_t i = 0; i < frames; ++i) {
        if (playing && until_step == 0) next_step(mix);
        if (until_step) --until_step;
        float dry[TRACKS] = {0};
        for (unsigned j = 0; j < VOICES; ++j) {
            Voice *v = &voices[j];
            if (!v->age) continue;
            dry[v->track] += render_voice(v);
        }
        float l = 0.0f, r = 0.0f;
        for (unsigned t = 0; t < TRACKS; ++t) {
            float sample = slap_fx_sample(t, dry[t]);
            energy_smooth[t] += (fm_fabsf(sample) - energy_smooth[t]) * 0.0015f;
            l += sample * left[t];
            r += sample * right[t];
        }
        if (!playing && startup_sample < RATE * 42u / 100u) {
            float tone = startup_sound();
            l += tone;
            r += tone;
        } else if (playing) {
            startup_sample = RATE;
        }
        master_gain += (target - master_gain) * 0.003f;
        l /= 1.0f + fm_fabsf(l);
        r /= 1.0f + fm_fabsf(r);
        lr[i * 2] = (int32_t)(l * master_gain * 8300000.0f);
        lr[i * 2 + 1] = (int32_t)(r * master_gain * 8300000.0f);
    }
    step_progress = playing && step_duration ? 14u - until_step * 14u / step_duration : 0u;
    for (unsigned t = 0; t < TRACKS; ++t) {
        track_energy[t] = (uint32_t)fm_minf(160.0f, energy_smooth[t] * 1850.0f);
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
} SongRecord;

static SongRecord song_io;
static const uint8_t save_keys[16] = PLAT_WHITE_KEYS;

static int step_valid(uint32_t word)
{
    return !word || ((word & ACTIVE) && ((word >> 8) & 127u) && !(word & 0x8000u) &&
                      ((word >> 16) & 255u) && (word >> 24) <= 100);
}

static int read_song(unsigned slot)
{
    if (plat_store_load(OBJ_SONG1 + slot, &song_io, sizeof(song_io)) != (int)sizeof(song_io) ||
        song_io.magic != SONG_MAGIC || song_io.version != 2 ||
        song_io.bpm < 40 || song_io.bpm > 240 || song_io.swing > 70) return 0;
    for (unsigned t = 0; t < TRACKS; ++t) {
        uint32_t mix = song_io.track[t].mix;
        if (!(mix & 255u) || (mix & 255u) > STEPS || (mix >> 24) > 1 ||
            song_io.track[t].patch >= (t == DRUMS ? 4u : SLAP_PATCHES) || (song_io.track[t].envelope & 0x80808080u)) return 0;
        uint32_t h = song_io.track[t].harmony, l = song_io.track[t].lfo;
        if ((h & 255u) >= HARMONY_KEYS || ((h >> 8) & 255u) >= HARMONY_SCALES ||
            ((h >> 16) & 255u) >= HARMONY_CHORDS || (h >> 24) > 101u ||
            (l & 0x80808080u) || (l & 255u) > 5u) return 0;
        for (unsigned i = 0; i < STEPS; ++i)
            if (!step_valid(song_io.track[t].steps[i])) return 0;
        for (unsigned i = 0; i < SLAP_FX_PAGES; ++i)
            if (!slap_fx_config_valid(i, song_io.track[t].fx[i])) return 0;
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
    slap_ui.step = 0;
    ++sequence_epoch;
    slap_ui.notice = "LOADED";
}

static void write_song(unsigned slot)
{
    song_io.magic = SONG_MAGIC;
    song_io.version = 2;
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

void app_input_only(void)
{
    uint32_t buttons = plat_buttons(), keys = plat_keys();
    uint32_t pressed = buttons & ~slap_ui.buttons, notes = keys & ~slap_ui.keys;
    if (pressed & (1u << B_PLAY)) { playing ^= 1u; ++sequence_epoch; }
    if (pressed & (1u << B_HOME))
        slap_ui.page = slap_ui.page == HOME ? VISUAL : HOME;
    if (pressed & (1u << B_SEQ)) slap_ui.page = SEQ;
    if (pressed & (1u << B_GLO)) slap_ui.page = GLOBAL;
    if (pressed & (1u << B_SAVE)) {
        slap_ui.page = SAVE;
        slap_ui.save_armed = 0;
        slap_ui.notice = 0;
    }
    if (pressed & (1u << B_FX)) slap_ui.page = FX;
    if (pressed & (1u << B_SEL) && slap_ui.page == HOME && slap_ui.track < DRUMS) slap_ui.page = HARMONY;
    if (pressed & (1u << B_LFO) && slap_ui.track < DRUMS) slap_ui.page = LFO;
    if (pressed & (1u << B_EDIT) && slap_ui.track < DRUMS) slap_ui.page = PATCH;
    if (pressed & (1u << B_ENV) && slap_ui.track < DRUMS) slap_ui.page = ENV;
    if (pressed & (1u << B_SEL) && slap_ui.page == GLOBAL) {
        unsigned mute = !(tracks[slap_ui.track].mix >> 24);
        track_setting(slap_ui.track, 24, mute);
    }
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
    if (slap_ui.page == SEQ)
        slap_ui.step = clamp(slap_ui.step + delta, 0, (int)(tracks[slap_ui.track].mix & 255u) - 1);
    else if (slap_ui.page == VISUAL)
        slap_ui.visual = clamp(slap_ui.visual + delta, 0, 8);
    else if (slap_ui.page == FX)
        slap_ui.fx_page = clamp(slap_ui.fx_page + delta, 0, SLAP_FX_PAGES - 1);
    else if (slap_ui.page == HOME || slap_ui.page == GLOBAL)
        slap_ui.track = clamp(slap_ui.track + delta, 0, TRACKS - 1);
    tempo = (uint32_t)clamp((int)tempo + plat_enc(EN_ALGO), 40, 240);
    int patch_delta = plat_enc(EN_PRESET);
    if (slap_ui.page == PATCH) patch_delta += delta;
    if (patch_delta) {
        int old_patch = (int)tracks[slap_ui.track].patch;
        if (slap_ui.track == DRUMS)
            tracks[DRUMS].patch = (unsigned)clamp(old_patch + patch_delta, 0, 3);
        else load_patch(slap_ui.track, (unsigned)clamp(old_patch + patch_delta, 0, SLAP_PATCHES-1));
    }
    int knob[4];
    for (unsigned k = 0; k < 4; ++k) knob[k] = plat_enc(EN_K1 + k);
    uint32_t mix = tracks[slap_ui.track].mix;
    if (slap_ui.page == FX) {
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(slap_ui.track, slap_ui.fx_page, k, knob[k]);
    } else if (slap_ui.page == SEQ) {
        volatile uint32_t *cell = &tracks[slap_ui.track].steps[slap_ui.step];
        uint32_t word = *cell;
        if (notes) {
            unsigned k = 0;
            while (!(notes & (1u << k))) ++k;
            unsigned pitch = 29u + k + (unsigned)slap_ui.octave * 12u;
            word = (word & 255u) == (pitch | ACTIVE) ? 0u : cell_word(pitch, 100, 140, 100);
        }
        if (word & ACTIVE) {
            word = cell_word((unsigned)clamp((int)(word & NOTES) + knob[0], 0, 127),
                (unsigned)clamp((int)((word >> 8) & 127u) + knob[1] * 3, 1, 127),
                (unsigned)clamp((int)((word >> 16) & 255u) + knob[2] * 5, 1, 255),
                (unsigned)clamp((int)(word >> 24) + knob[3] * 5, 0, 100));
        }
        *cell = word;
    } else if (slap_ui.page == HARMONY || slap_ui.page == LFO) {
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
    } else if (slap_ui.page == ENV) {
        uint32_t env = tracks[slap_ui.track].envelope;
        for (unsigned k = 0; k < 4; ++k) {
            unsigned shift = k * 8u;
            int v = clamp((int)((env >> shift) & 255u) + knob[k] * 2, 0, 127);
            env = (env & ~(255u << shift)) | (uint32_t)v << shift;
        }
        tracks[slap_ui.track].envelope = env;
    } else if (slap_ui.page != PATCH && slap_ui.page != VISUAL) {
        track_setting(slap_ui.track, 8, (unsigned)clamp((int)((mix >> 8) & 255u) + knob[0] * 4, 0, 255));
        track_setting(slap_ui.track, 0, (unsigned)clamp((int)(mix & 255u) + knob[1], 1, STEPS));
        track_setting(slap_ui.track, 16, (unsigned)clamp((int)((mix >> 16) & 255u) + knob[2] * 4, 0, 255));
        swing = (unsigned)clamp((int)swing + knob[3] * 2, 0, 70);
        slap_ui.step = clamp(slap_ui.step, 0, (int)(tracks[slap_ui.track].mix & 255u) - 1);
    }
    input_track = (unsigned)slap_ui.track;
    input_octave = (unsigned)slap_ui.octave;
    keys_audio = keys;
    slap_ui.keys = keys;
    slap_ui.buttons = buttons;
    plat_leds(buttons | (playing << B_PLAY), keys);
}

static void decimal(char *out, unsigned value)
{
    if (value >= 100) *out++ = (char)('0' + value / 100);
    if (value >= 10) *out++ = (char)('0' + value / 10 % 10);
    *out++ = (char)('0' + value % 10);
    *out = 0;
}

#include "visuals.h"

static void draw_panel(void)
{
    static const char *const page_names[] = {"SLAP-5", "SEQUENCE", "GLOBAL", "PATCH", "ENV", "FX", "VISUAL", "SAVE", "HARMONY", "LFO"};
    static const char *const lfo_labels[] = {"WAVE", "RATE", "PHS", "FADE"};
    static const char *const harmony_labels[] = {"KEY", "SCALE", "CHORD", "STRUM"};
    static const char *const env_labels[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    static const char *const labels[] = {"PAN", "STEPS", "LEVEL", "SWING"};
    static const char *const edit_labels[] = {"NOTE", "VEL", "GATE", "HIT %"};
    char number[4];
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
    for (unsigned t = 0; t < TRACKS; ++t) {
        decimal(number, t + 1u);
        cv_rect(8 + (int)t * 46, 28, 40, 18, t == selected ? track_color[t] : RGB(27,36,54));
        cv_text(25 + (int)t * 46, 30, &FONT_XS, number, t == selected ? C_BLACK : track_color[t]);
    }
    cv_blit(0, 0);
    cv_begin(240, 32, background);
    const SlapPatch *patch = &slap_patches[tracks[selected].patch];
    const char *name = slap_ui.page == SAVE ? "SONG SLOTS" : slap_ui.page == FX ? slap_fx_pages[slap_ui.fx_page] :
                       selected == DRUMS ? drum_kits[tracks[DRUMS].patch & 3u] : patch->name;
    const felucca_font_t *name_font = text_w(&FONT_M, name) > 150 ? &FONT_B : &FONT_M;
    cv_text(8, 1, name_font, name, C_WHITE);
    decimal(number, position / 16u % 999u + 1u);
    cv_text(163, 5, &FONT_XS, "BAR", c);
    cv_text(190, 5, &FONT_XS, number, C_WHITE);
    cv_text(163, 19, &FONT_XS, playing ? "PLAY" : "STOP", c);
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
        cv_text(8, 2, &FONT_B, slap_engine_names[patch->engine], c);
        cv_text(8, 21, &FONT_XS, "SELECT PATCH  /  PLAY KEYS", C_WHITE);
        cv_blit(0, 80);
        for (int y = 120; y < 240; y += 40) {
            cv_begin(240, 40, background);
            cv_blit(0, y);
        }
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
                int y = 100 - (int)((word & NOTES) % 24u) * 4 - strip * 36;
                int width = 2 + (int)((word >> 16) & 255u) * 13 / 192;
                if (word & ACTIVE) cv_rect(x + 2, y, width, 5, c);
            }
            if (scroll) cv_rect(50, 0, 1, 36, C_WHITE);
        }
        cv_blit(0, 80 + strip * 36);
    }
    cv_begin(240, 48, background);
    uint32_t word = tracks[selected].steps[slap_ui.step];
    unsigned values[] = { (settings >> 8) & 255u, length, ((settings >> 16) & 255u) * 100u / 255u, swing };
    if (slap_ui.page == SEQ) {
        values[0] = word & NOTES;
        values[1] = (word >> 8) & 127u;
        values[2] = (word >> 16) & 255u;
        values[3] = word >> 24;
    }
    if (slap_ui.page == ENV)
        for (unsigned k = 0; k < 4; ++k) values[k] = (env >> (k*8u)) & 255u;
    for (unsigned k = 0; k < 4; ++k) {
        int x = 8 + (int)k * 58;
        const char *label = slap_ui.page == FX ? slap_fx_labels[slap_ui.fx_page][k] :
                            slap_ui.page == LFO ? lfo_labels[k] : slap_ui.page == HARMONY ? harmony_labels[k] :
                            slap_ui.page == SEQ ? edit_labels[k] : slap_ui.page == ENV ? env_labels[k] : labels[k];
        if (!label) continue;
        cv_text(x, 2, &FONT_XS, label, c);
        if (slap_ui.page == LFO || slap_ui.page == HARMONY) {
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
        cv_text(x, 15, &FONT_M, number, C_WHITE);
    }
    cv_blit(0, 188);
}

void app_frame(void)
{
    app_input_only();
    uint32_t now = plat_ms();
    if (now - slap_ui.frame_at >= 45u) {
        slap_ui.frame_at = now;
        if (slap_ui.booting && now - slap_ui.boot_at < 500u)
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
    else if (name_matches(name, "playing")) *result = (int)playing;
    else if (name_matches(name, "step")) *result = (int)positions;
    else if (name_matches(name, "slot")) *result = slap_ui.slot + 1;
    else if (name_matches(name, "fx_page")) *result = slap_ui.fx_page + 1;
    else if (name_matches(name, "fx_k1")) *result = slap_fx_value(slap_ui.track, slap_ui.fx_page, 0);
    else if (name_matches(name, "patch")) *result = (int)tracks[slap_ui.track].patch;
    else if (name_matches(name, "length")) *result = tracks[slap_ui.track].mix & 255u;
    else if (name_matches(name, "pitch")) {
        uint32_t word = tracks[slap_ui.track].steps[slap_ui.step];
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
        patch >= (track == DRUMS ? 4u : SLAP_PATCHES)) return;
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
