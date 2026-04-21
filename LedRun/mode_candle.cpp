#include "mode_candle.h"
#include "ledrun.h"
#include <Arduino.h>
#include <stdlib.h>

/*
 * Portage du rendu led.run/light?t=candle
 * 5 ellipses empilées, opacité et position animées à 20 fps.
 * Port from led.run/lights/candle/renderer.js (MIT).
 */

#define N_LAYERS 5
#define FLAME_CX 86    /* centre horizontal */
#define FLAME_CY 195   /* centre vertical (légèrement bas = flamme qui monte) */

/* Définitions statiques des couches (externe → interne) */
static const uint16_t L_W[]        = {168, 128,  86,  48,  20};
static const uint16_t L_H[]        = {310, 240, 170,  95,  42};
static const uint8_t  L_OPA_BASE[] = { 50, 110, 160, 205, 235};
static const uint16_t L_INTERVAL[] = {120, 100,  80,  60,  40}; /* ms */

static lv_obj_t  *s_layer[N_LAYERS];
static float      s_opa[N_LAYERS];
static float      s_opa_tgt[N_LAYERS];
static uint32_t   s_next_ms[N_LAYERS];
static uint8_t    s_warmth = 8;
static lv_timer_t *s_timer = nullptr;

static lv_color_t layer_color(int i)
{
    /* Palette ambre : de quasi-noir (extérieur) à jaune vif (noyau) */
    switch (i) {
        case 0: return lv_color_make( 18,   4,   0);
        case 1: return lv_color_make( 85,  18,   0);
        case 2: return lv_color_make(175,  55,   0);
        case 3: return lv_color_make(238, 118,   8);
        case 4: return lv_color_make(255, 220,  90);
        default: return lv_color_white();
    }
}

static void anim_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t now    = millis();
    int      jitter = s_warmth * 2;   /* amplitude de jitter en px */
    float    amp    = 20.0f + s_warmth * 15.0f;  /* amplitude d'opacité */

    for (int i = 0; i < N_LAYERS; i++) {
        /* Interpolation douce vers la cible (15 % par frame) */
        s_opa[i] += (s_opa_tgt[i] - s_opa[i]) * 0.15f;

        /* Nouvelle cible quand l'intervalle est écoulé */
        if (now >= s_next_ms[i]) {
            float base  = (float)L_OPA_BASE[i];
            float delta = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * amp;
            float tgt   = base + delta;
            s_opa_tgt[i] = (tgt < 8) ? 8 : (tgt > 255) ? 255 : tgt;

            uint16_t iv = L_INTERVAL[i] - s_warmth * 4;
            if (iv < 20) iv = 20;
            s_next_ms[i] = now + iv;
        }

        /* Jitter de position (couches internes uniquement) */
        int dx = 0, dy = 0;
        if (i >= 2 && jitter > 0) {
            dx = (rand() % (jitter * 2 + 1)) - jitter;
            dy = (rand() % (jitter     + 1)) - jitter / 2;
        }

        lv_obj_set_pos(s_layer[i],
            FLAME_CX - L_W[i] / 2 + dx,
            FLAME_CY - L_H[i] / 2 + dy);
        lv_obj_set_style_bg_opa(s_layer[i], (lv_opa_t)(uint8_t)s_opa[i], 0);
    }

    /* LED : pulse ambre synchronisé sur l'opacité du noyau */
    uint8_t br = (uint8_t)(s_opa[4] * 200.0f / 255.0f);
    LedRun_SetLED(br, br / 5, 0);
}

void Candle_Start(uint8_t warmth, uint32_t /*color_rgb*/)
{
    s_warmth = (warmth < 1) ? 1 : (warmth > 10) ? 10 : warmth;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(6, 1, 0), 0);

    for (int i = 0; i < N_LAYERS; i++) {
        s_layer[i] = lv_obj_create(scr);
        lv_obj_set_size(s_layer[i], L_W[i], L_H[i]);
        lv_obj_set_pos(s_layer[i],
            FLAME_CX - L_W[i] / 2,
            FLAME_CY - L_H[i] / 2);
        lv_obj_set_style_bg_color(s_layer[i],  layer_color(i), 0);
        lv_obj_set_style_bg_opa(s_layer[i],    L_OPA_BASE[i],  0);
        lv_obj_set_style_border_width(s_layer[i], 0, 0);
        lv_obj_set_style_radius(s_layer[i],    LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(s_layer[i], 0, 0);
        lv_obj_set_style_pad_all(s_layer[i],   0, 0);

        s_opa[i]     = L_OPA_BASE[i];
        s_opa_tgt[i] = L_OPA_BASE[i];
        s_next_ms[i] = millis() + L_INTERVAL[i];
    }

    s_timer = lv_timer_create(anim_cb, 50, nullptr);  /* 20 fps */
}

void Candle_Stop(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = nullptr;
    }
    LedRun_SetLED(0, 0, 0);
}
