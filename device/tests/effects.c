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
    float effect_difference = 0.0f;
    uint32_t saved = slap_fx_config(target, 6);
    for (unsigned i = 0; i < 34000; ++i) {
        if (i == 0) slap_fx_begin_live(112, target | mode << 3 | 63u << 7 | 33u << 13 | 47u << 19);
        if (i == 28000) slap_fx_begin_live(112, 0);
        float dry = 0.16f * sinf(i * 0.083f);
        float sample = slap_fx_sample(target, dry);
        assert(isfinite(sample) && fabsf(sample) < 0.18f);
        if (i >= 24000 && i < 28000) {
            effect_difference += fabsf(sample - dry);
            tail_peak = fmaxf(tail_peak, fabsf(sample));
        }
        if (i) largest_step = fmaxf(largest_step, fabsf(sample - previous));
        if (i > 31000) assert(fabsf(sample - dry) < 0.0001f);
        previous = sample;
    }
    assert(largest_step < 0.035f);
    if (mode == 10) assert(tail_peak < 0.001f);
    else assert(effect_difference > 10.0f);
    assert(slap_fx_config(target, 6) == saved);
}

static void test_repeat_and_reverse_direction(void)
{
    const unsigned step = 44100u * 15u / 112u / 4u * 4u;
    for (unsigned target = 0; target <= SLAP_FX_TRACKS; target += SLAP_FX_TRACKS) {
        slap_fx_init();
        slap_fx_begin_live(112, target | 3u << 3 | 63u << 7 | 63u << 19);
        float first = 0.0f, second = 0.0f;
        for (unsigned i = 0; i < 3u * step; ++i) {
            float x = i < step ? 0.18f : 0.0f;
            if (target < SLAP_FX_TRACKS) x = slap_fx_sample(target, x);
            else { float right = x; slap_fx_mix_sample(&x, &right); }
            if (i == step + step / 2u) first = x;
            if (i == 2u * step + step / 2u) second = x;
        }
        assert(first > 0.15f && second > 0.15f);
        slap_fx_begin_live(112, 0);
        for (unsigned i = 0; i < 4096u; ++i) {
            if (target < SLAP_FX_TRACKS) slap_fx_sample(target, 0.0f);
            else { float left = 0.0f, right = 0.0f; slap_fx_mix_sample(&left, &right); }
        }
    }
    slap_fx_init();
    slap_fx_begin_live(112, 2u | 11u << 3 | 63u << 7 | 63u << 19);
    float earlier = 0.0f, later = 0.0f;
    for (unsigned i = 0; i < 7400u; ++i) {
        float x = slap_fx_sample(2, i * 0.000028f);
        if (i == 6400u) earlier = x;
        if (i == 7300u) later = x;
    }
    assert(earlier - later > 0.020f);
    slap_fx_begin_live(112, 0);
    for (unsigned i = 0; i < 4096u; ++i) slap_fx_sample(2, 0.0f);
    puts("Beat Repeat recaptures on track and mix; Rewind reverses a rising ramp.");
}

static void test_full_mix(void)
{
    uint32_t saved[SLAP_FX_TRACKS][SLAP_FX_PAGES];
    for (unsigned track = 0; track < SLAP_FX_TRACKS; ++track)
        for (unsigned page = 0; page < SLAP_FX_PAGES; ++page)
            saved[track][page] = slap_fx_config(track, page);
    for (unsigned mode = 1; mode <= 11; ++mode) {
        slap_fx_begin_live(112, SLAP_FX_TRACKS | mode << 3 | 48u << 7 | 25u << 13 | 37u << 19);
        float difference = 0.0f;
        for (unsigned i = 0; i < 12000; ++i) {
            float dry_left = 0.09f * sinf(i * 0.09f);
            float dry_right = 0.12f * sinf(i * 0.15f);
            float left = dry_left, right = dry_right;
            if (slap_fx_mix_active()) slap_fx_mix_sample(&left, &right);
            assert(isfinite(left) && isfinite(right) && fabsf(left) < 2.0f && fabsf(right) < 2.0f);
            difference += fabsf(left - dry_left) + fabsf(right - dry_right);
        }
        assert(difference > 1.0f);
        slap_fx_begin_live(112, 0);
        for (unsigned i = 0; i < 4000; ++i) {
            float left = 0.09f, right = -0.12f;
            if (slap_fx_mix_active()) slap_fx_mix_sample(&left, &right);
            if (i == 3999) assert(left == 0.09f && right == -0.12f);
        }
    }
    for (unsigned track = 0; track < SLAP_FX_TRACKS; ++track)
        for (unsigned page = 0; page < SLAP_FX_PAGES; ++page)
            assert(saved[track][page] == slap_fx_config(track, page));
}

static void test_stereo_brake_pitch(void)
{
    unsigned crossings[3] = {0}, stereo_frames = 0;
    float previous = 0.0f;
    slap_fx_begin_live(112, SLAP_FX_TRACKS | 10u << 3 | 63u << 7 | 63u << 19);
    for (unsigned i = 0; i < 26000u; ++i) {
        float left = 0.18f * sinf(i * 0.09f), right = -left;
        slap_fx_mix_sample(&left, &right);
        if (i > 300u && i < 10000u) {
            assert(fabsf(left + right) < 0.001f);
            stereo_frames += fabsf(left - right) > 0.02f;
        }
        if (i >= 2000u && i < 11000u && previous < 0.0f && left >= 0.0f)
            ++crossings[(i - 2000u) / 3000u];
        if (i > 24000u) assert(fabsf(left) < 0.001f && fabsf(right) < 0.001f);
        previous = left;
    }
    assert(stereo_frames > 4000u);
    assert(crossings[0] > crossings[1] && crossings[1] > crossings[2] && crossings[2] > 0u);
    printf("Stereo brake pitch descent: %u, %u, %u positive crossings.\n",
           crossings[0], crossings[1], crossings[2]);
    slap_fx_begin_live(112, 0);
    for (unsigned i = 0; i < 4000u; ++i) {
        float left = 0.0f, right = 0.0f;
        if (slap_fx_mix_active()) slap_fx_mix_sample(&left, &right);
    }
}

static void test_assignable_dsp(void)
{
    for (unsigned effect = SLAP_LIVE_DIRECT; effect < SLAP_LIVE_PARAMETERS; ++effect) {
        uint32_t assignments = 0x03020000u | effect;
        slap_fx_begin_assigned(112, 2u | 5u << 3 | 32u << 7, assignments);
        float error = 0.0f;
        for (unsigned i = 0; i < 700u; ++i) {
            float dry = 0.22f * sinf(i * 0.19f);
            float wet = slap_fx_sample(2, dry);
            assert(isfinite(wet) && fabsf(wet) < 1.0f);
            error += fabsf(wet - dry);
        }
        assert(error > 1.0f);
        slap_fx_begin_live(112, 0);
    }
}

static void test_slicer_boundaries(void)
{
    const unsigned rate = 44100u * 15u / 137u;
    slap_fx_restore(0, 2, 2u | 1u << 8 | 1u << 16 | 127u << 24);
    slap_fx_begin(137);
    float previous = 0.0f, difference = 0.0f;
    for (unsigned i = 0; i < rate * 16u; ++i) {
        if (!(i % rate)) slap_fx_transport_step(i / rate, rate, rate);
        float dry = 0.15f * sinf(i * 0.067f);
        float wet = slap_fx_sample(0, dry);
        assert(isfinite(wet) && fabsf(wet) < 0.151f);
        assert(fabsf(wet - previous) < 0.015f);
        difference += fabsf(wet - dry);
        previous = wet;
    }
    assert(difference > 100.0f);
    slap_fx_restore(0, 2, 2u | 1u << 8 | 1u << 16);
    slap_fx_begin(137);
    for (unsigned i = 0; i < 16000u; ++i) {
        float dry = 0.15f * sinf(i * 0.067f);
        assert(slap_fx_sample(0, dry) == dry);
    }
    slap_fx_restore(0, 2, 1u << 16 | 127u << 24);
}

static void test_slicer_preview(void)
{
    slap_fx_init();
    slap_fx_restore(0, 2, 2u | 1u << 8 | 3u << 16 | 127u << 24);
    slap_fx_begin(112);
    slap_fx_transport_step(0, 5904, 5904);
    assert(slap_fx_slicer_state(0) & 64u);
    for (unsigned i = 0; i < 1476; ++i)
        slap_fx_sample(0, 0.25f);
    assert(slap_fx_slicer_state(0) & 128u);
    SlapSlicePreview preview;
    slap_fx_slicer_preview(0, &preview);
    assert(preview.captured > 0 && preview.wave[0] > 0);
    unsigned initial_replay = preview.replay;
    for (unsigned i = 0; i < 200; ++i)
        slap_fx_sample(0, 0.0f);
    slap_fx_slicer_preview(0, &preview);
    assert(preview.replay > initial_replay && preview.slice_position > 0);
    assert(!(slap_fx_slicer_state(1) & (16u | 64u | 128u)));
    slap_fx_restore(1, 2, 1u | 3u << 8 | 1u << 16 | 127u << 24);
    slap_fx_begin(137);
    slap_fx_transport_step(5, 4828, 4828);
    assert((slap_fx_slicer_state(0) & 15u) == 4u);
    assert((slap_fx_slicer_state(1) & 15u) == 5u);
    slap_fx_slicer_preview(1, &preview);
    assert(preview.captured == 0 && preview.replay == 0 && preview.slice_position == 0);
    slap_fx_transport_step(0, 4828, 4828);
    for (unsigned track = 0; track < 2; ++track) {
        assert((slap_fx_slicer_state(track) & 15u) == 0u);
        slap_fx_slicer_preview(track, &preview);
        assert(preview.captured == 0 && preview.slice_position == 0);
    }
    slap_fx_restore(0, 2, 0);
    slap_fx_begin(160);
    assert(!(slap_fx_slicer_state(0) & (16u | 64u | 128u)));
    slap_fx_slicer_preview(0, &preview);
    assert(preview.captured == 0 && preview.replay == 0);

}

int main(void)
{
    slap_fx_init();
    test_live_tape(10);
    test_live_tape(11);
    test_live_tape(3);
    test_repeat_and_reverse_direction();
    test_full_mix();
    test_stereo_brake_pitch();
    test_assignable_dsp();
    test_slicer_boundaries();
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
    test_slicer_preview();
    puts("Five simultaneous effect chains remained bounded across all seven inserts.");
}
