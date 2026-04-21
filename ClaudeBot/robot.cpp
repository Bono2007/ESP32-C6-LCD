#include "robot.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

/* ── Layout (172×320) ──────────────────────────────── */
/* Antenna */
#define ANT_X   83
#define ANT_Y   32
#define ANT_W    6
#define ANT_H   26
#define TIP_CX  86
#define TIP_CY  22
#define TIP_R   10

/* Head (top-left + size) */
#define HEAD_X   21
#define HEAD_Y   55
#define HEAD_W  130
#define HEAD_H   90

/* Eyes (center) */
#define EL_CX   55
#define EL_CY   88
#define ER_CX  117
#define ER_CY   88
#define EYE_R   13
#define PUP_R    6

/* Mouth arc (top-left) */
#define MOUTH_X  68
#define MOUTH_Y 107
#define MOUTH_D  36

/* Body */
#define BODY_X   36
#define BODY_Y  162
#define BODY_W  100
#define BODY_H   65

/* Arms */
#define ARM_W   22
#define ARM_H   42
#define ARM_LX  12
#define ARM_RX 138
#define ARM_Y  172

/* Legs */
#define LEG_W   24
#define LEG_H   30
#define LEG_LX  52
#define LEG_RX  96
#define LEG_Y  230

/* ── Objects ───────────────────────────────────────── */
static lv_obj_t *s_ant_pole, *s_ant_tip;
static lv_obj_t *s_head;
static lv_obj_t *s_eye_l, *s_eye_r;
static lv_obj_t *s_pup_l, *s_pup_r;
static lv_obj_t *s_mouth;
static lv_obj_t *s_body;
static lv_obj_t *s_arm_l, *s_arm_r;
static lv_obj_t *s_leg_l, *s_leg_r;
static lv_obj_t *s_status;
static lv_obj_t *s_overlay;

static lv_timer_t *s_timer    = nullptr;
static bot_state_t s_state    = BOT_IDLE;
static uint32_t    s_tick     = 0;
static uint32_t    s_blink_at = 60;
static bool        s_blinking = false;
static uint32_t    s_blink_t0 = 0;
static uint32_t    s_done_t0  = 0;

/* ── Helpers ───────────────────────────────────────── */
static lv_obj_t *make_rect(lv_obj_t *scr,
    int x, int y, int w, int h, lv_color_t c, int r = 4)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static lv_obj_t *make_circle(lv_obj_t *scr,
    int cx, int cy, int r, lv_color_t c)
{
    return make_rect(scr, cx-r, cy-r, r*2, r*2, c, LV_RADIUS_CIRCLE);
}

static void set_pupils(int dx, int dy)
{
    lv_obj_set_pos(s_pup_l, EL_CX - PUP_R + dx, EL_CY - PUP_R + dy);
    lv_obj_set_pos(s_pup_r, ER_CX - PUP_R + dx, ER_CY - PUP_R + dy);
}

static void set_overlay(lv_color_t c, lv_opa_t opa)
{
    lv_obj_set_style_bg_color(s_overlay, c, 0);
    lv_obj_set_style_bg_opa(s_overlay, opa, 0);
}

static void set_mouth(uint16_t start, uint16_t end)
{
    lv_arc_set_angles(s_mouth, start, end);
}

static void reset_arms(void)
{
    lv_obj_set_pos(s_arm_l, ARM_LX, ARM_Y);
    lv_obj_set_pos(s_arm_r, ARM_RX, ARM_Y);
}

/* ── Init ──────────────────────────────────────────── */
static void anim_cb(lv_timer_t *) { Robot_Tick(); }

void Robot_Init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(6, 8, 15), 0);

    s_ant_pole = make_rect(scr, ANT_X, ANT_Y, ANT_W, ANT_H,
                           lv_color_make(74, 144, 255), 3);
    s_ant_tip  = make_circle(scr, TIP_CX, TIP_CY, TIP_R,
                             lv_color_make(255, 112, 67));

    s_head = make_rect(scr, HEAD_X, HEAD_Y, HEAD_W, HEAD_H,
                       lv_color_make(52, 56, 110), 20);

    s_eye_l = make_circle(scr, EL_CX, EL_CY, EYE_R, lv_color_make(232, 232, 255));
    s_eye_r = make_circle(scr, ER_CX, ER_CY, EYE_R, lv_color_make(232, 232, 255));
    s_pup_l = make_circle(scr, EL_CX, EL_CY, PUP_R, lv_color_make(10, 10, 26));
    s_pup_r = make_circle(scr, ER_CX, ER_CY, PUP_R, lv_color_make(10, 10, 26));

    /* Mouth arc */
    s_mouth = lv_arc_create(scr);
    lv_obj_set_pos(s_mouth, MOUTH_X, MOUTH_Y);
    lv_obj_set_size(s_mouth, MOUTH_D, MOUTH_D);
    lv_arc_set_bg_angles(s_mouth, 0, 360);
    lv_arc_set_angles(s_mouth, 200, 340);  /* smile */
    lv_obj_set_style_arc_opa(s_mouth, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_mouth, lv_color_make(200, 180, 160), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_mouth, 4, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_mouth, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_mouth, LV_OBJ_FLAG_CLICKABLE);

    s_body  = make_rect(scr, BODY_X, BODY_Y, BODY_W, BODY_H,
                        lv_color_make(42, 45, 94), 12);
    s_arm_l = make_rect(scr, ARM_LX, ARM_Y, ARM_W, ARM_H,
                        lv_color_make(42, 45, 94), 8);
    s_arm_r = make_rect(scr, ARM_RX, ARM_Y, ARM_W, ARM_H,
                        lv_color_make(42, 45, 94), 8);
    s_leg_l = make_rect(scr, LEG_LX, LEG_Y, LEG_W, LEG_H,
                        lv_color_make(42, 45, 94), 6);
    s_leg_r = make_rect(scr, LEG_RX, LEG_Y, LEG_W, LEG_H,
                        lv_color_make(42, 45, 94), 6);

    /* Status label */
    s_status = lv_label_create(scr);
    lv_label_set_text(s_status, "");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_status, 172);
    lv_obj_set_pos(s_status, 0, 278);

    /* Full-screen overlay for flash effects */
    s_overlay = make_rect(scr, 0, 0, 172, 320, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_TRANSP, 0);

    s_timer = lv_timer_create(anim_cb, 50, nullptr);  /* 20 fps */
}

/* ── State animations ──────────────────────────────── */

static void tick_idle(void)
{
    float p = sinf(s_tick * 0.05f);  /* ~6s period */

    /* Antenna tip breathes (orange ↔ brighter) */
    uint8_t r = (uint8_t)(200 + p * 55);
    uint8_t g = (uint8_t)( 80 + p * 32);
    lv_obj_set_style_bg_color(s_ant_tip, lv_color_make(r, g, 30), 0);

    /* Blink every 3–5 s */
    if (!s_blinking && s_tick >= s_blink_at) {
        s_blinking = true;
        s_blink_t0 = s_tick;
    }
    if (s_blinking) {
        uint32_t dt = s_tick - s_blink_t0;
        if (dt < 2) {
            lv_obj_set_size(s_pup_l, PUP_R * 2, 2);
            lv_obj_set_size(s_pup_r, PUP_R * 2, 2);
            lv_obj_set_pos(s_pup_l, EL_CX - PUP_R, EL_CY - 1);
            lv_obj_set_pos(s_pup_r, ER_CX - PUP_R, ER_CY - 1);
        } else {
            lv_obj_set_size(s_pup_l, PUP_R * 2, PUP_R * 2);
            lv_obj_set_size(s_pup_r, PUP_R * 2, PUP_R * 2);
            set_pupils(0, 0);
            s_blinking = false;
            s_blink_at = s_tick + 60 + rand() % 40;
        }
    }

    set_mouth(200, 340);
    lv_label_set_text(s_status, "");
    set_overlay(lv_color_black(), LV_OPA_TRANSP);

    uint8_t lb = (uint8_t)(15 + (p + 1.0f) * 15.0f);
    ClaudeBot_SetLED(0, 0, lb);
}

static void tick_thinking(void)
{
    /* Pupils sweep L-R */
    int dx = (int)(sinf(s_tick * 0.3f) * 5.0f);
    set_pupils(dx, 0);

    /* Antenna oscillates */
    int ax = (int)(sinf(s_tick * 0.5f) * 7.0f);
    lv_obj_set_pos(s_ant_tip, TIP_CX - TIP_R + ax, TIP_CY - TIP_R);

    set_mouth(255, 285);  /* petit arc neutre */

    static const char *dots[] = {".", "..", "..."};
    lv_label_set_text(s_status, dots[(s_tick / 6) % 3]);
    lv_obj_set_style_text_color(s_status, lv_color_make(200, 180, 60), 0);

    set_overlay(lv_color_black(), LV_OPA_TRANSP);

    float p = (sinf(s_tick * 0.3f) + 1.0f) * 0.5f;
    ClaudeBot_SetLED((uint8_t)(60 * p), (uint8_t)(50 * p), 0);
}

static void tick_waiting(void)
{
    int dy = (int)(sinf(s_tick * 0.6f) * 4.0f);
    lv_obj_set_pos(s_arm_l, ARM_LX, ARM_Y + dy);
    lv_obj_set_pos(s_arm_r, ARM_RX, ARM_Y + dy);

    set_pupils(0, 0);
    set_mouth(20, 160);  /* frown */

    bool show = (s_tick % 10) < 7;
    lv_label_set_text(s_status, show ? "?" : "");
    lv_obj_set_style_text_color(s_status, lv_color_make(255, 60, 60), 0);

    lv_opa_t opa = (s_tick % 10 < 5) ? 45 : LV_OPA_TRANSP;
    set_overlay(lv_color_make(200, 0, 0), opa);
    ClaudeBot_SetLED((s_tick % 10 < 5) ? 160 : 0, 0, 0);
}

static void tick_done(void)
{
    uint32_t dt = s_tick - s_done_t0;

    if (dt < 10) {
        lv_opa_t opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - dt / 10.0f));
        set_overlay(lv_color_make(0, 200, 80), opa);
        ClaudeBot_SetLED(0, (uint8_t)(120 * (1.0f - dt / 10.0f)), 0);
    } else {
        set_overlay(lv_color_black(), LV_OPA_TRANSP);
        ClaudeBot_SetLED(0, 0, 0);
    }

    /* Bras levés */
    lv_obj_set_pos(s_arm_l, ARM_LX, ARM_Y - 12);
    lv_obj_set_pos(s_arm_r, ARM_RX, ARM_Y - 12);
    set_mouth(190, 350);  /* grand sourire */

    lv_label_set_text(s_status, "OK");
    lv_obj_set_style_text_color(s_status, lv_color_make(60, 220, 80), 0);

    if (dt >= 40) {  /* retour idle après 2 s */
        reset_arms();
        Robot_SetState(BOT_IDLE);
    }
}

static void tick_alert(void)
{
    bool on = (s_tick % 4) < 2;
    set_overlay(lv_color_make(220, 0, 0), on ? LV_OPA_50 : LV_OPA_TRANSP);
    ClaudeBot_SetLED(on ? 200 : 0, 0, 0);
    set_mouth(20, 160);

    lv_label_set_text(s_status, on ? "!" : "");
    lv_obj_set_style_text_color(s_status, lv_color_make(255, 60, 60), 0);

    /* Bras alternés */
    lv_obj_set_pos(s_arm_l, ARM_LX, (s_tick % 8 < 4) ? ARM_Y - 10 : ARM_Y);
    lv_obj_set_pos(s_arm_r, ARM_RX, (s_tick % 8 < 4) ? ARM_Y       : ARM_Y - 10);
}

/* ── Public API ────────────────────────────────────── */
void Robot_Tick(void)
{
    s_tick++;
    switch (s_state) {
        case BOT_IDLE:     tick_idle();     break;
        case BOT_THINKING: tick_thinking(); break;
        case BOT_WAITING:  tick_waiting();  break;
        case BOT_DONE:     tick_done();     break;
        case BOT_ALERT:    tick_alert();    break;
    }
}

void Robot_SetState(bot_state_t new_state)
{
    if (new_state == s_state) return;

    if (s_state == BOT_WAITING || s_state == BOT_DONE ||
        s_state == BOT_ALERT)
        reset_arms();

    if (s_state == BOT_THINKING) {
        lv_obj_set_pos(s_ant_tip, TIP_CX - TIP_R, TIP_CY - TIP_R);
        set_pupils(0, 0);
    }

    s_state = new_state;
    if (new_state == BOT_DONE)    s_done_t0 = s_tick;
    if (new_state == BOT_IDLE)  { set_mouth(200, 340); lv_label_set_text(s_status, ""); }
    if (new_state == BOT_THINKING) { lv_obj_set_size(s_pup_l, PUP_R*2, PUP_R*2); lv_obj_set_size(s_pup_r, PUP_R*2, PUP_R*2); }
}
