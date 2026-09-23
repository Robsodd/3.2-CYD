#ifndef FISH_APP_H
#define FISH_APP_H

#include <Arduino.h>
#include <Preferences.h>
#include "lvgl.h"

// --- Image Arrays ---
LV_IMG_DECLARE(background);
LV_IMG_DECLARE(ClownFishLeft);
LV_IMG_DECLARE(ClownFishMove);
LV_IMG_DECLARE(ClownFishRight);
LV_IMG_DECLARE(ClownFishIdle);
LV_IMG_DECLARE(ClownFishEat);
LV_IMG_DECLARE(ClownFishSwallow);

// --- State Variables ---
int fish_zoom = 256;
int fish_count = 1;
String last_fed_date = "";

// Pointers for animation
lv_obj_t *fishtank_screen = NULL;
lv_obj_t *parent_fish;
lv_obj_t *baby_fish_array[10]; // Support up to 10 babies
uint8_t current_frame = 0;
bool is_eating = false;
lv_timer_t *anim_timer = NULL;

const lv_img_dsc_t *swim_frames[4] = {
    &ClownFishLeft, &ClownFishMove, &ClownFishRight, &ClownFishIdle};

// --- Cleanup Function ---
void deinit_fish_app()
{
  if (anim_timer != NULL)
  {
    lv_timer_del(anim_timer);
    anim_timer = NULL;
  }
  if (fishtank_screen != NULL)
  {
    lv_obj_del(fishtank_screen);
    fishtank_screen = NULL;
  }
}

// --- NVRAM Feeding Logic ---
void process_fish_feeding(String currentDate)
{
  Preferences prefs;
  prefs.begin("fishtank", false);

  last_fed_date = prefs.getString("last_fed", "");
  fish_zoom = prefs.getInt("zoom", 256);
  fish_count = prefs.getInt("count", 1);

  if (currentDate != last_fed_date && currentDate != "")
  {
    fish_zoom += 30; // Grow by ~12% per day
    if (fish_zoom >= 512)
    {
      fish_zoom = 256;
      if (fish_count < 11)
      {
        fish_count++; // Spawn baby (cap at 11 total fish)
      }
    }
    prefs.putString("last_fed", currentDate);
    prefs.putInt("zoom", fish_zoom);
    prefs.putInt("count", fish_count);
  }
  prefs.end();
}

// --- Animation Callback ---
void fish_anim_timer_cb(lv_timer_t *timer)
{
  if (is_eating)
  {
    current_frame = (current_frame + 1) % 2;
    lv_img_set_src(parent_fish, current_frame == 0 ? &ClownFishEat : &ClownFishSwallow);
  }
  else
  {
    current_frame = (current_frame + 1) % 4;
    lv_img_set_src(parent_fish, swim_frames[current_frame]);
    for (int i = 1; i < fish_count; i++)
    {
      if (baby_fish_array[i - 1] != NULL)
      {
        lv_img_set_src(baby_fish_array[i - 1], &ClownFishIdle);
      }
    }
  }
}

// --- Main App Initialization ---
void init_fish_app(String currentDateString)
{
  // 1. Clean up any previous instances
  deinit_fish_app();
  process_fish_feeding(currentDateString);

  // 2. Create a dedicated screen
  fishtank_screen = lv_obj_create(NULL);
  lv_obj_clear_flag(fishtank_screen, LV_OBJ_FLAG_SCROLLABLE);

  // 3. Render Background
  lv_obj_t *ship_bg = lv_img_create(fishtank_screen);
  lv_img_set_src(ship_bg, &background);
  lv_obj_align(ship_bg, LV_ALIGN_CENTER, 0, 0);

  // 4. Render Parent Fish
  parent_fish = lv_img_create(fishtank_screen);
  lv_img_set_src(parent_fish, &ClownFishLeft);
  lv_img_set_zoom(parent_fish, fish_zoom);
  lv_obj_align(parent_fish, LV_ALIGN_CENTER, 0, 0);

  // 5. Render Babies
  for (int i = 1; i < fish_count; i++)
  {
    baby_fish_array[i - 1] = lv_img_create(fishtank_screen);
    lv_img_set_src(baby_fish_array[i - 1], &ClownFishIdle);
    lv_img_set_zoom(baby_fish_array[i - 1], 256);
    lv_obj_align(baby_fish_array[i - 1], LV_ALIGN_CENTER, -40 - (i * 20), (i * 15));
  }

  // 6. Start Animation Timer & Load Screen
  anim_timer = lv_timer_create(fish_anim_timer_cb, 200, NULL);
  lv_disp_load_scr(fishtank_screen);
}

void trigger_eating()
{
  is_eating = true;
  current_frame = 0;
}

void stop_eating()
{
  is_eating = false;
  current_frame = 0;
}

#endif