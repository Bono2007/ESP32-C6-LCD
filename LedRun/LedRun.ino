#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "ledrun.h"

void setup() {
    Serial.begin(115200);
    LCD_Init();
    Lvgl_Init();   /* → LedRun_Init() */
}

void loop() {
    Timer_Loop();
    LedRun_HandleClient();
    delay(5);
}
