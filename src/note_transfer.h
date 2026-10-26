#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <atomic>
#include <esp_https_server.h>
#include "badge.h"

class NoteTransfer {
public:
    explicit NoteTransfer(Badge& badge):badge_(badge) {}
    bool offer(const String& name, JsonObject out);
    bool offerBadge(const String& upload, JsonObject out);
    bool busy();

private:
    bool start();
    bool createIdentity();
    static esp_err_t sendNote(httpd_req_t* request);
    static esp_err_t receiveBadge(httpd_req_t* request);
    bool makeOffer(const String& reference, bool upload, size_t bytes, JsonObject out);
    bool authorize(httpd_req_t* request, bool upload, char* reference, uint32_t& bytes);
    Badge& badge_;
    httpd_handle_t server_ = nullptr;
    std::array<uint8_t, 768> certificate_{};
    std::array<uint8_t, 256> key_{};
    size_t certificateBytes_ = 0, keyBytes_ = 0;
    String encodedCertificate_;
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    char name_[26]{}, ticket_[65]{};
    uint32_t expires_ = 0;
    uint32_t bytes_ = 0;
    bool upload_ = false;
    std::atomic<bool> transferring_{false};
};
