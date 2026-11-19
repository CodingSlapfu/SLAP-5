/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once

static const struct DrumKit {
    const char *name;
    uint8_t circuit, kick_pitch, snare_pitch, metal_pitch, clap_rate, decay;
} drum_kits[] = {
    {"STUDIO KIT", 0, 100, 100, 100, 100, 100},
    {"BRUSH KIT", 1, 100, 100, 100, 100, 100},
    {"FM KIT", 2, 100, 100, 100, 100, 100},
    {"INDUSTRIAL", 3, 100, 100, 100, 100, 100},
    {"ACOUSTIC STUDIO", 0, 96, 92, 117, 90, 116},
    {"BRUSHED JAZZ", 1, 79, 72, 72, 67, 145},
    {"ACID JAZZ", 1, 113, 114, 82, 91, 121},
    {"HOUSE", 2, 95, 130, 124, 110, 88},
    {"DEEP HOUSE", 2, 83, 107, 89, 77, 135},
    {"RUFF HOUSE", 3, 103, 127, 110, 135, 104},
    {"HIP-HOP", 0, 83, 86, 75, 84, 147},
    {"BOOM BAP", 0, 94, 79, 93, 125, 89},
    {"TRAP", 2, 72, 138, 140, 115, 165},
    {"808 STYLE", 0, 70, 119, 90, 120, 198},
    {"909 STYLE", 2, 112, 100, 120, 89, 111},
    {"JUNGLE", 1, 121, 133, 125, 96, 79},
    {"DRUM AND BASS", 2, 107, 145, 105, 126, 84},
    {"DUBSTEP", 3, 73, 86, 78, 155, 149},
    {"SYNTHWAVE", 2, 99, 117, 110, 81, 161},
    {"TECHNO", 3, 105, 92, 147, 118, 106},
    {"IDM", 3, 127, 68, 172, 103, 81},
    {"EXPERIMENTAL FM", 2, 142, 152, 132, 147, 124},
    {"LO-FI", 1, 78, 63, 58, 73, 134},
    {"AMBIENT", 1, 67, 83, 80, 65, 185},
    {"ELECTRO", 2, 117, 90, 143, 158, 96},
    {"BREAKS", 0, 108, 129, 134, 107, 93},
    {"DOWNTEMPO", 1, 88, 95, 64, 95, 154},
    {"ELECTRO ACOUSTIC", 0, 134, 119, 75, 152, 117},
    {"WAREHOUSE", 3, 92, 113, 156, 84, 97},
    {"DUST ROOM", 1, 95, 69, 85, 121, 115},
    {"NEON PERC", 2, 131, 148, 122, 168, 87},
    {"GLASS METAL", 3, 151, 125, 183, 124, 157},
};
enum { DRUM_KITS = sizeof(drum_kits) / sizeof(drum_kits[0]) };
