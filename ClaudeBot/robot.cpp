#include "robot.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

/* ── Layout (172×320) ──────────────────────────────── */
/* Oreilles (remplacent l'antenne) */
#define EAR_L_CX  46
#define EAR_L_CY  56
#define EAR_R_CX 126
#define EAR_R_CY  56
#define EAR_R     14

/* Tête */
#define HEAD_X    14
#define HEAD_Y    62
#define HEAD_W   144
#define HEAD_H   100

/* Sourcils (rects fins au-dessus des yeux) */
#define BROW_H     5
#define BROW_W    28
#define BROW_L_X  38
#define BROW_R_X 106
#define BROW_Y    82

/* Yeux (centres) */
#define EL_CX   57
#define EL_CY  110
#define ER_CX  115
#define ER_CY  110
#define EYE_R   15
#define PUP_R    7

/* Bouche arc */
#define MOUTH_X   66
#define MOUTH_Y  138
#define MOUTH_D   40

/* Corps */
#define BODY_X   36
#define BODY_Y  178
#define BODY_W  100
#define BODY_H   60

/* Bras */
#define ARM_W   22
#define ARM_H   44
#define ARM_LX  11
#define ARM_RX 139
#define ARM_Y  180

/* Jambes */
#define LEG_W   24
#define LEG_H   28
#define LEG_LX  52
#define LEG_RX  96
#define LEG_Y  240

/* Status label */
#define STATUS_Y 282

/* ── Palette Clawd (orange Claude) ─────────────────── */
#define C_BG        lv_color_make(  6,   8,  15)
#define C_HEAD      lv_color_make(212,  80,  28)   /* orange Claude */
#define C_BODY      lv_color_make(180,  62,  18)   /* orange foncé */
#define C_EAR       lv_color_make(240, 110,  55)   /* orange clair */
#define C_EYE       lv_color_make(255, 240, 230)   /* blanc chaud */
#define C_PUPIL     lv_color_make( 20,   5,   0)   /* brun très foncé */
#define C_BROW      lv_color_make(120,  35,   5)   /* brun sourcil */
#define C_MOUTH     lv_color_make(255, 200, 170)   /* beige peau */
#define C_RED       lv_color_make(200,   0,   0)
#define C_GREEN     lv_color_make(  0, 190,  80)

/* ── Objets LVGL ───────────────────────────────────── */
static lv_obj_t *s_ear_l, *s_ear_r;
static lv_obj_t *s_head;
static lv_obj_t *s_brow_l, *s_brow_r;
static lv_obj_t *s_eye_l, *s_eye_r;
static lv_obj_t *s_pup_l, *s_pup_r;
static lv_obj_t *s_mouth;
static lv_obj_t *s_body;
static lv_obj_t *s_arm_l, *s_arm_r;
static lv_obj_t *s_leg_l, *s_leg_r;
static lv_obj_t *s_status;
static lv_obj_t *s_overlay;

static lv_timer_t *s_timer  = nullptr;
static bot_state_t s_state  = BOT_IDLE;
static uint32_t    s_tick   = 0;
static uint32_t    s_blink_at = 70;
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

static lv_obj_t *make_circle(lv_obj_t *scr, int cx, int cy, int r, lv_color_t c)
{
    return make_rect(scr, cx - r, cy - r, r * 2, r * 2, c, LV_RADIUS_CIRCLE);
}

static void set_pupils(int dx, int dy)
{
    lv_obj_set_pos(s_pup_l, EL_CX - PUP_R + dx, EL_CY - PUP_R + dy);
    lv_obj_set_pos(s_pup_r, ER_CX - PUP_R + dx, ER_CY - PUP_R + dy);
}

static void set_brows(int dy_inner, int dy_outer)
{
    /* Sourcil gauche : côté interne = droite, côté externe = gauche */
    lv_obj_set_pos(s_brow_l, BROW_L_X, BROW_Y + dy_outer);
    lv_obj_set_pos(s_brow_r, BROW_R_X, BROW_Y + dy_outer);
    (void)dy_inner;  /* TODO: sourcils inclinés avec 2 rects si besoin */
}

static void set_mouth(uint16_t start, uint16_t end, lv_color_t c)
{
    lv_arc_set_angles(s_mouth, start, end);
    lv_obj_set_style_arc_color(s_mouth, c, LV_PART_INDICATOR);
}

static void set_overlay(lv_color_t c, lv_opa_t opa)
{
    lv_obj_set_style_bg_color(s_overlay, c, 0);
    lv_obj_set_style_bg_opa(s_overlay, opa, 0);
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
    lv_obj_set_style_bg_color(scr, C_BG, 0);

    /* Oreilles (derrière la tête) */
    s_ear_l = make_circle(scr, EAR_L_CX, EAR_L_CY, EAR_R, C_EAR);
    s_ear_r = make_circle(scr, EAR_R_CX, EAR_R_CY, EAR_R, C_EAR);

    /* Tête */
    s_head = make_rect(scr, HEAD_X, HEAD_Y, HEAD_W, HEAD_H, C_HEAD, 26);

    /* Sourcils */
    s_brow_l = make_rect(scr, BROW_L_X, BROW_Y, BROW_W, BROW_H, C_BROW, 2);
    s_brow_r = make_rect(scr, BROW_R_X, BROW_Y, BROW_W, BROW_H, C_BROW, 2);

    /* Yeux */
    s_eye_l = make_circle(scr, EL_CX, EL_CY, EYE_R, C_EYE);
    s_eye_r = make_circle(scr, ER_CX, ER_CY, EYE_R, C_EYE);

    /* Pupilles */
    s_pup_l = make_circle(scr, EL_CX, EL_CY, PUP_R, C_PUPIL);
    s_pup_r = make_circle(scr, ER_CX, ER_CY, PUP_R, C_PUPIL);

    /* Bouche (arc) */
    s_mouth = lv_arc_create(scr);
    lv_obj_set_pos(s_mouth, MOUTH_X, MOUTH_Y);
    lv_obj_set_size(s_mouth, MOUTH_D, MOUTH_D);
    lv_arc_set_bg_angles(s_mouth, 0, 360);
    lv_arc_set_angles(s_mouth, 195, 345);
    lv_obj_set_style_arc_opa(s_mouth, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_mouth, C_MOUTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_mouth, 5, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_mouth, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_mouth, LV_OBJ_FLAG_CLICKABLE);

    /* Corps */
    s_body  = make_rect(scr, BODY_X, BODY_Y, BODY_W, BODY_H, C_BODY, 14);
    s_arm_l = make_rect(scr, ARM_LX, ARM_Y, ARM_W, ARM_H, C_BODY, 8);
    s_arm_r = make_rect(scr, ARM_RX, ARM_Y, ARM_W, ARM_H, C_BODY, 8);
    s_leg_l = make_rect(scr, LEG_LX, LEG_Y, LEG_W, LEG_H, C_BODY, 6);
    s_leg_r = make_rect(scr, LEG_RX, LEG_Y, LEG_W, LEG_H, C_BODY, 6);

    /* Status (grande police pour waiting) */
    s_status = lv_label_create(scr);
    lv_label_set_text(s_status, "");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_status, 172);
    lv_obj_set_pos(s_status, 0, STATUS_Y);

    /* Overlay flash plein écran */
    s_overlay = make_rect(scr, 0, 0, 172, 320, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_TRANSP, 0);

    s_timer = lv_timer_create(anim_cb, 50, nullptr);
}

/* ── IDLE ──────────────────────────────────────────── */
static void tick_idle(void)
{
    float p = sinf(s_tick * 0.05f);

    /* Oreilles pulsent doucement */
    uint8_t er = (uint8_t)(220 + p * 20);
    uint8_t eg = (uint8_t)( 95 + p * 15);
    lv_obj_set_style_bg_color(s_ear_l, lv_color_make(er, eg, 45), 0);
    lv_obj_set_style_bg_color(s_ear_r, lv_color_make(er, eg, 45), 0);

    /* Sourcils neutres */
    set_brows(0, 0);

    /* Clignement */
    if (!s_blinking && s_tick >= s_blink_at) {
        s_blinking = true; s_blink_t0 = s_tick;
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
            s_blink_at = s_tick + 55 + rand() % 45;
        }
    }

    set_mouth(195, 345, C_MOUTH);
    set_overlay(lv_color_black(), LV_OPA_TRANSP);
    lv_label_set_text(s_status, "");

    /* LED : orange doux pulsé */
    uint8_t lb = (uint8_t)(25 + (p + 1.0f) * 20.0f);
    ClaudeBot_SetLED(lb, lb / 5, 0);
}

/* ── THINKING ──────────────────────────────────────── */
static void tick_thinking(void)
{
    /* Pupilles montent légèrement à gauche (concentration) */
    int dx = (int)(sinf(s_tick * 0.2f) * 4.0f) - 2;
    int dy = -3;
    set_pupils(dx, dy);

    /* Sourcils froncés (vers le bas, rapprochés) */
    set_brows(-2, 4);

    /* Oreilles oscillent vite */
    int ox = (int)(sinf(s_tick * 0.8f) * 3.0f);
    lv_obj_set_pos(s_ear_l, EAR_L_CX - EAR_R + ox, EAR_L_CY - EAR_R);
    lv_obj_set_pos(s_ear_r, EAR_R_CX - EAR_R + ox, EAR_R_CY - EAR_R);

    set_mouth(255, 285, C_MOUTH);   /* bouche neutre/fermée */

    static const char *dots[] = {".", "..", "..."};
    lv_label_set_text(s_status, dots[(s_tick / 5) % 3]);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status, lv_color_make(255, 200, 80), 0);

    set_overlay(lv_color_black(), LV_OPA_TRANSP);

    /* LED : jaune pulsé */
    float p = (sinf(s_tick * 0.4f) + 1.0f) * 0.5f;
    ClaudeBot_SetLED((uint8_t)(70 * p), (uint8_t)(55 * p), 0);
}

/* ── WAITING — priorité rouge festive ──────────────── */
static void tick_waiting(void)
{
    /* Fond rouge qui pulse entre 55% et 85% opacité */
    float p = (sinf(s_tick * 0.4f) + 1.0f) * 0.5f;
    lv_opa_t opa = (lv_opa_t)(140 + (uint8_t)(75 * p));
    set_overlay(C_RED, opa);

    /* Robot qui saute — tête, corps, bras, jambes bougent ensemble */
    int dy = (int)(sinf(s_tick * 0.8f) * 6.0f);
    lv_obj_set_pos(s_ear_l, EAR_L_CX - EAR_R, EAR_L_CY - EAR_R + dy);
    lv_obj_set_pos(s_ear_r, EAR_R_CX - EAR_R, EAR_R_CY - EAR_R + dy);
    lv_obj_set_pos(s_head,  HEAD_X, HEAD_Y + dy);
    lv_obj_set_pos(s_brow_l, BROW_L_X, BROW_Y + dy - 3);  /* sourcils levés */
    lv_obj_set_pos(s_brow_r, BROW_R_X, BROW_Y + dy - 3);
    lv_obj_set_pos(s_eye_l, EL_CX - EYE_R, EL_CY - EYE_R + dy);
    lv_obj_set_pos(s_eye_r, ER_CX - EYE_R, ER_CY - EYE_R + dy);
    lv_obj_set_pos(s_pup_l, EL_CX - PUP_R, EL_CY - PUP_R + dy);
    lv_obj_set_pos(s_pup_r, ER_CX - PUP_R, ER_CY - PUP_R + dy);
    lv_obj_set_pos(s_mouth, MOUTH_X, MOUTH_Y + dy);
    lv_obj_set_pos(s_body,  BODY_X, BODY_Y + dy);
    lv_obj_set_pos(s_leg_l, LEG_LX, LEG_Y + dy);
    lv_obj_set_pos(s_leg_r, LEG_RX, LEG_Y + dy);

    /* Bras qui agitent alternativement */
    int arm_dy_l = (s_tick % 8 < 4) ? dy - 14 : dy + 4;
    int arm_dy_r = (s_tick % 8 < 4) ? dy + 4  : dy - 14;
    lv_obj_set_pos(s_arm_l, ARM_LX, ARM_Y + arm_dy_l);
    lv_obj_set_pos(s_arm_r, ARM_RX, ARM_Y + arm_dy_r);

    /* Bouche ouverte (surprise) */
    set_mouth(150, 390, lv_color_make(255, 100, 100));

    /* Grand "?" clignotant */
    bool show = (s_tick % 6) < 4;
    lv_label_set_text(s_status, show ? "?" : "");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_status, lv_color_white(), 0);
    lv_obj_set_pos(s_status, 0, 270);

    /* LED : rouge stroboscope */
    ClaudeBot_SetLED((s_tick % 4 < 2) ? 220 : 50, 0, 0);
}

/* ── DONE ──────────────────────────────────────────── */
static void tick_done(void)
{
    uint32_t dt = s_tick - s_done_t0;

    /* Flash vert décroissant */
    if (dt < 12) {
        lv_opa_t opa = (lv_opa_t)(LV_OPA_COVER * (1.0f - dt / 12.0f));
        set_overlay(C_GREEN, opa);
        ClaudeBot_SetLED(0, (uint8_t)(100 * (1.0f - dt / 12.0f)), 0);
    } else {
        set_overlay(lv_color_black(), LV_OPA_TRANSP);
        ClaudeBot_SetLED(0, 0, 0);
    }

    /* Bras levés, grand sourire */
    lv_obj_set_pos(s_arm_l, ARM_LX, ARM_Y - 16);
    lv_obj_set_pos(s_arm_r, ARM_RX, ARM_Y - 16);
    set_mouth(185, 355, C_MOUTH);
    set_brows(0, -4);  /* sourcils levés = joyeux */

    lv_label_set_text(s_status, "");

    if (dt >= 35) {
        reset_arms();
        Robot_SetState(BOT_IDLE);
    }
}

/* ── ALERT ─────────────────────────────────────────── */
static void tick_alert(void)
{
    bool on = (s_tick % 4) < 2;
    set_overlay(C_RED, on ? LV_OPA_60 : LV_OPA_TRANSP);
    ClaudeBot_SetLED(on ? 220 : 0, 0, 0);
    set_mouth(20, 160, lv_color_make(255, 80, 80));
    set_brows(-3, 4);

    lv_label_set_text(s_status, on ? "!" : "");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_status, lv_color_white(), 0);
    lv_obj_set_pos(s_status, 0, STATUS_Y);

    lv_obj_set_pos(s_arm_l, ARM_LX, (s_tick % 8 < 4) ? ARM_Y - 12 : ARM_Y);
    lv_obj_set_pos(s_arm_r, ARM_RX, (s_tick % 8 < 4) ? ARM_Y       : ARM_Y - 12);
}

/* ── Public ────────────────────────────────────────── */
static void reset_all_positions(void)
{
    lv_obj_set_pos(s_ear_l,  EAR_L_CX - EAR_R, EAR_L_CY - EAR_R);
    lv_obj_set_pos(s_ear_r,  EAR_R_CX - EAR_R, EAR_R_CY - EAR_R);
    lv_obj_set_pos(s_head,   HEAD_X, HEAD_Y);
    lv_obj_set_pos(s_brow_l, BROW_L_X, BROW_Y);
    lv_obj_set_pos(s_brow_r, BROW_R_X, BROW_Y);
    lv_obj_set_pos(s_eye_l,  EL_CX - EYE_R, EL_CY - EYE_R);
    lv_obj_set_pos(s_eye_r,  ER_CX - EYE_R, ER_CY - EYE_R);
    lv_obj_set_pos(s_mouth,  MOUTH_X, MOUTH_Y);
    lv_obj_set_pos(s_body,   BODY_X, BODY_Y);
    lv_obj_set_pos(s_leg_l,  LEG_LX, LEG_Y);
    lv_obj_set_pos(s_leg_r,  LEG_RX, LEG_Y);
    lv_obj_set_pos(s_status, 0, STATUS_Y);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_size(s_pup_l, PUP_R * 2, PUP_R * 2);
    lv_obj_set_size(s_pup_r, PUP_R * 2, PUP_R * 2);
    reset_arms();
}

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
    reset_all_positions();
    set_overlay(lv_color_black(), LV_OPA_TRANSP);
    s_state = new_state;
    if (new_state == BOT_DONE) s_done_t0 = s_tick;
}
