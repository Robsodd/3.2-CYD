#ifndef FOOTBALL_APP_H
#define FOOTBALL_APP_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "Config.h"
#include "ui.h"

// --- External OS Variables ---
extern AppState currentApp;

// --- Football Globals ---
std::vector<LeagueRow> leagueTable;
String currentTeam = "Arsenal";
String currentLeague = "4328";
String leagueTitle = "PREMIER LEAGUE";
String todayDate = "2026-08-26";
String football_status_msg = "Fetching API...";

// State tracking for the app
bool is_football_loaded = false;
lv_obj_t *football_screen = NULL;

String getLeagueName(String id)
{
    if (id == "4328")
        return "PREMIER LEAGUE";
    if (id == "4329")
        return "CHAMPIONSHIP";
    if (id == "4396")
        return "LEAGUE ONE";
    if (id == "4397")
        return "LEAGUE TWO";
    if (id == "4590")
        return "NATIONAL LEAGUE";
    return "LEAGUE TABLE";
}

// --- NEW BOOT INITIALIZATION FUNCTION ---
// --- BOOT INITIALIZATION FUNCTION ---
void init_football_app()
{
    Serial.println("--- Booting Football App ---");

    Preferences preferences;
    // Open the namespace in read/write mode
    preferences.begin("rainbow", false);

    // Pull saved settings, falling back to defaults only if nothing is saved
    currentTeam = preferences.getString("favTeam", "Southend United");
    currentLeague = preferences.getString("favLeague", "4590"); // National League

    String lastFetchDate = preferences.getString("fetchDate", "");
    String lastFetchLeague = preferences.getString("fetchLeague", "");

    if (WiFi.status() != WL_CONNECTED)
    {
        football_status_msg = "Error: No Wi-Fi!";
        Serial.println(football_status_msg);
        // return;
    }

    if (todayDate == lastFetchDate && currentLeague == lastFetchLeague)
    {
        String cachedTable = preferences.getString("tableData", "");
        if (cachedTable != "")
        {
            Serial.println("Football API Skipped. Table loaded from flash cache.");
            JsonDocument cacheDoc;
            deserializeJson(cacheDoc, cachedTable);
            JsonArray arr = cacheDoc.as<JsonArray>();

            leagueTable.clear();
            for (JsonObject obj : arr)
            {
                LeagueRow row;
                row.pos = obj["pos"].as<String>();
                row.name = obj["name"].as<String>();
                row.pts = obj["pts"].as<String>();
                row.highlight = obj["highlight"].as<bool>();
                leagueTable.push_back(row);
            }
            preferences.end();
            football_status_msg = "";
            return;
        }
    }
    preferences.end();

    Serial.println("Contacting TheSportsDB securely...");

    // We can safely use the Secure Client now because LVGL hasn't eaten our RAM yet!
    WiFiClientSecure *client = new WiFiClientSecure;

    if (client)
    {
        client->setInsecure(); // Skip certificate validation for speed/stability
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(7000); // Bumped to 7 seconds just in case the TLS handshake is slow

        // Reverted to HTTPS, using free API key 1
        String apiUrl = "https://www.thesportsdb.com/api/v1/json/3/lookuptable.php?l=" + currentLeague;
        Serial.print("Connecting to URL: ");
        Serial.println(apiUrl);

        http.begin(*client, apiUrl);
        http.addHeader("User-Agent", "Mozilla/5.0");
        http.addHeader("Accept", "application/json");

        int httpCode = http.GET();
        Serial.printf("HTTP Code Returned: %d\n", httpCode);

        if (httpCode > 0)
        {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                String payload = http.getString();
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);

                if (!error)
                {
                    std::vector<LeagueRow> tempFullTable;
                    int targetIndex = -1;
                    int currentIndex = 0;

                    JsonArray table = doc["table"].as<JsonArray>();
                    String targetTeam = currentTeam;
                    targetTeam.replace("-", " ");
                    targetTeam.toLowerCase();

                    for (JsonObject team : table)
                    {
                        LeagueRow row;
                        row.name = team["strTeam"].as<String>();
                        row.pos = team["intRank"].as<String>();
                        row.pts = team["intPoints"].as<String>();
                        row.highlight = false;

                        String currentTeamLower = row.name;
                        currentTeamLower.toLowerCase();

                        if (currentTeamLower.indexOf(targetTeam) != -1)
                        {
                            row.highlight = true;
                            targetIndex = currentIndex;
                        }
                        tempFullTable.push_back(row);
                        currentIndex++;
                    }

                    int start = 0;
                    int end = min(5, (int)tempFullTable.size());
                    if (targetIndex != -1)
                    {
                        start = max(0, targetIndex - 2);
                        end = min((int)tempFullTable.size(), start + 5);
                        if (end - start < 5 && tempFullTable.size() >= 5)
                        {
                            start = max(0, end - 5);
                        }
                    }

                    leagueTable.clear();
                    JsonDocument saveDoc;
                    JsonArray saveArray = saveDoc.to<JsonArray>();

                    for (int i = start; i < end; i++)
                    {
                        leagueTable.push_back(tempFullTable[i]);
                        JsonObject obj = saveArray.add<JsonObject>();
                        obj["pos"] = tempFullTable[i].pos;
                        obj["name"] = tempFullTable[i].name;
                        obj["pts"] = tempFullTable[i].pts;
                        obj["highlight"] = tempFullTable[i].highlight;
                    }

                    String jsonToSave;
                    serializeJson(saveDoc, jsonToSave);

                    preferences.begin("rainbow", false);
                    preferences.putString("fetchDate", todayDate);
                    preferences.putString("fetchLeague", currentLeague);
                    preferences.putString("tableData", jsonToSave);
                    preferences.end();

                    football_status_msg = "";
                    Serial.println("Success! Table saved to flash.");
                }
                else
                {
                    football_status_msg = "JSON Parse Failed!";
                    Serial.println(football_status_msg);
                }
            }
            else
            {
                football_status_msg = "HTTP Error: " + String(httpCode);
                Serial.println(football_status_msg);
            }
        }
        else
        {
            football_status_msg = "API Timeout/Failed: " + http.errorToString(httpCode);
            Serial.println(football_status_msg);
        }
        http.end();
        delete client;
    }
    else
    {
        football_status_msg = "Client memory error!";
        Serial.println(football_status_msg);
    }
}

void build_football_screen()
{
    if (football_screen == NULL)
    {
        football_screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(football_screen, lv_color_hex(0x000000), 0);
    }
    else
    {
        lv_obj_clean(football_screen);
    }

    // Create a scrollable container for the table
    lv_obj_t *table_container = lv_obj_create(football_screen);
    lv_obj_set_size(table_container, 320, 190); // Leaves room at the bottom for the home button
    lv_obj_align(table_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0x121212), 0);
    lv_obj_set_style_border_width(table_container, 0, 0);

    // Enable native swipe scrolling
    lv_obj_add_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    // Populate the container dynamically
    int yPos = 50;
    for (int i = 0; i < leagueTable.size(); i++)
    {
        lv_obj_t *row = lv_btn_create(table_container); // Make rows interactive buttons
        lv_obj_set_size(row, 280, 40);
        lv_obj_set_pos(row, 0, yPos);

        if (leagueTable[i].highlight)
        {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x38003C), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x282828), 0);
        }

        lv_obj_t *lbl_pos = lv_label_create(row);
        lv_label_set_text_fmt(lbl_pos, "%s.", leagueTable[i].pos.c_str());
        lv_obj_align(lbl_pos, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *lbl_name = lv_label_create(row);
        lv_label_set_text(lbl_name, leagueTable[i].name.c_str());
        lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 30, 0);

        lv_obj_t *lbl_pts = lv_label_create(row);
        lv_label_set_text(lbl_pts, leagueTable[i].pts.c_str());
        lv_obj_align(lbl_pts, LV_ALIGN_RIGHT_MID, 0, 0);

        // Add a click event to the row (e.g., to expand stats later)
        lv_obj_add_event_cb(row, [](lv_event_t *e)
                            {
                                Serial.println("Row clicked!");
                                // Future expansion: pop up a modal with team form or upcoming fixtures
                            },
                            LV_EVENT_CLICKED, NULL);

        yPos += 45;
    }

    // // --- HOME BUTTON ---
    // lv_obj_t *home_btn = lv_btn_create(football_screen);
    // lv_obj_set_size(home_btn, 70, 35);
    // lv_obj_align(home_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    // lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x282828), 0);

    // lv_obj_add_event_cb(home_btn, [](lv_event_t *e)
    //                     {
    //     currentApp = APP_HOME;
    //     is_football_loaded = false;
    //     lv_disp_load_scr(ui_ScreenHome); }, LV_EVENT_CLICKED, NULL);

    // lv_obj_t *home_lbl = lv_label_create(home_btn);
    // lv_label_set_text(home_lbl, "HOME");
    // lv_obj_center(home_lbl);
}

// --- STREAMLINED RUNTIME LOGIC ---
void run_football_app()
{
    if (!is_football_loaded)
    {
        // Because the data was fetched at boot, we just build and show the screen immediately
        build_football_screen();
        lv_disp_load_scr(football_screen);
        is_football_loaded = true;
    }
}

#endif