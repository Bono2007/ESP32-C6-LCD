#include "esp_log.h"
#include "lvgl.h"
#include "cJSON.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Proxmox Display starting (LVGL %d.%d.%d)",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    (void)cJSON_Parse;
}
