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

static void visualizer_draw(unsigned kind, unsigned time)
{
    static const char *const names[] = {
        "DNA HELIX", "CYBER CITY", "FERROFLUID", "STARFIGHTER", "SOLAR SYSTEM",
        "CRT PLASMA", "VINYL", "TESLA LIGHTNING", "PIANO WATERFALL"
    };
    int energy[TRACKS];
    for (unsigned t = 0; t < TRACKS; ++t) energy[t] = (int)(track_energy[t] >> 2);
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
            for (unsigned i = 0; i < 17; ++i) {
                unsigned t = i % TRACKS, seed = i*173u;
                int x = (int)((i*17u + 290u - (time/17u)%290u) % 290u) - 20;
                int h = 36 + (int)(seed % 110u) + energy[t];
                cv_rect(x, 226 - h - sy, 13, h, RGB(24,30,51));
                cv_rect(x, 226 - h - sy, 13, 2, track_color[t]);
                for (int y = 233 - h; y < 225; y += 8)
                    if ((seed + (unsigned)y + time/45u) % 6u < 2u + (unsigned)energy[t]/10u)
                        cv_rect(x+3, y-sy, 6, 3, track_color[t]);
            }
            break;
        case 2:
            for (int x = 0; x < 240; x += 4) {
                for (unsigned t = 0; t < TRACKS; ++t) {
                    int y = 98 + (int)t * 13 + visual_wave((unsigned)x*11u + t*77u + time*5u, 11 + energy[t]);
                    cv_rect(x, y-sy, 3, 5, track_color[t]);
                }
            }
            break;
        case 3:
            for (unsigned i = 0; i < 10; ++i) {
                unsigned t = i % TRACKS;
                int radius = 5 + (int)((i*15u + time/2u) % 145u) + energy[t]/3;
                int center = 120 + visual_wave(time + 70u*i, radius/5);
                cv_line(center-radius, 120-radius/2-sy, center+radius, 120-radius/2-sy, track_color[t]);
                cv_line(center-radius, 120+radius/2-sy, center+radius, 120+radius/2-sy, track_color[t]);
                cv_line(center-radius, 120-radius/2-sy, center-radius, 120+radius/2-sy, track_color[t]);
                cv_line(center+radius, 120-radius/2-sy, center+radius, 120+radius/2-sy, track_color[t]);
            }
            break;
        case 4:
            cv_rect(111, 111-sy, 18, 18, RGB(255,183,89));
            for (unsigned t = 0; t < TRACKS; ++t) {
                int r = 22 + (int)t*19;
                for (unsigned n = 0; n < 1024u; n += 43u)
                    cv_rect(120+visual_wave(n+256,r), 120+visual_wave(n,r/2)-sy, 2, 2, RGB(39,48,67));
                unsigned theta = time * (6u-t) + t*157u;
                int size = 5 + energy[t]/6;
                cv_rect(120-size/2+visual_wave(theta+256,r),120-size/2+visual_wave(theta,r/2)-sy,size,size,track_color[t]);
            }
            break;
        case 5:
            for (int y = sy; y < sy + 40; y += 5)
                for (int x = 0; x < 240; x += 6) {
                    unsigned t = (unsigned)(x/48);
                    int a = 69 + visual_wave((unsigned)(x*5+y*4) + time*4u,36+energy[t]) +
                                 visual_wave((unsigned)(y*8) - time*3u,31);
                    cv_rect(x,y-sy,5,3,RGB(clamp(a,0,170),clamp(a+(int)t*15,0,160),clamp(a*2,20,230)));
                }
            break;
        case 6:
            for (unsigned a = 0; a < 1024; a += 13u)
                for (int r = 26; r < 93; r += 9)
                    cv_rect(112+visual_wave(a+256,r),129+visual_wave(a,r*3/4)-sy,3,3,RGB(55,66,81));
            cv_rect(103,120-sy,18,18,track_color[(positions/4u)%TRACKS]);
            cv_line(207,49-sy,185,101-sy, C_WHITE);
            cv_line(185,101-sy,170,114-sy, C_WHITE);
            cv_rect(110+visual_wave(time*2u,68),127+visual_wave(time*2u+256,51)-sy,5+energy[4]/5,4,track_color[0]);
            break;
        case 7:
            for (unsigned t = 0; t < TRACKS; ++t) {
                int x = 18 + (int)t*45;
                for (int i = 0; i < 11; ++i) {
                    int n = 18 + (int)t*45 + visual_wave(time*19u + t*118u + (unsigned)i*931u,8+energy[t]);
                    cv_line(x,42+i*16-sy,n,58+i*16-sy,track_color[t]);
                    x = n;
                }
            }
            break;
        default:
            for (unsigned t = 0; t < TRACKS; ++t) {
                int x = 20 + (int)t*42;
                cv_line(x+27,38-sy,x,220-sy,track_color[t]);
                for (unsigned i = 0; i < 12; ++i) {
                    unsigned length = tracks[t].mix & 255u;
                    uint32_t word = tracks[t].steps[(positions+i)%length];
                    if (!(word & ACTIVE)) continue;
                    int y = 213 - (int)i*14;
                    int note = (int)(word & NOTES);
                    cv_rect(x + (int)i*2 + (note % 7), y-sy, 16, 4+(int)((word>>16)&255u)/30, track_color[t]);
                }
            }
            cv_rect(12,218-sy,216,2,C_WHITE);
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
            int end = clamp((int)elapsed - (int)i*26, 0, 210);
            int y = 63 + (int)i*27;
            cv_line(14,y-sy,14+end,y-sy,track_color[i]);
            if (end > 95) cv_rect(31+(int)i*35,y-8-sy,7,9,track_color[i]);
        }
        if (elapsed > 245) {
            cv_rect(10,94-sy,220,52,RGB(4,7,14));
            block_text(31,101-sy,5,"SLAP-5",C_WHITE);
        }
        cv_blit(0,sy);
    }
}
