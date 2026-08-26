#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "driver/rtc_io.h"
#include <Preferences.h>
#include <WebServer.h>
#include <Update.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <time.h>

#include "Config.h"
#include "webpages.h"

// --- Include App Modules ---
#include "apps/fishtank/FishTankApp.h"
#include "apps/ledcontroller/LedControllerApp.h"
#include "apps/weather/WeatherApp.h"
#include "apps/football/FootballApp.h"
#include "apps/iris/SystemMonitorApp.h"
#include "apps/iris/SummaryMonitorApp.h"
#include "apps/calibration/CalibrationApp.h"

// --- Screen & Touch Config ---
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
int live_calData[4] = {1180, 2880, 160, 5100};

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

// --- System Variables ---
WebServer server(80);
Preferences preferences;
AppState currentApp = APP_HOME;

bool isBleConnected = false;
bool showStatusBar = true;
String currentBrowserTime = "--:--";
String currentDate = "--/--";
String bleBuffer = "";
uint32_t lastClockUpdate = 0;
uint32_t lastBleDataTime = 0;
int currentParseIndex = -1;
String adminPassword = "";
String customDeviceName = "CYD-Dashboard";

// --- App Registry for Dynamic Grid ---
struct AppItem
{
    AppState id;
    const char *name;
    const char *prefKey; // The key used in flash memory
    bool enabled;        // Is it currently visible?
};

AppItem myApps[] = {
    {APP_FISH_TANK, "Fish Tank", "en_fish", true},
    {APP_LED_CONTROLLER, "LED Controller", "en_led", true},
    {APP_WEATHER, "Live Weather", "en_wea", true},
    {APP_FOOTBALL, "League Table", "en_ftb", true},
    {APP_SYSTEM_STATS, "Sys Monitor", "en_sys", true},
    {APP_SUMMARY_MONITOR, "Global Summary", "en_sum", true},
    {APP_CALIBRATION, "Calibration", "en_cal", true}};
const int numApps = sizeof(myApps) / sizeof(myApps[0]);

void init_web_server()
{
    // Check if device has been set up (has a password)
    preferences.begin("rainbow", false);
    adminPassword = preferences.getString("adminPwd", "");
    String wifiSsid = preferences.getString("ssid", "");
    preferences.end();

    if (adminPassword == "" || wifiSsid == "")
    {
        // --- FIRST BOOT SETUP MODE ---
        WiFi.mode(WIFI_AP);
        WiFi.softAP("CYD-Dashboard-Setup");

        server.on("/", HTTP_GET, []()
                  { server.send(200, "text/html", setupHTML); });

        server.on("/save", HTTP_POST, []()
                  {
            if (server.hasArg("pwd") && server.hasArg("devName")) {
                preferences.begin("rainbow", false);
                preferences.putString("adminPwd", server.arg("pwd"));
                preferences.putString("devName", server.arg("devName"));
                preferences.end();

                server.send(200, "text/plain", "Settings saved! Rebooting...");
                delay(2000);
                ESP.restart();
            } else {
                server.send(400, "text/plain", "Missing form data.");
            } });
    }
    else
    {
        // --- NORMAL / OTA UPDATE MODE ---
        server.on("/", HTTP_GET, []()
                  {
            if (!server.authenticate("admin", adminPassword.c_str())) return server.requestAuthentication();
            // Serve the new combined dashboard
            server.send(200, "text/html", dashboardHTML); });

        // Catch the new App Toggles form submission
        server.on("/updateSettings", HTTP_POST, []()
                  {
            if (!server.authenticate("admin", adminPassword.c_str())) return server.requestAuthentication();
            
            preferences.begin("rainbow", false);
            preferences.putBool("en_fish", server.hasArg("en_fish"));
            preferences.putBool("en_led", server.hasArg("en_led"));
            preferences.putBool("en_wea", server.hasArg("en_wea"));
            preferences.putBool("en_ftb", server.hasArg("en_ftb"));
            preferences.putBool("en_sys", server.hasArg("en_sys"));
            preferences.putBool("en_sum", server.hasArg("en_sum"));
            preferences.end();
            
            server.send(200, "text/plain", "Settings Updated! Rebooting...");
            delay(2000);
        ESP.restart(); });

        server.on("/update", HTTP_POST, []()
                  {
            if (!server.authenticate("admin", adminPassword.c_str())) return server.requestAuthentication();
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "Update Failed." : "Success! Rebooting...");
            delay(2000);
            ESP.restart(); }, []()
                  {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
            } else if (upload.status == UPLOAD_FILE_END) {
                Update.end(true);
            } });
        server.on("/save", HTTP_POST, []()
                  {
            if (server.hasArg("pwd")) {
                preferences.begin("rainbow", false);
                
                // Security & Network
                preferences.putString("adminPwd", server.arg("pwd"));
                preferences.putString("devName", server.arg("devName"));
                if (server.hasArg("ssid")) preferences.putString("ssid", server.arg("ssid"));
                if (server.hasArg("pass")) preferences.putString("pass", server.arg("pass"));
                
                // Football
                if (server.hasArg("team")) preferences.putString("team", server.arg("team"));
                if (server.hasArg("league")) preferences.putString("league", server.arg("league"));
                
                // App Toggles (Checkboxes only send a value if they are checked)
                preferences.putBool("en_fish", server.hasArg("en_fish"));
                preferences.putBool("en_led", server.hasArg("en_led"));
                preferences.putBool("en_wea", server.hasArg("en_wea"));
                preferences.putBool("en_ftb", server.hasArg("en_ftb"));
                preferences.putBool("en_sys", server.hasArg("en_sys"));
                preferences.putBool("en_sum", server.hasArg("en_sum"));

                preferences.end();
                server.send(200, "text/plain", "Settings saved! Rebooting...");
                delay(2000);
                ESP.restart();
            } });
    }

    server.begin();
    Serial.println("Web server started successfully!");
}
// --- Shared Status Bar ---
void drawStatusBar(TFT_eSprite &spr)
{
    if (!showStatusBar)
        return;
    spr.fillRect(0, 0, 320, 20, tft.color565(15, 15, 15));
    spr.drawLine(0, 20, 320, 20, tft.color565(50, 50, 50));

    spr.setTextSize(1);
    spr.setTextColor(TFT_WHITE);
    spr.drawCentreString(currentBrowserTime, 160, 6, 1);

    if (WiFi.status() == WL_CONNECTED)
    {
        spr.setTextColor(tft.color565(0, 255, 0));
        spr.drawString("WIFI", 285, 6, 1);
    }
    else
    {
        spr.setTextColor(tft.color565(150, 150, 150));
        spr.drawString("NO-WIFI", 270, 6, 1);
    }
}

// --- Touch Helper ---
bool get_mapped_touch(int16_t &outX, int16_t &outY)
{
    uint16_t dummyX, dummyY;
    if (!tft.getTouch(&dummyX, &dummyY))
        return false;

    uint16_t touchX, touchY;
    tft.getTouchRaw(&touchX, &touchY);

    uint16_t temp = touchX;
    touchX = touchY;
    touchY = temp;

    outX = map(touchX, live_calData[0], live_calData[1], screenWidth, 0);
    outY = map(touchY, live_calData[2], live_calData[3], 0, screenHeight);

    if (outX < 0)
        outX = 0;
    if (outX > screenWidth)
        outX = screenWidth;
    if (outY < 0)
        outY = 0;
    if (outY > screenHeight)
        outY = screenHeight;
    return true;
}

// --- BLE String Parsing & Callbacks ---
String splitString(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

uint16_t hexToRGB565(String hex)
{
    long number = strtol(hex.c_str(), NULL, 16);
    int r = (number >> 16) & 0xFF;
    int g = (number >> 8) & 0xFF;
    int b = number & 0xFF;
    return tft.color565(r, g, b);
}

void processBleData(String data)
{
    if (data.startsWith("Z|"))
    {
        currentBrowserTime = data.substring(2);
        lastBleDataTime = millis();
        return;
    }

    if (data.startsWith("T|"))
    {
        String title = splitString(data, '|', 1);
        String colorHex = splitString(data, '|', 2);
        String hidden = splitString(data, '|', 3);

        uint16_t titleColor = hexToRGB565(colorHex);
        int hiddenCount = hidden.toInt();
        currentParseIndex = -1;

        for (int i = 0; i < monitorInstances.size(); i++)
        {
            if (monitorInstances[i].name == title)
            {
                currentParseIndex = i;
                monitorInstances[i].color = titleColor;
                monitorInstances[i].hiddenAlerts = hiddenCount;
                monitorInstances[i].updatedThisCycle = true;
                break;
            }
        }

        if (currentParseIndex == -1)
        {
            IrisInstance newInst;
            newInst.name = title;
            newInst.color = titleColor;
            newInst.hiddenAlerts = hiddenCount;
            newInst.updatedThisCycle = true;
            monitorInstances.push_back(newInst);
            currentParseIndex = monitorInstances.size() - 1;
        }
        return;
    }

    if (data.startsWith("C|"))
    {
        String compName = splitString(data, '|', 1);
        int statusCode = splitString(data, '|', 2).toInt();

        if (currentParseIndex != -1)
        {
            bool updated = false;
            for (auto &comp : monitorInstances[currentParseIndex].components)
            {
                if (comp.name == compName)
                {
                    comp.status = statusCode;
                    comp.updatedThisCycle = true;
                    updated = true;
                    break;
                }
            }
            if (!updated)
            {
                IrisComponent newComp;
                newComp.name = compName;
                newComp.status = statusCode;
                newComp.updatedThisCycle = true;
                monitorInstances[currentParseIndex].components.push_back(newComp);
            }
        }
        return;
    }
}

class MyCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            bleBuffer += rxValue.c_str();
        }
    }
};

class MyServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        isBleConnected = true;
        bleBuffer = "";
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 60);
    }
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        isBleConnected = false;
        bleBuffer = "";
        NimBLEDevice::startAdvertising();
    }
};

void startBLEServer()
{
    NimBLEDevice::init(customDeviceName.c_str());
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

// --- Dynamic Brightness Engine ---
int timeToMins(String timeStr)
{
    if (timeStr.length() < 5 || timeStr.indexOf(':') == -1)
        return -1;
    int h = timeStr.substring(0, 2).toInt();
    int m = timeStr.substring(3, 5).toInt();
    return (h * 60) + m;
}

void updateBrightness()
{
    if (!currentWeatherData.valid || currentBrowserTime == "--:--")
    {
        analogWrite(BACKLIGHT_PIN, 255);
        return;
    }
    int currMins = timeToMins(currentBrowserTime);
    int srMins = timeToMins(currentWeatherData.sunrise);
    int ssMins = timeToMins(currentWeatherData.sunset);

    if (currMins < 0 || srMins < 0 || ssMins < 0)
        return;
    int targetB = (currMins > srMins + 60 && currMins < ssMins - 60) ? 255 : 10;
    analogWrite(BACKLIGHT_PIN, targetB);
}

// --- Dynamic Grid Menu Generator for LVGL ---
static void app_button_event_cb(lv_event_t *e)
{
    AppState *targetApp = (AppState *)lv_event_get_user_data(e);
    currentApp = *targetApp;
}

static lv_obj_t *home_flex_container = NULL;

void build_app_menu()
{
    // 1. Destroy the old container if it exists so we can cleanly rebuild it
    if (home_flex_container != NULL)
    {
        lv_obj_del(home_flex_container);
    }

    // 2. Create the scrolling flex container
    home_flex_container = lv_obj_create(ui_ScreenHome);
    lv_obj_set_size(home_flex_container, 300, 180);
    lv_obj_align(home_flex_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(home_flex_container, LV_FLEX_FLOW_ROW_WRAP);

    // 3. Loop through apps and ONLY generate buttons if they are enabled
    for (int i = 0; i < numApps; i++)
    {
        if (myApps[i].enabled)
        {
            lv_obj_t *btn = lv_btn_create(home_flex_container);
            lv_obj_set_size(btn, 135, 45);
            lv_obj_add_event_cb(btn, app_button_event_cb, LV_EVENT_CLICKED, &myApps[i].id);

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, myApps[i].name);
            lv_obj_center(label);
        }
    }

    // 4. Add the permanent "Settings" Button at the end
    lv_obj_t *btn_settings = lv_btn_create(home_flex_container);
    lv_obj_set_size(btn_settings, 135, 45);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x555555), 0); // Make it grey to stand out
    lv_obj_add_event_cb(btn_settings, [](lv_event_t *e)
                        {
                            build_settings_menu(); // Call the settings generator!
                        },
                        LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_settings = lv_label_create(btn_settings);
    lv_label_set_text(label_settings, "Settings");
    lv_obj_center(label_settings);
}

// --- Display & Touch Drivers ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    int16_t x, y;
    if (get_mapped_touch(x, y))
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

static lv_obj_t *settings_screen = NULL;

static void settings_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    AppItem *app = (AppItem *)lv_event_get_user_data(e);

    // Update the struct and save to flash memory
    app->enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    preferences.begin("rainbow", false);
    preferences.putBool(app->prefKey, app->enabled);
    preferences.end();
}

void build_settings_menu()
{
    // Create a new blank screen
    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Toggle Apps");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Scrolling List Container
    lv_obj_t *list = lv_list_create(settings_screen);
    lv_obj_set_size(list, 300, 150);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);

    // Create a toggle for every app in the registry
    for (int i = 0; i < numApps; i++)
    {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 50);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, myApps[i].name);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
        if (myApps[i].enabled)
            lv_obj_add_state(sw, LV_STATE_CHECKED);

        // Attach the save event to the switch
        lv_obj_add_event_cb(sw, settings_switch_cb, LV_EVENT_VALUE_CHANGED, &myApps[i]);
    }

    // Back Button
    lv_obj_t *back_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    // Back button event: Rebuild the home menu and load it!
    lv_obj_add_event_cb(back_btn, [](lv_event_t *e)
                        {
        build_app_menu(); 
        lv_scr_load(ui_ScreenHome); }, LV_EVENT_CLICKED, NULL);

    // Load the settings screen
    lv_scr_load(settings_screen);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    preferences.begin("rainbow", false);
    adminPassword = preferences.getString("adminPwd", "");
    customDeviceName = preferences.getString("devName", "CYD-Dashboard");
    showStatusBar = preferences.getBool("st_bar", true);
    preferences.end();

    rtc_gpio_deinit((gpio_num_t)27);
    pinMode(BACKLIGHT_PIN, OUTPUT);
    analogWrite(BACKLIGHT_PIN, 255);

    tft.begin();
    tft.setRotation(1);
    uint16_t dummyCal[5] = {0, 4095, 0, 4095, 0};
    tft.setTouch(dummyCal);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    build_app_menu(); // Populate the grid automatically
    init_web_server();
    startBLEServer();
}

void loop()
{
    // 1. Process BLE Payloads
    if (bleBuffer.indexOf('#') != -1)
    {
        int endPos = bleBuffer.indexOf('#');
        String command = bleBuffer.substring(0, endPos);
        bleBuffer = bleBuffer.substring(endPos + 1);
        processBleData(command);
    }

    // 2. Real-Time Clock Sync Check
    if (WiFi.status() == WL_CONNECTED && millis() - lastClockUpdate > 10000)
    {
        lastClockUpdate = millis();
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0))
        {
            char timeStr[6];
            strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
            currentBrowserTime = String(timeStr);
            updateBrightness();
        }
    }

    // 3. State Machine Routing
    switch (currentApp)
    {
    case APP_HOME:
        if (lv_scr_act() != ui_ScreenHome)
        {
            lv_disp_load_scr(ui_ScreenHome);
        }
        lv_tick_inc(5);
        lv_timer_handler();
        break;
    case APP_FISH_TANK:
        run_fishtank_app();
        break;
    case APP_LED_CONTROLLER:
        run_led_app();
        break;
    case APP_WEATHER:
        run_weather_app();
        break;
    case APP_FOOTBALL:
        run_football_app();
        break;
    case APP_SYSTEM_STATS:
        run_monitor_app();
        break;
    case APP_SUMMARY_MONITOR:
        run_summary_app();
        break;
    case APP_CALIBRATION:
        run_calibration_app();
        break;
    }
    server.handleClient();
    delay(5);
}