#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include "policy.h"
#include "regional_settings.h"
#include "ui_sounds.h"
#include "ui_theme.h"
#include "clock_model.h"
#include "home_layout.h"

struct Favorite {
    String label;
    String entity;
};

struct Settings {
    String language="en", timeZone="UTC";
    std::array<int,launcher::appCount> homeApps=launcher::defaultOrder;
    size_t homeAppCount=launcher::appCount;
    uint8_t brightness = 160;
    uint8_t volume = 80;
    uint8_t noteVolume = 80;
    bool doNotDisturb = false;
    uint8_t soundPack = 1;
    uint8_t colorTheme = 0;
    bool badgeAutoRotate = true, badgeAnimate = true, badgeKeepAwake = false;
    bool presentationPageKeys=false;
    uint8_t clockFace=0;
    bool clockKeepAwake=false;
    uint32_t timerSeconds=300;
    uint8_t vibration = 120;
    uint16_t screenTimeout = 45;
    String ssid, wifiPassword;
    String haUrl, haToken, haCa;
    String mqttHost, mqttUser, mqttPassword, mqttCa;
    uint16_t mqttPort = 1883;
    bool mqttTls = false;
    std::array<Favorite, policy::maxFavorites> favorites{};
    size_t favoriteCount = 0;
    bool favoritesMqtt = false;

    bool load();
    bool save() const;
    bool update(JsonObjectConst input, String& error);
    bool updateFavorites(JsonArrayConst input, String& error, bool automaticNames = false);
    void toJson(JsonObject out, bool includeSecrets = false) const;
};
