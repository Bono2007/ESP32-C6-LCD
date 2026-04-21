#pragma once
#include "LVGL_Driver.h"

/* ── WiFi ─────────────────────────────────────────────── */
#define WIFI_SSID     "VotreSSID"
#define WIFI_PASSWORD "VotreMotDePasse"
#define HTTP_PORT     80

/* ── Mode par défaut ──────────────────────────────────── */
#define DEFAULT_WARMTH  8           /* 1–10 */
#define DEFAULT_COLOR   0xFF9329u   /* ambre */
#define DEFAULT_BG      0x000000u

typedef enum { MODE_CANDLE, MODE_TEXT, MODE_SOLID } ledrun_mode_t;

void LedRun_Init(void);
void LedRun_HandleClient(void);
void LedRun_SetLED(uint8_t r, uint8_t g, uint8_t b);
