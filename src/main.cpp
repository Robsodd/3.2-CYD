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

lv_obj_t *global_bar_menu = NULL;
lv_obj_t *global_home_btn = NULL;
bool menu_visible = true;
bool fishtank_initialized = false;

// --- Screen & Touch Config ---
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
int live_calData[4] = {1180, 2880, 160, 5100};

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;

// --- System Variables ---
WebServer server(80);
Preferences preferences;
AppState currentApp = APP_HOME;

lv_obj_t *top_clock_label = NULL;

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

void build_settings_menu();

// --- App Registry for Dynamic Grid ---
struct AppItem
{
  AppState id;
  const char *name;
  const char *prefKey; // The key used in flash memory
  bool enabled;        // Is it currently visible?
  void (*init)();      // The new function pointer for boot initialization
};

AppItem myApps[] = {
    {APP_FISH_TANK, "Fish Tank", "en_fish", true, nullptr},
    {APP_LED_CONTROLLER, "LED Controller", "en_led", true, nullptr},
    {APP_WEATHER, "Live Weather", "en_wea", true, nullptr},
    {APP_FOOTBALL, "League Table", "en_ftb", true, init_football_app},
    {APP_SYSTEM_STATS, "Sys Monitor", "en_sys", true, nullptr},
    {APP_SUMMARY_MONITOR, "Global Summary", "en_sum", true, nullptr},
    {APP_CALIBRATION, "Calibration", "en_cal", true, nullptr}};
const int numApps = sizeof(myApps) / sizeof(myApps[0]);

// Callback to animate the Y position
void anim_y_cb(void *var, int32_t v)
{
  lv_obj_set_y((lv_obj_t *)var, v);
}

// Slide out (Up)
void slide_menu_out()
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, global_bar_menu);
  lv_anim_set_values(&a, 0, -60); // Slide up out of bounds
  lv_anim_set_time(&a, 400);      // 400ms duration
  lv_anim_set_exec_cb(&a, anim_y_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_start(&a);
}

// Slide in (Down)
void slide_menu_in()
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, global_bar_menu);
  lv_anim_set_values(&a, -60, 0); // Slide back down to 0 offset
  lv_anim_set_time(&a, 400);
  lv_anim_set_exec_cb(&a, anim_y_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

void init_web_server()
{
  preferences.begin("rainbow", false);
  adminPassword = preferences.getString("adminPwd", "");
  String wifiSsid = preferences.getString("ssid", "");
  String wifiPass = preferences.getString("pass", "");
  preferences.end();

  if (adminPassword == "" || wifiSsid == "")
  {
    // --- FIRST BOOT SETUP MODE ---
    Serial.println("Entering Setup Mode (AP)");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CYD-Dashboard-Setup");

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", setupHTML); });

    server.on("/save", HTTP_POST, []()
              {
            if (server.hasArg("pwd")) {
                preferences.begin("rainbow", false);
                preferences.putString("adminPwd", server.arg("pwd"));
                preferences.putString("devName", server.arg("devName"));
                if (server.hasArg("ssid")) preferences.putString("ssid", server.arg("ssid"));
                if (server.hasArg("pass")) preferences.putString("pass", server.arg("pass"));
                
                // Save Football Settings
                if (server.hasArg("team") && server.arg("team") != "") {
                    preferences.putString("favTeam", server.arg("team"));
                    preferences.putString("tableData", ""); // Clear cache
                }
                if (server.hasArg("league")) {
                    preferences.putString("favLeague", server.arg("league"));
                    preferences.putString("tableData", ""); // Clear cache
                }

                // Save Toggles
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
            } else {
                server.send(400, "text/plain", "Missing form data.");
            } });
  }
  else
  {
    // --- NORMAL BOOT: CONNECT TO HOME WI-FI ---
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(wifiSsid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

    // Wait up to 10 seconds for a connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nConnected to Wi-Fi!");
      Serial.print("IMPORTANT - Your CYD IP Address is: ");
      Serial.println(WiFi.localIP());

      // Sync the time now that we have internet!
      configTzTime("GMT0BST,M3.5.0/1,M10.5.0", "pool.ntp.org");
    }
    else
    {
      Serial.println("\nFailed to connect to Wi-Fi. It will keep trying in the background.");
    }

    // --- NORMAL / OTA UPDATE MODE ---
    server.on("/", HTTP_GET, []()
              {
            if (!server.authenticate("admin", adminPassword.c_str())) return server.requestAuthentication();
            server.send(200, "text/html", dashboardHTML); });

    server.on("/updateSettings", HTTP_POST, []()
              {
            if (!server.authenticate("admin", adminPassword.c_str())) return server.requestAuthentication();
            
            preferences.begin("rainbow", false);
            
            // Save Football Settings from everyday dashboard
            if (server.hasArg("team") && server.arg("team") != "") {
                preferences.putString("favTeam", server.arg("team"));
                preferences.putString("tableData", ""); // Clear cache
            }
            if (server.hasArg("league")) {
                preferences.putString("favLeague", server.arg("league"));
                preferences.putString("tableData", ""); // Clear cache
            }

            // Save Toggles
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

// ==========================================
// THE C-BRIDGE CAGE (DO NOT REMOVE WRAPPER)
// ==========================================
extern "C"
{
  // --- X-AXIS MINIMUM (Left side) ---
  void x_min_up(lv_event_t *e)
  {
    live_calData[0] += 50;
    lv_label_set_text_fmt(ui_valueXmin, "%d", live_calData[0]);
  }
  void x_min_down(lv_event_t *e)
  {
    live_calData[0] -= 50;
    lv_label_set_text_fmt(ui_valueXmin, "%d", live_calData[0]);
  }

  // --- X-AXIS MAXIMUM (Right side) ---
  void x_max_up(lv_event_t *e)
  {
    live_calData[1] += 50;
    lv_label_set_text_fmt(ui_valueXMax, "%d", live_calData[1]);
  }
  void x_max_down(lv_event_t *e)
  {
    live_calData[1] -= 50;
    lv_label_set_text_fmt(ui_valueXMax, "%d", live_calData[1]);
  }

  // --- Y-AXIS MINIMUM (Top side) ---
  void y_min_up(lv_event_t *e)
  {
    live_calData[2] += 50;
    lv_label_set_text_fmt(ui_valueXmin1, "%d", live_calData[2]);
  }
  void y_min_down(lv_event_t *e)
  {
    live_calData[2] -= 50;
    lv_label_set_text_fmt(ui_valueXmin1, "%d", live_calData[2]);
  }

  // --- Y-AXIS MAXIMUM (Bottom side) ---
  void y_max_up(lv_event_t *e)
  {
    live_calData[3] += 50;
    lv_label_set_text_fmt(ui_valueYMax, "%d", live_calData[3]);
  }
  void y_max_down(lv_event_t *e)
  {
    live_calData[3] -= 50;
    lv_label_set_text_fmt(ui_valueYMax, "%d", live_calData[3]);
  }

  // --- SAVE AND EXIT BUTTON ---
  void save_calibration_btn(lv_event_t *e)
  {
    preferences.begin("rainbow", false);
    preferences.putBool("is_cal", true);
    preferences.putInt("cal_x0", live_calData[0]);
    preferences.putInt("cal_x1", live_calData[1]);
    preferences.putInt("cal_y0", live_calData[2]);
    preferences.putInt("cal_y1", live_calData[3]);
    preferences.end();

    Serial.println("Calibration Saved to Flash Memory!");

    currentApp = APP_HOME;
    lv_disp_load_scr(ui_ScreenHome);
  }

  // --- FORCE RECALIBRATE (From Home Screen) ---
  void force_recalibrate(lv_event_t *e)
  {
    currentApp = APP_CALIBRATION;
    lv_disp_load_scr(ui_Calibrate);
  }
} // <--- End of the C-Bridge Cage

static lv_obj_t *home_flex_container = NULL;

void build_app_menu()
{
  // 1. Destroy the old container if it exists so we can cleanly rebuild it
  if (home_flex_container != NULL)
  {
    lv_obj_del(home_flex_container);
  }

  // 2. Create the container on your SquareLine home screen
  home_flex_container = lv_obj_create(ui_ScreenHome);

  // --- THE FULL SCREEN FIX ---
  // Make it 100% width and height
  lv_obj_set_size(home_flex_container, lv_pct(100), lv_pct(100));
  lv_obj_align(home_flex_container, LV_ALIGN_CENTER, 0, 0);

  // Remove the "popup" styling (borders, rounded corners) and set a solid dark background
  lv_obj_set_style_border_width(home_flex_container, 0, 0);
  lv_obj_set_style_radius(home_flex_container, 0, 0);
  lv_obj_set_style_bg_color(home_flex_container, lv_color_hex(0x121212), 0);

  // Center the buttons nicely within the full screen
  lv_obj_set_flex_flow(home_flex_container, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(home_flex_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  // ---------------------------

  // 3. Loop through apps and ONLY generate buttons if they are enabled
  for (int i = 0; i < numApps; i++)
  {
    if (myApps[i].enabled)
    {
      lv_obj_t *btn = lv_btn_create(home_flex_container);
      lv_obj_set_size(btn, 135, 45); // Keep button sizes the same
      lv_obj_add_event_cb(btn, app_button_event_cb, LV_EVENT_CLICKED, &myApps[i].id);

      lv_obj_t *label = lv_label_create(btn);
      lv_label_set_text(label, myApps[i].name);
      lv_obj_center(label);
    }
  }

  // 4. Add the permanent "Settings" Button at the end
  lv_obj_t *btn_settings = lv_btn_create(home_flex_container);
  lv_obj_set_size(btn_settings, 135, 45);
  lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x555555), 0);
  lv_obj_add_event_cb(btn_settings, [](lv_event_t *e)
                      { build_settings_menu(); }, LV_EVENT_CLICKED, NULL);

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

void menubar()
{
  // 1. Shift all existing Home screen apps down by 40 pixels so they aren't covered
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(ui_ScreenHome); i++)
  {
    lv_obj_t *child = lv_obj_get_child(ui_ScreenHome, i);
    lv_obj_set_y(child, lv_obj_get_y(child) + 40);
  }

  // 2. Create the bar on the top layer
  global_bar_menu = ui_barMenu_create(lv_layer_top());
  lv_obj_align(global_bar_menu, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(global_bar_menu, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_bg_opa(global_bar_menu, 220, 0);
  lv_obj_set_style_border_width(global_bar_menu, 0, 0);

  // 3. Setup the Home Button
  global_home_btn = ui_comp_get_child(global_bar_menu, UI_COMP_BARMENU_BTNHOME1);
  lv_obj_set_style_bg_color(global_home_btn, lv_color_hex(0x2d2d2d), 0);
  lv_obj_set_style_border_width(global_home_btn, 0, 0);
  lv_obj_set_style_radius(global_home_btn, 8, 0);

  lv_obj_t *home_lbl = ui_comp_get_child(global_bar_menu, UI_COMP_BARMENU_BTNHOME1_LABEL2);
  lv_obj_set_style_text_color(home_lbl, lv_color_hex(0xFFFFFF), 0);

  // Go Home on click and force the menu to snap back to visible
  lv_obj_add_event_cb(global_home_btn, [](lv_event_t *e)
                      {
                        if (currentApp == APP_FISH_TANK)
                        {
                          deinit_fish_app(); // Delete tank memory
                        }
                        currentApp = APP_HOME;
                        fishtank_initialized = false;    // Reset trigger flag
                        lv_disp_load_scr(ui_ScreenHome); // Load SquareLine home screen
                      },
                      LV_EVENT_CLICKED, NULL);

  // 4. Setup the Clock
  top_clock_label = ui_comp_get_child(global_bar_menu, UI_COMP_BARMENU_CONTAINER1_DATETIME);
  lv_obj_set_style_text_color(top_clock_label, lv_color_hex(0x4da6ff), 0);
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  preferences.begin("rainbow", false);
  adminPassword = preferences.getString("adminPwd", "");
  customDeviceName = preferences.getString("devName", "CYD-Dashboard");
  showStatusBar = preferences.getBool("st_bar", true);
  String wifiSsid = preferences.getString("ssid", "");
  String wifiPass = preferences.getString("pass", "");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  // Wait up to 10 seconds for a connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IMPORTANT - Your CYD IP Address is: ");
    Serial.println(WiFi.localIP());

    // Sync the time now that we have internet!
    configTzTime("GMT0BST,M3.5.0/1,M10.5.0", "pool.ntp.org");
  }
  else
  {
    Serial.println("\nFailed to connect to Wi-Fi. It will keep trying in the background.");
  }

  // Load calibration flag
  bool isCalibrated = preferences.getBool("is_cal", false);

  // Load the saved calibration points (using your default array as fallbacks)
  live_calData[0] = preferences.getInt("cal_x0", 1180);
  live_calData[1] = preferences.getInt("cal_x1", 2880);
  live_calData[2] = preferences.getInt("cal_y0", 160);
  live_calData[3] = preferences.getInt("cal_y1", 5100);

  preferences.end();

  if (!isCalibrated)
  {
    currentApp = APP_CALIBRATION;
  }
  else
  {
    currentApp = APP_HOME;
  }

  rtc_gpio_deinit((gpio_num_t)27);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, 255);

  tft.begin();
  tft.setRotation(1);
  uint16_t dummyCal[5] = {0, 4095, 0, 4095, 0};
  tft.setTouch(dummyCal);

  lv_init();
  buf = (lv_color_t *)malloc((screenWidth * screenHeight / 10) * sizeof(lv_color_t));
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
  lv_indev_t *indev_pointer = lv_indev_drv_register(&indev_drv);

  // --- Add a Persistent Cursor ---
  lv_obj_t *cursor_obj = lv_obj_create(lv_layer_sys());
  lv_obj_set_size(cursor_obj, 15, 15);
  lv_obj_set_style_radius(cursor_obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cursor_obj, lv_color_hex(0xFF2222), 0);
  lv_obj_set_style_bg_opa(cursor_obj, LV_OPA_70, 0);
  lv_obj_set_style_border_width(cursor_obj, 2, 0);
  lv_obj_set_style_border_color(cursor_obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_clear_flag(cursor_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_indev_set_cursor(indev_pointer, cursor_obj);

  Serial.println("Running App Boot Sequences...");

  for (int i = 0; i < numApps; i++)
  {
    // Only run the init function if the app is enabled AND an init function exists
    if (myApps[i].enabled && myApps[i].init != nullptr)
    {
      myApps[i].init();
    }
  }
  Serial.println("App boot sequences complete. Starting GUI...");

  ui_init();

  build_app_menu(); // Populate the grid automatically
  menubar();        // Add the top bar with clock and home button
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
      if (top_clock_label != NULL)
      {
        lv_label_set_text(top_clock_label, currentBrowserTime.c_str());
      }
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
    break; // Removed the ticks from here!

  case APP_FISH_TANK:
    if (!fishtank_initialized)
    {
      init_fish_app(currentBrowserTime);
      fishtank_initialized = true;
    }
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
  uint32_t inactive_time = lv_disp_get_inactive_time(NULL);

  if (currentApp == APP_HOME)
  {
    // 1. HOME SCREEN: Hide the home button and lock the bar open
    lv_obj_add_flag(global_home_btn, LV_OBJ_FLAG_HIDDEN);

    if (!menu_visible)
    {
      lv_obj_set_y(global_bar_menu, 0); // Snap it back if it was hidden in an app
      menu_visible = true;
    }
  }
  else
  {
    // 2. INSIDE AN APP: Show the home button and run the auto-hide timer
    lv_obj_clear_flag(global_home_btn, LV_OBJ_FLAG_HIDDEN);

    if (menu_visible && inactive_time > 10000)
    {
      slide_menu_out();
      menu_visible = false;
    }
    else if (!menu_visible && inactive_time < 10000)
    {
      slide_menu_in();
      menu_visible = true;
    }
  }

  server.handleClient();

  // --- LVGL ENGINE SECURED AT THE BOTTOM ---
  // This ensures your screen ALWAYS processes touches and animations
  // no matter which app is currently open!
  lv_tick_inc(5);
  lv_timer_handler();

  delay(5);
}