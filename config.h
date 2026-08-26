#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- HARDWARE ---
// CYD 3.2 uses specific pins for touch and backlight
#define BACKLIGHT_PIN 27

// --- STATE MACHINES ---
// We define all our possible apps here so the whole system knows about them
enum AppState { 
    APP_HOME, 
    APP_LED_CONTROLLER, 
    APP_FISH_TANK,
    APP_WEATHER,
    APP_SYSTEM_STATS, 
	APP_FOOTBALL,
	APP_SUMMARY_MONITOR,
	APP_CALIBRATION
};

// --- System Monitor Structures ---
struct IrisComponent {
    String name;
    int status;
    bool updatedThisCycle;
};

struct IrisInstance {
    String name;
    uint16_t color;
    int hiddenAlerts; 
    bool updatedThisCycle;
    std::vector<IrisComponent> components;
};


// --- Add this below WeatherData ---
struct LeagueRow {
    String pos;
    String name;
    String pts;
    bool highlight;
};

// --- SHARED DATA STRUCTURES ---
// These are your blueprints. Any app can use these now!
struct WeatherLoc {
    String name;
    float lat;
    float lon;
};

struct WeatherData {
    float temp;
    float wind;
    int rain;
    int code; 
    String condition;
    String sunrise; 
    String sunset;  
    bool valid = false;
};


#endif