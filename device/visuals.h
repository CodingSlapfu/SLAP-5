/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once

/* SLAP BLOCK display lettering. Seven rows of deliberately wide stems. */
static const unsigned char slap_block[37][7] = {
    {14,31,27,27,31,27,27}, {30,31,27,30,27,31,30},
    {15,31,24,24,24,31,15}, {30,31,27,27,27,31,30},
    {31,31,24,30,24,31,31}, {31,31,24,30,24,24,24},
    {15,31,24,27,27,31,15}, {27,27,27,31,27,27,27},
    {31,31,14,14,14,31,31}, {15,15,3,3,27,31,14},
    {27,30,28,28,30,27,27}, {24,24,24,24,24,31,31},
    {17,27,31,31,27,27,27}, {27,31,31,31,27,27,27},
    {14,31,27,27,27,31,14}, {30,31,27,31,30,24,24},
    {14,31,27,27,31,31,15}, {30,31,27,31,30,27,27},
    {15,31,24,30,3,31,30}, {31,31,14,14,14,14,14},
    {27,27,27,27,27,31,14}, {27,27,27,27,27,14,4},
    {27,27,27,31,31,31,17}, {27,27,14,4,14,27,27},
    {27,27,27,14,4,4,4}, {31,31,6,12,24,31,31},
    {14,31,27,27,27,31,14}, {12,28,12,12,12,31,31},
    {14,31,3,14,24,31,31}, {30,31,3,14,3,31,30},
    {27,27,27,31,31,3,3}, {31,31,24,30,3,31,30},
    {15,31,24,30,27,31,14}, {31,31,3,6,12,12,12},
    {14,31,27,14,27,31,14}, {14,31,27,15,3,31,30},
    {0,0,0,31,31,0,0}
};

static void slap_wordmark(int center, int y)
{
    cv_text(center - text_w(&FONT_L, "SLAP-5") / 2, y, &FONT_L, "SLAP-5", C_WHITE);
}

static const uint16_t track_color[] = {
    RGB(28,226,255), RGB(171,112,255), RGB(255,89,176), RGB(255,180,58), RGB(62,242,137)
};

static const uint16_t track_surface[] = {
    RGB(0,47,67), RGB(45,20,79), RGB(73,15,51), RGB(67,42,10), RGB(10,54,30)
};

static const uint16_t track_rule[] = {
    RGB(0,107,138), RGB(94,45,155), RGB(145,27,99), RGB(141,85,17), RGB(17,112,53)
};

static int visual_wave(unsigned phase, int amplitude)
{
    return (int)(slap_oscillator((float)(phase & 1023u) * (1.0f / 1024.0f)) * (float)amplitude);
}

static void visual_disc(int x, int y, int radius, int sy, uint16_t color)
{
    int first = -radius > sy - y ? -radius : sy - y;
    int last = radius < sy + 39 - y ? radius : sy + 39 - y;
    int radius_squared = radius * radius;
    for (int dy = first; dy <= last; ++dy) {
        int span = radius;
        int square = radius_squared - dy * dy;
        while (span * span > square) --span;
        cv_rect(x-span, y+dy-sy, 2*span+1, 1, color);
    }
}

#include "dancer-art.h"

static void dancer_draw(unsigned time)
{
    unsigned energy = 0;
    for (unsigned track = 0; track < TRACKS; ++track)
        energy += track_energy[track] + note_activity[track] * 2u;
    unsigned active = dancer_reactivity && energy > 12u;
    unsigned cycle = active ? ((positions & 31u) * 15u + step_progress) / 16u : (time / 85u) % 4u;
    if (active && !playing) cycle = time / 19u;
    unsigned pose = active ? (cycle + dancer_pattern * 10u) % 30u : (cycle + 6u * dancer_pattern) % 30u;
    unsigned light_track = ((positions >> 2) + dancer_pattern) % TRACKS;
    const uint8_t *span = dancer_frames[pose];
    for (int sy = 0; sy < 240; sy += 40) {
        cv_begin(240, 40, RGB(8, 7, 24));
        if (dancer_lighting) {
            int spread = 16 + (int)(active ? note_activity[DRUMS] / 2u : 3u);
            cv_line(38, 9 - sy, 120 - spread, 218 - sy, track_rule[light_track]);
            cv_line(201, 9 - sy, 120 + spread, 218 - sy, track_rule[(light_track + 2u) % TRACKS]);
        }
        cv_rect(15, 218 - sy, 210, 2, track_rule[light_track]);
        for (int y = 0; y < 40; y += 2) {
            const uint8_t *row = span;
            if (dancer_outline) {
                while (*row != 255u) {
                    cv_rect(row[0] * 2 - 1, y, (row[1] >> 3) * 2 + 4, 2, track_rule[light_track]);
                    row += 2;
                }
            }
            while (*span != 255u) {
                cv_rect(span[0] * 2, y, ((span[1] >> 3) + 1) * 2, 2, dancer_palette[span[1] & 7u]);
                span += 2;
            }
            ++span;
        }
        if (sy == 0) cv_text(8, 5, &FONT_B, "PARALLAX-8 DANCER", C_WHITE);
        if (sy == 200) {
            cv_text(8, 23, &FONT_T, "K1 PATTERN  K2 OUTLINE  K3 LIGHTS  K4 REACT", C_WHITE);
        }
        cv_blit(0, sy);
    }
}

static void visualizer_draw(unsigned kind, unsigned time, unsigned intro)
{
    if (kind == 6u) {
        dancer_draw(time);
        return;
    }
    static const char *const names[] = {
        "DNA HELIX", "FERROFLUID", "GRAVITY RINGS", "SOLAR SYSTEM",
        "", "DIGITAL VU METER", "", "SLAP REACTOR"
    };
    int energy[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t)
        energy[t] = intro ? clamp((int)intro / 24 - (int)t * 3, 0, 34) :
            (int)(track_energy[t] >> 2) + (int)(note_activity[t] >> 2);
    float levels[SPECTRUM_BANDS];
    static unsigned orbit_time, orbit_phase[8], orbit_speed[8];
    for (unsigned b = 0; b < SPECTRUM_BANDS; ++b) {
        levels[b] = fm_clampf((fm_log2f(fm_maxf(visual_band[b], 0.000001f)) + 18.0f) / 13.0f, 0.0f, 1.0f);
    }
    if (kind == 3) {
        static const uint8_t bands[] = {0, 1, 2, 3, 4, 5, 6, 0};
        static const uint8_t rates[] = {24, 17, 13, 10, 6, 4, 3, 2};
        unsigned elapsed = time - orbit_time;
        // Resume from the old position after leaving the visualizer.
        if (elapsed > 25u) elapsed = 0;
        orbit_time = time;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned response = i < TRACKS ? note_activity[i] * 3u + (unsigned)energy[i] * 2u :
                (unsigned)(levels[bands[i]] * 150.0f);
            unsigned target = rates[i] * (16u + response);
            orbit_speed[i] = (5u * orbit_speed[i] + target) / 6u;
            orbit_phase[i] += elapsed * orbit_speed[i];
        }
    }
    enum { REACTOR_POINTS = (1023 / 53) + 1 };
    static struct { int8_t x, y; } reactor[TRACKS][REACTOR_POINTS];
    if (kind == 7) {
        for (unsigned t = 0; t < TRACKS; ++t) {
            for (unsigned point = 0; point < REACTOR_POINTS; ++point) {
                unsigned angle = point * 53u + time * (t + 1u);
                int pulse = visual_wave(angle * 3u + time * 5u, energy[t] / 5);
                int radius = clamp(28 + (int)t * 14 + energy[t] / 3 + pulse, 12, 112);
                reactor[t][point].x = (int8_t)visual_wave(angle + 256u, radius);
                reactor[t][point].y = (int8_t)visual_wave(angle, radius);
            }
        }
    }
    for (int sy = 0; sy < 240; sy += 40) {
        cv_begin(240, 40, C_BLACK);
        switch (kind) {
        case 0:
            for (int y = 38; y < 230; y += 7) {
                unsigned t = (unsigned)y / 7u % TRACKS;
                int x = visual_wave(time * 3u + (unsigned)y * 13u, 46 + energy[t]);
                if (y + 7 < sy || y - 5 >= sy + 40) continue;
                int span = x < 0 ? -x : x;
                int depth = visual_wave(time * 3u + (unsigned)y * 13u + 256u, 3);
                cv_rect(120 - span, y - sy, span * 2 + 1, 1, track_surface[t]);
                visual_disc(120 + x, y, 5 + depth / 2, sy, track_rule[t]);
                visual_disc(120 - x, y, 5 - depth / 2, sy, track_rule[(t + 2u) % TRACKS]);
                visual_disc(120 + x, y, 2 + (depth > 0), sy, track_color[t]);
                visual_disc(120 - x, y, 2 + (depth < 0), sy, track_color[(t + 2u) % TRACKS]);
                if (energy[t] > 7 && !(y % 3))
                    cv_rect(120 + x / 2, y - sy - 3, 2, 2, C_WHITE);
            }
            break;
        case 1:
            for (int x = 0; x < 240; x += 4) {
                unsigned t = (unsigned)x / 48u;
                int field = visual_wave((unsigned)x * 13u + time * 6u, 14 + energy[t] / 2);
                int peak = 143 + visual_wave(x * 25u - time * 3u, 12 + energy[t]) + field;
                int mirror = 173 - field / 2;
                cv_rect(x, peak - sy, 4, mirror - peak > 0 ? mirror - peak : 1, track_surface[t]);
                cv_rect(x, peak - sy, 4, 2, track_color[t]);
                cv_rect(x, mirror - sy + 3, 3, 2, track_rule[t]);
                if (field < -8) cv_rect(x + 1, peak - sy - 2, 1, 1, C_WHITE);
            }
            break;
        case 2:
            for (unsigned t = 0; t < TRACKS; ++t) {
                int radius = 24 + (int)t*17 + energy[t]/5;
                unsigned activity = note_activity[t];
                if (activity > 7u) {
                    int wave_radius = clamp(radius + 13 - (int)activity / 5, 12, 116);
                    for (unsigned angle = 0; angle < 1024u; angle += 64u) {
                        int x = 120 + visual_wave(angle + 256u, wave_radius);
                        int y = 130 + visual_wave(angle, wave_radius / 2);
                        cv_rect(x, y - sy, 2, 2, track_color[t]);
                    }
                }
                for (unsigned k = 0; k < 1024; k += 37) {
                    unsigned phase = k + time*(3u+t);
                    int x = 120 + visual_wave(phase+256, radius);
                    int y = 130 + visual_wave(phase, radius/2) + visual_wave(2*phase+t*80u,20);
                    if (y + 4 < sy || y - 4 >= sy + 40) continue;
                    visual_disc(x, y, 4, sy, track_surface[t]);
                    visual_disc(x, y, 2 + (int)(t == 4), sy, track_color[t]);
                    if (k % 3u == 0) cv_rect(x - 4, y + 2 - sy, 2, 1, track_rule[t]);
                }
            }
            break;
        case 3: {
            static const uint8_t sizes[] = {2,4,5,3,12,9,6,6};
            static const uint16_t planet[] = { RGB(160,151,137), RGB(229,185,109),
                RGB(51,151,224), RGB(210,78,44), RGB(218,158,113), RGB(222,188,116),
                RGB(116,216,221), RGB(46,80,222) };
            int bass = (int)(24.0f * fm_maxf(levels[0], levels[1]));
            int mids = (int)(100.0f * fm_maxf(levels[2], levels[3]));
            int highs = (int)(80.0f * fm_maxf(levels[5], levels[6]));
            for (unsigned star = 0; star < 22; ++star) {
                int brightness = 38 + (int)((star * 23u + time / 19u) % 20u) + highs;
                cv_rect((int)(star * 71u % 233u), 33 + (int)(star * 93u % 200u)-sy,
                        highs > 35 ? 2 : 1, 1, RGB(brightness, brightness, brightness + 25));
            }
            if (energy[4] > 5)
                for (unsigned k = 0; k < 1024; k += 59)
                    cv_rect(120+visual_wave(k+256,22+energy[4]/2),
                        128+visual_wave(k,22+energy[4]/2)-sy, 2, 2, RGB(255,128,20));
            visual_disc(120,128,19+bass,sy,RGB(71,27,13));
            visual_disc(120,128,14+bass,sy,RGB(255,143+bass*3,22));
            visual_disc(120,128,11+bass/2,sy,RGB(255,228,102));
            for (unsigned i = 0; i < 8; ++i) {
                int r = 21 + (int)i*12;
                for (unsigned k=0; k<1024; k+=57)
                    cv_rect(120+visual_wave(k+256,r),128+visual_wave(k,r*3/4)-sy,1,1,RGB(46+mids,62+mids,85+mids));
                unsigned phase = (orbit_phase[i] >> 12) + i * 113u;
                int x = 120+visual_wave(phase+256,r), y = 128+visual_wave(phase,r*3/4);
                if (i == 5) cv_line(x-13,y+4-sy,x+13,y-4-sy,RGB(237,202,142));
                if (i < TRACKS && energy[i] > 8)
                    visual_disc(x, y, sizes[i] + 2 + energy[i] / 13, sy, track_surface[i]);
                visual_disc(x,y,sizes[i],sy,i < TRACKS ? track_color[i] : planet[i]);
                if (sizes[i] > 3) {
                    visual_disc(x - 1, y - 1, (int)sizes[i] / 3, sy, RGB(250,218,165));
                    cv_rect(x + sizes[i] / 2, y - sy + 2, 2, 1, RGB(65,40,52));
                }
                if (i == 2 || i == 4) cv_rect(x-2,y-sy,5,1,i == 2 ? RGB(103,212,105) : RGB(165,93,65));
            }
            break;
        }
        case 5: {
            uint32_t step = positions % pattern_length(tracks[slap_ui.track].mix);
            if (sy == 40) {
                char beat[8], bar[12];
                decimal(beat, (step % 16u) / 4u + 1u);
                decimal(bar, step / 16u + 1u);
                cv_text(13, 2, &FONT_XS, "BEAT", track_color[0]);
                cv_text(52, 0, &FONT_M, beat, C_WHITE);
                cv_text(127, 2, &FONT_XS, "BAR", track_color[4]);
                cv_text(159, 0, &FONT_M, bar, C_WHITE);
            }
            for (unsigned track = 0; track < TRACKS; ++track) {
                int y = 77 + (int)track * 29;
                if (y + 13 < sy || y >= sy + 40) continue;
                char name[] = {'T', (char)('1' + track), 0};
                cv_text(8, y - sy, &FONT_XS, name, track_color[track]);
                unsigned active = (track_energy[track] * 23u + 80u) / 160u;
                unsigned peak = (track_peak[track] * 22u + 80u) / 160u;
                for (unsigned cell = 0; cell < 23; ++cell) {
                    uint16_t color = cell >= active ? track_surface[track] : track_color[track];
                    if (track_peak[track] && cell == peak) color = C_WHITE;
                    cv_rect(31 + (int)cell * 9, y - sy, 7, 12, color);
                }
            }
            break;
        }
        default:
            visual_disc(120, 126, 24 + energy[4] / 2, sy, RGB(30,24,66));
            visual_disc(120, 126, 17 + energy[4] / 3, sy, RGB(63,34,107));
            for (unsigned t = 0; t < TRACKS; ++t) {
                for (unsigned point = 0; point < REACTOR_POINTS; ++point) {
                    int x = 120 + reactor[t][point].x;
                    int y = 127 + reactor[t][point].y;
                    if (y + 6 < sy || y - 6 >= sy + 40) continue;
                    visual_disc(x, y, 3 + energy[t] / 10, sy, track_surface[t]);
                    visual_disc(x, y, 1 + energy[t] / 12, sy, track_color[t]);
                    if (energy[t] > 8 && point % 3u == 0) {
                        cv_rect(x + 4, y - sy, 2, 2, C_WHITE);
                        cv_rect(x - 5, y + 2 - sy, 2, 1, track_color[t]);
                    }
                }
            }
            slap_wordmark(120, 110-sy);
            break;
        }
        if (!sy) {
            cv_rect(0,0,240,33,C_BLACK);
            cv_text(9,6,&FONT_B,names[kind],C_WHITE);
        }
        cv_blit(0, sy);
    }
}

static void draw_boot(unsigned elapsed)
{
    visualizer_draw(7, elapsed / 4u, elapsed + 1u);
}
