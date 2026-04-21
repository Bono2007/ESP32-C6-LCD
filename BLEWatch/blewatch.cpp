#include "blewatch.h"
#include "LVGL_Driver.h"
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <stdint.h>

/* ── WS2812 RGB LED ─────────────────────────────────────────────── */
#define LED_PIN    8
#define LED_COUNT  1

static Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ── RSSI proximity thresholds (dBm) ───────────────────────────── */
static constexpr int8_t kVeryCloseRssi  = -40;
static constexpr int8_t kCloseRssi      = -55;
static constexpr int8_t kNearRssi       = -67;
static constexpr int8_t kTooFarRssi     = -80;

typedef enum { PROX_VERY_CLOSE, PROX_CLOSE, PROX_NEAR, PROX_TOO_FAR, PROX_FAR } Proximity;

static Proximity rssi_to_proximity(int8_t rssi)
{
    if (rssi >= kVeryCloseRssi) return PROX_VERY_CLOSE;
    if (rssi >= kCloseRssi)     return PROX_CLOSE;
    if (rssi >= kNearRssi)      return PROX_NEAR;
    if (rssi >= kTooFarRssi)    return PROX_TOO_FAR;
    return PROX_FAR;
}

static const char *proximity_label(Proximity p)
{
    switch (p) {
        case PROX_VERY_CLOSE: return "TRES PROCHE";
        case PROX_CLOSE:      return "PROCHE";
        case PROX_NEAR:       return "PRES";
        case PROX_TOO_FAR:    return "LOIN";
        default:              return "TRES LOIN";
    }
}

/* ── Vulnerable OUI list (39 entries, documented CVEs) ─────────── */
static const char *kVulnOUI[] = {
    "00:02:72", /* Cambridge Silicon Radio / Qualcomm */
    "00:17:E9",
    "00:1B:DC",
    "04:52:C7", /* Texas Instruments */
    "00:12:4B",
    "00:16:C4",
    "00:18:31",
    "00:1A:7D",
    "04:A3:16",
    "78:A5:04",
    "D0:B5:C2",
    "28:CC:FF", /* Broadcom */
    "00:10:18",
    "00:90:4C",
    "00:E0:4C",
    "04:F0:21",
    "60:D8:19",
    "AC:22:0B",
    "CC:40:D0",
    "E4:60:73",
    "00:07:4D", /* Silicon Labs */
    "00:0D:6F",
    "84:2E:14",
    "B0:91:22",
    "00:1E:64", /* Nordic Semiconductor */
    "D0:2F:B8",
    "F4:CE:36",
    "F4:F9:51",
    "00:24:01", /* Microchip */
    "04:91:62",
    "D8:80:39",
    "00:00:F0", /* Samsung (SweynTooth) */
    "8C:77:12",
    "BC:20:A4",
    "FC:A1:3E",
    "30:AE:A4", /* Espressif */
    "24:0A:C4",
    "30:C6:F7",
    "A4:CF:12",
};
static constexpr size_t kVulnOUICount = sizeof(kVulnOUI) / sizeof(kVulnOUI[0]);

static bool is_vulnerable_oui(const uint8_t *mac)
{
    char oui[9];
    snprintf(oui, sizeof(oui), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
    for (size_t i = 0; i < kVulnOUICount; i++) {
        if (strcasecmp(oui, kVulnOUI[i]) == 0) return true;
    }
    return false;
}

/* ── Device table ───────────────────────────────────────────────── */
#define MAX_DEVICES 64

struct BleDevice {
    uint8_t  mac[6];
    int8_t   rssi;
    char     name[32];
    bool     vulnerable;
    uint32_t last_seen_ms;
    uint32_t very_close_since_ms;
    bool     vuln_checked;
};

static BleDevice   s_devices[MAX_DEVICES];
static int         s_device_count = 0;
static SemaphoreHandle_t s_mutex;

/* Index of the device currently shown (sticky selection) */
static int s_selected = -1;

/* ── LVGL widgets ───────────────────────────────────────────────── */
static lv_obj_t *s_lbl_count;
static lv_obj_t *s_lbl_name;
static lv_obj_t *s_lbl_mac;
static lv_obj_t *s_lbl_rssi;
static lv_obj_t *s_lbl_prox;
static lv_obj_t *s_bar_signal;
static lv_obj_t *s_lbl_vuln;

/* ── LED helpers ────────────────────────────────────────────────── */
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}

static void led_off(void) { led_set(0, 0, 0); }

/* ── BLE scan callback ──────────────────────────────────────────── */
class BlewatchCallback : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *dev) override {
        const NimBLEAddress &addr = dev->getAddress();
        const uint8_t *mac = addr.getNative();
        int8_t rssi = (int8_t)dev->getRSSI();
        const char *name = dev->haveName() ? dev->getName().c_str() : "";

        xSemaphoreTake(s_mutex, portMAX_DELAY);

        /* Search for existing entry */
        int idx = -1;
        for (int i = 0; i < s_device_count; i++) {
            if (memcmp(s_devices[i].mac, mac, 6) == 0) { idx = i; break; }
        }
        if (idx < 0 && s_device_count < MAX_DEVICES) {
            idx = s_device_count++;
            memcpy(s_devices[idx].mac, mac, 6);
            s_devices[idx].very_close_since_ms = 0;
            s_devices[idx].vuln_checked = false;
            s_devices[idx].vulnerable   = false;
        }
        if (idx >= 0) {
            s_devices[idx].rssi         = rssi;
            s_devices[idx].last_seen_ms = millis();
            strncpy(s_devices[idx].name, name, sizeof(s_devices[idx].name) - 1);
            s_devices[idx].name[sizeof(s_devices[idx].name) - 1] = '\0';

            /* Vulnerability check: trigger after 3 s in VERY_CLOSE range */
            if (rssi_to_proximity(rssi) == PROX_VERY_CLOSE) {
                if (s_devices[idx].very_close_since_ms == 0)
                    s_devices[idx].very_close_since_ms = millis();
                if (!s_devices[idx].vuln_checked &&
                    millis() - s_devices[idx].very_close_since_ms >= 3000) {
                    s_devices[idx].vulnerable   = is_vulnerable_oui(mac);
                    s_devices[idx].vuln_checked = true;
                }
            } else {
                s_devices[idx].very_close_since_ms = 0;
            }
        }

        xSemaphoreGive(s_mutex);
    }
};

static BlewatchCallback s_scan_cb;

/* ── Background scan task ───────────────────────────────────────── */
static void scan_task(void *pv)
{
    (void)pv;
    NimBLEDevice::init("");
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_scan_cb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    for (;;) {
        scan->start(5, false);  /* 5-second scan */
        scan->clearResults();
    }
}

/* ── Sticky selection: pick strongest close device ──────────────── */
static int pick_best_device(void)
{
    int best = -1;
    int8_t best_rssi = INT8_MIN;
    for (int i = 0; i < s_device_count; i++) {
        if (rssi_to_proximity(s_devices[i].rssi) <= PROX_NEAR &&
            s_devices[i].rssi > best_rssi) {
            best_rssi = s_devices[i].rssi;
            best      = i;
        }
    }
    /* Sticky: only switch if new candidate is significantly stronger (+10 dB) */
    if (s_selected >= 0 && best >= 0 && best != s_selected &&
        s_devices[best].rssi < s_devices[s_selected].rssi + 10) {
        return s_selected;
    }
    return best;
}

/* ── UI update (called from LVGL timer) ─────────────────────────── */
static void ui_update_cb(lv_timer_t *t)
{
    (void)t;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Purge stale entries (unseen > 10 s) */
    uint32_t now = millis();
    for (int i = 0; i < s_device_count; ) {
        if (now - s_devices[i].last_seen_ms > 10000) {
            s_devices[i] = s_devices[--s_device_count];
            if (s_selected == s_device_count) s_selected = -1;
        } else {
            i++;
        }
    }

    s_selected = pick_best_device();

    char buf[64];
    snprintf(buf, sizeof(buf), "Appareils: %d", s_device_count);
    lv_label_set_text(s_lbl_count, buf);

    if (s_selected < 0) {
        lv_label_set_text(s_lbl_name, "En attente...");
        lv_label_set_text(s_lbl_mac,  "--:--:--:--:--:--");
        lv_label_set_text(s_lbl_rssi, "RSSI: --");
        lv_label_set_text(s_lbl_prox, "");
        lv_label_set_text(s_lbl_vuln, "");
        lv_bar_set_value(s_bar_signal, 0, LV_ANIM_OFF);
        led_off();
    } else {
        const BleDevice &d = s_devices[s_selected];
        Proximity prox = rssi_to_proximity(d.rssi);

        lv_label_set_text(s_lbl_name, d.name[0] ? d.name : "(inconnu)");

        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
        lv_label_set_text(s_lbl_mac, buf);

        snprintf(buf, sizeof(buf), "RSSI: %d dBm", d.rssi);
        lv_label_set_text(s_lbl_rssi, buf);
        lv_label_set_text(s_lbl_prox, proximity_label(prox));

        /* Signal bar: map -100..0 dBm → 0..100 */
        int bar_val = (int)d.rssi + 100;
        if (bar_val < 0)   bar_val = 0;
        if (bar_val > 100) bar_val = 100;
        lv_bar_set_value(s_bar_signal, bar_val, LV_ANIM_ON);

        /* LED color */
        if (d.vulnerable) {
            lv_label_set_text(s_lbl_vuln, "! OUI VULNÉRABLE !");
            lv_obj_set_style_text_color(s_lbl_vuln, lv_color_make(255, 0, 0), 0);
            led_set(255, 0, 0);
        } else {
            switch (prox) {
                case PROX_VERY_CLOSE: led_set(0, 0, 255);   break; /* bleu */
                case PROX_CLOSE:      led_set(0, 255, 255);  break; /* cyan */
                case PROX_NEAR:       led_set(255, 165, 0);  break; /* orange */
                default:              led_set(0, 32, 0);      break; /* vert faible */
            }
            lv_label_set_text(s_lbl_vuln, d.vuln_checked ? "OUI connu" : "");
        }
    }

    xSemaphoreGive(s_mutex);
}

/* ── Public init ────────────────────────────────────────────────── */
void Blewatch_Init(void)
{
    led.begin();
    led.setBrightness(80);
    led_off();

    s_mutex = xSemaphoreCreateMutex();

    /* Build LVGL UI */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "BLEWatch");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_make(0, 200, 255), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_lbl_count = lv_label_create(scr);
    lv_label_set_text(s_lbl_count, "Appareils: 0");
    lv_obj_set_style_text_font(s_lbl_count, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_count, lv_color_white(), 0);
    lv_obj_align(s_lbl_count, LV_ALIGN_TOP_MID, 0, 36);

    s_bar_signal = lv_bar_create(scr);
    lv_obj_set_size(s_bar_signal, 140, 12);
    lv_obj_align(s_bar_signal, LV_ALIGN_TOP_MID, 0, 60);
    lv_bar_set_range(s_bar_signal, 0, 100);
    lv_bar_set_value(s_bar_signal, 0, LV_ANIM_OFF);

    s_lbl_prox = lv_label_create(scr);
    lv_label_set_text(s_lbl_prox, "");
    lv_obj_set_style_text_font(s_lbl_prox, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_prox, lv_color_make(255, 220, 0), 0);
    lv_obj_align(s_lbl_prox, LV_ALIGN_TOP_MID, 0, 84);

    s_lbl_name = lv_label_create(scr);
    lv_label_set_text(s_lbl_name, "En attente...");
    lv_obj_set_style_text_font(s_lbl_name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_name, lv_color_white(), 0);
    lv_obj_set_width(s_lbl_name, 160);
    lv_label_set_long_mode(s_lbl_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(s_lbl_name, LV_ALIGN_TOP_MID, 0, 110);

    s_lbl_mac = lv_label_create(scr);
    lv_label_set_text(s_lbl_mac, "--:--:--:--:--:--");
    lv_obj_set_style_text_font(s_lbl_mac, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_mac, lv_color_make(160, 160, 160), 0);
    lv_obj_align(s_lbl_mac, LV_ALIGN_TOP_MID, 0, 134);

    s_lbl_rssi = lv_label_create(scr);
    lv_label_set_text(s_lbl_rssi, "RSSI: --");
    lv_obj_set_style_text_font(s_lbl_rssi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_rssi, lv_color_make(200, 200, 200), 0);
    lv_obj_align(s_lbl_rssi, LV_ALIGN_TOP_MID, 0, 158);

    s_lbl_vuln = lv_label_create(scr);
    lv_label_set_text(s_lbl_vuln, "");
    lv_obj_set_style_text_font(s_lbl_vuln, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_vuln, lv_color_make(255, 100, 0), 0);
    lv_obj_align(s_lbl_vuln, LV_ALIGN_BOTTOM_MID, 0, -16);

    /* Periodic UI refresh every 200 ms */
    lv_timer_create(ui_update_cb, 200, NULL);

    /* BLE scan runs in its own FreeRTOS task */
    xTaskCreate(scan_task, "ble_scan", 8192, NULL, 1, NULL);
}
