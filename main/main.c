#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_driver.h"
#include "dashboard_ui.h"
#include "lvgl.h"

static void lvgl_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(display_driver_init());
    dashboard_ui_init();
    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);

    ui_data_t mock = {
        .cpu_pct       = 67.0f,
        .mem_gb        = 12.4f,
        .maxmem_gb     = 32.0f,
        .tx_mbps       = 2.3f,
        .rx_mbps       = 8.1f,
        .uptime_s      = 14 * 86400 + 7 * 3600 + 32 * 60,
        .connected     = true,
        .last_update_s = 3,
    };
    dashboard_ui_update(&mock);
}
