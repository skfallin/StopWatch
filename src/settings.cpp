#include "i18n.h"
#include "settings.h"
#include <Preferences.h>
#include <cstring>

namespace {
bool stringField(JsonObjectConst input, const char* key, String& target, size_t limit, String& error) {
    if (!input[key].is<const char*>()) {
        error = String(i18n::text("Campo non valido: ")) + key;
        return false;
    }
    const JsonString text = input[key].as<JsonString>();
    String value = input[key].as<String>();
    if (value.length() > limit || std::strlen(text.c_str()) != text.size() ||
        value.indexOf('\r') >= 0 || value.indexOf('\n') >= 0) {
        error = String(i18n::text("Campo troppo lungo o non valido: ")) + key;
        return false;
    }
    target = value;
    return true;
}
}

void Settings::toJson(JsonObject out, bool includeSecrets) const {
    out["language"]=language;
    out["time_zone"]=timeZone;
    if(!includeSecrets) {
        auto zones=out["time_zones"].to<JsonArray>();
        for(const auto& zone:regional::zones) zones.add(zone.id);
    }
    auto apps=out["home_apps"].to<JsonArray>();
    for(size_t i=0;i<homeAppCount;++i) apps.add(launcher::ids[homeApps[i]]);
    out["brightness"] = brightness;
    out["volume"] = volume;
    out["note_volume"] = noteVolume;
    out["do_not_disturb"] = doNotDisturb;
    out["sound_pack"] = sfx::packIds[soundPack];
    out["color_theme"] = ui::themes[colorTheme].id;
    out["presentation_page_keys"]=presentationPageKeys;
    out["clock_face"]=clockwork::faceIds[clockFace];
    out["clock_keep_awake"]=clockKeepAwake;
    out["timer_seconds"]=timerSeconds;
    auto badge=out["badge"].to<JsonObject>();
    badge["auto_rotate"]=badgeAutoRotate; badge["animate"]=badgeAnimate; badge["keep_awake"]=badgeKeepAwake;
    out["vibration"] = vibration;
    out["screen_timeout"] = screenTimeout;
    out["ssid"] = ssid;
    out["ha_url"] = haUrl;
    out["ha_ca"] = haCa;
    out["wifi_password_saved"] = !wifiPassword.isEmpty();
    out["ha_token_saved"] = !haToken.isEmpty();
    out["mqtt_host"] = mqttHost;
    out["mqtt_port"] = mqttPort;
    out["mqtt_tls"] = mqttTls;
    out["mqtt_user"] = mqttUser;
    out["mqtt_ca"] = mqttCa;
    out["mqtt_password_saved"] = !mqttPassword.isEmpty();
    out["favorites_mqtt"] = favoritesMqtt;
    if (includeSecrets) {
        out["wifi_password"] = wifiPassword;
        out["ha_token"] = haToken;
        out["mqtt_password"] = mqttPassword;
    }
    auto list = out["favorites"].to<JsonArray>();
    for (size_t i = 0; i < favoriteCount; ++i) {
        auto item = list.add<JsonObject>();
        item["label"] = favorites[i].label;
        item["entity"] = favorites[i].entity;
    }
}

bool Settings::update(JsonObjectConst input, String& error) {
    Settings next = *this;
    if(!input["language"].isUnbound()) {
        if(!stringField(input,"language",next.language,2,error) || !regional::language(next.language.c_str())) {
            error=i18n::text("Lingua non valida");return false;
        }
    }
    if(!input["time_zone"].isUnbound()) {
        if(!stringField(input,"time_zone",next.timeZone,40,error) || !regional::rule(next.timeZone.c_str())) {
            error=i18n::text("Fuso orario non valido");return false;
        }
    }
    const auto number = [&](const char* key, int minimum, int maximum, int& value) {
        if (!input[key].is<int>()) { error = String(i18n::text("Campo non valido: ")) + key; return false; }
        value = input[key].as<int>();
        if (value < minimum || value > maximum) { error = String(i18n::text("Fuori intervallo: ")) + key; return false; }
        return true;
    };
    int brightnessValue, volumeValue, vibrationValue, timeoutValue;
    if (!number("brightness", 16, 255, brightnessValue) || !number("volume", 0, 200, volumeValue) ||
        !number("vibration", 0, 255, vibrationValue) || !number("screen_timeout", 15, 300, timeoutValue)) return false;
    next.brightness = brightnessValue;
    next.volume = volumeValue;
    if(!input["do_not_disturb"].isUnbound()) {
        if(!input["do_not_disturb"].is<bool>()) { error=i18n::text("Modalita non disturbare non valida"); return false; }
        next.doNotDisturb=input["do_not_disturb"].as<bool>();
    }
    if(!input["note_volume"].isUnbound()) {
        int noteVolumeValue;
        if(!number("note_volume",0,200,noteVolumeValue)) return false;
        next.noteVolume=noteVolumeValue;
    }
    next.vibration = vibrationValue;
    next.screenTimeout = timeoutValue;
    if (!input["sound_pack"].isNull()) {
        String pack;
        if (!stringField(input,"sound_pack",pack,16,error)) return false;
        const int index=sfx::packIndex(pack.c_str());
        if (index<0) { error=i18n::text("Pacchetto suoni non valido"); return false; }
        next.soundPack=index;
    }
    if (!input["color_theme"].isNull()) {
        String theme;
        if (!stringField(input,"color_theme",theme,16,error)) return false;
        const int index=ui::themeIndex(theme.c_str());
        if (index<0) { error=i18n::text("Tema colori non valido"); return false; }
        next.colorTheme=index;
    }
    if(!input["presentation_page_keys"].isUnbound()) {
        if(!input["presentation_page_keys"].is<bool>()) { error=i18n::text("Tasti Slide non validi");return false; }
        next.presentationPageKeys=input["presentation_page_keys"].as<bool>();
    }
    if(!input["home_apps"].isUnbound()) {
        if(!input["home_apps"].is<JsonArrayConst>() || input["home_apps"].size()<1 || input["home_apps"].size()>launcher::appCount) { error=i18n::text("Home non valida");return false; }
        std::array<bool,launcher::appCount> seen{};next.homeAppCount=0;
        for(auto item:input["home_apps"].as<JsonArrayConst>()) {
            const bool valid=item.is<const char*>() && std::strlen(item.as<const char*>())==item.as<JsonString>().size();
            const int app=valid?launcher::index(item.as<const char*>()):-1;
            if(app<0 || seen[app]) { error=i18n::text("App Home non valida o duplicata");return false; }
            seen[app]=true;next.homeApps[next.homeAppCount++]=app;
        }
        if(!seen[7]) { error=i18n::text("Sistema deve restare visibile");return false; }
    }
    String ssidValue, password, token;
    if(!input["clock_face"].isUnbound()) {
        String face;
        if(!stringField(input,"clock_face",face,16,error)) return false;
        const int index=clockwork::faceIndex(face.c_str());
        if(index<0) { error=i18n::text("Quadrante non valido");return false; }
        next.clockFace=index;
    }
    if(!input["clock_keep_awake"].isUnbound()) {
        if(!input["clock_keep_awake"].is<bool>()) { error=i18n::text("Display orologio non valido");return false; }
        next.clockKeepAwake=input["clock_keep_awake"].as<bool>();
    }
    if(!input["timer_seconds"].isUnbound()) {
        int seconds;
        if(!number("timer_seconds",1,clockwork::maxTimerSeconds,seconds)) return false;
        next.timerSeconds=seconds;
    }
    if(!input["badge"].isNull()) {
        const auto badge=input["badge"];
        if(!badge["auto_rotate"].is<bool>() || !badge["animate"].is<bool>() || !badge["keep_awake"].is<bool>()) {
            error=i18n::text("Impostazioni Badge non valide"); return false;
        }
        next.badgeAutoRotate=badge["auto_rotate"].as<bool>(); next.badgeAnimate=badge["animate"].as<bool>();
        next.badgeKeepAwake=badge["keep_awake"].as<bool>();
    }
    if (!stringField(input, "ssid", ssidValue, 32, error) ||
        !stringField(input, "wifi_password", password, 63, error) ||
        !stringField(input, "ha_url", next.haUrl, 256, error) ||
        !stringField(input, "ha_token", token, 2048, error)) return false;
    if (ssidValue != ssid || !password.isEmpty()) next.wifiPassword = password;
    next.ssid = ssidValue;
    if (next.ssid.isEmpty()) next.wifiPassword = "";
    if (!next.wifiPassword.isEmpty() && next.wifiPassword.length() < 8) {
        error = i18n::text("La password Wi-Fi deve avere almeno 8 caratteri"); return false;
    }
    if (!token.isEmpty()) next.haToken = token;
    if (!input["ha_ca"].is<const char*>() || input["ha_ca"].as<String>().length() > 4096) {
        error = i18n::text("Certificato CA non valido"); return false;
    }
    next.haCa = input["ha_ca"].as<String>();
    next.haUrl.trim();
    while (next.haUrl.endsWith("/")) next.haUrl.remove(next.haUrl.length() - 1);
    if (!policy::baseUrl(next.haUrl.c_str())) { error = i18n::text("URL Home Assistant non valido"); return false; }
    if (!next.haCa.isEmpty() && (next.haCa.indexOf("-----BEGIN CERTIFICATE-----") < 0 ||
        next.haCa.indexOf("-----END CERTIFICATE-----") < 0)) {
        error = i18n::text("Certificato CA PEM non valido"); return false;
    }
    if (next.haUrl.isEmpty()) next.haToken = "";
    else if (next.haToken.isEmpty()) { error = i18n::text("Inserisci il token Home Assistant"); return false; }
    // Older web clients omit MQTT fields; preserve the existing integration.
    if (input["mqtt_host"].isNull() == false) {
        String mqttPasswordValue;
        int port;
        if (!stringField(input, "mqtt_host", next.mqttHost, 253, error) ||
            !stringField(input, "mqtt_user", next.mqttUser, 128, error) ||
            (!input["mqtt_password"].isNull() && !stringField(input, "mqtt_password", mqttPasswordValue, 256, error)) ||
            !number("mqtt_port", 1, 65535, port)) return false;
        if (!policy::brokerHost(next.mqttHost.c_str()) || !input["mqtt_tls"].is<bool>()) {
            error = i18n::text("Broker MQTT non valido"); return false;
        }
        next.mqttPort = port;
        next.mqttTls = input["mqtt_tls"].as<bool>();
        if (!input["mqtt_ca"].is<const char*>() || input["mqtt_ca"].as<String>().length() > 4096) {
            error = i18n::text("CA MQTT non valida"); return false;
        }
        next.mqttCa = input["mqtt_ca"].as<String>();
        if (!next.mqttCa.isEmpty() && (next.mqttCa.indexOf("-----BEGIN CERTIFICATE-----") < 0 ||
            next.mqttCa.indexOf("-----END CERTIFICATE-----") < 0)) {
            error = i18n::text("CA MQTT PEM non valida"); return false;
        }
        if (next.mqttHost != mqttHost || next.mqttUser != mqttUser || !mqttPasswordValue.isEmpty())
            next.mqttPassword = mqttPasswordValue;
        if (next.mqttHost.isEmpty()) next.mqttPassword = "";
    }
    if (!input["favorites"].is<JsonArrayConst>() || !next.updateFavorites(input["favorites"].as<JsonArrayConst>(), error)) return false;
    bool changed = next.favoriteCount != favoriteCount;
    for (size_t i = 0; i < next.favoriteCount && !changed; ++i)
        changed = next.favorites[i].entity != favorites[i].entity || next.favorites[i].label != favorites[i].label;
    if (!input["favorites_mqtt"].isNull()) {
        if (!input["favorites_mqtt"].is<bool>()) { error = i18n::text("Modalita preferiti non valida"); return false; }
        next.favoritesMqtt = input["favorites_mqtt"].as<bool>();
    } else if (changed) next.favoritesMqtt = false;
    *this = next;
    return true;
}

bool Settings::updateFavorites(JsonArrayConst input, String& error, bool automaticNames) {
    if (input.size() > policy::maxFavorites) { error = i18n::text("Sono consentiti al massimo 8 preferiti"); return false; }
    std::array<Favorite, policy::maxFavorites> next{};
    size_t count = 0;
    for (auto item : input) {
        if (!item.is<JsonObjectConst>()) { error = i18n::text("Preferito non valido"); return false; }
        Favorite favorite;
        if (!stringField(item.as<JsonObjectConst>(), "label", favorite.label, automaticNames ? 128 : 32, error) ||
            !stringField(item.as<JsonObjectConst>(), "entity", favorite.entity, 96, error)) return false;
        favorite.label.trim();
        if (automaticNames) {
            const auto label = policy::utf8Prefix(favorite.label.c_str(), 32);
            favorite.label = String(label.data(), label.size());
        }
        if (favorite.label.isEmpty() || !policy::entityId(favorite.entity.c_str())) {
            error = i18n::text("Ogni preferito richiede nome e entity_id validi"); return false;
        }
        next[count++] = favorite;
    }
    favorites = next;
    favoriteCount = count;
    return true;
}

bool Settings::save() const {
    JsonDocument document;
    toJson(document.to<JsonObject>(), true);
    String json;
    serializeJson(document, json);
    if (json.length() > policy::maxConfigBytes) return false;
    Preferences preferences;
    if (!preferences.begin("stopwatch", false)) return false;
    const bool ok = preferences.putString("config", json) == json.length();
    preferences.end();
    return ok;
}

bool Settings::load() {
    Preferences preferences;
    if (!preferences.begin("stopwatch", false)) return false;
    const String json = preferences.getString("config", "");
    preferences.end();
    if (json.isEmpty()) return true;
    if (json.length() > policy::maxConfigBytes) return false;
    JsonDocument document;
    if (deserializeJson(document, json) || !document.is<JsonObject>()) return false;
    // Existing installations used Italian and Rome before regional settings existed.
    if(document["language"].isUnbound()) document["language"]="it";
    if(document["time_zone"].isUnbound()) document["time_zone"]="Europe/Rome";
    if(document["note_volume"].isUnbound()) document["note_volume"]=document["volume"];
    String error;
    return update(document.as<JsonObjectConst>(), error);
}
