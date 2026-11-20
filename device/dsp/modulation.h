/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once

enum { HARMONY_KEYS = 12, HARMONY_SCALES = 6, HARMONY_CHORDS = 4 };
static const char *const harmony_keys[HARMONY_KEYS] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
static const char *const harmony_scales[HARMONY_SCALES] = {
    "CHROMATIC","MAJOR","MINOR","DORIAN","PENTA","BLUES"
};
static const char *const harmony_chords[HARMONY_CHORDS] = {
    "OFF","TRIAD","SEVENTH","FIFTH"
};
static const char *const lfo_waves[6] = {"OFF","SINE","TRI","SAW","SQUARE","S & H"};
static const uint16_t scale_tones[HARMONY_SCALES] = {
    0x0fffu, 0x0ab5u, 0x05adu, 0x06adu, 0x0295u, 0x04e9u
};

static unsigned note_in_scale(unsigned note, unsigned root, unsigned scale)
{
    while (note && !(scale_tones[scale] & (1u << ((note + 12 - root) % 12u)))) --note;
    return note;
}

static unsigned scale_degree(unsigned note, unsigned degrees, unsigned root, unsigned scale)
{
    while (degrees && note < 127u) {
        ++note;
        if (scale_tones[scale] & (1u << ((note + 12 - root) % 12u))) --degrees;
    }
    return note;
}

static inline __attribute__((always_inline)) float lfo_shape(unsigned kind, float phase, float held)
{
    switch (kind) {
    case 1: return lfo_sine(phase);
    case 2: return 1.0f - 4.0f * fm_fabsf(phase - 0.5f);
    case 3: return 2.0f * phase - 1.0f;
    case 4: return phase < 0.5f ? 1.0f : -1.0f;
    case 5: return held;
    default: return 0.0f;
    }
}
