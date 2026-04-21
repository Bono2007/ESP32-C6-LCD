#include "threadscan.h"
#include "LVGL_Driver.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_NeoPixel.h>
#include <esp_ieee802154.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>

/* ── LED ────────────────────────────────────────────────────────── */
#define LED_PIN   8
#define LED_COUNT 1
static Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ── 802.15.4 energy scan (canaux 11–26) ────────────────────────── */
#define CH_MIN  11
#define CH_MAX  26
#define CH_NUM  (CH_MAX - CH_MIN + 1)  /* 16 */

static int8_t            s_energy[CH_NUM];
static volatile bool     s_ed_done = false;
static volatile int      s_ed_ch   = 0;
static SemaphoreHandle_t s_mutex;

/* Callback faible fourni par le SDK — on le redéfinit ici */
extern "C" void esp_ieee802154_energy_detect_done(int8_t power)
{
    s_energy[s_ed_ch] = power;
    s_ed_done = true;
}

/* Scan 802.15.4 synchrone canal par canal */
static void scan_802154(void)
{
    esp_ieee802154_enable();
    esp_ieee802154_set_panid(0xFFFF);
    esp_ieee802154_set_short_address(0xFFFE);

    for (int ch = CH_MIN; ch <= CH_MAX; ch++) {
        s_ed_ch  = ch - CH_MIN;
        s_ed_done = false;
        esp_ieee802154_set_channel((uint8_t)ch);
        esp_ieee802154_energy_detect(20);          /* 20 ms par canal */
        uint32_t t0 = millis();
        while (!s_ed_done && millis() - t0 < 100) vTaskDelay(1);
        if (!s_ed_done) s_energy[s_ed_ch] = -100;
    }

    esp_ieee802154_disable();
}

/* ── Matter BLE (UUID de service 0xFFF6) ────────────────────────── */
#define MATTER_UUID  "0000fff6-0000-1000-8000-00805f9b34fb"
#define MAX_MATTER   5

struct MatterDevice {
    char    name[32];
    uint8_t mac[6];
    int8_t  rssi;
};

static MatterDevice s_matter[MAX_MATTER];
static int          s_matter_count = 0;

class MatterCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        if (!dev.haveServiceUUID()) return;
        /* Cherche UUID Matter 0xFFF6 parmi les UUIDs annoncés */
        bool found = false;
        for (int i = 0; i < (int)dev.getServiceUUIDCount(); i++) {
            if (dev.getServiceUUID(i).equals(BLEUUID(MATTER_UUID))) {
                found = true; break;
            }
        }
        if (!found) return;

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_matter_count < MAX_MATTER) {
            MatterDevice &d = s_matter[s_matter_count++];
            memcpy(d.mac, dev.getAddress().getNative(), 6);
            d.rssi = (int8_t)dev.getRSSI();
            strncpy(d.name,
                dev.haveName() ? dev.getName().c_str() : "Matter device",
                sizeof(d.name) - 1);
            d.name[sizeof(d.name) - 1] = '\0';
        }
        xSemaphoreGive(s_mutex);
    }
};

static MatterCallback s_matter_cb;

static void scan_ble_matter(void)
{
    s_matter_count = 0;
    BLEDevice::init("");
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_matter_cb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(5, false);    /* 5 secondes */
    scan->clearResults();
    BLEDevice::deinit(true);  /* libère la radio pour le prochain cycle 802.15.4 */
}

/* ── LVGL widgets ───────────────────────────────────────────────── */

/* Section 802.15.4 */
#define BAR_W  8     /* largeur d'une barre canal px */
#define BAR_H  48    /* hauteur max */
#define BAR_Y  42    /* y du bas des barres */

static lv_obj_t *s_bar_rect[CH_NUM];   /* rectangles verticaux */
static lv_obj_t *s_lbl_best;           /* meilleur canal */
static lv_obj_t *s_lbl_154_status;

/* Section Matter BLE */
static lv_obj_t *s_matter_lbl[MAX_MATTER];
static lv_obj_t *s_lbl_matter_status;

/* Statut cycle */
static lv_obj_t *s_lbl_phase;

/* ── Couleur barre selon énergie ────────────────────────────────── */
static lv_color_t energy_color(int8_t e)
{
    if (e >= -75) return lv_color_make(255,  60,  60);   /* rouge  — fort */
    if (e >= -85) return lv_color_make(255, 180,   0);   /* orange — moyen */
    if (e >= -92) return lv_color_make(100, 200,  80);   /* vert   — faible */
    return              lv_color_make( 40,  40,  40);    /* gris   — rien */
}

/* ── Mise à jour UI depuis le résultat des scans ────────────────── */
static void update_ui(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* — Barres 802.15.4 — */
    int   best_ch  = CH_MIN;
    int8_t best_e  = -127;
    for (int i = 0; i < CH_NUM; i++) {
        int8_t e = s_energy[i];

        /* Hauteur proportionnelle : -100 dBm → 2px, -40 dBm → BAR_H */
        int h = (int)(e + 100) * BAR_H / 60;
        if (h < 2)    h = 2;
        if (h > BAR_H) h = BAR_H;

        lv_obj_set_height(s_bar_rect[i], h);
        lv_obj_set_y(s_bar_rect[i], BAR_Y - h);
        lv_obj_set_style_bg_color(s_bar_rect[i], energy_color(e), 0);

        if (e > best_e) { best_e = e; best_ch = CH_MIN + i; }
    }

    /* Meilleur canal */
    char buf[48];
    if (best_e > -92) {
        snprintf(buf, sizeof(buf), "Meilleur: ch %d (%d dBm)", best_ch, best_e);
        lv_obj_set_style_text_color(s_lbl_best,
            best_e >= -80 ? lv_color_make(255,60,60) : lv_color_make(255,200,0), 0);
    } else {
        snprintf(buf, sizeof(buf), "Spectre libre");
        lv_obj_set_style_text_color(s_lbl_best, lv_color_make(80,80,80), 0);
    }
    lv_label_set_text(s_lbl_best, buf);

    /* LED : rouge si réseau détecté, vert si libre */
    if (best_e >= -85) {
        led.setPixelColor(0, led.Color(200, 0, 0));
    } else {
        led.setPixelColor(0, led.Color(0, 80, 0));
    }
    led.show();

    /* — Matter BLE — */
    for (int i = 0; i < MAX_MATTER; i++) {
        if (i < s_matter_count) {
            snprintf(buf, sizeof(buf), "%-14s %ddB",
                s_matter[i].name, s_matter[i].rssi);
            lv_label_set_text(s_matter_lbl[i], buf);
        } else {
            lv_label_set_text(s_matter_lbl[i], "");
        }
    }

    if (s_matter_count == 0) {
        lv_label_set_text(s_lbl_matter_status, "Aucun appareil Matter");
        lv_obj_set_style_text_color(s_lbl_matter_status,
            lv_color_make(80, 80, 80), 0);
    } else {
        snprintf(buf, sizeof(buf), "%d appareil(s) Matter", s_matter_count);
        lv_label_set_text(s_lbl_matter_status, buf);
        lv_obj_set_style_text_color(s_lbl_matter_status,
            lv_color_make(0, 220, 120), 0);
    }

    xSemaphoreGive(s_mutex);
}

/* ── Tâche de scan principale ───────────────────────────────────── */
static void scan_task(void *pv)
{
    (void)pv;
    nvs_flash_init();   /* requis avant BLE */

    for (;;) {
        /* Phase 1 : 802.15.4 (~350 ms) */
        lv_label_set_text(s_lbl_phase, "802.15.4 scan...");
        scan_802154();

        /* Phase 2 : BLE Matter (5 s) */
        lv_label_set_text(s_lbl_phase, "BLE Matter scan (5s)...");
        scan_ble_matter();

        /* Mise à jour écran */
        lv_label_set_text(s_lbl_phase, "");
        update_ui();

        vTaskDelay(pdMS_TO_TICKS(2000));   /* pause 2 s avant prochain cycle */
    }
}

/* ── Init UI ────────────────────────────────────────────────────── */
void ThreadScan_Init(void)
{
    led.begin();
    led.setBrightness(70);
    led.setPixelColor(0, led.Color(0, 0, 80));
    led.show();

    s_mutex = xSemaphoreCreateMutex();
    memset(s_energy,  -100, sizeof(s_energy));
    memset(s_matter,     0, sizeof(s_matter));

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* ── En-tête ── */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ThreadScan");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_make(80, 180, 255), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);

    s_lbl_phase = lv_label_create(scr);
    lv_label_set_text(s_lbl_phase, "init...");
    lv_obj_set_style_text_font(s_lbl_phase, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_phase, lv_color_make(120, 120, 120), 0);
    lv_obj_align(s_lbl_phase, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* ── Section 802.15.4 ── */
    lv_obj_t *lbl_154 = lv_label_create(scr);
    lv_label_set_text(lbl_154, "802.15.4  ch 11 ──── 26");
    lv_obj_set_style_text_font(lbl_154, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_154, lv_color_make(180, 180, 180), 0);
    lv_obj_set_pos(lbl_154, 4, 22);

    /* 16 barres verticales */
    int x_start = 4;
    for (int i = 0; i < CH_NUM; i++) {
        s_bar_rect[i] = lv_obj_create(scr);
        lv_obj_set_size(s_bar_rect[i], BAR_W, 2);
        lv_obj_set_pos(s_bar_rect[i], x_start + i * (BAR_W + 1), BAR_Y - 2);
        lv_obj_set_style_bg_color(s_bar_rect[i], lv_color_make(40, 40, 40), 0);
        lv_obj_set_style_border_width(s_bar_rect[i], 0, 0);
        lv_obj_set_style_radius(s_bar_rect[i], 2, 0);
    }

    /* Label axe canaux */
    lv_obj_t *lbl_ch = lv_label_create(scr);
    lv_label_set_text(lbl_ch, "11  13  15  17  19  21  23  25");
    lv_obj_set_style_text_font(lbl_ch, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ch, lv_color_make(60, 60, 60), 0);
    lv_obj_set_pos(lbl_ch, 4, BAR_Y + 2);

    s_lbl_best = lv_label_create(scr);
    lv_label_set_text(s_lbl_best, "Scan en cours...");
    lv_obj_set_style_text_font(s_lbl_best, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_best, lv_color_make(120, 120, 120), 0);
    lv_obj_set_pos(s_lbl_best, 4, BAR_Y + 16);

    /* Séparateur */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 164, 1);
    lv_obj_set_pos(sep, 4, BAR_Y + 34);
    lv_obj_set_style_bg_color(sep, lv_color_make(50, 50, 50), 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    /* ── Section Matter BLE ── */
    lv_obj_t *lbl_matter = lv_label_create(scr);
    lv_label_set_text(lbl_matter, "Matter BLE  (UUID 0xFFF6)");
    lv_obj_set_style_text_font(lbl_matter, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_matter, lv_color_make(180, 180, 180), 0);
    lv_obj_set_pos(lbl_matter, 4, BAR_Y + 38);

    int y_matter = BAR_Y + 56;
    for (int i = 0; i < MAX_MATTER; i++) {
        s_matter_lbl[i] = lv_label_create(scr);
        lv_label_set_text(s_matter_lbl[i], "");
        lv_obj_set_style_text_font(s_matter_lbl[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_matter_lbl[i], lv_color_make(0, 210, 130), 0);
        lv_obj_set_pos(s_matter_lbl[i], 4, y_matter + i * 20);
    }

    s_lbl_matter_status = lv_label_create(scr);
    lv_label_set_text(s_lbl_matter_status, "Scan en cours...");
    lv_obj_set_style_text_font(s_lbl_matter_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_matter_status, lv_color_make(80, 80, 80), 0);
    lv_obj_set_pos(s_lbl_matter_status, 4, y_matter + MAX_MATTER * 20 + 4);

    xTaskCreate(scan_task, "thread_scan", 8192, NULL, 1, NULL);
}
