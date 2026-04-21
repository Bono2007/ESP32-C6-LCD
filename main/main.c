#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_driver.h"
#include "wifi_manager.h"
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
    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0a0a1a), 0);
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label, lv_color_hex(0xf59e0b), 0);
    lv_label_set_text(label, "WiFi...");
    lv_obj_center(label);

    esp_err_t ret = wifi_manager_init();
    if (ret == ESP_OK) {
        lv_label_set_text(label, "WiFi OK");
        lv_obj_set_style_text_color(label, lv_color_hex(0x10b981), 0);
    } else {
        lv_label_set_text(label, "WiFi FAIL");
        lv_obj_set_style_text_color(label, lv_color_hex(0xef4444), 0);
    }
}
