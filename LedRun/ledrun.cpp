#include "ledrun.h"
#include "mode_candle.h"
#include "mode_text.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <string.h>
#include <stdlib.h>

#define LED_PIN   8
#define LED_COUNT 1

static Adafruit_NeoPixel s_led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static WebServer         s_server(HTTP_PORT);

static ledrun_mode_t s_mode     = MODE_CANDLE;
static uint8_t       s_warmth   = DEFAULT_WARMTH;
static uint32_t      s_color    = DEFAULT_COLOR;
static uint32_t      s_bg_color = DEFAULT_BG;
static char          s_text[64] = "Hello";

/* ── LED ─────────────────────────────────────────────── */
void LedRun_SetLED(uint8_t r, uint8_t g, uint8_t b)
{
    s_led.setPixelColor(0, s_led.Color(r, g, b));
    s_led.show();
}

/* ── Mode switch ─────────────────────────────────────── */
static void switch_mode(ledrun_mode_t new_mode)
{
    if (s_mode == MODE_CANDLE) Candle_Stop();
    if (s_mode == MODE_TEXT)   Text_Stop();

    lv_obj_clean(lv_scr_act());
    s_mode = new_mode;

    if (s_mode == MODE_CANDLE) Candle_Start(s_warmth, s_color);
    if (s_mode == MODE_TEXT)   Text_Start(s_text, s_color, s_bg_color);
    if (s_mode == MODE_SOLID) {
        uint8_t r = (s_bg_color >> 16) & 0xFF;
        uint8_t g = (s_bg_color >>  8) & 0xFF;
        uint8_t b =  s_bg_color        & 0xFF;
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(r, g, b), 0);
        LedRun_SetLED(r / 3, g / 3, b / 3);
    }
}

/* ── Utilitaires HTTP ────────────────────────────────── */
static uint32_t parse_color(const String &s, uint32_t def)
{
    if (s.length() < 6) return def;
    return (uint32_t)strtoul(s.c_str(), nullptr, 16);
}

static void send_ok(void)
{
    s_server.send(200, "application/json", "{\"ok\":true}");
}

/* ── Handlers HTTP ───────────────────────────────────── */
static void handle_root(void)
{
    char buf[160];
    const char *mode_str = (s_mode == MODE_CANDLE) ? "candle"
                         : (s_mode == MODE_TEXT)   ? "text"
                                                   : "solid";
    snprintf(buf, sizeof(buf),
        "{\"mode\":\"%s\",\"warmth\":%d,\"text\":\"%s\","
        "\"color\":\"%06X\",\"bg\":\"%06X\"}",
        mode_str, s_warmth, s_text, s_color, s_bg_color);
    s_server.send(200, "application/json", buf);
}

static void handle_candle(void)
{
    if (s_server.hasArg("warmth")) {
        int w = s_server.arg("warmth").toInt();
        s_warmth = (w < 1) ? 1 : (w > 10) ? 10 : (uint8_t)w;
    }
    if (s_server.hasArg("color"))
        s_color = parse_color(s_server.arg("color"), s_color);

    switch_mode(MODE_CANDLE);
    send_ok();
}

static void handle_text(void)
{
    if (s_server.hasArg("text")) {
        strncpy(s_text, s_server.arg("text").c_str(), sizeof(s_text) - 1);
        s_text[sizeof(s_text) - 1] = '\0';
    }
    if (s_server.hasArg("color"))
        s_color = parse_color(s_server.arg("color"), s_color);
    if (s_server.hasArg("bg"))
        s_bg_color = parse_color(s_server.arg("bg"), s_bg_color);

    if (s_mode == MODE_TEXT)
        Text_SetText(s_text);   /* mise à jour à chaud */
    else
        switch_mode(MODE_TEXT);

    send_ok();
}

static void handle_solid(void)
{
    if (s_server.hasArg("color"))
        s_bg_color = parse_color(s_server.arg("color"), 0xFF0000u);

    switch_mode(MODE_SOLID);
    send_ok();
}

/* ── Init ────────────────────────────────────────────── */
void LedRun_Init(void)
{
    s_led.begin();
    s_led.setBrightness(80);
    LedRun_SetLED(0, 0, 40);   /* bleu = démarrage */

    /* Écran de connexion */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Connexion WiFi...");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(80, 80, 80), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *lbl_ip = lv_label_create(scr);
    lv_label_set_text(lbl_ip, "");
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ip, lv_color_make(0, 200, 100), 0);
    lv_obj_set_width(lbl_ip, LCD_WIDTH - 8);
    lv_obj_set_style_text_align(lbl_ip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, 10);

    lv_timer_handler();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(250);
    }

    char ip_buf[48];
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(ip_buf, sizeof(ip_buf), "%s", WiFi.localIP().toString().c_str());
        LedRun_SetLED(0, 60, 0);

        s_server.on("/",       HTTP_GET, handle_root);
        s_server.on("/candle", HTTP_GET, handle_candle);
        s_server.on("/text",   HTTP_GET, handle_text);
        s_server.on("/solid",  HTTP_GET, handle_solid);
        s_server.begin();
    } else {
        snprintf(ip_buf, sizeof(ip_buf), "Pas de WiFi — mode local");
        LedRun_SetLED(60, 0, 0);
    }

    lv_label_set_text(lbl_ip, ip_buf);
    lv_timer_handler();
    delay(2500);

    lv_obj_clean(scr);
    switch_mode(MODE_CANDLE);
}

void LedRun_HandleClient(void)
{
    if (WiFi.status() == WL_CONNECTED)
        s_server.handleClient();
}
