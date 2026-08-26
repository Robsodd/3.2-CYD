#ifndef WEATHER_APP_H
#define WEATHER_APP_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "Config.h" // Gives access to WeatherData and WeatherLoc

extern TFT_eSPI tft;
extern AppState currentApp;
extern bool showStatusBar; 
extern void drawStatusBar(TFT_eSprite& spr); // Tells compiler this exists in your main file
extern bool get_mapped_touch(int16_t &outX, int16_t &outY); // Helper from main file

static TFT_eSprite wSpr = TFT_eSprite(&tft); // Local sprite for this app

std::vector<WeatherLoc> weatherLocations;
WeatherData currentWeatherData;
int currentWeatherIndex = 0;
uint32_t lastWeatherFetch = 0;

// Animation State Variables
float windmillAngle = 0;
struct Raindrop { float x, y, speed; };
Raindrop drops[30];
bool weatherAnimInit = false;
uint32_t lastLightningFlash = 0;
bool isFlashing = false;

String getWeatherDescription(int code) {
    switch (code) {
        case 0: return "Clear sky";
        case 1: case 2: case 3: return "Partly cloudy";
        case 45: case 48: return "Fog";
        case 51: case 53: case 55: return "Drizzle";
        case 61: case 63: case 65: return "Rain";
        case 71: case 73: case 75: return "Snow";
        case 95: case 96: case 99: return "Thunderstorm";
        default: return "Unknown";
    }
}

void fetchAndDisplayWeather() {
    // --- NEW: Default to London if no locations are configured! ---
    if (weatherLocations.empty()) {
        weatherLocations.push_back({"London", 51.5074, -0.1278});
    }

    if (WiFi.status() != WL_CONNECTED) return;
    
    // Only fetch every 10 minutes to save API limits
    if (currentWeatherData.valid && (millis() - lastWeatherFetch < 600000)) return; 

    HTTPClient http;
    float lat = weatherLocations[currentWeatherIndex].lat;
    float lon = weatherLocations[currentWeatherIndex].lon;
  
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) + 
                 "&longitude=" + String(lon, 4) + 
                 "&current=temperature_2m,weather_code,wind_speed_10m" +
                 "&daily=precipitation_probability_max,sunrise,sunset&timezone=auto&forecast_days=1";
                 
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            currentWeatherData.temp = doc["current"]["temperature_2m"];
            currentWeatherData.wind = doc["current"]["wind_speed_10m"];
            currentWeatherData.code = doc["current"]["weather_code"]; 
            currentWeatherData.condition = getWeatherDescription(currentWeatherData.code);
            currentWeatherData.rain = doc["daily"]["precipitation_probability_max"][0];
            String srRaw = doc["daily"]["sunrise"][0].as<String>();
            String ssRaw = doc["daily"]["sunset"][0].as<String>();
            currentWeatherData.sunrise = srRaw.substring(11, 16); 
            currentWeatherData.sunset = ssRaw.substring(11, 16);
            currentWeatherData.valid = true;
            lastWeatherFetch = millis();
        }
    }
    http.end();
}

void drawWeatherFrame() {
    if (!weatherAnimInit) {
        wSpr.createSprite(320, 240);
        for (int i = 0; i < 30; i++) {
            drops[i] = {(float)random(0, 320), (float)random(-240, 0), random(4, 9) / 1.0f};
        }
        weatherAnimInit = true;
    }

    uint16_t bgColor = TFT_BLACK;
    int yOffset = showStatusBar ? 20 : 0;
    bool isThunderstorm = (currentWeatherData.code >= 95);
    bool isRaining = (currentWeatherData.code >= 51 && currentWeatherData.code <= 65) || isThunderstorm;

    if (isThunderstorm) {
        if (random(100) < 2) { isFlashing = true; lastLightningFlash = millis(); }
        if (isFlashing && (millis() - lastLightningFlash < 100)) bgColor = TFT_WHITE; 
        else { isFlashing = false; bgColor = tft.color565(10, 10, 30); }
    } else if (currentWeatherData.code == 0) {
        bgColor = tft.color565(0, 40, 80); 
    } else {
        bgColor = tft.color565(20, 30, 40); 
    }

    wSpr.fillSprite(bgColor);

    if (weatherLocations.empty() || !currentWeatherData.valid) {
        wSpr.setTextColor(TFT_YELLOW, TFT_BLACK);
        wSpr.drawCentreString(weatherLocations.empty() ? "No Locations" : "Fetching...", 160, 110 + yOffset, 2);
    } else {
        // --- SCENERY ---
        int iconX = 250; 
        int iconY = 60 + yOffset; 

        if (currentWeatherData.code == 0) {
            wSpr.fillCircle(iconX, iconY, 20, TFT_YELLOW);
            for (int i = 0; i < 8; i++) {
                float angle = i * (PI / 4.0);
                wSpr.drawLine(iconX + cos(angle)*25, iconY + sin(angle)*25, iconX + cos(angle)*35, iconY + sin(angle)*35, TFT_ORANGE);
            }
        } else {
            uint16_t cloudColor = isThunderstorm ? tft.color565(80, 80, 90) : TFT_LIGHTGREY;
            wSpr.fillCircle(iconX - 15, iconY + 5, 15, cloudColor);
            wSpr.fillCircle(iconX + 15, iconY + 5, 12, cloudColor);
            wSpr.fillCircle(iconX, iconY - 5, 20, cloudColor);
            wSpr.fillRect(iconX - 15, iconY + 5, 30, 15, cloudColor);

            if (isThunderstorm && !isFlashing) {
                wSpr.fillTriangle(iconX - 5, iconY + 15, iconX + 5, iconY + 15, iconX - 2, iconY + 30, TFT_YELLOW);
                wSpr.fillTriangle(iconX - 2, iconY + 25, iconX + 8, iconY + 25, iconX, iconY + 45, TFT_YELLOW);
            }
        }

        // --- WINDMILL ---
        int wmX = 260; 
        int wmY = 190 + yOffset;
        
        wSpr.fillTriangle(wmX - 15, 240, wmX + 15, 240, wmX, wmY - 10, tft.color565(100, 60, 30));
        wSpr.fillCircle(wmX, wmY, 4, TFT_DARKGREY); 

        float rotationSpeed = currentWeatherData.wind * 0.02; 
        if (rotationSpeed < 0.05) rotationSpeed = 0.05; 
        windmillAngle += rotationSpeed;

        for (int i = 0; i < 3; i++) {
            float rad = windmillAngle + (i * (2 * PI / 3));
            int tipX = wmX + cos(rad) * 35;
            int tipY = wmY + sin(rad) * 35;
            
            int base1X = wmX + cos(rad + 0.2) * 5;
            int base1Y = wmY + sin(rad + 0.2) * 5;
            int base2X = wmX + cos(rad - 0.2) * 5;
            int base2Y = wmY + sin(rad - 0.2) * 5;
            wSpr.fillTriangle(base1X, base1Y, base2X, base2Y, tipX, tipY, TFT_WHITE);
        }

        // --- RAIN ---
        if (isRaining) {
            uint16_t dropColor = tft.color565(100, 150, 255);
            float windDrift = currentWeatherData.wind * 0.15; 
            for (int i = 0; i < 30; i++) {
                drops[i].y += drops[i].speed;
                drops[i].x += windDrift;
                if (drops[i].y > 240 || drops[i].x > 320) {
                    drops[i].y = random(-50, 0);
                    drops[i].x = random(-50, 320); 
                }
                wSpr.drawLine(drops[i].x, drops[i].y, drops[i].x - (windDrift * 1.5), drops[i].y + (drops[i].speed * 1.5), dropColor);
            }
        }

        // --- TEXT ---
        wSpr.setTextSize(3);
        String locName = weatherLocations[currentWeatherIndex].name;
        if (locName.length() > 9) locName = locName.substring(0, 9);
        
        wSpr.setTextColor(TFT_BLACK); wSpr.drawString(locName, 12, 62 + yOffset); 
        wSpr.setTextColor(TFT_CYAN);  wSpr.drawString(locName, 10, 60 + yOffset);

        wSpr.setTextSize(2);
        wSpr.setTextColor(TFT_BLACK); wSpr.drawString(currentWeatherData.condition, 12, 92 + yOffset);
        wSpr.setTextColor(TFT_WHITE); wSpr.drawString(currentWeatherData.condition, 10, 90 + yOffset);

        wSpr.setTextSize(5);
        String tempStr = String(currentWeatherData.temp, 1) + "c";
        wSpr.setTextColor(TFT_BLACK);  wSpr.drawString(tempStr, 12, 122 + yOffset);
        wSpr.setTextColor(TFT_YELLOW); wSpr.drawString(tempStr, 10, 120 + yOffset);

        wSpr.setTextSize(2);
        wSpr.setTextColor(TFT_BLACK); wSpr.drawString("Wind: " + String(currentWeatherData.wind, 1), 12, 172 + yOffset);
        wSpr.setTextColor(TFT_LIGHTGREY); wSpr.drawString("Wind: " + String(currentWeatherData.wind, 1), 10, 170 + yOffset);
        
        wSpr.setTextColor(TFT_BLACK); wSpr.drawString("Rain: " + String(currentWeatherData.rain) + "%", 12, 197 + yOffset);
        if (currentWeatherData.rain > 0) wSpr.setTextColor(tft.color565(100, 150, 255));
        else wSpr.setTextColor(TFT_LIGHTGREY);
        wSpr.drawString("Rain: " + String(currentWeatherData.rain) + "%", 10, 195 + yOffset);

        wSpr.setTextColor(TFT_BLACK); wSpr.drawString(currentWeatherData.sunrise + " - " + currentWeatherData.sunset, 12, 222 + yOffset);
        wSpr.setTextColor(TFT_ORANGE); wSpr.drawString(currentWeatherData.sunrise + " - " + currentWeatherData.sunset, 10, 220 + yOffset);
    }

    // Call shared status bar function before UI buttons
    drawStatusBar(wSpr);

    // --- ON-SCREEN UI ---
    int btnY = 10 + yOffset; 
    wSpr.fillRoundRect(10, btnY, 70, 35, 5, tft.color565(40, 40, 40));
    wSpr.drawRect(10, btnY, 70, 35, tft.color565(100, 100, 100)); 
    wSpr.setTextSize(2);
    wSpr.setTextColor(TFT_WHITE);
    wSpr.drawCentreString("HOME", 45, btnY + 10, 1);

    wSpr.pushSprite(0,0);
}

// --- App Wrapper ---
void run_weather_app() {
    fetchAndDisplayWeather();
    drawWeatherFrame();

    int16_t touchX, touchY;
    if (get_mapped_touch(touchX, touchY)) {
        // Updated touch bounds to account for yOffset dynamically
        int btnY = showStatusBar ? 30 : 10;
        if (touchX >= 10 && touchX <= 80 && touchY >= btnY && touchY <= btnY + 35) {
            currentApp = APP_HOME; 
            lv_obj_invalidate(lv_scr_act()); 
            delay(200); 
        }
    }
}

#endif