#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "claudebot.h"

void setup() {
    LCD_Init();
    Lvgl_Init();   /* → ClaudeBot_Init() */
}

void loop() {
    Timer_Loop();
    ClaudeBot_HandleClient();
    delay(5);
}
