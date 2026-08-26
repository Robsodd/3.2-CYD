#ifndef LED_CONTROLLER_APP_H
#define LED_CONTROLLER_APP_H

#include <lvgl.h>
#include "ui.h" // Gives access to your ui_ScreenLEDs

extern AppState currentApp;

// Flag to track if the screen is loaded so we don't reload it every frame
bool ledScreenLoaded = false; 

void run_led_app() {
    // 1. Load the LED screen if it isn't already active
    if (!ledScreenLoaded) {
        lv_disp_load_scr(ui_ScreenLEDs);
        ledScreenLoaded = true;
    }

    // 2. Keep the LVGL operating system ticking while in this app
    lv_tick_inc(5); 
    lv_timer_handler(); 

    // Note: To exit this app, you just need a "Home" button on your 
    // ui_ScreenLEDs in SquareLine Studio. Set that button's click event 
    // to call a custom function that sets `currentApp = APP_HOME;` 
    // and `ledScreenLoaded = false;`.
}

#endif