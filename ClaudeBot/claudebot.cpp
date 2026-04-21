#include "claudebot.h"
#include "robot.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>

#define LED_PIN 8

static Adafruit_NeoPixel s_led(1, LED_PIN, NEO_GRB + NEO_KHZ800);
static WebServer         s_server(HTTP_PORT);
static bool              s_wifi_ok = false;

void ClaudeBot_SetLED(uint8_t r, uint8_t g, uint8_t b)
{
    s_led.setPixelColor(0, s_led.Color(r, g, b));
    s_led.show();
}

/* ── Handlers ──────────────────────────────────────── */
static void handle_state(void)
{
    if (!s_server.hasArg("mode")) {
        s_server.send(400, "application/json", "{\"error\":\"mode required\"}");
        return;
    }
    String mode = s_server.arg("mode");
    bot_state_t st = BOT_IDLE;

    if      (mode == "thinking") st = BOT_THINKING;
    else if (mode == "working")  st = BOT_WORKING;
    else if (mode == "juggling") st = BOT_JUGGLING;
    else if (mode == "sweeping") st = BOT_SWEEPING;
    else if (mode == "sleeping") st = BOT_SLEEPING;
    else if (mode == "waiting")  st = BOT_WAITING;
    else if (mode == "done")     st = BOT_DONE;
    else if (mode == "alert")    st = BOT_ALERT;
    else if (mode == "idle")     st = BOT_IDLE;

    Robot_SetState(st);
    s_server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_root(void)
{
    s_server.send(200, "text/plain",
        "ClaudeBot OK\n"
        "GET /state?mode=idle|thinking|working|juggling|sweeping|sleeping|waiting|done|alert\n");
}

/* ── Init ──────────────────────────────────────────── */
void ClaudeBot_Init(void)
{
    s_led.begin();
    s_led.setBrightness(80);
    ClaudeBot_SetLED(0, 0, 40);  /* bleu démarrage */

    /* Splash WiFi */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Connexion WiFi...");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(80, 80, 80), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *lbl_ip = lv_label_create(scr);
    lv_label_set_text(lbl_ip, "");
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_ip, 172);
    lv_obj_set_style_text_align(lbl_ip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, 14);
    lv_timer_handler();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000)
        delay(250);

    char buf[48];
    if (WiFi.status() == WL_CONNECTED) {
        s_wifi_ok = true;
        snprintf(buf, sizeof(buf), "%s", WiFi.localIP().toString().c_str());
        lv_obj_set_style_text_color(lbl_ip, lv_color_make(0, 200, 100), 0);
        ClaudeBot_SetLED(0, 40, 0);
        s_server.on("/",      HTTP_GET, handle_root);
        s_server.on("/state", HTTP_GET, handle_state);
        s_server.begin();
    } else {
        snprintf(buf, sizeof(buf), "Pas de WiFi");
        lv_obj_set_style_text_color(lbl_ip, lv_color_make(180, 60, 60), 0);
        ClaudeBot_SetLED(40, 0, 0);
    }

    lv_label_set_text(lbl_ip, buf);
    lv_timer_handler();
    delay(2500);

    lv_obj_clean(scr);
    Robot_Init();
}

void ClaudeBot_HandleClient(void)
{
    if (s_wifi_ok) s_server.handleClient();
}
