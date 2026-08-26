#include "ui.h"

// Tell this file about our master tuning function in the main sketch
extern void tune_calibration(int index, int amount);

// --- ADDED BACK TO FIX THE COMPILER ERROR ---
void force_recalibrate(lv_event_t * e) {
    // We are leaving this safely blank since we are using live-tuning now!
}
// --------------------------------------------

// --- X MIN (Index 0) ---
void x_min_up(lv_event_t * e) { tune_calibration(0, 20); }
void x_min_down(lv_event_t * e) { tune_calibration(0, -20); }

// --- X MAX (Index 1) ---
void x_max_up(lv_event_t * e) { tune_calibration(1, 20); }
void x_max_down(lv_event_t * e) { tune_calibration(1, -20); }

// --- Y MIN (Index 2) ---
void y_min_up(lv_event_t * e) { tune_calibration(2, 20); }
void y_min_down(lv_event_t * e) { tune_calibration(2, -20); }

// --- Y MAX (Index 3) ---
void y_max_up(lv_event_t * e) { tune_calibration(3, 20); }
void y_max_down(lv_event_t * e) { tune_calibration(3, -20); }