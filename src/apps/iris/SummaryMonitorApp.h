#ifndef SUMMARY_MONITOR_APP_H
#define SUMMARY_MONITOR_APP_H

#include <lvgl.h>
#include "Config.h"
#include "ui.h"

extern AppState currentApp;

// External data from SystemMonitorApp
extern std::vector<IrisInstance> monitorInstances;
extern uint32_t themeColors[5];
extern int themeColorIndex;

// --- State Variables ---
bool is_summary_loaded = false;
lv_obj_t *summary_screen = NULL;
uint32_t last_summary_refresh = 0;

void build_summary_screen()
{
    if (summary_screen == NULL)
    {
        summary_screen = lv_obj_create(NULL);
    }
    else
    {
        lv_obj_clean(summary_screen);
    }

    lv_obj_set_style_bg_color(summary_screen, lv_color_hex(0x000000), 0);

    // --- Calculate Totals ---
    int totalOk = 0, totalWarn = 0, totalFail = 0;
    for (auto &inst : monitorInstances)
    {
        for (auto &comp : inst.components)
        {
            if (comp.status == 1)
                totalOk++;
            else if (comp.status == 0)
                totalFail++;
            else if (comp.status == 2)
                totalWarn++;
        }
    }

    // --- Header ---
    lv_obj_t *title = lv_label_create(summary_screen);
    lv_label_set_text(title, "GLOBAL SUMMARY");
    lv_obj_set_style_text_color(title, lv_color_hex(themeColors[themeColorIndex]), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *line = lv_obj_create(summary_screen);
    lv_obj_set_size(line, 300, 2);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(line, lv_color_hex(themeColors[themeColorIndex]), 0);
    lv_obj_set_style_border_width(line, 0, 0);

    // --- Layout Constants ---
    int blockHeight = 42;
    int blockWidth = 300;
    int startY = 45;
    int spacing = 8;

    // --- FAIL BLOCK (Red) ---
    lv_obj_t *fail_block = lv_obj_create(summary_screen);
    lv_obj_set_size(fail_block, blockWidth, blockHeight);
    lv_obj_align(fail_block, LV_ALIGN_TOP_MID, 0, startY);
    lv_obj_set_style_bg_color(fail_block, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(fail_block, 0, 0);
    lv_obj_set_style_radius(fail_block, 5, 0);
    lv_obj_clear_flag(fail_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fail_lbl = lv_label_create(fail_block);
    lv_label_set_text(fail_lbl, "FAIL");
    lv_obj_set_style_text_color(fail_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(fail_lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(fail_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *fail_val = lv_label_create(fail_block);
    lv_label_set_text_fmt(fail_val, "%d", totalFail);
    lv_obj_set_style_text_color(fail_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(fail_val, &lv_font_montserrat_24, 0);
    lv_obj_align(fail_val, LV_ALIGN_RIGHT_MID, -10, 0);

    // --- WARN BLOCK (Yellow) ---
    lv_obj_t *warn_block = lv_obj_create(summary_screen);
    lv_obj_set_size(warn_block, blockWidth, blockHeight);
    lv_obj_align(warn_block, LV_ALIGN_TOP_MID, 0, startY + (blockHeight + spacing));
    lv_obj_set_style_bg_color(warn_block, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_border_width(warn_block, 0, 0);
    lv_obj_set_style_radius(warn_block, 5, 0);
    lv_obj_clear_flag(warn_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *warn_lbl = lv_label_create(warn_block);
    lv_label_set_text(warn_lbl, "WARN");
    lv_obj_set_style_text_color(warn_lbl, lv_color_hex(0x000000), 0); // Black text on yellow
    lv_obj_set_style_text_font(warn_lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(warn_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *warn_val = lv_label_create(warn_block);
    lv_label_set_text_fmt(warn_val, "%d", totalWarn);
    lv_obj_set_style_text_color(warn_val, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(warn_val, &lv_font_montserrat_24, 0);
    lv_obj_align(warn_val, LV_ALIGN_RIGHT_MID, -10, 0);

    // --- OK BLOCK (Green) ---
    lv_obj_t *ok_block = lv_obj_create(summary_screen);
    lv_obj_set_size(ok_block, blockWidth, blockHeight);
    lv_obj_align(ok_block, LV_ALIGN_TOP_MID, 0, startY + ((blockHeight + spacing) * 2));
    lv_obj_set_style_bg_color(ok_block, lv_color_hex(0x009600), 0); // Darker Green
    lv_obj_set_style_border_width(ok_block, 0, 0);
    lv_obj_set_style_radius(ok_block, 5, 0);
    lv_obj_clear_flag(ok_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ok_lbl = lv_label_create(ok_block);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(ok_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *ok_val = lv_label_create(ok_block);
    lv_label_set_text_fmt(ok_val, "%d", totalOk);
    lv_obj_set_style_text_color(ok_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ok_val, &lv_font_montserrat_24, 0);
    lv_obj_align(ok_val, LV_ALIGN_RIGHT_MID, -10, 0);

    // --- HOME BUTTON ---
    lv_obj_t *home_btn = lv_btn_create(summary_screen);
    lv_obj_set_size(home_btn, 70, 35);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x282828), 0);

    lv_obj_add_event_cb(home_btn, [](lv_event_t *e)
                        {
        currentApp = APP_HOME;          
        is_summary_loaded = false;     
        lv_disp_load_scr(ui_ScreenHome); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, "HOME");
    lv_obj_center(home_lbl);
}

void run_summary_app()
{
    if (!is_summary_loaded)
    {
        build_summary_screen();
        lv_disp_load_scr(summary_screen);
        is_summary_loaded = true;
        last_summary_refresh = millis();
    }

    // Auto-refresh the summary every 2 seconds to catch real-time BLE updates
    if (millis() - last_summary_refresh > 2000)
    {
        build_summary_screen();
        last_summary_refresh = millis();
    }
}

// --- OPTIONAL BOOT INIT ---
// Included so you can assign it in the myApps[] registry without compiler errors
void init_summary_app()
{
    // BLE handles real-time updates natively, so nothing needs to be fetched at boot!
}

#endif