#include "mode_text.h"
#include "ledrun.h"
#include <string.h>

static lv_obj_t *s_lbl = nullptr;
static uint8_t   s_fr = 255, s_fg = 255, s_fb = 255;

static const lv_font_t *pick_font(const char *text)
{
    int len = (int)strlen(text);
    if (len <=  4) return &lv_font_montserrat_48;
    if (len <=  7) return &lv_font_montserrat_36;
    if (len <= 13) return &lv_font_montserrat_28;
    return              &lv_font_montserrat_20;
}

void Text_Start(const char *text, uint32_t fg_rgb, uint32_t bg_rgb)
{
    uint8_t br = (bg_rgb >> 16) & 0xFF;
    uint8_t bg = (bg_rgb >>  8) & 0xFF;
    uint8_t bb =  bg_rgb        & 0xFF;
    s_fr = (fg_rgb >> 16) & 0xFF;
    s_fg = (fg_rgb >>  8) & 0xFF;
    s_fb =  fg_rgb        & 0xFF;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(br, bg, bb), 0);

    s_lbl = lv_label_create(scr);
    lv_label_set_text(s_lbl, text);
    lv_obj_set_style_text_font(s_lbl,  pick_font(text), 0);
    lv_obj_set_style_text_color(s_lbl, lv_color_make(s_fr, s_fg, s_fb), 0);
    lv_obj_set_style_text_align(s_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl, LCD_WIDTH - 8);
    lv_label_set_long_mode(s_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl, LV_ALIGN_CENTER, 0, 0);

    LedRun_SetLED(s_fr / 4, s_fg / 4, s_fb / 4);
}

void Text_SetText(const char *text)
{
    if (!s_lbl) return;
    lv_obj_set_style_text_font(s_lbl, pick_font(text), 0);
    lv_label_set_text(s_lbl, text);
    lv_obj_align(s_lbl, LV_ALIGN_CENTER, 0, 0);
}

void Text_Stop(void)
{
    if (s_lbl) {
        lv_obj_delete(s_lbl);
        s_lbl = nullptr;
    }
    LedRun_SetLED(0, 0, 0);
}
