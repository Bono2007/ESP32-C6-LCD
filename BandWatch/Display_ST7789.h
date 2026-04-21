#pragma once

#include <Arduino.h>
#include <SPI.h>

#define LCD_WIDTH   172
#define LCD_HEIGHT  320
#define LCD_X_OFFSET 34
#define LCD_Y_OFFSET 0

/* SPI pins (Spotpear / Waveshare ESP32-C6 LCD 1.47") */
#define LCD_MISO_PIN  5
#define LCD_MOSI_PIN  6
#define LCD_SCLK_PIN  7
#define LCD_CS_PIN    14
#define LCD_DC_PIN    15
#define LCD_RST_PIN   21
#define LCD_BL_PIN    22

#define LCD_SPI_FREQ  (80000000UL)

#define BL_PWM_FREQ   1000
#define BL_PWM_RES    10
#define BL_PWM_CH     0

void LCD_Init(void);
void LCD_SetCursor(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_addWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *color);
void Backlight_Init(void);
void Set_Backlight(uint8_t percent);
