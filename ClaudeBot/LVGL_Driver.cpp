#include "LVGL_Driver.h"
#include "claudebot.h"

static lv_color_t buf1[LVGL_BUF_LEN];
static lv_color_t buf2[LVGL_BUF_LEN];

void Lvgl_Display_LCD(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

void Lvgl_Touchpad_Read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev; (void)data;
}

void example_increase_lvgl_tick(void *arg)
{
    (void)arg;
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void Lvgl_Init(void)
{
    lv_init();

    lv_display_t *disp = lv_display_create(LVGL_WIDTH, LVGL_HEIGHT);
    lv_display_set_flush_cb(disp, Lvgl_Display_LCD);
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, Lvgl_Touchpad_Read);

    ClaudeBot_Init();

    const esp_timer_create_args_t tick_args = {
        .callback = &example_increase_lvgl_tick,
        .name     = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer = NULL;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
}

void Timer_Loop(void)
{
    lv_timer_handler();
}
