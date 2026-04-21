#include "bandwatch.h"
#include "LVGL_Driver.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <math.h>

/* ── WS2812 RGB LED ─────────────────────────────────────────────── */
#define LED_PIN    8
#define LED_COUNT  1

static Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ── Channel monitoring config ──────────────────────────────────── */
#define NUM_CHANNELS     13
#define DWELL_MS         260
#define UI_REFRESH_MS    120
#define STRONG_RSSI_DBM  -65
#define MAX_TX_PER_CH    24

struct ChannelStats {
    uint32_t packets;
    uint32_t bytes;
    int8_t   rssi_sum;
    uint8_t  rssi_count;
    uint8_t  strong_count;
    uint8_t  unique_tx;
    uint32_t packets_prev;
    uint32_t bytes_prev;
    uint32_t ts_prev;
};

static ChannelStats s_stats[NUM_CHANNELS];
static SemaphoreHandle_t s_mutex;
static int s_current_ch = 0;

/* ── Promiscuous callback ───────────────────────────────────────── */
static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type == WIFI_PKT_MISC) return;
    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    int ch = s_current_ch;
    if (ch < 0 || ch >= NUM_CHANNELS) return;

    portENTER_CRITICAL_ISR(nullptr);
    s_stats[ch].packets++;
    s_stats[ch].bytes += pkt->rx_ctrl.sig_len;
    int8_t rssi = (int8_t)pkt->rx_ctrl.rssi;
    s_stats[ch].rssi_sum += rssi;
    s_stats[ch].rssi_count++;
    if (rssi >= STRONG_RSSI_DBM) s_stats[ch].strong_count++;
    portEXIT_CRITICAL_ISR(nullptr);
}

/* ── Activity score (logarithmic) ───────────────────────────────── */
static float activity_score(int ch)
{
    const ChannelStats &s = s_stats[ch];
    uint32_t dt = millis() - s.ts_prev;
    if (dt == 0) dt = 1;

    float pps  = (float)(s.packets - s.packets_prev) * 1000.0f / dt;
    float bps  = (float)(s.bytes   - s.bytes_prev)   * 1000.0f / dt;
    float srat = s.rssi_count ? (float)s.strong_count / s.rssi_count : 0.0f;

    float score = (log1p(pps)  / log1p(200.0f)) * 0.40f
                + (log1p(bps)  / log1p(1e6f))   * 0.30f
                + srat                           * 0.20f
                + (s.unique_tx / (float)MAX_TX_PER_CH) * 0.10f;
    return score > 1.0f ? 1.0f : score;
}

/* ── Channel hopper task ────────────────────────────────────────── */
static void hop_task(void *pv)
{
    (void)pv;
    wifi_country_t country = { "FR", 1, 13, 20, WIFI_COUNTRY_POLICY_MANUAL };
    esp_wifi_set_country(&country);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);

    for (;;) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            esp_wifi_set_channel(ch + 1, WIFI_SECOND_CHAN_NONE);
            s_current_ch = ch;
            vTaskDelay(pdMS_TO_TICKS(DWELL_MS));

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_stats[ch].packets_prev = s_stats[ch].packets;
            s_stats[ch].bytes_prev   = s_stats[ch].bytes;
            s_stats[ch].ts_prev      = millis();
            xSemaphoreGive(s_mutex);
        }
    }
}

/* ── LVGL widgets ───────────────────────────────────────────────── */
static lv_obj_t *s_lbl_ch[3];
static lv_obj_t *s_bar_ch[3];
static lv_obj_t *s_lbl_global;
static lv_obj_t *s_bar_global;

/* ── UI update ──────────────────────────────────────────────────── */
static void ui_update_cb(lv_timer_t *t)
{
    (void)t;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Compute scores for all channels */
    float scores[NUM_CHANNELS];
    float global = 0.0f;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        scores[i] = activity_score(i);
        if (scores[i] > global) global = scores[i];
    }

    /* Top 3 busiest */
    int rank[3] = { -1, -1, -1 };
    float used[NUM_CHANNELS];
    memcpy(used, scores, sizeof(scores));
    for (int r = 0; r < 3; r++) {
        float best = -1.0f;
        for (int i = 0; i < NUM_CHANNELS; i++) {
            if (used[i] > best) { best = used[i]; rank[r] = i; }
        }
        if (rank[r] >= 0) used[rank[r]] = -1.0f;
    }

    xSemaphoreGive(s_mutex);

    lv_bar_set_value(s_bar_global, (int)(global * 100), LV_ANIM_ON);

    for (int r = 0; r < 3; r++) {
        if (rank[r] < 0) {
            lv_label_set_text(s_lbl_ch[r], "Ch --");
            lv_bar_set_value(s_bar_ch[r], 0, LV_ANIM_OFF);
        } else {
            char buf[32];
            uint8_t avg_rssi = s_stats[rank[r]].rssi_count
                ? (uint8_t)(s_stats[rank[r]].rssi_sum / s_stats[rank[r]].rssi_count + 100)
                : 0;
            snprintf(buf, sizeof(buf), "Ch %2d  %3u pkts  %d%%",
                rank[r] + 1,
                (unsigned)s_stats[rank[r]].packets,
                avg_rssi);
            lv_label_set_text(s_lbl_ch[r], buf);
            lv_bar_set_value(s_bar_ch[r], (int)(scores[rank[r]] * 100), LV_ANIM_ON);
        }
    }

    /* LED color: green (quiet) → red (saturated) */
    uint8_t g = (uint8_t)((1.0f - global) * 180);
    uint8_t r = (uint8_t)(global * 255);
    led.setPixelColor(0, led.Color(r, g, 0));
    led.show();
}

/* ── Public init ────────────────────────────────────────────────── */
void Bandwatch_Init(void)
{
    led.begin();
    led.setBrightness(60);
    /* Self-test flash */
    led.setPixelColor(0, led.Color(255, 255, 255));
    led.show();
    delay(200);
    led.setPixelColor(0, led.Color(0, 0, 0));
    led.show();

    s_mutex = xSemaphoreCreateMutex();
    memset(s_stats, 0, sizeof(s_stats));

    /* Init WiFi in station mode for promiscuous capture */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    /* Build LVGL UI */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "BandWatch 2.4 GHz");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_make(0, 200, 100), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *lbl_g = lv_label_create(scr);
    lv_label_set_text(lbl_g, "Activité globale");
    lv_obj_set_style_text_font(lbl_g, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_g, lv_color_make(200, 200, 200), 0);
    lv_obj_align(lbl_g, LV_ALIGN_TOP_MID, 0, 30);

    s_bar_global = lv_bar_create(scr);
    lv_obj_set_size(s_bar_global, 150, 12);
    lv_obj_align(s_bar_global, LV_ALIGN_TOP_MID, 0, 50);
    lv_bar_set_range(s_bar_global, 0, 100);
    lv_bar_set_value(s_bar_global, 0, LV_ANIM_OFF);

    lv_obj_t *lbl_top = lv_label_create(scr);
    lv_label_set_text(lbl_top, "Top 3 canaux");
    lv_obj_set_style_text_font(lbl_top, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_top, lv_color_make(180, 180, 180), 0);
    lv_obj_align(lbl_top, LV_ALIGN_TOP_MID, 0, 74);

    for (int r = 0; r < 3; r++) {
        s_lbl_ch[r] = lv_label_create(scr);
        lv_label_set_text(s_lbl_ch[r], "Ch -- ---");
        lv_obj_set_style_text_font(s_lbl_ch[r], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_lbl_ch[r], lv_color_white(), 0);
        lv_obj_align(s_lbl_ch[r], LV_ALIGN_TOP_LEFT, 8, 96 + r * 54);

        s_bar_ch[r] = lv_bar_create(scr);
        lv_obj_set_size(s_bar_ch[r], 155, 10);
        lv_obj_align(s_bar_ch[r], LV_ALIGN_TOP_MID, 0, 116 + r * 54);
        lv_bar_set_range(s_bar_ch[r], 0, 100);
        lv_bar_set_value(s_bar_ch[r], 0, LV_ANIM_OFF);
    }

    lv_timer_create(ui_update_cb, UI_REFRESH_MS, NULL);

    xTaskCreate(hop_task, "band_hop", 4096, NULL, 1, NULL);
}
