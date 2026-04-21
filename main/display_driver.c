#include "display_driver.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "display";

static esp_lcd_panel_handle_t s_panel;
static lv_disp_draw_buf_t     s_draw_buf;
static lv_disp_drv_t          s_disp_drv;
static lv_color_t              s_buf1[LCD_H_RES * 20];
static lv_color_t              s_buf2[LCD_H_RES * 20];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_draw_bitmap(s_panel,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              color_map);
    lv_disp_flush_ready(drv);
}

esp_err_t display_driver_init(void)
{
    ESP_LOGI(TAG, "Init SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num     = CONFIG_LCD_MOSI_GPIO,
        .miso_io_num     = -1,
        .sclk_io_num     = CONFIG_LCD_CLK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * 20 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Init LCD IO");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = CONFIG_LCD_DC_GPIO,
        .cs_gpio_num       = CONFIG_LCD_CS_GPIO,
        .pclk_hz           = 10 * 1000 * 1000,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    ESP_LOGI(TAG, "Init ST7789 panel");
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_LCD_RST_GPIO,
        .rgb_endian     = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    /* Offset colonne requis pour ST7789 172px (contrôleur interne 240px) */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 34, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    /* Backlight ON avant le test pour voir les couleurs */
    ESP_LOGI(TAG, "Backlight ON");
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << CONFIG_LCD_BL_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(CONFIG_LCD_BL_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* TEST HARDWARE : bandes couleur (INVON actif : 0x0000=blanc, 0xF800=cyan, 0x07E0=magenta) */
    {
        static uint16_t test_line[LCD_H_RES];
        for (int y = 0; y < LCD_V_RES; y++) {
            uint16_t color = (y < 107) ? 0x0000 :   /* blanc  */
                             (y < 214) ? 0xF800 :   /* cyan   */
                             0x07E0;                /* magenta */
            for (int x = 0; x < LCD_H_RES; x++) test_line[x] = color;
            esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + 1, test_line);
        }
        ESP_LOGI(TAG, "TEST: 3 bandes couleur envoyees");
    }

    ESP_LOGI(TAG, "Init LVGL");
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, LCD_H_RES * 20);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = LCD_H_RES;
    s_disp_drv.ver_res  = LCD_V_RES;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    return ESP_OK;
}
