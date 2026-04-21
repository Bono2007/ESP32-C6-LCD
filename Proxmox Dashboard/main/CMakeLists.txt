idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "proxmox_client.c"
        "display_driver.c"
        "dashboard_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_http_client
        esp_lcd
        json
        driver
        nvs_flash
        esp_timer
        freertos
)
