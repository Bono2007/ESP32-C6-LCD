#include "dashboard_ui.h"
#include "lvgl.h"
#include <stdio.h>

/* Couleurs */
#define C_BG       0x0a0a1a
#define C_HEADER   0x0d1b3e
#define C_CPU      0x00d4ff
#define C_RAM      0xa855f7
#define C_TX       0x10b981
#define C_RX       0xf59e0b
#define C_UPTIME   0xf472b6
#define C_GRAY     0x64748b
#define C_OK       0x10b981
#define C_ERR      0xef4444
#define C_SEP      0x0f172a

/* Widgets persistants pour mise à jour */
static lv_obj_t *s_lbl_status;
static lv_obj_t *s_lbl_cpu_val;
static lv_obj_t *s_bar_cpu;
static lv_obj_t *s_lbl_ram_val;
static lv_obj_t *s_bar_ram;
static lv_obj_t *s_lbl_tx;
static lv_obj_t *s_lbl_rx;
static lv_obj_t *s_lbl_uptime;
static lv_obj_t *s_lbl_footer;

static lv_obj_t *make_separator(lv_obj_t *parent, int y)
{
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 152, 1);
    lv_obj_set_pos(sep, 10, y);
    lv_obj_set_style_bg_color(sep, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    return sep;
}

void dashboard_ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header (y=0, h=30) */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 172, 30);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(C_HEADER), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, "PROXMOX");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_title, 8, 4);

    lv_obj_t *lbl_host = lv_label_create(header);
    lv_label_set_text(lbl_host, "10.0.0.1 \xc2\xb7 pve");
    lv_obj_set_style_text_color(lbl_host, lv_color_hex(C_GRAY), 0);
    lv_obj_set_style_text_font(lbl_host, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lbl_host, 8, 18);

    s_lbl_status = lv_label_create(header);
    lv_label_set_text(s_lbl_status, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(C_OK), 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_RIGHT_MID, -8, 0);

    /* CPU (y=30, h=55) */
    lv_obj_t *lbl_cpu_title = lv_label_create(scr);
    lv_label_set_text(lbl_cpu_title, "CPU");
    lv_obj_set_style_text_color(lbl_cpu_title, lv_color_hex(C_GRAY), 0);
    lv_obj_set_style_text_font(lbl_cpu_title, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lbl_cpu_title, 10, 36);

    s_lbl_cpu_val = lv_label_create(scr);
    lv_label_set_text(s_lbl_cpu_val, "0%");
    lv_obj_set_style_text_color(s_lbl_cpu_val, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(s_lbl_cpu_val, &lv_font_montserrat_24, 0);
    lv_obj_align(s_lbl_cpu_val, LV_ALIGN_TOP_RIGHT, -10, 32);

    s_bar_cpu = lv_bar_create(scr);
    lv_obj_set_size(s_bar_cpu, 152, 6);
    lv_obj_set_pos(s_bar_cpu, 10, 72);
    lv_bar_set_range(s_bar_cpu, 0, 100);
    lv_obj_set_style_bg_color(s_bar_cpu, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_color(s_bar_cpu, lv_color_hex(C_CPU), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_cpu, 3, 0);
    lv_obj_set_style_radius(s_bar_cpu, 3, LV_PART_INDICATOR);

    make_separator(scr, 82);

    /* RAM (y=83, h=55) */
    lv_obj_t *lbl_ram_title = lv_label_create(scr);
    lv_label_set_text(lbl_ram_title, "RAM");
    lv_obj_set_style_text_color(lbl_ram_title, lv_color_hex(C_GRAY), 0);
    lv_obj_set_style_text_font(lbl_ram_title, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lbl_ram_title, 10, 88);

    s_lbl_ram_val = lv_label_create(scr);
    lv_label_set_text(s_lbl_ram_val, "0 / 0 GB");
    lv_obj_set_style_text_color(s_lbl_ram_val, lv_color_hex(C_RAM), 0);
    lv_obj_set_style_text_font(s_lbl_ram_val, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_ram_val, LV_ALIGN_TOP_RIGHT, -10, 84);

    s_bar_ram = lv_bar_create(scr);
    lv_obj_set_size(s_bar_ram, 152, 6);
    lv_obj_set_pos(s_bar_ram, 10, 118);
    lv_bar_set_range(s_bar_ram, 0, 100);
    lv_obj_set_style_bg_color(s_bar_ram, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_bg_color(s_bar_ram, lv_color_hex(C_RAM), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_ram, 3, 0);
    lv_obj_set_style_radius(s_bar_ram, 3, LV_PART_INDICATOR);

    make_separator(scr, 128);

    /* Réseau (y=129, h=75) */
    lv_obj_t *lbl_net_title = lv_label_create(scr);
    lv_label_set_text(lbl_net_title, "R\xc3\x89SEAU");
    lv_obj_set_style_text_color(lbl_net_title, lv_color_hex(C_GRAY), 0);
    lv_obj_set_style_text_font(lbl_net_title, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lbl_net_title, 10, 133);

    /* Card TX */
    lv_obj_t *card_tx = lv_obj_create(scr);
    lv_obj_set_size(card_tx, 72, 44);
    lv_obj_set_pos(card_tx, 10, 147);
    lv_obj_set_style_bg_color(card_tx, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_border_color(card_tx, lv_color_hex(C_TX), 0);
    lv_obj_set_style_border_side(card_tx, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(card_tx, 2, 0);
    lv_obj_set_style_pad_all(card_tx, 4, 0);
    lv_obj_clear_flag(card_tx, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_tx_title = lv_label_create(card_tx);
    lv_label_set_text(lbl_tx_title, "\xe2\x86\x91 TX");
    lv_obj_set_style_text_color(lbl_tx_title, lv_color_hex(C_TX), 0);
    lv_obj_set_style_text_font(lbl_tx_title, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_tx_title, LV_ALIGN_TOP_MID, 0, 0);

    s_lbl_tx = lv_label_create(card_tx);
    lv_label_set_text(s_lbl_tx, "0.0 MB/s");
    lv_obj_set_style_text_color(s_lbl_tx, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_lbl_tx, &lv_font_montserrat_12, 0);
    lv_obj_align(s_lbl_tx, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Card RX */
    lv_obj_t *card_rx = lv_obj_create(scr);
    lv_obj_set_size(card_rx, 72, 44);
    lv_obj_set_pos(card_rx, 90, 147);
    lv_obj_set_style_bg_color(card_rx, lv_color_hex(C_SEP), 0);
    lv_obj_set_style_border_color(card_rx, lv_color_hex(C_RX), 0);
    lv_obj_set_style_border_side(card_rx, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(card_rx, 2, 0);
    lv_obj_set_style_pad_all(card_rx, 4, 0);
    lv_obj_clear_flag(card_rx, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_rx_title = lv_label_create(card_rx);
    lv_label_set_text(lbl_rx_title, "\xe2\x86\x93 RX");
    lv_obj_set_style_text_color(lbl_rx_title, lv_color_hex(C_RX), 0);
    lv_obj_set_style_text_font(lbl_rx_title, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_rx_title, LV_ALIGN_TOP_MID, 0, 0);

    s_lbl_rx = lv_label_create(card_rx);
    lv_label_set_text(s_lbl_rx, "0.0 MB/s");
    lv_obj_set_style_text_color(s_lbl_rx, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_lbl_rx, &lv_font_montserrat_12, 0);
    lv_obj_align(s_lbl_rx, LV_ALIGN_BOTTOM_MID, 0, 0);

    make_separator(scr, 196);

    /* Uptime (y=197, h=45) */
    lv_obj_t *lbl_up_title = lv_label_create(scr);
    lv_label_set_text(lbl_up_title, "UPTIME");
    lv_obj_set_style_text_color(lbl_up_title, lv_color_hex(C_GRAY), 0);
    lv_obj_set_style_text_font(lbl_up_title, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lbl_up_title, 10, 202);

    s_lbl_uptime = lv_label_create(scr);
    lv_label_set_text(s_lbl_uptime, "0j 00:00");
    lv_obj_set_style_text_color(s_lbl_uptime, lv_color_hex(C_UPTIME), 0);
    lv_obj_set_style_text_font(s_lbl_uptime, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_uptime, LV_ALIGN_TOP_RIGHT, -10, 198);

    make_separator(scr, 240);

    /* Footer (y=241) */
    s_lbl_footer = lv_label_create(scr);
    lv_label_set_text(s_lbl_footer, "...");
    lv_obj_set_style_text_color(s_lbl_footer, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_text_font(s_lbl_footer, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(s_lbl_footer, 10, 245);
}

void dashboard_ui_update(const ui_data_t *data)
{
    char buf[48];

    lv_obj_set_style_text_color(s_lbl_status,
        lv_color_hex(data->connected ? C_OK : C_ERR), 0);

    snprintf(buf, sizeof(buf), "%.0f%%", data->cpu_pct);
    lv_label_set_text(s_lbl_cpu_val, buf);
    lv_bar_set_value(s_bar_cpu, (int)data->cpu_pct, LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.1f / %.0f GB", data->mem_gb, data->maxmem_gb);
    lv_label_set_text(s_lbl_ram_val, buf);
    int ram_pct = (data->maxmem_gb > 0.0f)
                  ? (int)(data->mem_gb / data->maxmem_gb * 100.0f)
                  : 0;
    lv_bar_set_value(s_bar_ram, ram_pct, LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%.1f MB/s", data->tx_mbps);
    lv_label_set_text(s_lbl_tx, buf);
    snprintf(buf, sizeof(buf), "%.1f MB/s", data->rx_mbps);
    lv_label_set_text(s_lbl_rx, buf);

    uint32_t days  = data->uptime_s / 86400;
    uint32_t hours = (data->uptime_s % 86400) / 3600;
    uint32_t mins  = (data->uptime_s % 3600) / 60;
    snprintf(buf, sizeof(buf), "%dj %02d:%02d", days, hours, mins);
    lv_label_set_text(s_lbl_uptime, buf);

    snprintf(buf, sizeof(buf), "M\xc3\xa0J il y a %ds", data->last_update_s);
    lv_label_set_text(s_lbl_footer, buf);
}
