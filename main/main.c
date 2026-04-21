#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_driver.h"
#include "lvgl.h"

static const char *TAG = "main";

static void lvgl_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Proxmox Display starting...");
    ESP_ERROR_CHECK(display_driver_init());

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0a0a1a), 0);
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "PROXMOX");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00d4ff), 0);
    lv_obj_center(label);

    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
}
