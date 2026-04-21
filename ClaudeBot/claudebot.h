#pragma once
#include "LVGL_Driver.h"

#define WIFI_SSID     "Xiaomi_A50B"
#define WIFI_PASSWORD "Pythagore"
#define HTTP_PORT     80

typedef enum {
    BOT_IDLE,
    BOT_THINKING,
    BOT_WAITING,
    BOT_DONE,
    BOT_ALERT,
} bot_state_t;

void ClaudeBot_Init(void);
void ClaudeBot_HandleClient(void);
void ClaudeBot_SetLED(uint8_t r, uint8_t g, uint8_t b);
