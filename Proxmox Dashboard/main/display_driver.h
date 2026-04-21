#pragma once
#include "esp_err.h"

#define LCD_H_RES 172
#define LCD_V_RES 320

esp_err_t display_driver_init(void);
