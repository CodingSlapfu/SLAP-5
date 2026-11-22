/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "synthesis.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void check_patches(int modern)
{
    uint32_t signatures[SLAP_PATCHES];
    int engine_counts[SLAP_ENGINES] = {0};
    for (unsigned i = 0; i < SLAP_PATCHES; ++i) {
        const SlapPatch *patch = &slap_patches[i];
        assert(patch->engine < SLAP_ENGINES && patch->name[0]);
        assert(patch->color <= 127 && patch->motion <= 127);
        assert(patch->attack <= 127 && patch->decay <= 127 &&
               patch->sustain <= 127 && patch->release <= 127);
        uint32_t hash = 2166136261u;
        float energy = 0.0f;
        for (unsigned pitch = 36; pitch <= 96; pitch += 24) {
            SlapSynth synth;
            int16_t string[SLAP_STRING_CAPACITY];
            slap_synth_start(&synth, string, patch->engine, pitch, patch->color, patch->motion, 0x51616942u, modern);
            for (int f = 0; f < 8192; ++f) {
                const float sample = slap_synth_sample(&synth);
                assert(isfinite(sample) && fabsf(sample) <= 2.8f);
                const int32_t quantized = (int32_t)roundf(sample * 12000.0f);
                hash = (hash ^ (uint32_t)quantized) * 16777619u;
                energy += sample * sample;
            }
        }
        for (unsigned j = 0; j < i; ++j) {
            assert(strcmp(patch->name, slap_patches[j].name) != 0);
            assert(hash != signatures[j]);
        }
        assert(energy >= 1.0f);
        signatures[i] = hash;
        engine_counts[patch->engine]++;
    }
    for (unsigned i = 0; i < SLAP_ENGINES; ++i) assert(engine_counts[i] >= 5);
    printf("Rendered %u factory patches across %u engines at three pitches.\n", SLAP_PATCHES, SLAP_ENGINES);
    puts("Every patch produced finite audio, measurable energy, and a distinct sample signature.");
}

static void check_fm_envelope(void)
{
    SlapSynth synth;
    int16_t buffer[SLAP_STRING_CAPACITY];
    const uint32_t settings[] = {SLAP_FM_ENV_DEFAULT, 0, 0, 0, 0, 0};
    for (unsigned mode = 0; mode <= 2; ++mode) {
        slap_synth_start(&synth, buffer, 0, 60, 64, 64, 1u, mode);
        slap_synth_fm_envelopes(&synth, settings, 16);
        assert(synth.operator_mask == (mode == 2u ? 1u : 0u));
        if (mode < 2u) continue;
        assert(fabsf(synth.operator_feedback - 0.47f) < 0.00001f);
        assert(fabsf(synth.operators[0].attack - 1.0f / 9.0f) < 0.00001f);
        assert(fabsf(synth.operators[0].decay - 1.0f / 543.0f) < 0.00001f);
        assert(fabsf(synth.operators[0].sustain - 5.0f / 15.0f) < 0.00001f);
        assert(fabsf(synth.operators[0].release - 1.0f / 178.0f) < 0.00001f);
        slap_synth_sample(&synth);
        float gain = synth.operators[0].gain;
        for (unsigned step = 1; step < SLAP_FM_ENV_DIVIDER; ++step)
            slap_synth_sample(&synth);
        assert(synth.operators[0].gain == gain);
        slap_synth_sample(&synth);
        assert(synth.operators[0].gain > gain);
    }
}

int main(void)
{
    check_fm_envelope();
    check_patches(0);
    check_patches(1);
    return 0;
}
