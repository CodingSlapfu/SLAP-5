/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#ifndef SLAP_OSCILLATOR_H
#define SLAP_OSCILLATOR_H

enum { SLAP_SINE_SIZE = 1024 };
extern const float slap_sine_table[SLAP_SINE_SIZE + 1];

static inline float slap_oscillator(float phase)
{
    float scaled = phase * SLAP_SINE_SIZE;
    int whole = (int)scaled;
    if (scaled < (float)whole) --whole;
    unsigned index = (unsigned)whole & (SLAP_SINE_SIZE - 1u);
    float first = slap_sine_table[index];
    return first + (scaled - (float)whole) * (slap_sine_table[index + 1] - first);
}

#endif
