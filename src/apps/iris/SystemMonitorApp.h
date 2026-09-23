#ifndef SYSTEM_MONITOR_APP_H
#define SYSTEM_MONITOR_APP_H

#include <algorithm>
#include <lvgl.h>
#include "Config.h"
#include "ui.h"

extern AppState currentApp;

extern String currentBrowserTime;
extern uint32_t lastBleDataTime;

// --- Monitor Globals ---
std::vector<IrisInstance> monitorInstances;
uint32_t themeColors[5] = {0x00FF00, 0x00FFFF, 0xFFFF00, 0xFF00FF, 0xFFFFFF};
int themeColorIndex = 0;
bool isInterruptActive = false;

// --- State Variables ---
int monitorPageIndex = 0;
bool is_sysmon_loaded = false;
lv_obj_t *sysmon_screen = NULL;
uint32_t last_sysmon_refresh = 0;

// Helper to convert 16-bit RGB565 (from BLE parser) to LVGL color object
lv_color_t colorFrom16(uint16_t c)
{
    lv_color_t color;
    color.full = c;
    return color;
}

// Forward Declaration
void build_sysmon_screen();

void advanceMonitorPage(lv_event_t *e)
{
    monitorPageIndex++;
    if (monitorPageIndex > monitorInstances.size())
    {
        monitorPageIndex = 0;
    }
    build_sysmon_screen();
}

void build_sysmon_screen()
{
    if (sysmon_screen == NULL)
    {
        sysmon_screen = lv_obj_create(NULL);
    }
    else
    {
        lv_obj_clean(sysmon_screen);
    }

    lv_obj_set_style_bg_color(sysmon_screen, lv_color_hex(0x000000), 0);

    // Transparent background button to capture "Tap anywhere to advance"
    lv_obj_t *bg_btn = lv_btn_create(sysmon_screen);
    lv_obj_set_size(bg_btn, 320, 240);
    lv_obj_set_style_bg_opa(bg_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bg_btn, 0, 0);
    lv_obj_set_style_shadow_width(bg_btn, 0, 0);
    lv_obj_add_event_cb(bg_btn, advanceMonitorPage, LV_EVENT_CLICKED, NULL);

    if (monitorPageIndex == 0)
    {
        // ==========================================
        // PAGE 0: SYSTEM OVERVIEW
        // ==========================================
        if (isInterruptActive)
        {
            lv_obj_t *alert_bg = lv_obj_create(sysmon_screen);
            lv_obj_set_size(alert_bg, 320, 40);
            lv_obj_align(alert_bg, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_set_style_bg_color(alert_bg, lv_color_hex(0xFF0000), 0);
            lv_obj_set_style_border_width(alert_bg, 0, 0);
            lv_obj_clear_flag(alert_bg, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t *alert_txt = lv_label_create(alert_bg);
            lv_label_set_text(alert_txt, "SYSTEM ALERT!");
            lv_obj_set_style_text_color(alert_txt, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(alert_txt, &lv_font_montserrat_20, 0);
            lv_obj_center(alert_txt);
        }
        else
        {
            lv_obj_t *title = lv_label_create(sysmon_screen);
            lv_label_set_text(title, "SYSTEM OVERVIEW");
            lv_obj_set_style_text_color(title, lv_color_hex(themeColors[themeColorIndex]), 0);
            lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
            lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

            lv_obj_t *line = lv_obj_create(sysmon_screen);
            lv_obj_set_size(line, 300, 2);
            lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 35);
            lv_obj_set_style_bg_color(line, lv_color_hex(themeColors[themeColorIndex]), 0);
            lv_obj_set_style_border_width(line, 0, 0);
        }

        // --- Calculate System Totals ---
        int badComps = 0;
        int totalHidden = 0;
        for (auto &inst : monitorInstances)
        {
            totalHidden += inst.hiddenAlerts;
            for (auto &comp : inst.components)
            {
                if (comp.status == 0 || comp.status == 2)
                    badComps++;
            }
        }

        // --- Stats Text ---
        lv_obj_t *lbl_time = lv_label_create(sysmon_screen);
        lv_label_set_text_fmt(lbl_time, "Time: %s", currentBrowserTime.c_str());
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(themeColors[themeColorIndex]), 0);
        lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 10, 50);

        lv_obj_t *lbl_servers = lv_label_create(sysmon_screen);
        lv_label_set_text_fmt(lbl_servers, "Servers: %d", monitorInstances.size());
        lv_obj_set_style_text_color(lbl_servers, lv_color_hex(themeColors[themeColorIndex]), 0);
        lv_obj_align(lbl_servers, LV_ALIGN_TOP_LEFT, 10, 80);

        lv_obj_t *lbl_alerts = lv_label_create(sysmon_screen);
        lv_label_set_text(lbl_alerts, "Alerts:");
        lv_obj_set_style_text_color(lbl_alerts, lv_color_hex(themeColors[themeColorIndex]), 0);
        lv_obj_align(lbl_alerts, LV_ALIGN_TOP_LEFT, 10, 110);

        lv_obj_t *lbl_alert_num = lv_label_create(sysmon_screen);
        lv_label_set_text_fmt(lbl_alert_num, "%d", badComps);
        lv_obj_set_style_text_color(lbl_alert_num, badComps > 0 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_font(lbl_alert_num, &lv_font_montserrat_36, 0);
        lv_obj_align(lbl_alert_num, LV_ALIGN_TOP_LEFT, 120, 100);

        if (totalHidden > 0)
        {
            lv_obj_t *lbl_hidden = lv_label_create(sysmon_screen);
            lv_label_set_text_fmt(lbl_hidden, "(Hidden: %d)", totalHidden);
            lv_obj_set_style_text_color(lbl_hidden, lv_color_hex(0xAAAAAA), 0);
            lv_obj_align(lbl_hidden, LV_ALIGN_TOP_LEFT, 120, 140);
        }

        String timeAgo = "--";
        if (lastBleDataTime > 0)
            timeAgo = String((millis() - lastBleDataTime) / 1000) + "s";
        lv_obj_t *lbl_last = lv_label_create(sysmon_screen);
        lv_label_set_text_fmt(lbl_last, "Last Data: %s", timeAgo.c_str());
        lv_obj_set_style_text_color(lbl_last, lv_color_hex(themeColors[themeColorIndex]), 0);
        lv_obj_align(lbl_last, LV_ALIGN_TOP_LEFT, 10, 170);
    }
    else
    {
        // ==========================================
        // PAGE 1+: INSTANCE DETAILS
        // ==========================================
        int idx = monitorPageIndex - 1;
        if (idx < monitorInstances.size())
        {
            IrisInstance &inst = monitorInstances[idx];

            // Sort components (Fail -> Warn -> OK)
            std::sort(inst.components.begin(), inst.components.end(), [](const IrisComponent &a, const IrisComponent &b)
                      {
                int wA = (a.status == 0) ? 0 : (a.status == 2) ? 1 : 2;
                int wB = (b.status == 0) ? 0 : (b.status == 2) ? 1 : 2;
                if (wA == wB) return a.name < b.name; 
                return wA < wB; });

            if (isInterruptActive)
            {
                lv_obj_t *alert_bg = lv_obj_create(sysmon_screen);
                lv_obj_set_size(alert_bg, 320, 40);
                lv_obj_align(alert_bg, LV_ALIGN_TOP_MID, 0, 0);
                lv_obj_set_style_bg_color(alert_bg, lv_color_hex(0xFF0000), 0);
                lv_obj_set_style_border_width(alert_bg, 0, 0);

                lv_obj_t *title = lv_label_create(alert_bg);
                lv_label_set_text(title, inst.name.c_str());
                lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
                lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
            }
            else
            {
                lv_obj_t *title = lv_label_create(sysmon_screen);
                lv_label_set_text(title, inst.name.c_str());
                lv_obj_set_style_text_color(title, colorFrom16(inst.color), 0);
                lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
                lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

                lv_obj_t *line = lv_obj_create(sysmon_screen);
                lv_obj_set_size(line, 300, 2);
                lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 35);
                lv_obj_set_style_bg_color(line, colorFrom16(inst.color), 0);
                lv_obj_set_style_border_width(line, 0, 0);
            }

            // --- Native Scrollable Container ---
            lv_obj_t *list = lv_obj_create(sysmon_screen);
            lv_obj_set_size(list, 320, 150); // Constrained to leave room for HOME button
            lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 45);
            lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(list, 0, 0);
            lv_obj_set_style_pad_all(list, 5, 0);

            // Populate the container
            for (int i = 0; i < inst.components.size(); i++)
            {
                lv_obj_t *row = lv_obj_create(list);
                lv_obj_set_size(row, 280, 25);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_pad_all(row, 0, 0);
                lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

                uint32_t cColor = 0xFFFFFF;
                String statStr = "UNK";
                if (inst.components[i].status == 1)
                {
                    cColor = 0x00FF00;
                    statStr = "OK";
                }
                else if (inst.components[i].status == 0)
                {
                    cColor = 0xFF0000;
                    statStr = "FAIL";
                }
                else if (inst.components[i].status == 2)
                {
                    cColor = 0xFFFF00;
                    statStr = "WARN";
                }

                String n = inst.components[i].name;
                if (n.length() > 22)
                    n = n.substring(0, 22) + "..";

                lv_obj_t *lbl_name = lv_label_create(row);
                lv_label_set_text(lbl_name, n.c_str());
                lv_obj_set_style_text_color(lbl_name, lv_color_hex(cColor), 0);
                lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 0, 0);

                lv_obj_t *lbl_stat = lv_label_create(row);
                lv_label_set_text(lbl_stat, statStr.c_str());
                lv_obj_set_style_text_color(lbl_stat, lv_color_hex(cColor), 0);
                lv_obj_align(lbl_stat, LV_ALIGN_RIGHT_MID, 0, 0);
            }
        }
    }

    // --- HOME BUTTON ---
    lv_obj_t *home_btn = lv_btn_create(sysmon_screen);
    lv_obj_set_size(home_btn, 70, 35);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x282828), 0);

    // Add event flag to stop the screen-wide "Advance Page" click from triggering when tapping Home
    lv_obj_add_flag(home_btn, LV_OBJ_FLAG_ADV_HITTEST);

    lv_obj_add_event_cb(home_btn, [](lv_event_t *e)
                        {
        currentApp = APP_HOME;          
        is_sysmon_loaded = false;     
        lv_disp_load_scr(ui_ScreenHome); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, "HOME");
    lv_obj_center(home_lbl);
}

void run_monitor_app()
{
    if (!is_sysmon_loaded)
    {
        monitorPageIndex = 0; // Reset to overview on load
        build_sysmon_screen();
        lv_disp_load_scr(sysmon_screen);
        is_sysmon_loaded = true;
        last_sysmon_refresh = millis();
    }

    // Refresh the screen every 2 seconds to reflect real-time BLE data updates
    if (millis() - last_sysmon_refresh > 2000)
    {
        // We only auto-refresh the Overview page so we don't reset the user's scroll position
        // if they are currently looking at a specific instance list!
        if (monitorPageIndex == 0)
        {
            build_sysmon_screen();
        }
        last_sysmon_refresh = millis();
    }
}

// --- OPTIONAL BOOT INIT ---
// Included so you can assign it in the myApps[] registry without compiler errors
void init_monitor_app()
{
    // BLE handles real-time updates natively, so nothing needs to be fetched at boot!
}

#endif