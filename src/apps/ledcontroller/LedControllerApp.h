#ifndef LED_CONTROLLER_APP_H
#define LED_CONTROLLER_APP_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include "ui.h"

extern AppState currentApp;

bool ledScreenLoaded = false;
bool ledDataFetched = false;
JsonDocument animDoc;
void build_dynamic_controls(int animIndex);

const char *stand1_ip = "192.168.1.96";  // Left Tower
const char *stand2_ip = "192.168.1.104"; // Right Tower

// State array to hold parameter keys safely in memory for LVGL callbacks
static String active_keys[10];
static int active_key_count = 0;
String current_anim_id = "";

// --- HTTP Command Sender ---
void send_stand_get(String endpoint)
{
    if (lv_obj_has_state(ui_CheckboxStand1, LV_STATE_CHECKED))
    {
        HTTPClient http;
        http.begin(String("http://") + stand1_ip + endpoint);
        http.GET();
        http.end();
    }
    if (lv_obj_has_state(ui_CheckboxStand2, LV_STATE_CHECKED))
    {
        HTTPClient http;
        http.begin(String("http://") + stand2_ip + endpoint);
        http.GET();
        http.end();
    }
}

// --- Dynamic UI Callbacks ---
void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    String *key = (String *)lv_event_get_user_data(e);
    int val = lv_slider_get_value(slider);

    send_stand_get("/api/setting?key=" + *key + "&val=" + String(val));
}

void color_event_cb(lv_event_t *e)
{
    lv_obj_t *cw = lv_event_get_target(e);
    lv_color_t color = lv_colorwheel_get_rgb(cw);

    // Convert LVGL 16-bit color to standard 24-bit HEX
    uint32_t c32 = lv_color_to32(color);
    char hexStr[10];
    sprintf(hexStr, "%%23%06X", c32 & 0xFFFFFF);

    send_stand_get("/api/setting?key=color&val=" + String(hexStr));
}

void dropdown_event_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t index = lv_dropdown_get_selected(dropdown);

    current_anim_id = animDoc[index]["id"].as<String>();

    // Tell stands to switch animations
    send_stand_get("/api/anim?name=" + current_anim_id);

    // Rebuild the controls beneath the dropdown
    build_dynamic_controls(index);
}

// --- Dynamic UI Builder ---
void build_dynamic_controls(int animIndex)
{
    // 1. Clear existing dynamically created controls
    lv_obj_clean(ui_LEDSettings);
    active_key_count = 0;

    // 2. Read the controls array for the newly selected animation
    JsonArray controls = animDoc[animIndex]["controls"].as<JsonArray>();

    // Set flow layout so items stack automatically
    lv_obj_set_flex_flow(ui_LEDSettings, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_LEDSettings, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (JsonObject ctrl : controls)
    {
        String type = ctrl["type"].as<String>();

        // Save the key into persistent memory so the callback can access it
        active_keys[active_key_count] = ctrl["key"].as<String>();
        String *keyPtr = &active_keys[active_key_count];

        // Add a Label for the control
        lv_obj_t *lbl = lv_label_create(ui_LEDSettings);
        lv_label_set_text(lbl, ctrl["label"].as<const char *>());
        lv_obj_set_style_pad_top(lbl, 10, 0);

        if (type == "slider")
        {
            lv_obj_t *slider = lv_slider_create(ui_LEDSettings);
            lv_obj_set_width(slider, lv_pct(90));
            lv_slider_set_range(slider, ctrl["min"].as<int>(), ctrl["max"].as<int>());
            lv_slider_set_value(slider, ctrl["default"].as<int>(), LV_ANIM_OFF);
            lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)keyPtr);
        }
        else if (type == "color")
        {
            // Built-in LVGL Color Wheel
            lv_obj_t *cw = lv_colorwheel_create(ui_LEDSettings, true);
            lv_obj_set_size(cw, 150, 150);
            lv_obj_add_event_cb(cw, color_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }

        active_key_count++;
    }
}

// --- Initialization ---
void init_led_app_ui()
{
    // Label the checkboxes
    lv_obj_t *chk1_lbl = lv_obj_get_child(ui_CheckboxStand1, 0);
    if (chk1_lbl)
        lv_label_set_text(chk1_lbl, "Left Tower");
    lv_obj_t *chk2_lbl = lv_obj_get_child(ui_CheckboxStand2, 0);
    if (chk2_lbl)
        lv_label_set_text(chk2_lbl, "Right Tower");

    // Default both to checked
    lv_obj_add_state(ui_CheckboxStand1, LV_STATE_CHECKED);
    lv_obj_add_state(ui_CheckboxStand2, LV_STATE_CHECKED);

    // Fetch JSON from the Left Tower
    HTTPClient http;
    http.begin(String("http://") + stand1_ip + "/animations.json");
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        String payload = http.getString();
        deserializeJson(animDoc, payload);

        // Build Dropdown Options List
        String optionsStr = "";
        for (JsonObject anim : animDoc.as<JsonArray>())
        {
            if (optionsStr != "")
                optionsStr += "\n";
            optionsStr += anim["title"].as<String>();
        }
        lv_dropdown_set_options(ui_AnimDropdown, optionsStr.c_str());

        // Attach Event and Trigger Initial Build
        lv_obj_add_event_cb(ui_AnimDropdown, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        build_dynamic_controls(0);
    }
    else
    {
        lv_dropdown_set_options(ui_AnimDropdown, "Network Error");
    }
    http.end();
}

// --- Main App Loop ---
void run_led_app()
{
    if (!ledScreenLoaded)
    {
        lv_disp_load_scr(ui_ScreenLEDs);

        if (!ledDataFetched)
        {
            init_led_app_ui();
            ledDataFetched = true;
        }
        ledScreenLoaded = true;
    }

    lv_tick_inc(5);
    lv_timer_handler();
}

#endif