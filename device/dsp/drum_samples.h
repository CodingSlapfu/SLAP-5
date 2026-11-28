/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t offset;
    uint16_t frames;
} DrumSample;

typedef struct {
    uint16_t id, cursor;
    int16_t previous, predictor;
    uint8_t index, quarter, packed_byte;
} DrumSampleVoice;

#include "slap_drum_samples.inc"

enum { SAMPLE_KITS = 25, SAMPLE_HITS = 16 };
_Static_assert(sizeof(drums_sample_index) / sizeof(*drums_sample_index) ==
               SAMPLE_KITS * SAMPLE_HITS, "Expected all sample assignments");

/* GM mappings. Unassigned notes cover all 16 slots chromatically. */
static unsigned drum_sample_hit(unsigned note)
{
    switch (note) {
    case 36: return 0; case 35: return 1; case 38: return 2; case 40: return 3;
    case 42: return 4; case 46: return 5; case 44: return 6; case 39: return 7;
    case 37: return 8; case 45: return 9; case 47: return 10; case 50: return 11;
    case 49: return 12; case 51: return 13; case 70: return 14; case 81: return 15;
    default: return note & 15u;
    }
}

/* The bank is immutable and cursors are private to each copied Voice. */
static inline __attribute__((always_inline)) int drum_sample_next(DrumSampleVoice *v, int *finished)
{
    static const uint16_t steps[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
        73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
        449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,
        2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,
        7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
        24623,27086,29794,32767
    };
    static const int8_t adjust[8] = {-1,-1,-1,-1,2,4,6,8};
    if (!v->quarter) {
        const DrumSample *sample = &drums_sample_index[v->id];
        if (v->cursor >= sample->frames) {
            *finished = 1;
            return 0;
        }
        // Both nibbles come from the same XIP byte. Keep it in the voice
        // across the four interpolated samples and across callback boundaries.
        unsigned high = v->cursor & 1u;
        if (!high) v->packed_byte = drum_sample_data[sample->offset + (v->cursor >> 1)];
        unsigned code = high ? v->packed_byte >> 4 : v->packed_byte & 15u;
        ++v->cursor;
        unsigned magnitude = code & 7u;
        int step = steps[v->index], delta = step >> 3;
        if (magnitude & 4u) delta += step;
        if (magnitude & 2u) delta += step >> 1;
        if (magnitude & 1u) delta += step >> 2;
        int value = v->predictor + (code & 8u ? -delta : delta);
        int index = v->index + adjust[magnitude];
        v->previous = v->predictor;
        v->predictor = (int16_t)(value < -32768 ? -32768 : value > 32767 ? 32767 : value);
        v->index = (uint8_t)(index < 0 ? 0 : index > 88 ? 88 : index);
    }
    int value = v->previous + ((v->predictor - v->previous) * v->quarter >> 2);
    v->quarter = (v->quarter + 1u) & 3u;
    return value;
}
