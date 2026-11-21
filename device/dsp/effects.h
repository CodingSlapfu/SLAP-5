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
void slap_fx_transport_step(unsigned position, unsigned step_frames, unsigned straight_frames);
void slap_fx_begin(unsigned bpm);
void slap_fx_begin_live(unsigned bpm, uint32_t live);
void slap_fx_begin_assigned(unsigned bpm, uint32_t live, uint32_t assignments);
enum { SLAP_LIVE_DIRECT = 4 + SLAP_FX_PAGES * 4, SLAP_LIVE_PARAMETERS = SLAP_LIVE_DIRECT + 7 };
const char *slap_live_parameter_name(unsigned id);
const char *slap_live_parameter_effect(unsigned id);
unsigned slap_live_parameter_limit(unsigned id);
unsigned slap_live_parameter_value(unsigned id, unsigned raw);
uint32_t slap_fx_config(unsigned track, unsigned page);
int slap_fx_config_valid(unsigned page, uint32_t word);
void slap_fx_restore(unsigned track, unsigned page, uint32_t word);
unsigned slap_fx_active_mask(void);
float slap_fx_sample(unsigned track, float dry);
unsigned slap_fx_mix_active(void);
unsigned slap_fx_brake_speed(unsigned track);
void slap_fx_mix_sample(float *left, float *right);
int slap_fx_response(unsigned track, unsigned point);
int slap_fx_trace(unsigned track, unsigned point);
unsigned slap_fx_slicer_cell(unsigned track, unsigned step);

#if !defined(PLAT_WEB)
enum { SLAP_FX_BATCH = 64, SLAP_FX_WORKER_TRACKS = 2 };
unsigned slap_fx_work(unsigned track);
void slap_fx_checkpoint(unsigned slot, unsigned track, unsigned frames);
void slap_fx_rollback(unsigned slot);
#endif
