/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once
#include <stdint.h>

enum { SLAP_FX_TRACKS = 5, SLAP_FX_PAGES = 9, SLAP_FX_PLOT = 72 };

extern const char *const slap_fx_pages[SLAP_FX_PAGES];
extern const char *const slap_fx_labels[SLAP_FX_PAGES][4];
extern const char *const slap_insert_names[8];

void slap_fx_init(void);
void slap_fx_change(unsigned track, unsigned page, unsigned knob, int delta);
unsigned slap_fx_value(unsigned track, unsigned page, unsigned knob);
void slap_fx_begin(unsigned bpm);
uint32_t slap_fx_config(unsigned track, unsigned page);
int slap_fx_config_valid(unsigned page, uint32_t word);
void slap_fx_restore(unsigned track, unsigned page, uint32_t word);
float slap_fx_sample(unsigned track, float dry);
int slap_fx_response(unsigned track, unsigned point);
int slap_fx_trace(unsigned track, unsigned point);
unsigned slap_fx_slicer_cell(unsigned track, unsigned step);
