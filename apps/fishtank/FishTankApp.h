#ifndef FISHTANK_APP_H
#define FISHTANK_APP_H

#include <TFT_eSPI.h>
#include <math.h>
#include <lvgl.h>
#include "Config.h"

extern TFT_eSPI tft;
extern AppState currentApp;
extern bool get_mapped_touch(int16_t &outX, int16_t &outY); // Link to main sketch touch handler

TFT_eSprite spr = TFT_eSprite(&tft); 

float fishX = 160, fishY = 120; 
float fishSpeed = 1.2;
int fishDir = 1;
float targetX = 160, targetY = 120;
float fishScale = 1.0; 

struct Flake { float x, y; bool active; };
Flake flakes[5];
struct Bubble { float x, y; int size; float speed; };
Bubble bubbles[8];
bool tankInit = false;
extern String currentBrowserTime;

void initFishTank() {
  spr.createSprite(320, 240); 
  for(int i=0; i<8; i++) {
    bubbles[i] = {(float)random(20, 300), (float)random(120, 240), random(3, 6), random(1, 4) / 2.0f};
  }
  for(int i=0; i<5; i++) flakes[i].active = false;
  targetX = random(40, 280);
  targetY = random(80, 200);
  tankInit = true;
}

void feedFish() {
  for(int i=0; i<5; i++) {
    if(!flakes[i].active) {
      flakes[i].x = random(40, 280);
      flakes[i].y = 0;
      flakes[i].active = true;
      break; 
    }
  }
}

void drawFishTankFrame() {
  if(!tankInit) initFishTank();
  float timeSecs = millis() / 1000.0; 

  // Background
  for (int y = 0; y < 240; y++) {
    int g = map(y, 0, 240, 180, 20);  
    int b = map(y, 0, 240, 255, 90);  
    spr.drawFastHLine(0, y, 320, tft.color565(0, g, b));
  }

  // Seaweed & Sand
  spr.fillEllipse(160, 250, 200, 40, tft.color565(194, 178, 128)); 
  float sway1 = sin(timeSecs * 1.5) * 6.0;         
  spr.fillTriangle(28 + sway1, 160, 20, 240, 35, 240, tft.color565(34, 139, 34));
  spr.fillTriangle(280 + sway1, 150, 270, 240, 290, 240, tft.color565(34, 139, 34));

  // Fish AI & Targeting
  float bobbing = sin(timeSecs * 2.5) * 4.0; 
  float tailSwish = sin(timeSecs * 10.0) * 8.0; 
  float s = fishScale;
  bool hasFoodTarget = false;
  float closestDist = 9999.0;
  int targetFlakeIdx = -1;

  for(int i=0; i<5; i++) {
    if(flakes[i].active) {
      float dist = sqrt(pow(flakes[i].x - fishX, 2) + pow(flakes[i].y - fishY, 2));
      if(dist < closestDist) {
        closestDist = dist; targetFlakeIdx = i; hasFoodTarget = true;
      }
    }
  }

  if (hasFoodTarget) {
    targetX = flakes[targetFlakeIdx].x; targetY = flakes[targetFlakeIdx].y;
  } else if (abs(fishX - targetX) < 15 && abs(fishY - targetY) < 15) {
    targetX = random(40, 280); targetY = random(60, 200); 
  }

  float dx = targetX - fishX;
  float dy = targetY - fishY;
  float distToTarget = sqrt(dx*dx + dy*dy);

  if (distToTarget > 1.0) {
    fishX += (dx / distToTarget) * fishSpeed;
    fishY += (dy / distToTarget) * (hasFoodTarget ? fishSpeed : (fishSpeed * 0.4));
  }
  if (dx > 0.5) fishDir = 1;
  else if (dx < -0.5) fishDir = -1;

  // Draw Entities
  for(int i=0; i<8; i++) {
    bubbles[i].y -= bubbles[i].speed;
    bubbles[i].x += sin(timeSecs * 2.0 + i) * 0.5; 
    if(bubbles[i].y < -10) { bubbles[i].y = 250; bubbles[i].x = random(20, 300); }
    spr.drawCircle(bubbles[i].x, bubbles[i].y, bubbles[i].size, tft.color565(150, 200, 255));
  }

  for(int i=0; i<5; i++) {
    if(flakes[i].active) {
      flakes[i].y += 1.5; flakes[i].x += sin(timeSecs * 4.0 + i) * 1.0; 
      spr.fillCircle(flakes[i].x, flakes[i].y, 3, tft.color565(200, 100, 50));
      if (abs(flakes[i].x - fishX) < (25 * s) && abs(flakes[i].y - (fishY + bobbing)) < (20 * s)) flakes[i].active = false; 
      if(flakes[i].y > 240) flakes[i].active = false; 
    }
  }

  float actualY = fishY + bobbing;
  uint16_t fCol = tft.color565(255, 120, 0);       
  uint16_t fTail = tft.color565(255, 160, 50);     
  
  if (fishDir == 1) {
    spr.fillTriangle(fishX - 12*s, actualY, fishX - 35*s, actualY - 15*s + (tailSwish*s), fishX - 35*s, actualY + 15*s + (tailSwish*s), fTail); 
  } else {
    spr.fillTriangle(fishX + 12*s, actualY, fishX + 35*s, actualY - 15*s + (tailSwish*s), fishX + 35*s, actualY + 15*s + (tailSwish*s), fTail); 
  }
  spr.fillEllipse(fishX, actualY, 22*s, 14*s, fCol);            
  
  // UI Buttons
  spr.fillRoundRect(10, 10, 70, 35, 5, tft.color565(40, 40, 40));
  spr.drawRect(10, 10, 70, 35, tft.color565(100, 100, 100)); 
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
  spr.drawCentreString("HOME", 45, 20, 1);

  spr.fillRoundRect(240, 10, 70, 35, 5, tft.color565(0, 150, 50));
  spr.drawRect(240, 10, 70, 35, tft.color565(0, 200, 100)); 
  spr.drawCentreString("FEED", 275, 20, 1);

  spr.pushSprite(0,0);
}

// --- App Wrapper ---
void run_fishtank_app() {
    // 1. Draw the frame
    drawFishTankFrame();

    // 2. Handle Touch Interactivity
    int16_t touchX, touchY;
    if (get_mapped_touch(touchX, touchY)) {
        
        // Check HOME Button (X: 10->80, Y: 10->45)
        if (touchX >= 10 && touchX <= 80 && touchY >= 10 && touchY <= 45) {
            currentApp = APP_HOME; 
            lv_obj_invalidate(lv_scr_act()); 
            delay(200); // Debounce
        }
        
        // Check FEED Button (X: 240->310, Y: 10->45)
        else if (touchX >= 240 && touchX <= 310 && touchY >= 10 && touchY <= 45) {
            feedFish();
            delay(200); // Debounce so it doesn't drop 5 flakes at once!
        }
    }
}

#endif