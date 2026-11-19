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

static void block_text(int x, int y, unsigned pixels, const char *text, uint16_t color)
{
    for (; *text; ++text, x += (int)pixels * 6) {
        int glyph = *text >= 'A' && *text <= 'Z' ? *text - 'A' :
                    *text >= '0' && *text <= '9' ? *text - '0' + 26 : *text == '-' ? 36 : -1;
        if (glyph < 0) continue;
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned column = 0; column < 5; ++column)
                if (slap_block[glyph][row] & (16u >> column))
                    cv_rect(x + (int)(column*pixels), y + (int)(row*pixels), (int)pixels, (int)pixels, color);
    }
}

static const uint16_t track_color[] = {
    RGB(28,226,255), RGB(171,112,255), RGB(255,89,176), RGB(255,180,58), RGB(62,242,137)
};

static int visual_wave(unsigned phase, int amplitude)
{
    return (int)(slap_sine((float)(phase & 1023u) * (1.0f / 1024.0f)) * (float)amplitude);
}

static void visual_disc(int x, int y, int radius, int sy, uint16_t color)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        if (y + dy < sy || y + dy >= sy + 40) continue;
        int span = radius;
        while (span*span + dy*dy > radius*radius) --span;
        cv_rect(x-span, y+dy-sy, 2*span+1, 1, color);
    }
}

static void visualizer_draw(unsigned kind, unsigned time)
{
    static const char *const names[] = {
        "DNA HELIX", "FERROFLUID", "GRAVITY RINGS", "SOLAR SYSTEM",
        "CRT PLASMA", "VU METER", "SPECTRUM REACTOR", "SLAP REACTOR"
    };
    int energy[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t) energy[t] = (int)(track_energy[t] >> 2);
    float meter[2] = {0};
    float levels[SPECTRUM_BANDS];
    static float holds[SPECTRUM_BANDS];
    if (kind == 5)
        for (unsigned c = 0; c < 2; ++c)
            meter[c] = fm_clampf((fm_log2f(fm_maxf(visual_rms[c], 0.000001f)) + 18.0f) / 15.0f, 0.0f, 1.0f);
    for (unsigned b = 0; b < SPECTRUM_BANDS; ++b) {
        levels[b] = fm_clampf((fm_log2f(fm_maxf(visual_band[b], 0.000001f)) + 18.0f) / 13.0f, 0.0f, 1.0f);
        holds[b] = fm_maxf(levels[b], holds[b] - 0.023f);
    }
    for (int sy = 0; sy < 240; sy += 40) {
        cv_begin(240, 40, RGB(4, 7, 14));
        switch (kind) {
        case 0:
            for (int y = 38; y < 230; y += 7) {
                unsigned t = (unsigned)y / 7u % TRACKS;
                int x = visual_wave(time * 3u + (unsigned)y * 13u, 46 + energy[t]);
                if (!(y % 2)) cv_rect(120 - (x < 0 ? -x : x), y - sy, 2 * (x < 0 ? -x : x), 1, RGB(34,54,80));
                cv_rect(116 + x, y - sy, 9, 5, track_color[t]);
                cv_rect(116 - x, y - sy, 9, 5, track_color[(t + 2u) % TRACKS]);
            }
            break;
        case 1:
            for (int x = 0; x < 240; x += 4) {
                for (unsigned t = 0; t < TRACKS; ++t) {
                    int y = 98 + (int)t * 13 + visual_wave((unsigned)x*11u + t*77u + time*5u, 11 + energy[t]);
                    cv_rect(x, y-sy, 3, 5, track_color[t]);
                }
            }
            break;
        case 2:
            for (unsigned t = 0; t < TRACKS; ++t) {
                int radius = 24 + (int)t*17 + energy[t]/5;
                for (unsigned k = 0; k < 1024; k += 37) {
                    unsigned phase = k + time*(3u+t);
                    int x = 120 + visual_wave(phase+256, radius);
                    int y = 130 + visual_wave(phase, radius/2) + visual_wave(2*phase+t*80u,20);
                    visual_disc(x,y,2+(int)(t==4),sy,track_color[t]);
                }
            }
            break;
        case 3: {
            static const uint8_t sizes[] = {2,4,5,3,12,9,6,6};
            static const uint8_t rates[] = {24,17,13,10,6,4,3,2};
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
            visual_disc(120,128,14+bass,sy,RGB(255,143+bass*3,22));
            visual_disc(120,128,11+bass/2,sy,RGB(255,228,102));
            for (unsigned i = 0; i < 8; ++i) {
                int r = 21 + (int)i*12;
                for (unsigned k=0; k<1024; k+=57)
                    cv_rect(120+visual_wave(k+256,r),128+visual_wave(k,r*3/4)-sy,1,1,RGB(46+mids,62+mids,85+mids));
                unsigned phase = time*rates[i]/160u+i*113u;
                int x = 120+visual_wave(phase+256,r), y = 128+visual_wave(phase,r*3/4);
                if (i == 5) cv_line(x-13,y+4-sy,x+13,y-4-sy,RGB(237,202,142));
                visual_disc(x,y,sizes[i],sy,planet[i]);
                if (i == 2 || i == 4) cv_rect(x-2,y-sy,5,1,i == 2 ? RGB(103,212,105) : RGB(165,93,65));
            }
            break;
        }
        case 4:
            for (int y = sy; y < sy + 40; y += 5)
                for (int x = 0; x < 240; x += 6) {
                    unsigned t = (unsigned)(x/48);
                    int a = 69 + visual_wave((unsigned)(x*5+y*4) + time*4u,36+energy[t]) +
                                 visual_wave((unsigned)(y*8) - time*3u,31);
                    cv_rect(x,y-sy,5,3,RGB(clamp(a,0,170),clamp(a+(int)t*15,0,160),clamp(a*2,20,230)));
                }
            break;
        case 5:
            for (unsigned channel = 0; channel < 2; ++channel) {
                int y = 43 + (int)channel * 93;
                cv_rect(9, y-sy, 222, 84, RGB(95,60,31));
                cv_rect(12, y+3-sy, 216, 78, RGB(255,195,100));
                for (unsigned tick = 0; tick <= 10; ++tick) {
                    unsigned angle = 64u + 38u * tick;
                    int xx = 120 + visual_wave(angle+256, 75);
                    int yy = y + 74 - visual_wave(angle, 65);
                    cv_rect(xx, yy-sy, 2, 6, tick <= 2u ? RGB(196,20,46) : C_BLACK);
                }
                unsigned angle = 448u - (unsigned)(384.0f * meter[channel]);
                cv_line(120, y+74-sy, 120+visual_wave(angle+256, 70),
                        y+74-visual_wave(angle, 63)-sy, RGB(151,19,37));
                cv_rect(20, y+55-sy, 9, 9, visual_peak[channel] >= 0.89f ?
                        RGB(255,16,38) : RGB(85,56,31));
                cv_text(190, y+56-sy, &FONT_B, channel ? "R" : "L", C_BLACK);
            }
            break;
        case 6:
            for (unsigned band = 0; band < SPECTRUM_BANDS; ++band) {
                int x = 9 + 32 * (int)band;
                uint16_t color = track_color[band % TRACKS];
                cv_rect(x, 42-sy, 28, 167, RGB(14,27,48));
                int height = (int)(156.0f * levels[band]);
                for (int row = 0; row < height; row += 7)
                    cv_rect(x + 2, 203-row-sy, 24, 5, color);
                cv_rect(x + 2, 203-(int)(156.0f * holds[band])-sy, 24, 2, C_WHITE);
            }
            break;
        default:
            for (unsigned t=0;t<TRACKS;++t) {
                int radius=28+(int)t*14;
                for (unsigned k=0;k<1024;k+=53) {
                    unsigned a=k+time*(t+1u);
                    int x=120+visual_wave(a+256,radius+energy[t]/3);
                    int y=127+visual_wave(a,radius+energy[t]/3);
                    visual_disc(x,y,2+energy[t]/10,sy,track_color[t]);
                }
            }
            cv_rect(46,107-sy,148,35,RGB(8,16,28));
            block_text(49,112-sy,4,"SLAP-5",C_WHITE);
            break;
        }
        if (!sy) {
            cv_rect(0,0,240,33,RGB(4,7,14));
            cv_text(9,6,&FONT_B,names[kind],C_WHITE);
        }
        cv_blit(0, sy);
    }
}

static void draw_boot(unsigned elapsed)
{
    for (int sy = 0; sy < 240; sy += 40) {
        cv_begin(240,40,RGB(4,7,14));
        for (unsigned i = 0; i < TRACKS; ++i) {
            int end = clamp((int)elapsed / 12 - (int)i*20, 0, 210);
            int y = 63 + (int)i*27;
            cv_line(14,y-sy,14+end,y-sy,track_color[i]);
            if (end > 95) {
                int junction = 31+(int)i*35;
                cv_line(junction, y-sy, junction, y-12-sy, track_color[i]);
                cv_rect(junction-3, y-16-sy, 7, 5, track_color[i]);
                int pulse = 14 + (int)((elapsed / 6u + i * 31u) % (unsigned)end);
                cv_rect(pulse, y-1-sy, 5, 3, C_WHITE);
            }
        }
        if (elapsed > 3200) {
            cv_rect(10,94-sy,220,52,RGB(4,7,14));
            int settle = clamp(18 - ((int)elapsed - 3200) / 28, 0, 18);
            block_text(31,101+settle-sy,5,"SLAP-5",C_WHITE);
        }
        cv_blit(0,sy);
    }
}
