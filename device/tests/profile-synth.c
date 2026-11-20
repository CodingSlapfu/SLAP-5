/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#define _POSIX_C_SOURCE 200809L
#include "synthesis.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { PROFILE_VOICES = 16, PROFILE_FRAMES = 32768, PROFILE_PASSES = 8 };

static uint64_t thread_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

int main(void)
{
    for (unsigned engine = 0; engine < SLAP_ENGINES; ++engine) {
        uint32_t hash = 2166136261u;
        uint64_t elapsed = 0;
        for (unsigned pass = 0; pass < PROFILE_PASSES; ++pass) {
            SlapSynth voices[PROFILE_VOICES];
            int16_t strings[PROFILE_VOICES][SLAP_STRING_CAPACITY];
            for (unsigned voice = 0; voice < PROFILE_VOICES; ++voice)
                slap_synth_start(&voices[voice], strings[voice], engine, 42 + voice * 2,
                                 37 + voice * 3, 42 + voice * 2, 0x51817651u + voice, pass & 1u);

            uint64_t started = thread_ns();
            for (unsigned frame = 0; frame < PROFILE_FRAMES; ++frame) {
                for (unsigned voice = 0; voice < PROFILE_VOICES; ++voice) {
                    float sample = slap_synth_sample(&voices[voice]);
                    uint32_t bits;
                    memcpy(&bits, &sample, sizeof bits);
                    hash = (hash ^ bits) * 16777619u;
                }
            }
            elapsed += thread_ns() - started;
        }
        printf("%u %.3f %08x\n", engine,
               elapsed / (double)(PROFILE_FRAMES * PROFILE_VOICES * PROFILE_PASSES), hash);
    }
}
