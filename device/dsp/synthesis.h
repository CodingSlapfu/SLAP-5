/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once
#include <stdint.h>

enum { SLAP_ENGINES = 25, SLAP_PATCHES = 128, SLAP_STRING_CAPACITY = 1024 };

typedef struct {
    const char *name;
    uint8_t engine, color, motion, attack, decay, sustain, release;
} SlapPatch;

typedef struct {
    float phase, second, third, slow, pole, feedback, strike;
    float inc, color, motion, slow_inc;
    uint32_t rng;
    uint16_t length, index, pickup;
    uint8_t engine, modern;
    int16_t *string;
} SlapSynth;

extern const char *const slap_engine_names[SLAP_ENGINES];
extern const SlapPatch slap_patches[SLAP_PATCHES];

float slap_pitch_increment(unsigned pitch);
void slap_synth_start(SlapSynth *synth, int16_t *string, unsigned engine, unsigned pitch,
                      unsigned color, unsigned motion, uint32_t seed, int modern);
float slap_synth_sample(SlapSynth *synth);
