/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "effects.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_live_tape(unsigned mode)
{
    unsigned target = 2;
    float previous = 0.0f, tail_peak = 0.0f, largest_step = 0.0f;
    unsigned input_crossings = 0, output_crossings = 0;
    uint32_t saved = slap_fx_config(target, 6);
    for (unsigned i = 0; i < 24000; ++i) {
        if (i == 0) slap_fx_begin_live(112, target | mode << 3 | 63u << 7 | 33u << 13 | 47u << 19);
        if (i == 18000) slap_fx_begin_live(112, 0);
        float dry = 0.16f * sinf(i * 0.083f);
        float sample = slap_fx_sample(target, dry);
        assert(isfinite(sample) && fabsf(sample) < 0.18f);
        if (i >= 12000 && i < 18000) {
            if (dry > 0.0f && sinf((i - 1u) * 0.083f) <= 0.0f) ++input_crossings;
            if (sample > 0.0f && previous <= 0.0f) ++output_crossings;
            tail_peak = fmaxf(tail_peak, fabsf(sample));
        }
        if (i) largest_step = fmaxf(largest_step, fabsf(sample - previous));
        if (i > 21000) assert(fabsf(sample - dry) < 0.0001f);
        previous = sample;
    }
    assert(largest_step < 0.035f);
    if (mode == 10) assert(tail_peak < 0.001f);
    else assert(output_crossings > input_crossings / 3u && output_crossings < input_crossings * 9u / 10u);
    assert(slap_fx_config(target, 6) == saved);
}

int main(void)
{
    slap_fx_init();
    test_live_tape(10);
    test_live_tape(11);
    for (unsigned t = 0; t < 5; ++t) {
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(t, 0, k, 31);
        for (unsigned k = 0; k < 4; ++k)
            slap_fx_change(t, 1, k, 45 - (int)k*6);
        slap_fx_change(t, 2, 0, 1 + (int)(t % 2));
        slap_fx_change(t, 2, 1, 4 + (int)t);
        for (unsigned p = 5; p < 9; ++p)
            for (unsigned k = 1; k < 4; ++k) slap_fx_change(t, p, k, 1);
    }
    for (int mode = 1; mode <= 7; ++mode) {
        float change = 0.0f;
        for (unsigned t = 0; t < 5; ++t) slap_fx_change(t, 3, 0, 1);
        slap_fx_begin(112);
        for (unsigned i = 0; i < 16000; ++i) {
            float dry = i < 8000 ? 0.16f * sinf((float)i*0.083f) : 0.0f;
            for (unsigned t = 0; t < 5; ++t) {
                float sample = slap_fx_sample(t, dry);
                assert(isfinite(sample) && fabsf(sample) < 2.0f);
                change += fabsf(sample-dry);
            }
        }
        assert(change > 10.0f);
    }
    assert(slap_fx_response(0, 0) != slap_fx_response(0, SLAP_FX_PLOT - 1u));
    puts("Five simultaneous effect chains remained bounded across all seven inserts.");
}
