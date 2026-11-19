/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "synthesis.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
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
            slap_synth_start(&synth, patch->engine, pitch, patch->color, patch->motion, 0x51616942u);
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
