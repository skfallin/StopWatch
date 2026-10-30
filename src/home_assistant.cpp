#include "i18n.h"
#include "home_assistant.h"
#include "light_control.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <memory>
#include <new>
#include <time.h>

extern const uint8_t builtinCaStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t builtinCaEnd[] asm("_binary_x509_crt_bundle_end");

bool HomeAssistant::begin() {
    requests_ = xQueueCreate(1, sizeof(Request*));
    results_ = xQueueCreate(1, sizeof(HomeResult*));
    if (requests_ && results_ && xTaskCreate(worker, "home-assistant", 10240, this, 1, nullptr) == pdPASS) return true;
    if (requests_) vQueueDelete(requests_);
    if (results_) vQueueDelete(results_);
    requests_ = results_ = nullptr;
    return false;
}

bool HomeAssistant::request(const Settings& settings, size_t favorite, bool action,int brightness) {
    if (busy_ || !requests_ || favorite >= settings.favoriteCount) return false;
    if(brightness< -1 || brightness>100 || (brightness>=0 && (!action || !settings.favorites[favorite].entity.startsWith("light.")))) return false;
    auto* job = new (std::nothrow) Request{settings.haUrl, settings.haToken, settings.haCa,
                                         settings.favorites[favorite].entity, favorite, action, millis(),brightness};
    if (!job) return false;
    if (xQueueSend(requests_, &job, 0) != pdTRUE) { delete job; return false; }
    busy_ = true;
    return true;
}

bool HomeAssistant::poll(HomeResult& result) {
    HomeResult* completed;
    if (!results_ || xQueueReceive(results_, &completed, 0) != pdTRUE) return false;
    result = *completed;
    delete completed;
    busy_ = false;
    return true;
}

void HomeAssistant::worker(void* context) {
    auto& self = *static_cast<HomeAssistant*>(context);
    for (;;) {
        Request* job;
        if (xQueueReceive(self.requests_, &job, portMAX_DELAY) != pdTRUE) continue;
        auto* result = new HomeResult(perform(*job));
        delete job;
        xQueueSend(self.results_, &result, portMAX_DELAY);
    }
}

HomeResult HomeAssistant::perform(const Request& request) {
    HomeResult result;
    result.favorite = request.favorite;
    result.action = request.action;
    result.entity = request.entity;
    result.requestedAt = request.requestedAt;
    if (WiFi.status() != WL_CONNECTED) { result.message = i18n::text("Wi-Fi non connesso"); return result; }
    if (request.url.isEmpty() || request.token.isEmpty()) { result.message = i18n::text("Configura Home Assistant dal telefono"); return result; }
    if (!policy::baseUrl(request.url.c_str()) || !policy::entityId(request.entity.c_str())) {
        result.message = i18n::text("Configurazione Home Assistant non valida"); return result;
    }
    WiFiClient plain;
    WiFiClientSecure secure;
    WiFiClient* transport = &plain;
    if (request.url.startsWith("https://")) {
        if (time(nullptr) < 1704067200) {
            result.message = i18n::text("HTTPS richiede l'orologio sincronizzato"); return result;
        }
        if (request.ca.isEmpty()) secure.setCACertBundle(builtinCaStart, builtinCaEnd - builtinCaStart);
        else secure.setCACert(request.ca.c_str());
        secure.setHandshakeTimeout(4);
        transport = &secure;
    }
    const auto configure = [&](HTTPClient& http, const String& path) {
        http.setConnectTimeout(2000);
        http.setTimeout(3000);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.useHTTP10(true);
        if (!http.begin(*transport, request.url + path)) return false;
        http.addHeader("Authorization", String("Bearer ") + request.token);
        return true;
    };
    if (request.action) {
        const auto service = request.brightness>=0?std::string_view(request.brightness?"turn_on":"turn_off"):policy::service(request.entity.c_str());
        if (service.empty()) { result.message = i18n::text("Entita in sola lettura"); return result; }
        HTTPClient http;
        const String domain = request.entity.substring(0, request.entity.indexOf('.'));
        if (!configure(http, String("/api/services/") + domain + "/" + String(service.data(), service.size()))) {
            result.message = i18n::text("Connessione Home Assistant fallita"); return result;
        }
        http.addHeader("Content-Type", "application/json");
        JsonDocument data;
        data["entity_id"] = request.entity;
        if(request.brightness>0) data["brightness_pct"]=request.brightness;
        String body;
        serializeJson(data, body);
        const int status = http.POST(body);
        http.end();
        if (status < 0) {
            // A timeout can happen after HA executed a toggle. Never retry it automatically.
            result.message = i18n::text("Esito incerto: verifica in Home Assistant"); return result;
        }
        if (status != 200 && status != 201) {
            result.message = String("Home Assistant: HTTP ") + status; return result;
        }
    }
    HTTPClient http;
    if (!configure(http, String("/api/states/") + request.entity)) {
        result.message = request.action ? i18n::text("Comando inviato, stato non disponibile") : i18n::text("Connessione fallita");
        return result;
    }
    const int status = http.GET();
    if (status != 200 || http.getSize() > 16384) {
        result.message = request.action ? i18n::text("Comando inviato, aggiorna lo stato") : String(i18n::text("Lettura fallita: HTTP ")) + status;
        http.end(); return result;
    }
    String body;
    body.reserve(2048);
    auto* stream = http.getStreamPtr();
    const uint32_t started = millis();
    const int expected = http.getSize();
    while ((http.connected() || stream->available()) && uint32_t(millis() - started) < 3500) {
        while (stream->available() && body.length() < 16384) body += char(stream->read());
        if (body.length() >= 16384 || (expected >= 0 && body.length() >= size_t(expected))) break;
        delay(1);
    }
    http.end();
    JsonDocument filter;
    filter["entity_id"] = true;
    filter["state"] = true;
    filter["attributes"]["unit_of_measurement"] = true;
    filter["attributes"]["brightness"] = true;
    filter["attributes"]["supported_color_modes"] = true;
    JsonDocument data;
    if (body.length() >= 16384 || deserializeJson(data, body, DeserializationOption::Filter(filter)) ||
        !data["state"].is<const char*>() || data["entity_id"].as<String>()!=request.entity) {
        result.message = request.action ? i18n::text("Comando inviato, risposta non valida") : i18n::text("Risposta Home Assistant non valida");
        return result;
    }
    result.state = data["state"].as<String>().substring(0, 64);
    result.unit = data["attributes"]["unit_of_measurement"].as<String>().substring(0, 16);
    if(request.entity.startsWith("light.")) {
        for(auto mode:data["attributes"]["supported_color_modes"].as<JsonArrayConst>())
            if(mode.is<const char*>() && casa::brightnessMode(mode.as<const char*>())) result.dimmable=true;
        if(result.dimmable && data["attributes"]["brightness"].is<int>()) result.brightness=casa::brightnessPercent(data["attributes"]["brightness"].as<int>());
    }
    result.success = true;
    result.message = request.action ? i18n::text("Comando inviato") : i18n::text("Stato aggiornato");
    return result;
}
