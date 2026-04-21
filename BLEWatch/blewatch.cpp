#include "blewatch.h"
#include "LVGL_Driver.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
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

/* ── OUI → fabricant (marques communes) ────────────────────────── */
struct OuiEntry { const char *oui; const char *brand; };
static const OuiEntry kOuiTable[] = {
    /* Apple */
    {"AC:BC:32","Apple"},{"F4:F1:5A","Apple"},{"DC:2C:6E","Apple"},
    {"A4:CF:99","Apple"},{"F0:D1:A9","Apple"},{"60:F8:1D","Apple"},
    {"78:4F:43","Apple"},{"70:EC:E4","Apple"},{"4C:57:CA","Apple"},
    {"28:CF:DA","Apple"},{"BC:9F:EF","Apple"},{"CC:08:8D","Apple"},
    /* Samsung */
    {"8C:77:12","Samsung"},{"BC:20:A4","Samsung"},{"FC:A1:3E","Samsung"},
    {"00:00:F0","Samsung"},{"94:76:B7","Samsung"},{"34:14:5F","Samsung"},
    {"F4:42:01","Samsung"},{"8C:A9:82","Samsung"},
    /* Google */
    {"54:60:09","Google"},{"3C:5A:B4","Google"},{"F4:F5:D8","Google"},
    {"A4:77:33","Google"},{"48:D6:D5","Google"},
    /* Fitbit */
    {"00:00:F0","Fitbit"},{"C0:48:E6","Fitbit"},{"00:2A:B0","Fitbit"},
    /* Garmin */
    {"C0:EE:FB","Garmin"},{"24:0B:CF","Garmin"},{"88:17:DB","Garmin"},
    /* Polar */
    {"00:22:D0","Polar"},{"A0:96:AE","Polar"},
    /* Xiaomi */
    {"28:6C:07","Xiaomi"},{"50:64:2B","Xiaomi"},{"78:11:DC","Xiaomi"},
    {"AC:C1:EE","Xiaomi"},{"64:09:80","Xiaomi"},{"FC:64:BA","Xiaomi"},
    /* Amazfit / Huami */
    {"C8:47:8C","Amazfit"},{"AC:88:FD","Amazfit"},
    /* OnePlus */
    {"8C:8D:28","OnePlus"},{"AC:B3:13","OnePlus"},
    /* Sony */
    {"00:13:A9","Sony"},{"E0:AE:5E","Sony"},{"8C:64:A2","Sony"},
    /* Bose */
    {"88:C6:26","Bose"},{"04:52:C7","Bose"},{"94:54:93","Bose"},
    /* Jabra */
    {"50:C2:ED","Jabra"},{"6C:40:08","Jabra"},
    /* JBL / Harman */
    {"00:26:83","Harman"},{"AC:37:43","Harman"},
    /* Beats */
    {"AC:9B:0A","Beats"},{"B8:78:2E","Beats"},
    /* Texas Instruments */
    {"04:52:C7","TI"},{"00:12:4B","TI"},{"D0:B5:C2","TI"},
    /* Nordic Semi */
    {"F4:CE:36","Nordic"},{"D0:2F:B8","Nordic"},{"F4:F9:51","Nordic"},
    /* Espressif */
    {"30:AE:A4","Espressif"},{"24:0A:C4","Espressif"},{"A4:CF:12","Espressif"},
    /* Raspberry Pi */
    {"DC:A6:32","RPi"},{"E4:5F:01","RPi"},{"28:CD:C1","RPi"},
    /* Tile */
    {"D0:03:4B","Tile"},{"CC:40:D0","Tile"},
};
static constexpr size_t kOuiTableCount = sizeof(kOuiTable) / sizeof(kOuiTable[0]);

static const char *oui_brand(const uint8_t *mac)
{
    char oui[9];
    snprintf(oui, sizeof(oui), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
    for (size_t i = 0; i < kOuiTableCount; i++) {
        if (strcasecmp(oui, kOuiTable[i].oui) == 0) return kOuiTable[i].brand;
    }
    return nullptr;
}

/* ── Vulnerable OUI list (39 entries, documented CVEs) ─────────── */
static const char *kVulnOUI[] = {
    "00:02:72", "00:17:E9", "00:1B:DC",
    "04:52:C7", "00:12:4B", "00:16:C4", "00:18:31", "00:1A:7D",
    "04:A3:16", "78:A5:04", "D0:B5:C2",
    "28:CC:FF", "00:10:18", "00:90:4C", "00:E0:4C", "04:F0:21",
    "60:D8:19", "AC:22:0B", "CC:40:D0", "E4:60:73",
    "00:07:4D", "00:0D:6F", "84:2E:14", "B0:91:22",
    "00:1E:64", "D0:2F:B8", "F4:CE:36", "F4:F9:51",
    "00:24:01", "04:91:62", "D8:80:39",
    "00:00:F0", "8C:77:12", "BC:20:A4", "FC:A1:3E",
    "30:AE:A4", "24:0A:C4", "30:C6:F7", "A4:CF:12",
};
static constexpr size_t kVulnOUICount = sizeof(kVulnOUI) / sizeof(kVulnOUI[0]);

/* mac[0..2] = OUI bytes (Bluedroid stores big-endian) */
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

static BleDevice        s_devices[MAX_DEVICES];
static int              s_device_count = 0;
static SemaphoreHandle_t s_mutex;
static int              s_selected = -1;

/* ── LVGL widgets — vue liste 8 lignes ──────────────────────────── */
#define LIST_ROWS 8
#define ROW_H     36   /* px par ligne : label 14 + barre 8 + espace 14 */
#define HDR_H     26

static lv_obj_t *s_lbl_header;
static lv_obj_t *s_row_lbl[LIST_ROWS];
static lv_obj_t *s_row_bar[LIST_ROWS];

/* ── LED helpers ────────────────────────────────────────────────── */
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}
static void led_off(void) { led_set(0, 0, 0); }

/* ── BLE scan callback ──────────────────────────────────────────── */
class BlewatchCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        uint8_t *mac   = dev.getAddress().getNative();
        int8_t   rssi  = (int8_t)dev.getRSSI();
        const char *name = dev.haveName() ? dev.getName().c_str() : "";

        xSemaphoreTake(s_mutex, portMAX_DELAY);

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
    BLEDevice::init("");
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_scan_cb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    for (;;) {
        scan->start(5, false);
        scan->clearResults();
    }
}

/* ── Trie les indices par RSSI décroissant (insertion sort, ≤64) ── */
static void sort_by_rssi(int *idx, int n)
{
    for (int i = 1; i < n; i++) {
        int key = idx[i];
        int j = i - 1;
        while (j >= 0 && s_devices[idx[j]].rssi < s_devices[key].rssi) {
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = key;
    }
}

/* ── Nom affiché pour un appareil ──────────────────────────────── */
static void device_label(const BleDevice &d, char *out, size_t sz)
{
    const char *brand = oui_brand(d.mac);
    if (d.name[0]) {
        snprintf(out, sz, "%s", d.name);
    } else if (brand) {
        snprintf(out, sz, "%s", brand);
    } else {
        snprintf(out, sz, "%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2]);
    }
    /* Tronque à 10 caractères pour tenir sur la ligne */
    if (strlen(out) > 10) out[10] = '\0';
}

/* ── UI update (LVGL timer) ─────────────────────────────────────── */
static void ui_update_cb(lv_timer_t *t)
{
    (void)t;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Purge appareils non vus depuis 15 s */
    uint32_t now = millis();
    for (int i = 0; i < s_device_count; ) {
        if (now - s_devices[i].last_seen_ms > 15000)
            s_devices[i] = s_devices[--s_device_count];
        else
            i++;
    }

    /* Tri par RSSI */
    int idx[MAX_DEVICES];
    for (int i = 0; i < s_device_count; i++) idx[i] = i;
    sort_by_rssi(idx, s_device_count);

    /* En-tête */
    char buf[48];
    snprintf(buf, sizeof(buf), "BLEWatch  [%d]", s_device_count);
    lv_label_set_text(s_lbl_header, buf);

    /* LED couleur selon le plus proche */
    if (s_device_count > 0) {
        const BleDevice &best = s_devices[idx[0]];
        if (best.vulnerable)                            led_set(255, 0, 0);
        else if (rssi_to_proximity(best.rssi) == PROX_VERY_CLOSE) led_set(0, 0, 255);
        else if (rssi_to_proximity(best.rssi) == PROX_CLOSE)      led_set(0, 200, 255);
        else if (rssi_to_proximity(best.rssi) == PROX_NEAR)       led_set(255, 165, 0);
        else                                            led_set(0, 24, 0);
    } else {
        led_off();
    }

    /* Remplissage des 8 lignes */
    for (int r = 0; r < LIST_ROWS; r++) {
        if (r < s_device_count) {
            const BleDevice &d = s_devices[idx[r]];

            char name[12];
            device_label(d, name, sizeof(name));

            /* "!" rouge si vuln confirmé, "?" gris si check en attente */
            char flag = ' ';
            if (d.vulnerable)                   flag = '!';
            else if (!d.vuln_checked &&
                     rssi_to_proximity(d.rssi) == PROX_VERY_CLOSE) flag = '?';

            snprintf(buf, sizeof(buf), "%-10s %4ddB %c", name, d.rssi, flag);
            lv_label_set_text(s_row_lbl[r], buf);

            /* Couleur texte : rouge si vuln, blanc sinon */
            lv_color_t col = d.vulnerable
                ? lv_color_make(255, 60, 60)
                : lv_color_white();
            lv_obj_set_style_text_color(s_row_lbl[r], col, 0);

            /* Barre signal : -100dBm=0, 0dBm=100 */
            int bar = (int)d.rssi + 100;
            if (bar < 0) bar = 0;
            if (bar > 100) bar = 100;
            lv_bar_set_value(s_row_bar[r], bar, LV_ANIM_OFF);

            /* Couleur barre selon proximité */
            lv_color_t bar_col;
            switch (rssi_to_proximity(d.rssi)) {
                case PROX_VERY_CLOSE: bar_col = lv_color_make(0, 120, 255);  break;
                case PROX_CLOSE:      bar_col = lv_color_make(0, 220, 180);  break;
                case PROX_NEAR:       bar_col = lv_color_make(220, 200, 0);  break;
                default:              bar_col = lv_color_make(80, 80, 80);   break;
            }
            lv_obj_set_style_bg_color(s_row_bar[r], bar_col, LV_PART_INDICATOR);

        } else {
            lv_label_set_text(s_row_lbl[r], "");
            lv_bar_set_value(s_row_bar[r], 0, LV_ANIM_OFF);
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

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* En-tête */
    s_lbl_header = lv_label_create(scr);
    lv_label_set_text(s_lbl_header, "BLEWatch  [0]");
    lv_obj_set_style_text_font(s_lbl_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_header, lv_color_make(0, 200, 255), 0);
    lv_obj_align(s_lbl_header, LV_ALIGN_TOP_MID, 0, 4);

    /* Ligne séparatrice */
    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, 164, 1);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, HDR_H - 2);
    lv_obj_set_style_bg_color(line, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_border_width(line, 0, 0);

    /* 8 lignes de liste */
    for (int r = 0; r < LIST_ROWS; r++) {
        int y = HDR_H + r * ROW_H + 2;

        s_row_lbl[r] = lv_label_create(scr);
        lv_label_set_text(s_row_lbl[r], "");
        lv_obj_set_style_text_font(s_row_lbl[r], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_row_lbl[r], lv_color_white(), 0);
        lv_obj_set_pos(s_row_lbl[r], 4, y);

        s_row_bar[r] = lv_bar_create(scr);
        lv_obj_set_size(s_row_bar[r], 160, 6);
        lv_obj_set_pos(s_row_bar[r], 4, y + 16);
        lv_bar_set_range(s_row_bar[r], 0, 100);
        lv_bar_set_value(s_row_bar[r], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_row_bar[r], lv_color_make(40, 40, 40), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_row_bar[r], lv_color_make(80, 80, 80), LV_PART_INDICATOR);
    }

    lv_timer_create(ui_update_cb, 300, NULL);

    xTaskCreate(scan_task, "ble_scan", 8192, NULL, 1, NULL);
}
