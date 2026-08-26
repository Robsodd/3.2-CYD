#ifndef SUMMARY_MONITOR_APP_H
#define SUMMARY_MONITOR_APP_H

#include <TFT_eSPI.h>
#include "Config.h"

extern TFT_eSPI tft;
extern AppState currentApp;
extern bool showStatusBar;
extern void drawStatusBar(TFT_eSprite& spr);
extern bool get_mapped_touch(int16_t &outX, int16_t &outY);

// We pull these directly from the SystemMonitorApp where they are defined!
extern std::vector<IrisInstance> monitorInstances;
extern uint16_t themeColors[5];
extern int themeColorIndex;

static TFT_eSprite sSpr = TFT_eSprite(&tft); 

void drawSummaryFrame() {
    if (!sSpr.created()) sSpr.createSprite(320, 240);
    sSpr.fillSprite(TFT_BLACK);

    int yOffset = showStatusBar ? 20 : 0;
    int totalOk = 0, totalWarn = 0, totalFail = 0;

    for (auto& inst : monitorInstances) {
        for (auto& comp : inst.components) {
            if (comp.status == 1) totalOk++;
            else if (comp.status == 0) totalFail++;
            else if (comp.status == 2) totalWarn++;
        }
    }

    sSpr.setTextSize(2);
    sSpr.setTextColor(themeColors[themeColorIndex], TFT_BLACK);
    sSpr.drawString("GLOBAL SUMMARY", 10, 8 + yOffset);
    sSpr.drawLine(10, 28 + yOffset, 310, 28 + yOffset, themeColors[themeColorIndex]);

    // --- CYD DYNAMIC LAYOUT CALCULATION ---
    int startY = 38 + yOffset; 
    // Leave 45px at the bottom to ensure the Home button has dedicated space
    int availableSpace = 240 - startY - 45; 
    int blockHeight = (availableSpace - 20) / 3; 
    int textCenterY = (blockHeight - 21) / 2; 
    int bWidth = 300; // Stretch blocks to fit the wider screen

    // --- FAIL BLOCK (Red) ---
    int y1 = startY;
    sSpr.fillRoundRect(10, y1, bWidth, blockHeight, 5, TFT_RED);
    sSpr.setTextColor(TFT_WHITE, TFT_RED);
    sSpr.setTextSize(3);
    sSpr.drawString("FAIL", 20, y1 + textCenterY);
    sSpr.drawRightString(String(totalFail), 295, y1 + textCenterY, 1);

    // --- WARN BLOCK (Yellow) ---
    int y2 = y1 + blockHeight + 10;
    sSpr.fillRoundRect(10, y2, bWidth, blockHeight, 5, TFT_YELLOW);
    sSpr.setTextColor(TFT_BLACK, TFT_YELLOW); 
    sSpr.setTextSize(3);
    sSpr.drawString("WARN", 20, y2 + textCenterY);
    sSpr.drawRightString(String(totalWarn), 295, y2 + textCenterY, 1);

    // --- OK BLOCK (Green) ---
    int y3 = y2 + blockHeight + 10;
    uint16_t darkGreen = tft.color565(0, 150, 0); 
    sSpr.fillRoundRect(10, y3, bWidth, blockHeight, 5, darkGreen);
    sSpr.setTextColor(TFT_WHITE, darkGreen);
    sSpr.setTextSize(3);
    sSpr.drawString("OK", 20, y3 + textCenterY);
    sSpr.drawRightString(String(totalOk), 295, y3 + textCenterY, 1);
    
    drawStatusBar(sSpr);

    // --- HOME BUTTON ---
    int btnX = 240;
    int btnY = 200;
    sSpr.fillRoundRect(btnX, btnY, 70, 35, 5, tft.color565(40, 40, 40));
    sSpr.drawRect(btnX, btnY, 70, 35, tft.color565(100, 100, 100)); 
    sSpr.setTextSize(2);
    sSpr.setTextColor(TFT_WHITE);
    sSpr.drawCentreString("HOME", btnX + 35, btnY + 10, 1);
    
    sSpr.pushSprite(0, 0);
}

void run_summary_app() {
    drawSummaryFrame();

    int16_t touchX, touchY;
    if (get_mapped_touch(touchX, touchY)) {
        if (touchX >= 240 && touchX <= 310 && touchY >= 200 && touchY <= 235) {
            currentApp = APP_HOME; 
            lv_obj_invalidate(lv_scr_act()); 
            delay(200); 
        }
    }
}

#endif