#pragma once

#include "settings.h"
#include "voice_notes.h"
#include <WebServer.h>

class Portal {
public:
    Portal(Settings& settings, VoiceNotes& notes, bool (*canSave)(), void (*onSaved)())
        : settings_(settings), notes_(notes), canSave_(canSave), onSaved_(onSaved) {}
    bool start();
    void stop();
    void update();
    bool active() const { return active_; }
    const String& ssid() const { return ssid_; }
    const String& password() const { return password_; }
    String wifiQr() const;
    String url() const { return "http://192.168.4.1/"; }

private:
    void routes();
    bool authorized(bool mutation = false);
    bool jsonBody(JsonDocument& document);
    void collectBody();
    void error(int code, const String& message);
    void sendJson(const JsonDocument& document);
    Settings& settings_;
    VoiceNotes& notes_;
    bool (*canSave_)();
    void (*onSaved_)();
    WebServer server_{80};
    bool active_ = false, routesReady_ = false;
    uint32_t started_ = 0;
    String ssid_, password_, body_;
};
