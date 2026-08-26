#ifndef SYSTEM_MONITOR_APP_H
#define SYSTEM_MONITOR_APP_H

#include <TFT_eSPI.h>
#include <algorithm> 
#include "Config.h"

extern TFT_eSPI tft;
extern AppState currentApp;
extern bool showStatusBar; 
extern String currentBrowserTime;
extern void drawStatusBar(TFT_eSprite& spr); 
extern bool get_mapped_touch(int16_t &outX, int16_t &outY); 

static TFT_eSprite mSpr = TFT_eSprite(&tft);

// --- Monitor Globals ---
std::vector<IrisInstance> monitorInstances;
uint16_t themeColors[5] = { TFT_GREEN, TFT_CYAN, TFT_YELLOW, TFT_MAGENTA, TFT_WHITE };
int themeColorIndex = 0; // Can be cycled later via settings
bool isInterruptActive = false;
extern uint32_t lastBleDataTime;

// --- State Variables ---
int monitorPageIndex = 0; 
int monitorScrollY = 0;
uint32_t monitorPageEnterTime = 0;
uint32_t lastScrollTick = 0;
uint32_t lastOverviewUpdate = 0;

// Forward declarations
void drawMonitorOverview();
void drawMonitorInstance(int idx);

void advanceMonitorPage() {
    monitorPageIndex++;
    if (monitorPageIndex > monitorInstances.size()) monitorPageIndex = 0; 
    monitorPageEnterTime = millis();
    monitorScrollY = 0;

    if (monitorPageIndex == 0) drawMonitorOverview();
    else drawMonitorInstance(monitorPageIndex - 1);
}

void drawUIOverlay() {
    drawStatusBar(mSpr);
    
    // Bottom-Right Home Button
    int btnX = 240;
    int btnY = 195;
    mSpr.fillRoundRect(btnX, btnY, 70, 35, 5, tft.color565(40, 40, 40));
    mSpr.drawRect(btnX, btnY, 70, 35, tft.color565(100, 100, 100)); 
    mSpr.setTextSize(2);
    mSpr.setTextColor(TFT_WHITE);
    mSpr.drawCentreString("HOME", btnX + 35, btnY + 10, 1);
}

void drawMonitorOverview() {
    if (!mSpr.created()) mSpr.createSprite(320, 240);
    mSpr.fillSprite(TFT_BLACK);
    
    int yOffset = showStatusBar ? 20 : 0;

    if (isInterruptActive) {
        mSpr.fillRect(0, yOffset, 320, 25, TFT_RED);
        mSpr.setTextColor(TFT_WHITE, TFT_RED);
        mSpr.setTextSize(2);
        mSpr.drawCentreString("SYSTEM ALERT!", 160, 5 + yOffset, 1);
    } else {
        mSpr.setTextColor(themeColors[themeColorIndex], TFT_BLACK);
        mSpr.setTextSize(2);
        mSpr.drawString("SYSTEM OVERVIEW", 10, 10 + yOffset);
        mSpr.drawLine(10, 30 + yOffset, 310, 30 + yOffset, themeColors[themeColorIndex]);
    }

    mSpr.setTextColor(themeColors[themeColorIndex], TFT_BLACK);
    mSpr.drawString("Time: " + currentBrowserTime, 10, 45 + yOffset);
    mSpr.drawString("Servers: " + String((int)monitorInstances.size()), 10, 80 + yOffset);

    int badComps = 0;
    int totalHidden = 0;
    for (auto& inst : monitorInstances) {
        totalHidden += inst.hiddenAlerts;
        for (auto& comp : inst.components) {
            if (comp.status == 0 || comp.status == 2) badComps++;
        }
    }

    mSpr.drawString("Alerts:", 10, 115 + yOffset);
    if (badComps > 0) mSpr.setTextColor(TFT_RED, TFT_BLACK);
    else mSpr.setTextColor(TFT_GREEN, TFT_BLACK);

    mSpr.setTextSize(4);
    // Shifted numbers to the right for wider screen
    mSpr.drawString(String(badComps), 120, 110 + yOffset); 
    
    if (totalHidden > 0) {
        mSpr.setTextSize(1);
        mSpr.setTextColor(TFT_DARKGREY, TFT_BLACK);
        mSpr.drawString("(Hidden: " + String(totalHidden) + ")", 120, 145 + yOffset);
    }

    mSpr.setTextSize(2);
    mSpr.setTextColor(themeColors[themeColorIndex], TFT_BLACK);

    String timeAgo = "--";
    if (lastBleDataTime > 0) timeAgo = String((millis() - lastBleDataTime) / 1000) + "s";
    mSpr.drawString("Last Data: " + timeAgo, 10, 170 + yOffset);
    
    drawUIOverlay();
    mSpr.pushSprite(0, 0); 
    lastOverviewUpdate = millis();
}

void drawMonitorInstance(int idx) {
    if (idx < 0 || idx >= monitorInstances.size()) return;
    if (!mSpr.created()) mSpr.createSprite(320, 240);
    
    IrisInstance& inst = monitorInstances[idx];

    // Sort components: Errors (0) top, Warnings (2) middle, OK (1) bottom
    std::sort(inst.components.begin(), inst.components.end(), [](const IrisComponent& a, const IrisComponent& b) {
        int wA = (a.status == 0) ? 0 : (a.status == 2) ? 1 : 2;
        int wB = (b.status == 0) ? 0 : (b.status == 2) ? 1 : 2;
        if (wA == wB) return a.name < b.name; 
        return wA < wB;
    });
    
    mSpr.fillSprite(TFT_BLACK);
    int yOffset = showStatusBar ? 20 : 0;

    // We only fill the top header background, so text scrolling behind it gets clipped out
    if (isInterruptActive) {
        mSpr.fillRect(0, yOffset, 320, 35, TFT_RED);
        mSpr.setTextColor(TFT_WHITE, TFT_RED);
    } else {
        mSpr.setTextColor(inst.color, TFT_BLACK);
    }
    
    mSpr.setTextSize(2);
    mSpr.drawString(inst.name, 10, 10 + yOffset);
    if (!isInterruptActive) mSpr.drawLine(10, 30 + yOffset, 310, 30 + yOffset, inst.color);

    int yStart = (40 + yOffset) - monitorScrollY;
    
    for (int i = 0; i < inst.components.size(); i++) {
        int yPos = yStart + (i * 25);
        
        // Clipping bounds: We don't draw the item if it scrolls up into the header
        if (yPos > (35 + yOffset) && yPos < 240) {
            uint16_t cColor = TFT_WHITE;
            String statStr = "UNK";
            if (inst.components[i].status == 1) { cColor = TFT_GREEN; statStr = "OK"; }
            else if (inst.components[i].status == 0) { cColor = TFT_RED; statStr = "FAIL"; }
            else if (inst.components[i].status == 2) { cColor = TFT_YELLOW; statStr = "WARN"; }

            mSpr.setTextColor(cColor, TFT_BLACK);
            String n = inst.components[i].name;
            if (n.length() > 22) n = n.substring(0, 22) + ".."; // Allowed longer names on CYD!
            
            mSpr.drawString(n, 10, yPos);
            mSpr.drawString(statStr, 260, yPos); // Shifted status to far right
        }
    }
    
    // Redraw the header rectangle to cover any text that scrolled up past the line
    mSpr.fillRect(0, yOffset, 320, 32, TFT_BLACK);
    mSpr.setTextColor(inst.color, TFT_BLACK);
    mSpr.drawString(inst.name, 10, 10 + yOffset);
    if (!isInterruptActive) mSpr.drawLine(10, 30 + yOffset, 310, 30 + yOffset, inst.color);

    drawUIOverlay();
    mSpr.pushSprite(0, 0); 
}

void run_monitor_app() {
    // 1. Auto-scroll and render logic
    if (monitorPageIndex == 0) {
        if (millis() - lastOverviewUpdate > 1000) drawMonitorOverview();
    } else {
        // Draw the instance screen
        drawMonitorInstance(monitorPageIndex - 1);
        
        // Handle Auto-Scrolling
        if (millis() - monitorPageEnterTime > 3000) {
            if (millis() - lastScrollTick > 50) {
                lastScrollTick = millis();
                IrisInstance& inst = monitorInstances[monitorPageIndex - 1];
                int contentHeight = inst.components.size() * 25;
                
                // Allow scrolling further since CYD has more vertical space
                int maxScroll = contentHeight - 140; 
                if (maxScroll < 0) maxScroll = 0;
                
                if (monitorScrollY < maxScroll) {
                    monitorScrollY += 2;
                }
            }
        }
    }

    // 2. Touch Navigation
    int16_t touchX, touchY;
    if (get_mapped_touch(touchX, touchY)) {
        // A. Check for Home Button tap
        if (touchX >= 240 && touchX <= 310 && touchY >= 195 && touchY <= 230) {
            currentApp = APP_HOME; 
            lv_obj_invalidate(lv_scr_act()); 
            delay(200); 
        } 
        // B. Tap anywhere else -> Next Page
        else {
            advanceMonitorPage();
            delay(250); // Debounce
        }
    }
}

#endif