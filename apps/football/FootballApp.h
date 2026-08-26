#ifndef FOOTBALL_APP_H
#define FOOTBALL_APP_H

#include <WiFi.h> 
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "Config.h" // Gives access to LeagueRow structure

extern TFT_eSPI tft;
extern AppState currentApp;
extern bool showStatusBar; 
extern void drawStatusBar(TFT_eSprite& spr); 
extern bool get_mapped_touch(int16_t &outX, int16_t &outY); 

static TFT_eSprite fSpr = TFT_eSprite(&tft);

// --- Football Globals ---
std::vector<LeagueRow> leagueTable;
String currentTeam = "Arsenal";     // Default fallback
String currentLeague = "4328";      // Premier League
String leagueTitle = "PREMIER LEAGUE";
String todayDate = "2026-08-26";    // We will update this dynamically later

// BLE Logo Buffer (64x64)
uint16_t teamLogoBuffer[4096];
int logoPixelIndex = 0;
bool hasLogo = false;

// Theme Colors (Premier League Purple)
uint16_t footballThemeBg = tft.color565(56, 0, 60); 
uint16_t footballThemeText = TFT_WHITE;

String getLeagueName(String id) {
    if (id == "4328") return "PREMIER LEAGUE";
    if (id == "4329") return "CHAMPIONSHIP";
    if (id == "4396") return "LEAGUE ONE";
    if (id == "4397") return "LEAGUE TWO";
    if (id == "4590") return "NATIONAL LEAGUE";
    return "LEAGUE TABLE";
}

void fetchFootballApi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot fetch API: Wi-Fi disconnected!");
        return;
    }

    Preferences preferences;
    preferences.begin("rainbow", false);
    String lastFetchDate = preferences.getString("fetchDate", "");
    String lastFetchLeague = preferences.getString("fetchLeague", "");

    if (todayDate == lastFetchDate && currentLeague == lastFetchLeague) {
        String cachedTable = preferences.getString("tableData", "");
        if (cachedTable != "") {
            JsonDocument cacheDoc;
            deserializeJson(cacheDoc, cachedTable);
            JsonArray arr = cacheDoc.as<JsonArray>();

            leagueTable.clear();
            for (JsonObject obj : arr) {
                LeagueRow row;
                row.pos = obj["pos"].as<String>();
                row.name = obj["name"].as<String>();
                row.pts = obj["pts"].as<String>();
                row.highlight = obj["highlight"].as<bool>();
                leagueTable.push_back(row);
            }
            preferences.end();
            return; // Loaded from cache!
        }
    }
    preferences.end(); 

    Serial.println("Contacting TheSportsDB...");
    WiFiClientSecure *client = new WiFiClientSecure;
    
    if (client) {
        client->setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        String apiUrl = "https://www.thesportsdb.com/api/v1/json/3/lookuptable.php?l=" + currentLeague;
        http.begin(*client, apiUrl);
        http.addHeader("User-Agent", "Mozilla/5.0");
        http.addHeader("Accept", "application/json");
        
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            JsonDocument doc; 
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                std::vector<LeagueRow> tempFullTable;
                int targetIndex = -1;
                int currentIndex = 0;

                JsonArray table = doc["table"].as<JsonArray>();
                String targetTeam = currentTeam; 
                targetTeam.replace("-", " "); 
                targetTeam.toLowerCase();

                for (JsonObject team : table) {
                    LeagueRow row;
                    row.name = team["strTeam"].as<String>();
                    row.pos = team["intRank"].as<String>();
                    row.pts = team["intPoints"].as<String>();
                    row.highlight = false;

                    String currentTeamLower = row.name;
                    currentTeamLower.toLowerCase();

                    if (currentTeamLower.indexOf(targetTeam) != -1) {
                        row.highlight = true;
                        targetIndex = currentIndex;
                    }
                    tempFullTable.push_back(row);
                    currentIndex++;
                }

                int start = 0;
                int end = min(5, (int)tempFullTable.size());
                if (targetIndex != -1) {
                    start = max(0, targetIndex - 2);
                    end = min((int)tempFullTable.size(), start + 5);
                    if (end - start < 5 && tempFullTable.size() >= 5) {
                        start = max(0, end - 5);
                    }
                }

                leagueTable.clear();
                JsonDocument saveDoc;
                JsonArray saveArray = saveDoc.to<JsonArray>();
                
                for (int i = start; i < end; i++) {
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
            }
        }
        http.end(); 
        delete client; 
    }
}

void drawFootballFrame() { 
    // Create sprite if it doesn't exist yet
    if (!fSpr.created()) fSpr.createSprite(320, 240);
    
    fSpr.fillSprite(TFT_BLACK);
    int yOffset = showStatusBar ? 20 : 0;

    fSpr.setTextSize(2);
    fSpr.setTextColor(footballThemeBg, TFT_BLACK);
    
    fSpr.drawString(leagueTitle, 10, 5 + yOffset); 
    fSpr.drawLine(10, 25 + yOffset, 220, 25 + yOffset, footballThemeBg);
    
    if (leagueTable.empty()) {
        fSpr.setTextColor(TFT_DARKGREY, TFT_BLACK);
        fSpr.drawString("Fetching...", 10, 80 + yOffset);
        
        drawStatusBar(fSpr);
        fSpr.pushSprite(0, 0);
        return;
    }

    // Shift logo to the right for the wider screen (X=250)
    if (hasLogo && logoPixelIndex == 4096) fSpr.pushImage(250, 30 + yOffset, 64, 64, teamLogoBuffer);

    fSpr.setTextSize(2);
    for (int i = 0; i < leagueTable.size(); i++) {
        int yPos = (45 + yOffset) + (i * 32); // Slightly more padding between rows
        
        if (leagueTable[i].highlight) {
            // Widen the highlight bar to 310 to fit the screen
            fSpr.fillRect(5, yPos - 3, 310, 28, footballThemeBg);
            fSpr.setTextColor(footballThemeText, footballThemeBg);
        } else {
            fSpr.setTextColor(TFT_WHITE, TFT_BLACK);
        }

        String posStr = leagueTable[i].pos + ".";
        if (posStr.length() < 3) posStr = " " + posStr; 
        String nameStr = leagueTable[i].name;
        
        // Spread the text out across the 320px width
        fSpr.drawString(posStr, 10, yPos);
        fSpr.drawString(nameStr, 60, yPos); // Shifted name right
        fSpr.drawString(leagueTable[i].pts, 270, yPos); // Shifted points right
    }

    drawStatusBar(fSpr);

    // --- ON-SCREEN UI ---
    // Placed in the bottom right corner
    int btnX = 240;
    int btnY = 195;
    fSpr.fillRoundRect(btnX, btnY, 70, 35, 5, tft.color565(40, 40, 40));
    fSpr.drawRect(btnX, btnY, 70, 35, tft.color565(100, 100, 100)); 
    fSpr.setTextSize(2);
    fSpr.setTextColor(TFT_WHITE);
    fSpr.drawCentreString("HOME", btnX + 35, btnY + 10, 1);

    fSpr.pushSprite(0, 0);
}

void run_football_app() {
    // Only fetch once per boot or when date changes (handled by cache logic)
    static bool fetched = false;
    if (!fetched) {
        fetchFootballApi();
        fetched = true;
    }

    drawFootballFrame();

    int16_t touchX, touchY;
    if (get_mapped_touch(touchX, touchY)) {
        // Touch bounds for bottom-right Home button
        if (touchX >= 240 && touchX <= 310 && touchY >= 195 && touchY <= 230) {
            currentApp = APP_HOME; 
            lv_obj_invalidate(lv_scr_act()); 
            delay(200); 
        }
    }
}

#endif