#pragma once

#include "settings.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct HomeResult {
    size_t favorite = 0;
    uint32_t requestedAt = 0;
    bool success = false;
    bool action = false;
    bool viaMqtt = false;
    String entity, state, unit, message;
};

class HomeAssistant {
public:
    bool begin();
    bool request(const Settings& settings, size_t favorite, bool action);
    bool poll(HomeResult& result);
    bool busy() const { return busy_; }

private:
    struct Request {
        String url, token, ca, entity;
        size_t favorite;
        bool action;
        uint32_t requestedAt;
    };
    static void worker(void* context);
    static HomeResult perform(const Request& request);
    QueueHandle_t requests_ = nullptr, results_ = nullptr;
    bool busy_ = false;
};
