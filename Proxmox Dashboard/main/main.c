#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "display_driver.h"
#include "wifi_manager.h"
#include "proxmox_client.h"
#include "dashboard_ui.h"
#include "lvgl.h"
#include "sdkconfig.h"

static const char       *TAG = "main";
static SemaphoreHandle_t s_data_mux;

static proxmox_data_t s_latest;
static volatile bool  s_data_ready  = false;
static volatile bool  s_error_state = false;
static bool           s_first_fetch = true;
static uint64_t       s_prev_netout = 0;
static uint64_t       s_prev_netin  = 0;
static uint32_t       s_last_update_s = 0;

/* Appelé depuis lvgl_task uniquement → thread-safe vis-à-vis de LVGL */
static void maybe_update_ui(void)
{
    if (!s_data_ready) return;

    xSemaphoreTake(s_data_mux, portMAX_DELAY);
    proxmox_data_t raw = s_latest;
    bool error = s_error_state;
    s_data_ready = false;
    xSemaphoreGive(s_data_mux);

    if (error) {
        ui_data_t err_ui = { .connected = false, .last_update_s = s_last_update_s };
        dashboard_ui_update(&err_ui);
        return;
    }

    float tx_mbps = 0.0f, rx_mbps = 0.0f;
    if (!s_first_fetch) {
        float dt = CONFIG_REFRESH_INTERVAL_MS / 1000.0f;
        tx_mbps = (raw.netout > s_prev_netout)
                  ? (float)(raw.netout - s_prev_netout) / dt / (1024.0f * 1024.0f)
                  : 0.0f;
        rx_mbps = (raw.netin > s_prev_netin)
                  ? (float)(raw.netin - s_prev_netin) / dt / (1024.0f * 1024.0f)
                  : 0.0f;
    }
    s_prev_netout = raw.netout;
    s_prev_netin  = raw.netin;
    s_first_fetch = false;
    s_last_update_s = 0;

    ui_data_t ui = {
        .cpu_pct       = raw.cpu * 100.0f,
        .mem_gb        = (float)raw.mem    / (1024.0f * 1024.0f * 1024.0f),
        .maxmem_gb     = (float)raw.maxmem / (1024.0f * 1024.0f * 1024.0f),
        .tx_mbps       = tx_mbps,
        .rx_mbps       = rx_mbps,
        .uptime_s      = raw.uptime,
        .connected     = true,
        .last_update_s = 0,
    };
    dashboard_ui_update(&ui);
    lv_refr_now(NULL);
}

static void lvgl_task(void *arg)
{
    uint32_t tick_count = 0;
    while (1) {
        maybe_update_ui();          /* mise à jour UI avant le rendu */
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
        if (++tick_count >= 200) {
            tick_count = 0;
            if (s_last_update_s < UINT32_MAX) s_last_update_s++;
        }
    }
}

static void fetch_timer_cb(void *arg)
{
    xTaskNotifyGive((TaskHandle_t)arg);
}

static void fetch_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        proxmox_data_t data;
        esp_err_t ret = proxmox_client_fetch(&data);

        xSemaphoreTake(s_data_mux, portMAX_DELAY);
        if (ret == ESP_OK) {
            s_latest      = data;
            s_error_state = false;
        } else {
            ESP_LOGW(TAG, "Fetch failed");
            s_error_state = true;
            s_first_fetch = true;
        }
        s_data_ready = true;
        xSemaphoreGive(s_data_mux);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Proxmox Display v1.0");

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    s_data_mux = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(display_driver_init());
    dashboard_ui_init();

    TaskHandle_t fetch_handle;
    xTaskCreate(lvgl_task,  "lvgl",  4096, NULL, 5, NULL);
    xTaskCreate(fetch_task, "fetch", 8192, NULL, 3, &fetch_handle);

    esp_err_t wifi_ret = wifi_manager_init();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi echec, redemarrage dans 30s");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
    }

    ESP_ERROR_CHECK(proxmox_client_init());

    xTaskNotifyGive(fetch_handle);

    esp_timer_handle_t timer;
    esp_timer_create_args_t timer_args = {
        .callback = fetch_timer_cb,
        .arg      = fetch_handle,
        .name     = "proxmox_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer,
        (uint64_t)CONFIG_REFRESH_INTERVAL_MS * 1000ULL));

    ESP_LOGI(TAG, "Running. Refresh toutes les %dms", CONFIG_REFRESH_INTERVAL_MS);
}
