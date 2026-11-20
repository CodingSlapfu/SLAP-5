/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "SLAP-5"
#define APP_SPLASH_RGB RGB(45,230,245)
#define APP_CPU2_EARLY 1  /* boot and verify Core 1 after rails settle, before the LCD and DSP init */
#define APP_BLOCK 256u
#define APP_CANVAS_PX (240u*52u)
#define APP_BLE_AT_BOOT 0
enum { OBJ_PANEL, OBJ_SONG1, OBJ_SONG8 = OBJ_SONG1 + 7, OBJ_SETTINGS, OBJ_NOBJ };
/* Eight A/B pairs below the OTA staging window and above AQUORA's bank area. */
#define APP_STORE_MAP {0xDC000u, 0xC0000u, 0xC2000u, 0xC4000u, 0xC6000u, \
    0xC8000u, 0xCA000u, 0xCC000u, 0xCE000u, 0xD0000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *title, const char *message);

#define APP_HOST_EXPECT
int app_expect(const char *name, int *result);
