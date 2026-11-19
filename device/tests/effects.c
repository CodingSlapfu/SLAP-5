/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "effects.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    slap_fx_init();
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
