#ifndef WEATHER_APP_H
#define WEATHER_APP_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <lvgl.h>
#include "Config.h"
#include "ui.h"

extern AppState currentApp;

uint32_t lastWeatherFetch = 0;
const uint32_t WEATHER_UPDATE_INTERVAL = 600000; // 10 minutes in milliseconds

std::vector<WeatherLoc> weatherLocations;
WeatherData currentWeatherData;
int currentWeatherIndex = 0;
String weather_status_msg = "Fetching Data...";

// State & Animation Variables
bool is_weather_loaded = false;
lv_obj_t *weather_screen = NULL;
uint32_t lastLightningFlash = 0;
bool isFlashing = false;
float windmillAngle = 0;

// LVGL Graphic Objects
lv_obj_t *obj_raindrops[30];
lv_obj_t *obj_blades[3];
lv_point_t blade_points[3][2]; // Coordinates for the 3 windmill lines
struct Raindrop
{
    float x, y, speed;
};
Raindrop drops[30];

String getWeatherDescription(int code)
{
    switch (code)
    {
    case 0:
        return "Clear sky";
    case 1:
    case 2:
    case 3:
        return "Partly cloudy";
    case 45:
    case 48:
        return "Fog";
    case 51:
    case 53:
    case 55:
        return "Drizzle";
    case 61:
    case 63:
    case 65:
        return "Rain";
    case 71:
    case 73:
    case 75:
        return "Snow";
    case 95:
    case 96:
    case 99:
        return "Thunderstorm";
    default:
        return "Unknown";
    }
}

// --- BOOT INITIALIZATION FUNCTION ---
void init_weather_app()
{
    Serial.println("--- Booting Weather App ---");

    if (weatherLocations.empty())
    {
        weatherLocations.push_back({"London", 51.5074, -0.1278});
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        weather_status_msg = "Error: No Wi-Fi!";
        Serial.println(weather_status_msg);
        return;
    }

    Serial.println("Contacting Open-Meteo securely...");
    WiFiClientSecure *client = new WiFiClientSecure;

    if (client)
    {
        client->setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(7000);

        float lat = weatherLocations[currentWeatherIndex].lat;
        float lon = weatherLocations[currentWeatherIndex].lon;
        String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
                     "&longitude=" + String(lon, 4) +
                     "&current=temperature_2m,weather_code,wind_speed_10m" +
                     "&daily=precipitation_probability_max,sunrise,sunset&timezone=auto&forecast_days=1";

        http.begin(*client, url);
        int httpCode = http.GET();

        if (httpCode > 0)
        {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                String payload = http.getString();
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);

                if (!error)
                {
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
                    weather_status_msg = "";
                    Serial.println("Weather API Success!");
                }
                else
                {
                    weather_status_msg = "Weather Parse Failed!";
                }
            }
            else
            {
                weather_status_msg = "HTTP Error: " + String(httpCode);
            }
        }
        else
        {
            weather_status_msg = "Weather API Timeout!";
        }
        http.end();
        delete client;
    }
}

void fetch_weather_data()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    WiFiClient *client = new WiFiClient; // Standard HTTP client (uses very little RAM)

    if (client)
    {
        HTTPClient http;
        http.setTimeout(5000);

        float lat = weatherLocations[currentWeatherIndex].lat;
        float lon = weatherLocations[currentWeatherIndex].lon;

        // Changed to http://
        String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
                     "&longitude=" + String(lon, 4) +
                     "&current=temperature_2m,weather_code,wind_speed_10m" +
                     "&daily=precipitation_probability_max,sunrise,sunset&timezone=auto&forecast_days=1";

        http.begin(*client, url);
        int httpCode = http.GET();

        if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY))
        {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error)
            {
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
                weather_status_msg = "";
                lastWeatherFetch = millis(); // Reset the timer on success
            }
        }
        http.end();
        delete client;
    }
}

// --- LVGL SCREEN GENERATOR ---
void build_weather_screen()
{
    if (weather_screen == NULL)
    {
        weather_screen = lv_obj_create(NULL);
    }
    else
    {
        lv_obj_clean(weather_screen);
    }

    bool isThunderstorm = (currentWeatherData.code >= 95);
    bool isRaining = (currentWeatherData.code >= 51 && currentWeatherData.code <= 65) || isThunderstorm;

    // Set Background Color
    if (isThunderstorm)
    {
        lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0x0A0A1E), 0);
    }
    else if (currentWeatherData.code == 0)
    {
        lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0x002850), 0); // Clear Sky
    }
    else
    {
        lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0x141E28), 0); // Cloudy
    }

    if (!currentWeatherData.valid)
    {
        lv_obj_t *loading = lv_label_create(weather_screen);
        lv_label_set_text(loading, weather_status_msg.c_str());
        lv_obj_set_style_text_color(loading, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(loading, LV_ALIGN_CENTER, 0, 0);
    }
    else
    {
        // --- TEXT DATA ---
        lv_obj_t *lbl_loc = lv_label_create(weather_screen);
        lv_label_set_text(lbl_loc, weatherLocations[currentWeatherIndex].name.c_str());
        lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_loc, lv_color_hex(0x00FFFF), 0);
        lv_obj_align(lbl_loc, LV_ALIGN_TOP_LEFT, 15, 40);

        lv_obj_t *lbl_cond = lv_label_create(weather_screen);
        lv_label_set_text(lbl_cond, currentWeatherData.condition.c_str());
        lv_obj_set_style_text_color(lbl_cond, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl_cond, LV_ALIGN_TOP_LEFT, 15, 70);

        lv_obj_t *lbl_temp = lv_label_create(weather_screen);
        lv_label_set_text_fmt(lbl_temp, "%.1fc", currentWeatherData.temp);
        lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(lbl_temp, LV_ALIGN_TOP_LEFT, 15, 95);

        lv_obj_t *lbl_wind = lv_label_create(weather_screen);
        lv_label_set_text_fmt(lbl_wind, "Wind: %.1f", currentWeatherData.wind);
        lv_obj_set_style_text_color(lbl_wind, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(lbl_wind, LV_ALIGN_TOP_LEFT, 15, 145);

        lv_obj_t *lbl_rain = lv_label_create(weather_screen);
        lv_label_set_text_fmt(lbl_rain, "Rain: %d%%", currentWeatherData.rain);
        lv_obj_set_style_text_color(lbl_rain, currentWeatherData.rain > 0 ? lv_color_hex(0x6496FF) : lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(lbl_rain, LV_ALIGN_TOP_LEFT, 15, 165);

        lv_obj_t *lbl_sun = lv_label_create(weather_screen);
        lv_label_set_text_fmt(lbl_sun, "%s - %s", currentWeatherData.sunrise.c_str(), currentWeatherData.sunset.c_str());
        lv_obj_set_style_text_color(lbl_sun, lv_color_hex(0xFFA500), 0);
        lv_obj_align(lbl_sun, LV_ALIGN_TOP_LEFT, 15, 185);

        // --- SCENERY (Sun/Clouds) ---
        int iconX = 220;
        int iconY = 50;
        if (currentWeatherData.code == 0)
        {
            lv_obj_t *sun = lv_obj_create(weather_screen);
            lv_obj_set_size(sun, 40, 40);
            lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(sun, lv_color_hex(0xFFDD00), 0);
            lv_obj_set_style_border_width(sun, 0, 0);
            lv_obj_set_pos(sun, iconX, iconY);
        }
        else
        {
            uint32_t cloudColor = isThunderstorm ? 0x50505A : 0xAAAAAA;
            lv_obj_t *cloud1 = lv_obj_create(weather_screen);
            lv_obj_set_size(cloud1, 60, 30);
            lv_obj_set_style_radius(cloud1, 15, 0);
            lv_obj_set_style_bg_color(cloud1, lv_color_hex(cloudColor), 0);
            lv_obj_set_style_border_width(cloud1, 0, 0);
            lv_obj_set_pos(cloud1, iconX - 15, iconY);

            lv_obj_t *cloud2 = lv_obj_create(weather_screen);
            lv_obj_set_size(cloud2, 40, 40);
            lv_obj_set_style_radius(cloud2, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(cloud2, lv_color_hex(cloudColor), 0);
            lv_obj_set_style_border_width(cloud2, 0, 0);
            lv_obj_set_pos(cloud2, iconX, iconY - 15);
        }

        // --- WINDMILL TOWER ---
        lv_obj_t *tower = lv_obj_create(weather_screen);
        lv_obj_set_size(tower, 20, 60);
        lv_obj_set_style_bg_color(tower, lv_color_hex(0x643C1E), 0);
        lv_obj_set_style_border_width(tower, 0, 0);
        lv_obj_set_style_radius(tower, 5, 0);
        lv_obj_set_pos(tower, 250, 160);

        // --- WINDMILL BLADES ---
        static lv_style_t style_line;
        lv_style_init(&style_line);
        lv_style_set_line_width(&style_line, 4);
        lv_style_set_line_color(&style_line, lv_color_hex(0xFFFFFF));
        lv_style_set_line_rounded(&style_line, true);

        for (int i = 0; i < 3; i++)
        {
            obj_blades[i] = lv_line_create(weather_screen);
            lv_obj_add_style(obj_blades[i], &style_line, 0);
            // Points are populated in the run loop
        }

        // --- RAINDROPS ---
        if (isRaining)
        {
            for (int i = 0; i < 30; i++)
            {
                drops[i] = {(float)random(0, 320), (float)random(-240, 0), random(4, 9) / 1.0f};
                obj_raindrops[i] = lv_obj_create(weather_screen);
                lv_obj_set_size(obj_raindrops[i], 2, drops[i].speed * 2);
                lv_obj_set_style_bg_color(obj_raindrops[i], lv_color_hex(0x6496FF), 0);
                lv_obj_set_style_border_width(obj_raindrops[i], 0, 0);
            }
        }
        else
        {
            for (int i = 0; i < 30; i++)
                obj_raindrops[i] = NULL;
        }
    }

    // --- HOME BUTTON ---
    lv_obj_t *home_btn = lv_btn_create(weather_screen);
    lv_obj_set_size(home_btn, 70, 35);
    lv_obj_align(home_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x282828), 0);

    lv_obj_add_event_cb(home_btn, [](lv_event_t *e)
                        {
        currentApp = APP_HOME;          
        is_weather_loaded = false;     
        lv_disp_load_scr(ui_ScreenHome); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home_lbl = lv_label_create(home_btn);
    lv_label_set_text(home_lbl, "HOME");
    lv_obj_center(home_lbl);
}

// --- RUNTIME ANIMATION LOOP ---
void run_weather_app()
{
    if (!is_weather_loaded)
    {
        build_weather_screen();
        lv_disp_load_scr(weather_screen);
        is_weather_loaded = true;
        windmillAngle = 0;
    }

    // --- PUT UI ON HOLD ---
    if (millis() - lastWeatherFetch > WEATHER_UPDATE_INTERVAL)
    {
        // 1. Alert the user that a sync is happening
        lv_obj_t *sync_label = lv_label_create(weather_screen);
        lv_label_set_text(sync_label, "Syncing...");
        lv_obj_set_style_text_color(sync_label, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(sync_label, LV_ALIGN_TOP_RIGHT, -15, 15);

        // 2. FORCE LVGL to draw the "Syncing..." text immediately
        lv_timer_handler();

        // 3. Perform the blocking HTTP fetch
        fetch_weather_data();

        // 4. Rebuild the screen with the fresh data (this removes the syncing label)
        build_weather_screen();
        lv_disp_load_scr(weather_screen);
    }

    if (currentWeatherData.valid)
    {
        // 1. Lightning Flash Logic
        bool isThunderstorm = (currentWeatherData.code >= 95);
        if (isThunderstorm)
        {
            if (random(100) < 2)
            {
                isFlashing = true;
                lastLightningFlash = millis();
                lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0xFFFFFF), 0); // Flash white
            }
            if (isFlashing && (millis() - lastLightningFlash > 100))
            {
                isFlashing = false;
                lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0x0A0A1E), 0); // Return to dark
            }
        }

        // 2. Windmill Animation
        float rotationSpeed = currentWeatherData.wind * 0.02;
        if (rotationSpeed < 0.05)
            rotationSpeed = 0.05;
        windmillAngle += rotationSpeed;

        int wmX = 260; // Center X of blades
        int wmY = 160; // Center Y of blades

        for (int i = 0; i < 3; i++)
        {
            float rad = windmillAngle + (i * (2 * PI / 3));
            blade_points[i][0] = {(lv_coord_t)wmX, (lv_coord_t)wmY};
            blade_points[i][1] = {(lv_coord_t)(wmX + cos(rad) * 35), (lv_coord_t)(wmY + sin(rad) * 35)};
            lv_line_set_points(obj_blades[i], blade_points[i], 2);
        }

        // 3. Rain Animation
        bool isRaining = (currentWeatherData.code >= 51 && currentWeatherData.code <= 65) || isThunderstorm;
        if (isRaining)
        {
            float windDrift = currentWeatherData.wind * 0.15;
            for (int i = 0; i < 30; i++)
            {
                if (obj_raindrops[i] != NULL)
                {
                    drops[i].y += drops[i].speed;
                    drops[i].x += windDrift;
                    if (drops[i].y > 240 || drops[i].x > 320)
                    {
                        drops[i].y = random(-50, 0);
                        drops[i].x = random(-50, 320);
                    }
                    lv_obj_set_pos(obj_raindrops[i], drops[i].x, drops[i].y);
                }
            }
        }
    }
}

#endif