/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "fastmath.h"

enum { TRACKS = 5, STEPS = 64, VOICES = 16, RATE = 44100 };
enum { HOME, SEQ, GLOBAL };
enum { NOTES = 0x7f, ACTIVE = 0x80, DRUMS = 4 };

typedef struct {
    volatile uint32_t steps[STEPS]; /* pitch, active flag, velocity, length, probability */
    volatile uint32_t mix;          /* LSB first: length, pan, level, mute */
} Track;

typedef struct {
    float phase, mod, inc, env, velocity, fall, bend;
    uint32_t remaining, age;
    uint8_t track, pitch, manual, releasing;
} Voice;

static Track tracks[TRACKS];
static Voice voices[VOICES];
static volatile uint32_t tempo = 112, swing, playing, positions, sequence_epoch;
static volatile uint32_t keys_audio, input_track, input_octave;
static uint32_t old_keys, key_pitches[27], key_tracks[27], ages, noise = 13579;
static uint32_t until_step, step_number, running_epoch;
static float master_gain;
static struct {
    uint32_t buttons, keys, frame_at;
    int page, track, step, octave;
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

void app_init(void)
{
    static const uint8_t bass[] = {36, 36, 39, 43, 34, 34, 39, 41};
    static const uint8_t lead[] = {72, 67, 70, 75, 74, 67, 65, 70};
    for (unsigned t = 0; t < TRACKS; ++t)
        tracks[t].mix = 16u | (128u << 8) | (170u << 16);
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

static uint32_t random_word(void)
{
    noise ^= noise << 13;
    noise ^= noise >> 17;
    noise ^= noise << 5;
    return noise;
}

static void trigger(unsigned track, unsigned pitch, unsigned velocity, uint32_t duration, int manual)
{
    Voice *voice = &voices[0];
    for (unsigned i = 0; i < VOICES; ++i) {
        if (!voices[i].age) { voice = &voices[i]; break; }
        if (voices[i].age < voice->age) voice = &voices[i];
    }
    *voice = (Voice) {
        .inc = 440.0f * fm_exp2f(((float)pitch - 69.0f) / 12.0f) / RATE,
        .velocity = (float)velocity / 127.0f, .fall = 1.0f, .bend = 1.0f,
        .remaining = duration, .age = ++ages,
        .track = (uint8_t)track, .pitch = (uint8_t)pitch, .manual = (uint8_t)manual
    };
}

static void release_note(unsigned track, unsigned pitch)
{
    for (unsigned i = 0; i < VOICES; ++i)
        if (voices[i].manual && voices[i].pitch == pitch && voices[i].track == track)
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
    if (!v->manual && v->remaining && !--v->remaining) v->releasing = 1;
    float target = v->releasing ? 0.0f : 1.0f;
    v->env += (target - v->env) * (v->releasing ? 0.00065f : 0.013f);
    if (v->releasing && v->env < 0.0001f) { v->age = 0; return 0.0f; }

    float wave;
    float inc = v->inc;
    v->mod += v->inc * (v->track == 1 ? 3.0f : 1.5f);
    if (v->mod >= 1.0f) v->mod -= 1.0f;
    switch (v->track) {
    case 0:
        wave = 0.75f * slap_sine(v->phase) + 0.25f * slap_sine(2.0f * v->phase);
        break;
    case 1:
        v->fall *= 0.99991f;
        wave = slap_sine(v->phase + 0.22f * v->fall * slap_sine(v->mod));
        break;
    case 2:
        wave = 0.7f * slap_sine(v->phase) + 0.22f * slap_sine(v->mod);
        break;
    case 3:
        v->fall *= 0.99952f;
        wave = 0.64f * slap_sine(v->phase) + 0.36f * slap_sine(v->phase * 4.0f);
        break;
    default: {
        float hiss = (float)(random_word() & 65535u) / 32768.0f - 1.0f;
        unsigned family = v->pitch % 6u;
        if (!family) {
            v->bend *= 0.9988f;
            inc = 0.0011f + 0.0032f * v->bend;
            v->fall *= 0.99955f;
            wave = slap_sine(v->phase);
        } else if (family <= 2) {
            inc = 0.0045f;
            v->fall *= 0.9989f;
            wave = 0.78f * hiss + 0.22f * slap_sine(v->phase);
        } else {
            v->fall *= 0.9971f;
            wave = hiss * (v->phase < 0.5f ? 0.75f : 0.37f);
        }
        break;
    }
    }
    v->phase += inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    if (v->fall < 0.00005f) { v->age = 0; return 0.0f; }
    return wave * v->env * v->velocity * v->fall * 0.15f;
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
    float target = master_q12 * (1.0f / 4096.0f);
    for (uint32_t i = 0; i < frames; ++i) {
        if (playing && until_step == 0) next_step(mix);
        if (until_step) --until_step;
        float l = 0.0f, r = 0.0f;
        for (unsigned j = 0; j < VOICES; ++j) {
            Voice *v = &voices[j];
            if (!v->age) continue;
            float sample = render_voice(v);
            l += sample * left[v->track];
            r += sample * right[v->track];
        }
        master_gain += (target - master_gain) * 0.003f;
        l /= 1.0f + fm_fabsf(l);
        r /= 1.0f + fm_fabsf(r);
        lr[i * 2] = (int32_t)(l * master_gain * 8300000.0f);
        lr[i * 2 + 1] = (int32_t)(r * master_gain * 8300000.0f);
    }
}

void app_panic(void)
{
    playing = 0;
    for (unsigned i = 0; i < VOICES; ++i) voices[i].age = 0;
    master_gain = 0.0f;
}

void app_input_only(void)
{
    uint32_t buttons = plat_buttons(), keys = plat_keys();
    uint32_t pressed = buttons & ~slap_ui.buttons, notes = keys & ~slap_ui.keys;
    if (pressed & (1u << B_PLAY)) { playing ^= 1u; ++sequence_epoch; }
    if (pressed & (1u << B_HOME)) slap_ui.page = HOME;
    if (pressed & (1u << B_SEQ)) slap_ui.page = SEQ;
    if (pressed & (1u << B_GLO)) slap_ui.page = GLOBAL;
    if (pressed & (1u << B_SEL) && slap_ui.page == GLOBAL) {
        unsigned mute = !(tracks[slap_ui.track].mix >> 24);
        track_setting(slap_ui.track, 24, mute);
    }
    if (pressed & (1u << B_OCTDN)) slap_ui.octave = clamp(slap_ui.octave - 1, 0, 4);
    if (pressed & (1u << B_OCTUP)) slap_ui.octave = clamp(slap_ui.octave + 1, 0, 4);
    int delta = plat_enc(EN_SELECT);
    if (slap_ui.page == SEQ)
        slap_ui.step = clamp(slap_ui.step + delta, 0, (int)(tracks[slap_ui.track].mix & 255u) - 1);
    else
        slap_ui.track = clamp(slap_ui.track + delta, 0, TRACKS - 1);
    tempo = (uint32_t)clamp((int)tempo + plat_enc(EN_ALGO), 40, 240);
    (void)plat_enc(EN_PRESET);
    int knob[4];
    for (unsigned k = 0; k < 4; ++k) knob[k] = plat_enc(EN_K1 + k);
    uint32_t mix = tracks[slap_ui.track].mix;
    if (slap_ui.page == SEQ) {
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
    } else {
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

static void draw_panel(void)
{
    static const uint16_t colors[] = {
        RGB(28,226,255), RGB(171,112,255), RGB(255,89,176), RGB(255,180,58), RGB(62,242,137)
    };
    static const char *const names[] = {"SUB BASS", "FM GLASS", "SOFT KEYS", "HARMONIC", "DRUMS"};
    static const char *const labels[] = {"PAN", "STEPS", "LEVEL", "SWING"};
    static const char *const edit_labels[] = {"NOTE", "VEL", "GATE", "HIT %"};
    char number[4];
    unsigned selected = (unsigned)slap_ui.track;
    uint32_t position = positions;
    uint32_t settings = tracks[selected].mix;
    unsigned length = settings & 255u, play_step = position % length;
    unsigned bank = (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) / 16u * 16u;
    uint16_t c = colors[selected], background = RGB(10,14,24);
    cv_begin(240, 48, background);
    cv_text(8, 1, &FONT_M, slap_ui.page == SEQ ? "SEQUENCE" : slap_ui.page == GLOBAL ? "GLOBAL" : "SLAP-5", C_WHITE);
    decimal(number, tempo);
    cv_text(164, 1, &FONT_M, number, C_WHITE);
    cv_text(210, 10, &FONT_XS, "BPM", C_WHITE);
    for (unsigned t = 0; t < TRACKS; ++t) {
        decimal(number, t + 1u);
        cv_rect(8 + (int)t * 46, 28, 40, 18, t == selected ? colors[t] : RGB(27,36,54));
        cv_text(25 + (int)t * 46, 30, &FONT_XS, number, t == selected ? C_BLACK : colors[t]);
    }
    cv_blit(0, 0);
    cv_begin(240, 32, background);
    cv_text(8, 1, &FONT_M, names[selected], C_WHITE);
    decimal(number, position / 16u % 999u + 1u);
    cv_text(163, 5, &FONT_XS, "BAR", c);
    cv_text(190, 5, &FONT_XS, number, C_WHITE);
    cv_text(163, 19, &FONT_XS, playing ? "PLAY" : "STOP", c);
    decimal(number, (slap_ui.page == SEQ ? (unsigned)slap_ui.step : play_step) + 1u);
    cv_text(202, 19, &FONT_XS, number, C_WHITE);
    cv_blit(0, 48);
    for (int strip = 0; strip < 3; ++strip) {
        cv_begin(240, 36, background);
        for (unsigned s = 0; s < 16; ++s) {
            if (bank + s >= length) break;
            int x = 8 + (int)s * 14;
            uint32_t word = tracks[selected].steps[bank + s];
            cv_rect(x, 0, 12, 36, s % 4 == 0 ? RGB(31,39,57) : RGB(19,25,39));
            if (bank + s == play_step && playing) cv_rect(x, 0, 2, 36, C_WHITE);
            if (slap_ui.page == SEQ && bank + s == (unsigned)slap_ui.step) cv_rect(x + 9, 0, 3, 36, c);
            int y = 100 - (int)((word & NOTES) % 24u) * 4 - strip * 36;
            if (word & ACTIVE) cv_rect(x + 2, y, 7, 5, c);
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
    for (unsigned k = 0; k < 4; ++k) {
        int x = 8 + (int)k * 58;
        cv_text(x, 2, &FONT_XS, slap_ui.page == SEQ ? edit_labels[k] : labels[k], c);
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
        draw_panel();
    }
}

void app_ui_init(void)
{
    slap_ui.octave = 2;
    input_octave = 2;
    draw_panel();
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
    else if (name_matches(name, "length")) *result = tracks[slap_ui.track].mix & 255u;
    else if (name_matches(name, "pitch")) {
        uint32_t word = tracks[slap_ui.track].steps[slap_ui.step];
        *result = word & ACTIVE ? (int)(word & NOTES) : -1;
    }
    else if (name_matches(name, "page")) *result = slap_ui.page;
    else return 0;
    return 1;
}
