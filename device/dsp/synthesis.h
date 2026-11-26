/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once
#include <stdint.h>

enum { SLAP_ENGINES = 25, SLAP_PATCHES = 136, SLAP_STRING_CAPACITY = 1024 };

typedef struct {
    const char *name;
    uint8_t engine, color, motion, attack, decay, sustain, release;
} SlapPatch;

typedef struct {
    float phase, second, third, slow, pole, feedback, strike;
    float inc, color, motion, slow_inc;
    union {
        struct { float gain, attack, decay, sustain, release; } operators[6];
        struct {
            float second_ratio, third_ratio, noise_filter, body;
            float second, second_strike, third_strike, noise_gain;
        } piano;
    };
    float operator_feedback;
    uint32_t rng, timbre;
    uint8_t operator_mask, operator_tick, operator_decay, operator_release;
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

/* An unset timbre uses the factory program. */
void slap_synth_timbre(SlapSynth *synth, uint32_t settings);
/* The unused algorithm code stores an explicit all-zero operator edit. */
#define SLAP_FM_ZERO_CONFIG (3u << 30)
enum { SLAP_FM_ENV_DIVIDER = 8, SLAP_FM_ENV_DEFAULT = 0x14561u };
static inline unsigned slap_fm_env_period(unsigned stage, unsigned value)
{
    unsigned squared = value * value;
    return stage == 0 ? 1u + 8u * squared : stage == 1 ? 3u + 15u * squared : 2u + 11u * squared;
}

void slap_synth_fm_envelopes(SlapSynth *synth, const uint32_t *operators, unsigned feedback);
float slap_wave_scan(float morph, float movement, float phase);
float slap_legacy_wave_scan(float color, float movement, float phase);
float slap_legacy_wave_preview(float morph, unsigned point);
float slap_wave_preview(float scan, unsigned band, unsigned point);
