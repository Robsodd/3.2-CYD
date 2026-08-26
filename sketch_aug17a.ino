#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "driver/rtc_io.h" 
#include "ui.h"
#include <Preferences.h>

// --- Permanent Memory for Calibration ---
Preferences preferences;

// --- Screen & Touch Config ---
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

// Default fallbacks (will be overwritten if permanent memory is found!)
int live_calData[4] = { 1180, 2880, 160, 5100 }; 

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); 
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

// --- ESP-NOW Broadcast Setup ---
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

// Global State
float current_brightness = 0.3f;
char current_anim[32] = "rainbow";

// --- ESP-NOW Send Helper ---
void broadcast_to_stands(const char* json_msg) {
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)json_msg, strlen(json_msg));
    if (result == ESP_OK) {
        Serial.print("ESP-NOW Sent: ");
        Serial.println(json_msg);
    } else {
        Serial.println("ESP-NOW Send Failed");
    }
}

// --- Command Builders ---
void send_play_animation(const char* anim_name, float brightness) {
    char msg[200];
    snprintf(msg, sizeof(msg), 
        "{\"cmd\":\"PLAY_ANIMATION\",\"payload\":{\"anim\":\"%s\",\"config\":{},\"brightness\":%.2f}}", 
        anim_name, brightness);
    broadcast_to_stands(msg);
}

void send_set_brightness(float brightness) {
    char msg[80];
    snprintf(msg, sizeof(msg), 
        "{\"cmd\":\"SET_BRIGHTNESS\",\"payload\":%.2f}", 
        brightness);
    broadcast_to_stands(msg);
}

// --- LVGL UI Event Callbacks ---

static void anim_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    char buf[32];
    lv_dropdown_get_selected_str(dropdown, buf, sizeof(buf));
    
    strncpy(current_anim, buf, sizeof(current_anim));
    Serial.print("Selected Animation: ");
    Serial.println(current_anim);
    
    send_play_animation(current_anim, current_brightness);
}

static void checkbox_stand_event_cb(lv_event_t * e) {
    bool stand1_active = lv_obj_has_state(ui_CheckboxStand1, LV_STATE_CHECKED);
    bool stand2_active = lv_obj_has_state(ui_CheckboxStand2, LV_STATE_CHECKED);
    Serial.printf("Target Stands -> Stand 1: %d, Stand 2: %d\n", stand1_active, stand2_active);
}

static void colorwheel_event_cb(lv_event_t * e) {
    lv_obj_t * cw = lv_event_get_target(e);
    lv_color_t c = lv_colorwheel_get_rgb(cw);
    
    char msg[120];
    snprintf(msg, sizeof(msg), 
        "{\"cmd\":\"SET_COLOR\",\"payload\":[%d,%d,%d]}", 
        c.ch.red * 8, c.ch.green * 4, c.ch.blue * 8); 
    broadcast_to_stands(msg);
}

// --- Display & Touch Drivers ---
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p ) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );
    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();
    lv_disp_flush_ready( disp );
}

void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data ) {
    uint16_t dummyX, dummyY;
    bool touched = tft.getTouch(&dummyX, &dummyY);

    if(touched) {
        data->state = LV_INDEV_STATE_PR;
        
        uint16_t touchX, touchY;
        tft.getTouchRaw(&touchX, &touchY);

        uint16_t temp = touchX;
        touchX = touchY;
        touchY = temp;

        int16_t finalX = map(touchX, live_calData[0], live_calData[1], screenWidth, 0);
        int16_t finalY = map(touchY, live_calData[2], live_calData[3], 0, screenHeight);

        if (finalX < 0) finalX = 0;
        if (finalX > screenWidth) finalX = screenWidth;
        if (finalY < 0) finalY = 0;
        if (finalY > screenHeight) finalY = screenHeight;

        data->point.x = finalX;
        data->point.y = finalY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// --- Network Init ---
void init_espnow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return;
    }
    Serial.println("ESP-NOW Initialized on Channel 1 (Broadcast Ready)");
}

void setup() {
    Serial.begin(115200);
    delay(1000); 

    // --- LOAD CALIBRATION FROM MEMORY ---
    preferences.begin("touch-cal", false);
    if (preferences.getBytesLength("mapCal") == sizeof(live_calData)) {
        Serial.println("Loading permanent calibration data...");
        preferences.getBytes("mapCal", live_calData, sizeof(live_calData));
    } else {
        Serial.println("No saved calibration found. Using default array.");
    }

    // 1. Hardware Backlight
    rtc_gpio_deinit((gpio_num_t)27); 
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH); 

    // 2. Display Setup
    tft.begin();
    tft.setRotation(1); 
    uint16_t dummyCal[5] = { 0, 4095, 0, 4095, 0 };
    tft.setTouch(dummyCal);

    // 3. Initialize Wireless
    init_espnow();

    // 4. Initialize LVGL Core
    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    // Initialize Touchpad Driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_t * indev_touchpad = lv_indev_drv_register( &indev_drv );

    // --- PERMANENT BLUE CURSOR TRACKER ---
    lv_obj_t * cursor_obj = lv_obj_create(lv_layer_sys()); 
    lv_obj_set_size(cursor_obj, 10, 10);
    lv_obj_set_style_radius(cursor_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cursor_obj, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(cursor_obj, 0, 0); 
    lv_obj_clear_flag(cursor_obj, LV_OBJ_FLAG_CLICKABLE); 
    
    lv_indev_set_cursor(indev_touchpad, cursor_obj);
    // -------------------------------------

    // 5. Build the UI
    ui_init(); 

    // --- POPULATE CALIBRATION UI LABELS ---
    char boot_buffer[16];
    
    if (ui_valueXmin1 != NULL) {
        snprintf(boot_buffer, sizeof(boot_buffer), "%d", live_calData[0]);
        lv_label_set_text(ui_valueXmin1, boot_buffer);
    }
    // Uncomment these if you added the other labels!
    // if (ui_valueXmax1 != NULL) { snprintf(boot_buffer, sizeof(boot_buffer), "%d", live_calData[1]); lv_label_set_text(ui_valueXmax1, boot_buffer); }
    // if (ui_valueYmin1 != NULL) { snprintf(boot_buffer, sizeof(boot_buffer), "%d", live_calData[2]); lv_label_set_text(ui_valueYmin1, boot_buffer); }
    // if (ui_valueYmax1 != NULL) { snprintf(boot_buffer, sizeof(boot_buffer), "%d", live_calData[3]); lv_label_set_text(ui_valueYmax1, boot_buffer); }

    // --- ATTACH EVENT HANDLERS ---
    if (ui_AnimDropdown != NULL) {
        //__all__ = ['plain_white', 'sunrise', "champagne", "helix_spin", "lava_lamp", 'matrix_rain', 'sparkle', 'fire_tower', 'rainbow_stationary', 'stop']
        lv_dropdown_set_options(ui_AnimDropdown, "plain_white\nsunrise\nchampagne\nhelix_spin\nlava_lamp\nmatrix\nfire_tower\nrainbow_stationary\nstop");
        lv_obj_add_event_cb(ui_AnimDropdown, anim_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (ui_CheckboxStand1 != NULL) {
        lv_obj_add_event_cb(ui_CheckboxStand1, checkbox_stand_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (ui_CheckboxStand2 != NULL) {
        lv_obj_add_event_cb(ui_CheckboxStand2, checkbox_stand_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (ui_ColourWheel != NULL) {
        lv_obj_add_event_cb(ui_ColourWheel, colorwheel_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

void loop() {
    lv_tick_inc(5); 
    lv_timer_handler(); 
    delay(5);
}

// --- THE PERMANENT LIVE TUNING BRIDGE ---
extern "C" void tune_calibration(int index, int amount) {
    live_calData[index] += amount;
    
    preferences.putBytes("mapCal", live_calData, sizeof(live_calData));
    
    char text_buffer[16]; 
    snprintf(text_buffer, sizeof(text_buffer), "%d", live_calData[index]);

    switch(index) {
        case 0: if (ui_valueXmin1 != NULL) lv_label_set_text(ui_valueXmin1, text_buffer); break;
        // case 1: if (ui_valueXmax1 != NULL) lv_label_set_text(ui_valueXmax1, text_buffer); break;
        // case 2: if (ui_valueYmin1 != NULL) lv_label_set_text(ui_valueYmin1, text_buffer); break;
        // case 3: if (ui_valueYmax1 != NULL) lv_label_set_text(ui_valueYmax1, text_buffer); break;
    }
    
    Serial.print("Permanently Saved Array: { ");
    Serial.print(live_calData[0]); Serial.print(", ");
    Serial.print(live_calData[1]); Serial.print(", ");
    Serial.print(live_calData[2]); Serial.print(", ");
    Serial.print(live_calData[3]); Serial.println(" };");
}

// Function to wipe the memory if you ever need a clean slate
extern "C" void execute_recalibration() {
    preferences.clear();
    ESP.restart(); 
}
