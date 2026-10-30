#include "i18n.h"
#include "portal.h"
#include "portal_page.h"
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <sys/time.h>

void Portal::error(int code, const String& message) {
    JsonDocument document;
    document["error"] = message;
    String body; serializeJson(document, body);
    server_.send(code, "application/json", body);
}

void Portal::sendJson(const JsonDocument& document) {
    String body; serializeJson(document, body);
    server_.send(200, "application/json", body);
}

bool Portal::authorized(bool mutation) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("X-Frame-Options", "DENY");
    if (!active_ || !server_.authenticate("stopwatch", password_.c_str())) {
        server_.requestAuthentication(BASIC_AUTH, "StopWatch", i18n::text("Usa stopwatch e il codice sul display"));
        return false;
    }
    if (mutation && (server_.header("X-StopWatch") != "1" || !server_.header("Content-Type").startsWith("application/json"))) {
        error(403, i18n::text("Richiesta non consentita")); return false;
    }
    if (notes_.busy()) { error(409, i18n::text("Registrazione in corso: riprova dopo il salvataggio")); return false; }
    return true;
}

bool Portal::jsonBody(JsonDocument& document) {
    if (body_.isEmpty() || body_.length() > policy::maxConfigBytes ||
        deserializeJson(document, body_) || !document.is<JsonObject>()) {
        error(400, i18n::text("JSON non valido o troppo grande")); return false;
    }
    return true;
}

void Portal::collectBody() {
    auto& raw = server_.raw();
    if (raw.status == RAW_START) {
        body_ = "";
        server_.client().setTimeout(250);
        if (server_.clientContentLength() <= 0 || size_t(server_.clientContentLength()) > policy::maxConfigBytes) {
            error(413, i18n::text("Richiesta troppo grande"));
            server_.client().stop();
        } else if (!authorized(true)) server_.client().stop();
    } else if (raw.status == RAW_WRITE) {
        if (body_.length() + raw.currentSize > policy::maxConfigBytes ||
            !body_.concat(reinterpret_cast<const char*>(raw.buf), raw.currentSize)) {
            body_ = "";
            server_.client().stop();
        }
    } else if (raw.status == RAW_ABORTED) body_ = "";
}

bool Portal::start() {
    if (active_) return true;
    char suffix[7], password[13];
    snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)(ESP.getEfuseMac() & 0xffffff));
    snprintf(password, sizeof(password), "%08lx%04lx", (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff));
    ssid_ = String("StopWatch-") + suffix;
    password_ = password;
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(ssid_.c_str(), password_.c_str(), 1, false, 1)) return false;
    if (!routesReady_) { routes(); routesReady_ = true; }
    server_.begin();
    started_ = millis();
    active_ = true;
    return true;
}

void Portal::stop() {
    if (!active_) return;
    server_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    active_ = false;
    password_ = "";
}

void Portal::update() {
    if (!active_) return;
    if (uint32_t(millis() - started_) > 600000) { stop(); return; }
    // Keep HTTP work out of the capture path; the phone can retry after recording.
    if (!notes_.busy()) server_.handleClient();
}

String Portal::wifiQr() const {
    return String("WIFI:T:WPA;S:") + ssid_ + ";P:" + password_ + ";;";
}

void Portal::routes() {
    const char* headers[] = {"X-StopWatch", "Content-Type"};
    server_.collectHeaders(headers, 2);
    server_.on("/", HTTP_GET, [this] {
        if (!authorized()) return;
        server_.sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; media-src blob:; frame-ancestors 'none'; form-action 'self'");
        server_.send_P(200, "text/html; charset=utf-8", portalPage);
    });
    server_.on("/api/status", HTTP_GET, [this] {
        if (!authorized()) return;
        JsonDocument data;
        data["wifi"] = WiFi.status() == WL_CONNECTED;
        data["storage_ready"] = notes_.ready();
        data["used"] = notes_.ready() ? LittleFS.usedBytes() : 0;
        data["total"] = notes_.ready() ? LittleFS.totalBytes() : 0;
        sendJson(data);
    });
    server_.on("/api/config", HTTP_GET, [this] {
        if (!authorized()) return;
        JsonDocument data;
        settings_.toJson(data.to<JsonObject>());
        sendJson(data);
    });
    server_.on("/api/config", HTTP_PUT, [this] {
        if (!authorized(true)) return;
        if (!canSave_()) { error(409, i18n::text("Attendi la richiesta Home Assistant")); return; }
        JsonDocument data;
        if (!jsonBody(data)) return;
        Settings next = settings_;
        String reason;
        if (!next.update(data.as<JsonObjectConst>(), reason)) { error(400, reason); return; }
        if (!next.save()) { error(500, i18n::text("Salvataggio non riuscito: configurazione precedente conservata")); return; }
        settings_ = next;
        server_.send(200, "application/json", "{\"saved\":true}");
        onSaved_();
    }, [this] { collectBody(); });
    server_.on("/api/time", HTTP_PUT, [this] {
        if (!authorized(true)) return;
        JsonDocument data;
        if (!jsonBody(data)) return;
        if (!data["epoch"].is<int64_t>()) { error(400, i18n::text("Ora non valida")); return; }
        const int64_t epoch = data["epoch"].as<int64_t>();
        if (epoch < 1704067200 || epoch > 4102444800LL) { error(400, i18n::text("Ora fuori intervallo")); return; }
        timeval now{time_t(epoch), 0};
        settimeofday(&now, nullptr);
        tm utc{}; gmtime_r(&now.tv_sec, &utc);
        if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&utc);
        server_.send(200, "application/json", "{\"saved\":true}");
    }, [this] { collectBody(); });
    server_.on("/api/notes", HTTP_GET, [this] {
        if (!authorized()) return;
        JsonDocument data;
        auto list = data["notes"].to<JsonArray>();
        if (notes_.ready()) {
            File root = LittleFS.open("/notes");
            for (File file = root.openNextFile(); file; file = root.openNextFile()) {
                if (!policy::noteFilename(file.name())) continue;
                auto item = list.add<JsonObject>();
                item["name"] = file.name();
                item["bytes"] = file.size();
                item["seconds"] = (file.size() > 44 ? file.size() - 44 : 0) / float(policy::sampleRate * 2);
            }
        }
        sendJson(data);
    });
    server_.on("/api/note", HTTP_GET, [this] {
        if (!authorized()) return;
        const String name = server_.arg("name"), offsetText = server_.arg("offset");
        if (!policy::noteFilename(name.c_str()) || offsetText.isEmpty() || offsetText.length() > 7) {
            error(400, i18n::text("Nome o posizione non validi")); return;
        }
        for (char c : offsetText) if (c < '0' || c > '9') { error(400, i18n::text("Posizione non valida")); return; }
        File file = notes_.ready() ? LittleFS.open(String("/notes/") + name, "r") : File{};
        const size_t offset = offsetText.toInt();
        if (!file) { error(404, i18n::text("Nota non trovata")); return; }
        if (offset >= file.size() || !file.seek(offset)) { error(416, i18n::text("Posizione fuori dal file")); return; }
        static uint8_t buffer[4096];
        const size_t bytes = file.read(buffer, std::min(sizeof(buffer), file.size() - offset));
        if (!bytes) { error(500, i18n::text("Lettura audio fallita")); return; }
        server_.setContentLength(bytes);
        server_.send(200, "application/octet-stream", "");
        server_.sendContent(reinterpret_cast<const char*>(buffer), bytes);
    });
    server_.on("/api/notes", HTTP_DELETE, [this] {
        if (!authorized(true)) return;
        JsonDocument data;
        if (!jsonBody(data)) return;
        const String name = data["name"] | "";
        if (!policy::noteFilename(name.c_str())) { error(400, i18n::text("Nome nota non valido")); return; }
        if (!notes_.ready() || !LittleFS.remove(String("/notes/") + name)) { error(404, i18n::text("Nota non trovata")); return; }
        server_.send(200, "application/json", "{\"deleted\":true}");
    }, [this] { collectBody(); });
    server_.onNotFound([this] { if (authorized()) error(404, i18n::text("Pagina non trovata")); });
}
