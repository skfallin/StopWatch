#include "mqtt_bridge.h"
#include "firmware.h"
#include "companion.h"
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <time.h>
#include <memory>
#include <new>

void MqttBridge::begin() {
    char identity[24];
    snprintf(identity, sizeof(identity), "stopwatch_%012llx", ESP.getEfuseMac() & 0xffffffffffffULL);
    id_ = identity;
    base_ = String("stopwatch/") + id_;
    commands_ = xQueueCreate(8, sizeof(Command));
    messages_ = xQueueCreate(4, sizeof(Message*));
    favoriteRequest_ = esp_random();
    musicRequest_=esp_random();
    configure();
}

void MqttBridge::configure() {
    if (host_ == settings_.mqttHost && port_ == settings_.mqttPort && tls_ == settings_.mqttTls &&
        user_ == settings_.mqttUser && password_ == settings_.mqttPassword && ca_ == settings_.mqttCa) return;
    reconfigure_ = true;
}

bool MqttBridge::publish(const String& topic, const String& body, bool retain, int qos) {
    return client_ && connected() && esp_mqtt_client_enqueue(client_, topic.c_str(), body.c_str(), body.length(), qos, retain, true) >= 0;
}

bool MqttBridge::available() const { return connected() || (relay_ && relay_->relayReady()); }

bool MqttBridge::dispatch(phone::Kind kind,const String& body,bool& viaRelay) {
    viaRelay=!connected();
    if(viaRelay) return relay_ && relay_->queueRelay(kind,body);
    return publish(base_+"/"+phone::name(kind)+"/request",body,false,0);
}

void MqttBridge::event(void* context, esp_event_base_t, int32_t id, void* data) {
    auto& self = *static_cast<MqttBridge*>(context);
    auto& message = *static_cast<esp_mqtt_event_handle_t>(data);
    if (id == MQTT_EVENT_CONNECTED) {
        self.connected_ = true;
        esp_mqtt_client_subscribe(self.client_, (self.base_ + "/set/+").c_str(), 0);
        esp_mqtt_client_subscribe(self.client_, "homeassistant/status", 0);
        esp_mqtt_client_subscribe(self.client_, (self.base_ + "/favorites/config").c_str(), 1);
        esp_mqtt_client_subscribe(self.client_, (self.base_ + "/favorite/result").c_str(), 0);
        esp_mqtt_client_subscribe(self.client_, (self.base_ + "/music/result").c_str(), 0);
        self.discover_ = true;
    } else if (id == MQTT_EVENT_DISCONNECTED || id == MQTT_EVENT_ERROR) self.connected_ = false;
    else if (id == MQTT_EVENT_DATA) {
        if (message.current_data_offset || message.data_len != message.total_data_len || message.data_len < 0 ||
            size_t(message.data_len) > maxMessageBytes || !message.topic) return;
        const String topic(message.topic, message.topic_len);
        const bool configuration = topic == self.base_ + "/favorites/config";
        const bool music=topic==self.base_+"/music/result";
        if (configuration || topic == self.base_ + "/favorite/result" || music) {
            if (!self.messages_ || (!configuration && (message.retain || message.dup))) return;
            if(!music && size_t(message.data_len)>policy::maxFavoriteMessageBytes) return;
            auto* incoming = new (std::nothrow) Message{};
            if (!incoming) return;
            incoming->kind = music?MessageKind::Music:configuration?MessageKind::Favorites:MessageKind::FavoriteResult;
            incoming->length = message.data_len;
            memcpy(incoming->body, message.data, message.data_len);
            if (xQueueSend(self.messages_, &incoming, 0) != pdTRUE) delete incoming;
            return;
        }
        if (message.data_len > 16) return;
        const String value(message.data, message.data_len);
        if (strlen(value.c_str()) != size_t(message.data_len)) return;
        if (topic == "homeassistant/status" && value == "online") { self.discover_ = true; return; }
        // Commands are momentary: never execute retained or replayed commands after reconnect.
        if (message.retain || message.dup || !topic.startsWith(self.base_ + "/set/")) return;
        const String key = topic.substring(self.base_.length() + 5);
        if (key.isEmpty() || key.length() >= sizeof(Command::key)) return;
        Command command{};
        key.toCharArray(command.key, sizeof(command.key));
        value.toCharArray(command.value, sizeof(command.value));
        xQueueSend(self.commands_, &command, 0);
    }
}

void MqttBridge::discovery() {
    struct Entity { const char* domain; const char* key; const char* name; const char* icon; const char* deviceClass; const char* unit; int minimum, maximum; };
    const Entity entities[] = {
        {"sensor", "battery", "Batteria", "mdi:battery", "battery", "%", 0, 0},
        {"sensor", "rssi", "Segnale Wi-Fi", "mdi:wifi", "signal_strength", "dBm", 0, 0},
        {"sensor", "notes", "Note vocali", "mdi:microphone", "", "", 0, 0},
        {"binary_sensor", "charging", "In carica", "mdi:power-plug", "battery_charging", "", 0, 0},
        {"binary_sensor", "recording", "Registrazione", "mdi:record-circle", "", "", 0, 0},
        {"number", "brightness", "Luminosita", "mdi:brightness-6", "", "", 16, 255},
        {"number", "volume", "Volume interazioni", "mdi:volume-high", "", "", 0, 200},
        {"number", "note_volume", "Volume note vocali", "mdi:account-voice", "", "", 0, 200},
        {"number", "vibration", "Vibrazione", "mdi:vibrate", "", "", 0, 255},
        {"number", "screen_timeout", "Timeout display", "mdi:timer-outline", "", "s", 15, 300},
        {"button", "vibrate", "Trova StopWatch", "mdi:vibrate", "", "", 0, 0},
        {"button", "beep", "Suona", "mdi:volume-high", "", "", 0, 0},
        {"event", "a", "Pulsante A", "mdi:gesture-tap-button", "", "", 0, 0},
        {"event", "b", "Pulsante B", "mdi:gesture-tap-button", "", "", 0, 0},
    };
    for (const auto& entity : entities) {
        JsonDocument config;
        config["unique_id"] = id_ + "_" + entity.key;
        config["name"] = entity.name;
        config["icon"] = entity.icon;
        config["availability_topic"] = base_ + "/availability";
        auto device = config["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add(id_);
        device["name"] = "StopWatch";
        device["manufacturer"] = "M5Stack";
        device["model"] = "StopWatch C152 · firmware personale";
        device["sw_version"] = firmware::version;
        const String domain = entity.domain;
        if (domain != "button") {
            config["state_topic"] = base_ + (domain == "event" ? String("/event/") + entity.key : "/state");
            if (domain == "event") {
                auto types = config["event_types"].to<JsonArray>();
                types.add("press"); types.add("hold"); types.add("release");
            } else if (domain == "binary_sensor")
                config["value_template"] = String("{{ 'ON' if value_json.") + entity.key + " else 'OFF' }}";
            else config["value_template"] = String("{{ value_json.") + entity.key + " }}";
        }
        if (domain == "number" || domain == "button") config["command_topic"] = base_ + "/set/" + entity.key;
        if (domain == "number") {
            config["min"] = entity.minimum;
            config["max"] = entity.maximum;
            config["step"] = 1;
            config["mode"] = "slider";
            config["entity_category"] = "config";
        }
        if (*entity.deviceClass) config["device_class"] = entity.deviceClass;
        if (*entity.unit) config["unit_of_measurement"] = entity.unit;
        if (domain == "sensor") config["entity_category"] = "diagnostic";
        String body;
        serializeJson(config, body);
        if (!publish(String("homeassistant/") + entity.domain + "/" + id_ + "/" + entity.key + "/config", body)) {
            discover_ = true; return;
        }
    }
    publish(base_ + "/availability", "online");
    stateDirty_ = true;
}

void MqttBridge::publishState() {
    JsonDocument state;
    status_(state.to<JsonObject>());
    state["brightness"] = settings_.brightness;
    state["volume"] = settings_.volume;
    state["note_volume"] = settings_.noteVolume;
    state["vibration"] = settings_.vibration;
    state["screen_timeout"] = settings_.screenTimeout;
    state["favorites_mqtt"] = settings_.favoritesMqtt;
    auto favorites = state["favorites"].to<JsonArray>();
    for (size_t i = 0; i < settings_.favoriteCount; ++i) {
        auto item = favorites.add<JsonObject>();
        item["entity"] = settings_.favorites[i].entity;
        item["label"] = settings_.favorites[i].label;
    }
    String body;
    serializeJson(state, body);
    if (publish(base_ + "/state", body)) { stateDirty_ = false; lastState_ = millis(); }
}

bool MqttBridge::requestFavorite(size_t favorite, bool action) {
    if (!available() || busy() || favorite >= settings_.favoriteCount ||
        (action && time(nullptr) < 1704067200)) return false;
    JsonDocument request;
    request["request"] = ++favoriteRequest_;
    request["entity"] = settings_.favorites[favorite].entity;
    request["action"] = action;
    request["sent_at"] = int64_t(time(nullptr));
    String body;
    serializeJson(request, body);
    // QoS 0 plus an expiry checked by HA prevents a delayed/replayed toggle.
    if (!dispatch(phone::Kind::Favorite,body,favoriteViaRelay_)) return false;
    favoriteResult_ = HomeResult{};
    favoriteResult_.favorite = favorite;
    favoriteResult_.action = action;
    favoriteResult_.entity = settings_.favorites[favorite].entity;
    favoriteResult_.requestedAt = millis();
    favoriteResult_.viaMqtt = true;
    favoriteAt_ = millis();
    favoritePending_ = true;
    return true;
}

bool MqttBridge::pollFavorite(HomeResult& result) {
    if (favoritePending_ && uint32_t(millis() - favoriteAt_) > (favoriteViaRelay_?phone::timeoutMs:8000)) {
        favoritePending_ = false;
        favoriteReady_ = true;
        favoriteResult_.message = favoriteResult_.action ? "Esito incerto: verifica in Home Assistant" : favoriteViaRelay_?"Home Assistant non risponde via iPhone":"Home Assistant non risponde via MQTT";
    }
    if (!favoriteReady_) return false;
    result = favoriteResult_;
    favoriteReady_ = false;
    return true;
}

void MqttBridge::button(const char* key, const char* event) {
    if (!connected()) return;
    JsonDocument message;
    message["event_type"] = event;
    String body;
    serializeJson(message, body);
    publish(base_ + "/event/" + key, body, false);
}

bool MqttBridge::requestMusic(music::Command command,const String& value,uint32_t offset) {
    if(!available() || musicPending_) return false;
    if(command!=music::Command::Refresh && command!=music::Command::Playlists && (time(nullptr)<1704067200 || !playback_.allows(command,millis()))) return false;
    if(command==music::Command::Source && std::find(playback_.sources.begin(),playback_.sources.end(),value.c_str())==playback_.sources.end()) return false;
    if(command==music::Command::Playlist) {
        bool listed=false;
        for(const auto& p:playlists_.items) if(p.uri==value.c_str()) listed=true;
        if(!listed || !music::playlistUri(value.c_str())) return false;
    }
    if(command==music::Command::Playlists && (offset>10000 || offset%music::playlistPageSize)) return false;
    JsonDocument data;
    data["request"]=++musicRequest_;data["command"]=music::commandName(command);
    if(command==music::Command::Source) data["source"]=value;
    if(command==music::Command::Playlist) data["playlist"]=value;
    if(command==music::Command::Playlists) data["offset"]=offset;
    data["sent_at"]=int64_t(time(nullptr));
    String body;serializeJson(data,body);
    if(!dispatch(phone::Kind::Music,body,musicViaRelay_)) return false;
    musicPending_=true;musicAt_=millis();musicMessage_="";musicDirty_=true;
    musicCommand_=command;playlistOffset_=offset;
    return true;
}

bool MqttBridge::pollMusic() {
    if(!available() && (playback_.available || musicPending_)) {
        playback_.available=false;musicPending_=false;musicMessage_="Wi-Fi o iPhone non disponibili";musicDirty_=true;
    } else if(musicPending_ && uint32_t(millis()-musicAt_)>(musicViaRelay_?phone::timeoutMs:8000)) {
        musicPending_=false;
        if(musicCommand_!=music::Command::Playlists) playback_.available=false;
        musicMessage_="Spotify non risponde · verifica HA";musicDirty_=true;
    }
    const bool dirty=musicDirty_;musicDirty_=false;return dirty;
}

void MqttBridge::update(bool screenOff) {
    if (!commands_ || !messages_) return;
    if (reconfigure_ && canSave_()) {
        if (client_) {
            if (connected())
                esp_mqtt_client_publish(client_, (base_ + "/availability").c_str(), "offline", 0, 1, true);
            esp_mqtt_client_stop(client_);
            esp_mqtt_client_destroy(client_);
            client_ = nullptr;
        }
        connected_ = false;
        playback_=music::Playback{};playlists_=music::PlaylistPage{};musicPending_=false;musicMessage_="";musicDirty_=true;
        discover_ = false;
        xQueueReset(commands_);
        Message* incoming;
        while (xQueueReceive(messages_, &incoming, 0) == pdTRUE) delete incoming;
        queuedFavorites_ = "";
        host_ = settings_.mqttHost; port_ = settings_.mqttPort; tls_ = settings_.mqttTls;
        user_ = settings_.mqttUser; password_ = settings_.mqttPassword; ca_ = settings_.mqttCa;
        reconfigure_ = false;
    }
    if (!client_ && !host_.isEmpty() && WiFi.status() == WL_CONNECTED && (!tls_ || time(nullptr) >= 1704067200)) {
        esp_mqtt_client_config_t config{};
        config.broker.address.hostname = host_.c_str();
        config.broker.address.port = port_;
        config.broker.address.transport = tls_ ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
        if (tls_) {
            if (ca_.isEmpty()) config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
            else config.broker.verification.certificate = ca_.c_str();
        }
        config.credentials.client_id = id_.c_str();
        config.credentials.username = user_.isEmpty() ? nullptr : user_.c_str();
        config.credentials.authentication.password = password_.isEmpty() ? nullptr : password_.c_str();
        const String availability = base_ + "/availability";
        config.session.last_will.topic = availability.c_str();
        config.session.last_will.msg = "offline";
        config.session.last_will.qos = 1;
        config.session.last_will.retain = 1;
        config.session.keepalive = 30;
        config.network.timeout_ms = 3000;
        config.network.reconnect_timeout_ms = 10000;
        // A bounded music/favorites packet plus topic/header fits without fragmentation.
        config.buffer.size = 5120;
        config.outbox.limit = 32768;
        client_ = esp_mqtt_client_init(&config);
        if (client_) {
            esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, event, this);
            if (esp_mqtt_client_start(client_) != ESP_OK) { esp_mqtt_client_destroy(client_); client_ = nullptr; }
        }
    }
    if (discover_.exchange(false) && connected()) discovery();
    if(relay_) for(const auto kind:{phone::Kind::Favorite,phone::Kind::Music}) {
        String payload;
        if(!relay_->pollRelay(kind,payload) || payload.length()>maxMessageBytes) continue;
        auto* message=new(std::nothrow) Message{};
        if(!message) continue;
        message->kind=kind==phone::Kind::Favorite?MessageKind::FavoriteResult:MessageKind::Music;
        message->length=payload.length();message->relay=true;
        memcpy(message->body,payload.c_str(),payload.length());
        if(xQueueSend(messages_,&message,0)!=pdTRUE) delete message;
    }
    Message* incoming;
    while (xQueueReceive(messages_, &incoming, 0) == pdTRUE) {
        std::unique_ptr<Message> message(incoming);
        if (message->kind==MessageKind::Favorites) { queuedFavorites_ = String(message->body, message->length); continue; }
        JsonDocument reply;
        if(message->kind==MessageKind::Music) {
            if(!musicPending_ || message->relay!=musicViaRelay_ || deserializeJson(reply,message->body,message->length) ||
               !reply["request"].is<uint32_t>() || reply["request"].as<uint32_t>()!=musicRequest_ ||
               !reply["success"].is<bool>()) continue;
            const bool catalog=musicCommand_==music::Command::Playlists;
            if(reply["success"].as<bool>() && !(catalog?
                music::parsePlaylists(reply["playlists"].as<JsonObjectConst>(),playlists_,playlistOffset_):
                music::parse(reply["playback"].as<JsonObjectConst>(),playback_,millis()))) continue;
            if(!reply["success"].as<bool>()) {
                if(!catalog) playback_.available=false;
                musicMessage_=message->relay && reply["error"].is<const char*>()?reply["error"].as<String>().substring(0,160):catalog?"Playlist non disponibili":"Spotify non disponibile in HA";
            }
            else musicMessage_="";
            musicPending_=false;musicDirty_=true;continue;
        }
        if (!favoritePending_ || message->relay!=favoriteViaRelay_ || deserializeJson(reply, message->body, message->length) ||
            !reply["request"].is<uint32_t>() || reply["request"].as<uint32_t>() != favoriteRequest_ ||
            !reply["entity"].is<const char*>() || favoriteResult_.favorite >= settings_.favoriteCount ||
            reply["entity"].as<String>() != favoriteResult_.entity ||
            favoriteResult_.entity != settings_.favorites[favoriteResult_.favorite].entity ||
            !reply["success"].is<bool>() || !reply["state"].is<const char*>()) continue;
        favoriteResult_.success = reply["success"].as<bool>();
        const String state = reply["state"].as<String>();
        const String unit = reply["unit"] | "";
        auto prefix = policy::utf8Prefix(state.c_str(), 64);
        favoriteResult_.state = String(prefix.data(), prefix.size());
        prefix = policy::utf8Prefix(unit.c_str(), 16);
        favoriteResult_.unit = String(prefix.data(), prefix.size());
        favoriteResult_.message = favoriteResult_.success ? (favoriteResult_.action ? "Comando inviato" : "Stato aggiornato") : "Entita non disponibile";
        if(!favoriteResult_.success && message->relay && reply["error"].is<const char*>()) favoriteResult_.message=reply["error"].as<String>().substring(0,160);
        favoritePending_ = false;
        favoriteReady_ = true;
    }
    if (!queuedFavorites_.isEmpty() && canSave_() && !busy()) {
        JsonDocument data;
        Settings next = settings_;
        String error;
        const bool valid = !deserializeJson(data, queuedFavorites_) && data["favorites"].is<JsonArrayConst>() &&
            next.updateFavorites(data["favorites"].as<JsonArrayConst>(), error, true);
        queuedFavorites_ = "";
        if (valid) {
            bool changed = !settings_.favoritesMqtt || next.favoriteCount != settings_.favoriteCount;
            for (size_t i = 0; i < next.favoriteCount && !changed; ++i)
                changed = next.favorites[i].entity != settings_.favorites[i].entity || next.favorites[i].label != settings_.favorites[i].label;
            next.favoritesMqtt = true;
            if (changed && next.save()) { settings_ = next; onSaved_(); }
            stateDirty_ = true;
        }
    }
    Command command;
    if (xQueueReceive(commands_, &command, 0) == pdTRUE && canSave_()) {
        const String key(command.key), value(command.value);
        if ((key == "vibrate" || key == "beep") && value == "PRESS") output_(key);
        else {
            uint32_t number = 0;
            bool valid = !value.isEmpty() && value.length() <= 3;
            for (char c : value) { if (c < '0' || c > '9') valid = false; else number = number * 10 + c - '0'; }
            Settings next = settings_;
            if (valid && key == "brightness" && number >= 16 && number <= 255) next.brightness = number;
            else if (valid && key == "volume" && number <= 200) next.volume = number;
            else if (valid && key == "note_volume" && number <= 200) next.noteVolume = number;
            else if (valid && key == "vibration" && number <= 255) next.vibration = number;
            else if (valid && key == "screen_timeout" && number >= 15 && number <= 300) next.screenTimeout = number;
            else valid = false;
            if (valid && next.save()) { settings_ = next; onSaved_(); }
            stateDirty_ = true;
        }
    }
    if (connected() && (stateDirty_ || uint32_t(millis() - lastState_) > (screenOff?60000u:15000u))) publishState();
}
