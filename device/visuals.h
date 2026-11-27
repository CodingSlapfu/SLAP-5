/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once

static void slap_wordmark(int center, int y)
{
    cv_text(center - text_w(&FONT_L, "SLAP-5") / 2, y, &FONT_L, "SLAP-5", C_WHITE);
}

static const uint16_t track_color[TRACKS] = {
    RGB(0,180,255), RGB(155,48,255), RGB(255,100,0), RGB(255,220,0), RGB(0,255,46)
};
static const uint16_t track_surface[TRACKS] = {
    RGB(0,35,73), RGB(45,20,79), RGB(77,27,5), RGB(65,53,4), RGB(10,54,30)
};
static const uint16_t track_rule[TRACKS] = {
    RGB(0,86,161), RGB(94,45,155), RGB(156,56,8), RGB(140,115,6), RGB(17,112,53)
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

static const uint8_t *dancer_row(const uint8_t *run, int y, int sway, uint16_t outline)
{
    int source_x = 0;
    while (*run) {
        unsigned code = *run++;
        unsigned gap = code >> 6;
        unsigned width = (code >> 3 & 7u) + 1u;
        if (gap == 3u) gap = *run++;
        if (width == 8u) width = *run++;
        source_x += (int)gap;
        int start = 24 + source_x * 3 / 2 + sway;
        source_x += (int)width;
        int end = 24 + source_x * 3 / 2 + sway;
        if (outline) cv_rect(start - 1, y, end - start + 2, 1, outline);
        else cv_rect(start, y, end - start, 1, dancer_palette[code & 7u]);
    }
    return run + 1;
}

static void dancer_draw(unsigned time)
{
    unsigned energy = 0;
    for (unsigned track = 0; track < TRACKS; ++track)
        energy += track_energy[track] + note_activity[track] * 2u;
    unsigned active = playing;
    unsigned accent = playing && dancer_reactivity && energy > 12u;
    unsigned cycle = ((positions & 31u) * 15u + step_progress) / 16u;
    unsigned recovery = time / 260u % 10u;
    unsigned pose = active ? (cycle * (1u + dancer_lighting) + dancer_pattern * 8u +
        note_activity[1] / 8u + note_activity[3] / 12u) % 24u :
        24u + (recovery <= 5u ? recovery : 10u - recovery);
    unsigned light_track = ((positions >> 2) + dancer_pattern) % TRACKS;
    int amplitude = dancer_outline ? 9 : 3;
    if (dancer_reactivity) amplitude += (int)((track_energy[0] + track_energy[2]) / 44u);
    int sway = active ? visual_wave(time * (dancer_pattern + 2u), amplitude) : visual_wave(time * 2u, 1);
    static unsigned started, prior_bucket, prior_pose, previous_pose, was_active;
    if (!active || !was_active) {
        started = time;
        prior_pose = previous_pose = pose;
        prior_bucket = time / 500u;
    }
    was_active = active;
    unsigned bucket = time / 500u;
    if (bucket != prior_bucket) {
        previous_pose = prior_pose;
        prior_pose = pose;
        prior_bucket = bucket;
    }
    unsigned trail_phase = (time - started) % 5000u;
    const uint8_t *trail = active && !dsp_pressure && trail_phase < 2500u ? dancer_frames[previous_pose] : 0;
    int spread_trail = trail_phase < 2000u ? 12 : 12 + (int)(trail_phase - 2000u) / 8;
    if (light_track & 1u) spread_trail = -spread_trail;
    const uint8_t *span = dancer_frames[pose];
    enum { DANCER_LIFT = 14 };
    int source_row = 0;
    while (source_row < DANCER_LIFT * DANCER_HEIGHT / 240) {
        while (*span) {
            unsigned code = *span++;
            if (code >> 6 == 3u) ++span;
            if ((code >> 3 & 7u) == 7u) ++span;
        }
        ++span;
        if (trail) {
            while (*trail) {
                unsigned code = *trail++;
                if (code >> 6 == 3u) ++trail;
                if ((code >> 3 & 7u) == 7u) ++trail;
            }
            ++trail;
        }
        ++source_row;
    }
    for (int sy = 0; sy < 240; sy += 40) {
        cv_begin(240, 40, C_BLACK);
        if (accent) {
            int spread = 16 + (int)(active ? note_activity[DRUMS] / 2u : 3u);
            int sweep = visual_wave(time * 3u, 28);
            cv_line(38, 9 - sy, 120 - spread + sweep, 218 - sy, track_rule[light_track]);
            cv_line(201, 9 - sy, 120 + spread - sweep, 218 - sy, track_rule[(light_track + 2u) % TRACKS]);
        }
        if (active) cv_rect(15, 218 - sy, 210, 2, track_rule[light_track]);
        for (int y = 0; y < 40 && source_row < DANCER_HEIGHT; ++y) {
            const uint8_t *next_trail = trail;
            if (trail && !(y & 3))
                next_trail = dancer_row(trail, y, sway + spread_trail, track_rule[light_track]);
            if (dancer_outline)
                dancer_row(span, y, sway, active ? track_rule[light_track] : RGB(23, 14, 35));
            const uint8_t *next = dancer_row(span, y, sway, 0);
            int next_row = (sy + y + DANCER_LIFT + 1) * DANCER_HEIGHT / 240;
            if (next_row != source_row) {
                span = next;
                if (trail && next_trail == trail) {
                    while (*next_trail) {
                        unsigned code = *next_trail++;
                        if (code >> 6 == 3u) ++next_trail;
                        if ((code >> 3 & 7u) == 7u) ++next_trail;
                    }
                    ++next_trail;
                }
                trail = next_trail;
                source_row = next_row;
            }
        }
        if (sy == 0) cv_text(8, 5, &FONT_B, "DANCER", C_WHITE);
        if (sy == 200) {
            cv_text(8, 23, &FONT_T, "K1 ENERGY  K2 GROOVE  K3 AMP  K4 POSE", C_WHITE);
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
            unsigned target = 6u * rates[i] * (16u + response);
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
                unsigned angle = time * 3u + (unsigned)y * (13u + (unsigned)(levels[0] * 3));
                int separation = 46 + energy[t] / 2 + (int)(levels[0] * 16);
                int x = visual_wave(angle, separation);
                if (y + 7 < sy || y - 5 >= sy + 40) continue;
                int span = x < 0 ? -x : x;
                int depth = visual_wave(angle + 256u, 3 + energy[3] / 16);
                cv_rect(120 - span, y - sy, span * 2 + 1, 1, track_surface[t]);
                visual_disc(120 + x, y, 5 + depth / 2, sy, track_rule[t]);
                visual_disc(120 - x, y, 5 - depth / 2, sy, track_rule[(t + 2u) % TRACKS]);
                visual_disc(120 + x, y, 2 + (depth > 0), sy, track_color[t]);
                visual_disc(120 - x, y, 2 + (depth < 0), sy, track_color[(t + 2u) % TRACKS]);
                if (note_activity[t] > 8u && !(y % 3))
                    cv_rect(120 + x / 2, y - sy - 3, 2 + (note_activity[4] > 22u), 2, C_WHITE);
            }
            break;
        case 1:
            for (int x = 0; x < 240; x += 4) {
                unsigned t = (unsigned)x / 48u;
                int field = visual_wave((unsigned)x * 13u + time * 6u, 14 + energy[t] / 2 + (int)(levels[0] * 17));
                int peak = 143 + visual_wave(x * 25u - time * 3u, 12 + energy[t] + (int)(levels[t] * 21)) + field;
                int spike = visual_wave((unsigned)x * 49u + time * 9u, (int)note_activity[t] / 4);
                peak = clamp(peak - (spike > 0 ? spike : 0), 39, 196);
                int mirror = 173 - field / 2;
                cv_rect(x, peak - sy, 4, mirror - peak > 0 ? mirror - peak : 1, track_surface[t]);
                cv_rect(x, peak - sy, 4, 2, track_color[t]);
                cv_rect(x, mirror - sy + 3, 3, 2, track_rule[t]);
                if (field < -8) cv_rect(x + 1, peak - sy - 2, 1, 1, C_WHITE);
            }
            break;
        case 2: {
            unsigned tilt_phase = (time % 1920000u) * tempo / 1875u;
            int tilt = visual_wave(tilt_phase * 3u, 43);
            int opening = 49 + visual_wave(tilt_phase * 3u + 256u, 26);
            for (unsigned t = 0; t < TRACKS; ++t) {
                int radius = 24 + (int)t*17 + energy[t]/5;
                int depth = 14 + (int)(levels[0] * 13);
                unsigned activity = note_activity[t];
                if (activity > 7u) {
                    int wave_radius = clamp(radius + 13 - (int)activity / 5, 12, 116);
                    for (unsigned angle = 0; angle < 1024u; angle += 64u) {
                        int x = 120 + visual_wave(angle + 256u, wave_radius);
                        int y = 130 + visual_wave(angle, wave_radius * opening / 100) + (x - 120) * tilt / 100;
                        cv_rect(x, y - sy, 2, 2, track_color[t]);
                    }
                }
                for (unsigned k = 0; k < 1024; k += 37) {
                    unsigned phase = k + time*(3u+t);
                    if (playing) phase += (positions & 63u) * 16u + step_progress;
                    int x = 120 + visual_wave(phase+256, radius);
                    int y = 130 + visual_wave(phase, radius * opening / 100)
                        + (x - 120) * tilt / 100 + visual_wave(2*phase+t*80u,depth);
                    if (y + 4 < sy || y - 4 >= sy + 40) continue;
                    visual_disc(x, y, 4, sy, track_surface[t]);
                    visual_disc(x, y, 2 + (int)(t == 4), sy, track_color[t]);
                    if (k % 3u == 0) cv_rect(x - 4, y + 2 - sy, 2, 1, track_rule[t]);
                }
            }
            break;
        }
        case 3: {
            static const uint8_t sizes[] = {5,7,8,6,13,9,6,6};
            static const uint16_t planet[] = { RGB(160,151,137), RGB(229,185,109),
                RGB(51,151,224), RGB(210,78,44), RGB(218,158,113), RGB(222,188,116),
                RGB(116,216,221), RGB(46,80,222) };
            int bass = (int)(24.0f * fm_maxf(levels[0], levels[1]));
            int mids = (int)(100.0f * fm_maxf(levels[2], levels[3]));
            int highs = (int)(80.0f * fm_maxf(levels[5], levels[6]));
            for (unsigned star = 0; star < 22; ++star) {
                int brightness = 38 + (int)((star * 23u + time / 19u) % 20u) + highs;
                cv_rect((int)(star * 71u % 233u), 33 + (int)((star * 93u + time / (12u - star % 3u)) % 200u)-sy,
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
                uint16_t orbit_ink = i == 3 && energy[3] > 7 ? track_rule[3] : RGB(46+mids,62+mids,85+mids);
                for (unsigned k=0; k<1024; k+=57)
                    cv_rect(120+visual_wave(k+256,r),128+visual_wave(k,r*3/4)-sy,1,1,orbit_ink);
                unsigned phase = (orbit_phase[i] >> 12) + i * 113u;
                int x = 120+visual_wave(phase+256,r), y = 128+visual_wave(phase,r*3/4);
                if (i == 5) cv_line(x-13,y+4-sy,x+13,y-4-sy,RGB(237,202,142));
                int size = sizes[i];
                if (i < TRACKS) {
                    size += clamp(energy[i] / 7, 0, 7);
                    if (energy[i] > 8) {
                        visual_disc(x, y, size + 3, sy, track_rule[i]);
                        for (unsigned tail = 1; tail <= 4; ++tail) {
                            unsigned trail = phase - tail * 27u;
                            visual_disc(120 + visual_wave(trail + 256u, r),
                                128 + visual_wave(trail, r * 3 / 4), 5 - (int)tail, sy, track_rule[i]);
                        }
                    }
                }
                uint16_t body = i < TRACKS ? track_color[i] : planet[i];
                if (i == 2 && energy[2] < 6) body = track_rule[2];
                if (i == 4 && note_activity[4] > 12u)
                    visual_disc(x, y, size + 2 + (int)note_activity[4] / 10, sy, track_rule[4]);
                visual_disc(x, y, size, sy, body);
                if (sizes[i] > 3) {
                    visual_disc(x - 1, y - 1, (int)sizes[i] / 3, sy, RGB(250,218,165));
                    cv_rect(x + sizes[i] / 2, y - sy + 2, 2, 1, RGB(65,40,52));
                }
                if (i == 2 || i == 4) cv_rect(x-2,y-sy,5,1,i == 2 ? RGB(103,212,105) : RGB(165,93,65));
            }
            break;
        }
        case 5: {
            if (sy == 40) {
                draw_transport_progress(positions, step_progress, pattern_length(tracks[slap_ui.track].mix),
                                        playing, track_color[slap_ui.track]);
            }
            for (unsigned track = 0; track < TRACKS; ++track) {
                int y = 77 + (int)track * 29;
                if (y + 16 < sy || y - 4 >= sy + 40) continue;
                char name[] = {'T', (char)('1' + track), 0};
                cv_text(8, y - sy, &FONT_XS, name, track_color[track]);
                cv_rect(29, y - sy - 4, 209, 20, RGB(20,23,28));
                unsigned active = (track_energy[track] * 23u + 80u) / 160u;
                unsigned peak = (track_peak[track] * 22u + 80u) / 160u;
                for (unsigned cell = 0; cell < 23; ++cell) {
                    uint16_t color = cell >= active ? track_surface[track] : track_color[track];
                    if (track_peak[track] && cell == peak) color = C_WHITE;
                    int x = 31 + (int)cell * 9;
                    int height = 11 + (int)(cell / 7u);
                    cv_rect(x, y + 12 - height - sy, 7, height, color);
                    if (cell < active) cv_rect(x, y + 12 - height - sy, 7, 1, C_WHITE);
                    if (!(cell % 5u)) cv_rect(x, y + 14 - sy, 4, 1, C_WHITE);
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
