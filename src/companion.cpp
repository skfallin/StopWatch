#include "i18n.h"
#include "companion.h"
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <host/ble_store.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_mbuf.h>
#include <services/gatt/ble_svc_gatt.h>
#include <mbedtls/base64.h>
#include <memory>
#include <cstring>
#include <new>
#include <sys/time.h>

namespace {
constexpr const char* serviceUuid = "75f3a100-9e9a-4b9c-9d73-c85f44e6a001";
constexpr const char* rxUuid = "75f3a101-9e9a-4b9c-9d73-c85f44e6a001";
constexpr const char* txUuid = "75f3a102-9e9a-4b9c-9d73-c85f44e6a001";
constexpr const char* mouseUuid = "75f3a103-9e9a-4b9c-9d73-c85f44e6a001";
}

bool Companion::begin() {
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)(ESP.getEfuseMac() & 0xffffff));
    name_ = String("StopWatch-") + suffix;
    packets_ = xQueueCreate(8, sizeof(Packet));
    mousePackets_ = xQueueCreate(8, sizeof(MousePacket));
    if (!packets_ || !mousePackets_ || !BLEDevice::init(name_)) return false;
    BLEDevice::setMTU(517);
    BLEDevice::setSecurityCallbacks(this);
    BLESecurity::setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    BLESecurity::setCapability(ESP_IO_CAP_OUT);
    BLESecurity::setKeySize(16);
    BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    ble_hs_cfg.sm_sc_only = 1;
    BLESecurity::setPassKey(true, esp_random() % 1000000);
    server_ = BLEDevice::createServer();
    if (!server_) return false;
    server_->setCallbacks(this);
    server_->advertiseOnDisconnect(false);
    auto* service = server_->createService(serviceUuid);
    auto* rx = service->createCharacteristic(rxUuid, BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN);
    rx->setCallbacks(this);
    tx_ = service->createCharacteristic(txUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_READ_ENC | BLECharacteristic::PROPERTY_READ_AUTHEN);
    tx_->setCallbacks(this);
    tx_->setValue("{\"id\":0,\"ok\":true,\"data\":{\"protocol\":1}}\n");
    // Keep the existing RX/TX handles stable for bonded iPhones with a cached GATT database.
    mouseRx_ = service->createCharacteristic(mouseUuid, BLECharacteristic::PROPERTY_WRITE_NR |
        BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN);
    mouseRx_->setCallbacks(this);
    service->start();
    auto* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x0c);
    advertising->setMaxPreferred(0x18);
    advertising->start();
    // Invalidate cached handles on bonded peers after a firmware restart, without deleting bonds.
    ble_svc_gatt_changed(1,0xffff);
    return true;
}

bool Companion::pairing() const {
    const uint32_t until = pairingUntil_.load();
    return until && int32_t(until - millis()) > 0;
}

void Companion::openPairing() {
    if (!server_) return;
    if (linked_) server_->disconnect(connection_);
    uint32_t code;
    do { code = esp_random() % 1000000; } while (code == 123456);
    passkey_ = BLESecurity::setPassKey(true, code);
    pairingUntil_ = millis() + 90000;
}

void Companion::closePairing() {
    pairingUntil_ = 0;
    if (linked_ && !authenticated_) reject_ = true;
}

int Companion::bondedPhones() const {
    if (!server_) return 0;
    ble_addr_t phones[CONFIG_BT_NIMBLE_MAX_BONDS];
    int count = 0;
    return ble_store_util_bonded_peers(phones, &count, CONFIG_BT_NIMBLE_MAX_BONDS) == 0 ? count : 0;
}

bool Companion::knownPhone(const ble_addr_t& address) const {
    ble_addr_t phones[CONFIG_BT_NIMBLE_MAX_BONDS];
    int count = 0;
    if (ble_store_util_bonded_peers(phones, &count, CONFIG_BT_NIMBLE_MAX_BONDS) != 0) return false;
    for (int i = 0; i < count; ++i) if (ble_addr_cmp(&phones[i], &address) == 0) return true;
    return false;
}

void Companion::forgetPhones() {
    badge_.abortUpload();
    closePairing();
    if (linked_) server_->disconnect(connection_);
    authenticated_ = false;
    ble_addr_t phones[CONFIG_BT_NIMBLE_MAX_BONDS];
    int count = 0;
    if (ble_store_util_bonded_peers(phones, &count, CONFIG_BT_NIMBLE_MAX_BONDS) == 0)
        for (int i = 0; i < count; ++i) ble_gap_unpair(&phones[i]);
    resetTransfer();
}

void Companion::onConnect(BLEServer*, ble_gap_conn_desc* peer) {
    reject_ = false;
    connection_ = peer->conn_handle;
    knownPeer_ = knownPhone(peer->peer_id_addr);
    authenticated_ = false;
    subscribed_ = false;
    linked_ = true;
    session_.fetch_add(1);
    if (!knownPeer_ && !pairing()) {
        reject_ = true;
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

void Companion::onDisconnect(BLEServer*, ble_gap_conn_desc*) {
    reject_ = false;
    authenticated_ = false;
    subscribed_ = false;
    linked_ = false;
    advertise_ = true;
    session_.fetch_add(1);
}

bool Companion::onSecurityRequest() { return pairing() || knownPeer_.load(); }

void Companion::onPassKeyNotify(uint32_t code) {
    if (pairing()) passkey_ = code;
    else { reject_ = true; ble_gap_terminate(connection_, BLE_ERR_REM_USER_CONN_TERM); }
}

void Companion::onAuthenticationComplete(ble_gap_conn_desc* result) {
    const bool secure = result->sec_state.encrypted && result->sec_state.authenticated &&
        result->sec_state.bonded && result->sec_state.key_size == 16;
    // Resumed encrypted bonds do not require a new passkey, including private addresses.
    if (secure && !reject_ && (pairing() || knownPeer_)) {
        authenticated_ = true;
        pairingUntil_ = 0;
        server_->requestConnParams(result->conn_handle, 12, 24, 0, 600);
    } else reject_ = true;
}

void Companion::onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc*) {
    const String value = characteristic->getValue();
    if(characteristic==mouseRx_) {
        if(!connected() || value.length()!=8) return;
        MousePacket packet{};packet.session=session_;
        memcpy(packet.bytes,value.c_str(),8);
        // Motion is best effort; never disconnect the authenticated phone for a busy pointer queue.
        xQueueSend(mousePackets_,&packet,0);
        return;
    }
    if (!connected() || value.isEmpty() || value.length() > sizeof(Packet::bytes)) {
        reject_ = true; return;
    }
    Packet packet{};
    packet.session = session_;
    packet.size = value.length();
    memcpy(packet.bytes, value.c_str(), packet.size);
    if (xQueueSend(packets_, &packet, 0) != pdTRUE) reject_ = true;
}

void Companion::onSubscribe(BLECharacteristic* characteristic, ble_gap_conn_desc*, uint16_t value) {
    if (characteristic != tx_) return;
    subscribed_ = (value & 1) != 0;
    if (subscribed_) notesChanged_ = true;
}

void Companion::resetTransfer() {
    relay_.enable(false);
    line_.clear();
    response_ = "";
    sent_ = 0;
}

void Companion::status(JsonObject out) const {
    out["relay_supported"]=true;
    out["relay_enabled"]=relayReady();
    out["protocol"] = 1;
    out["note_chunk_bytes"] = maxBleAudioChunkBytes;
    out["name"] = name_;
    out["battery"] = M5.Power.getBatteryLevel();
    out["charging"] = M5.Power.isCharging() == m5::Power_Class::is_charging;
    out["wifi"] = WiFi.status() == WL_CONNECTED;
    if (WiFi.status() == WL_CONNECTED) out["rssi"] = WiFi.RSSI();
    out["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    out["storage_ready"] = notes_.ready();
    out["storage_used"] = notes_.ready() ? LittleFS.usedBytes() : 0;
    out["storage_total"] = notes_.ready() ? LittleFS.totalBytes() : 0;
    out["psram_free"] = ESP.getFreePsram();
    out["notes"] = notes_.count();
    out["recording"] = notes_.busy();
    badge_.status(out["badge"].to<JsonObject>());
    out["note_seconds"] = notes_.seconds();
    out["bonded_phones"] = bondedPhones();
    out["microphone"] = M5.Mic.isEnabled();
    out["imu"] = M5.Imu.isEnabled();
    out["rtc"] = M5.Rtc.isEnabled();
    out["touch"] = M5.Touch.isEnabled();
    out["button_a"] = M5.BtnA.isPressed();
    out["button_b"] = M5.BtnB.isPressed();
    usb_.status(out["usb"].to<JsonObject>());
    extraStatus_(out);
}

void Companion::request(const std::string& line) {
    JsonDocument input, reply;
    if (deserializeJson(input, line) || !input["id"].is<uint32_t>() || !input["op"].is<const char*>()) {
        reject_ = true; return;
    }
    reply["id"] = input["id"].as<uint32_t>();
    reply["ok"] = true;
    auto data = reply["data"].to<JsonObject>();
    const String op = input["op"].as<String>();
    const auto fail = [&](const String& reason) { reply["ok"] = false; reply["error"] = reason; };
    if (op == "status") status(data);
    else if (op == "usb.status") usb_.status(data);
    else if (op == "usb.input") {
        if(notes_.busy()) fail(i18n::text("Attendi il salvataggio della nota"));
        else if(!input["data"].is<JsonObjectConst>()) fail(i18n::text("Comando USB non valido"));
        else if(const auto* reason=usb_.input(input["data"].as<JsonObjectConst>())) fail(reason);
        usb_.status(data);
    }
    else if (op == "config.get") settings_.toJson(data);
    else if(op=="relay.credentials") {
        if(!settings_.haUrl.startsWith("https://") || settings_.haToken.isEmpty()) fail(i18n::text("Configura l'indirizzo HTTPS remoto e il token Home Assistant"));
        else { data["url"]=settings_.haUrl;data["token"]=settings_.haToken;data["enabled"]=true; }
    } else if(op=="relay.configure") {
        if(!input["data"]["enabled"].is<bool>()) fail(i18n::text("Ponte iPhone non valido"));
        else { relay_.enable(input["data"]["enabled"].as<bool>());data["enabled"]=relay_.enabled(); }
    } else if(op=="relay.reply") {
        const auto value=input["data"].as<JsonObjectConst>();
        const auto kind=phone::kind(value["kind"] | "");
        if(kind==phone::Kind::Invalid || !value["payload"].is<const char*>()) fail(i18n::text("Risposta ponte non valida"));
        else {
            const auto payload=value["payload"].as<JsonString>();
            JsonDocument result;
            if(payload.size()>phone::resultLimit(kind) || std::strlen(payload.c_str())!=payload.size() ||
               deserializeJson(result,payload.c_str(),payload.size()) || !result["request"].is<uint32_t>() || !result["success"].is<bool>() ||
               !relay_.finish(kind,result["request"].as<uint32_t>(),std::string(payload.c_str(),payload.size()),millis())) fail(i18n::text("Richiesta ponte scaduta"));
        }
    }
    else if (notes_.busy()) fail(i18n::text("Attendi il salvataggio della nota"));
    else if (op == "config.set") {
        Settings next = settings_;
        String reason;
        if (!canSave_()) fail(i18n::text("Attendi la richiesta Home Assistant"));
        else if (!input["data"].is<JsonObjectConst>()) fail(i18n::text("Configurazione non valida"));
        else if (!next.update(input["data"].as<JsonObjectConst>(), reason)) fail(reason);
        else if (!next.save()) fail(i18n::text("Salvataggio fallito: impostazioni precedenti conservate"));
        else { settings_ = next; onSaved_(); settings_.toJson(data); }
    } else if (op == "time.set") {
        const int64_t epoch = input["data"]["epoch"] | int64_t(0);
        if (epoch < 1704067200 || epoch > 4102444800LL) fail(i18n::text("Ora non valida"));
        else {
            timeval now{time_t(epoch), 0};
            settimeofday(&now, nullptr);
            tm utc{}; gmtime_r(&now.tv_sec, &utc);
            if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&utc);
        }
    } else if (op == "output") {
        if (!output_(input["data"]["action"] | "")) fail(i18n::text("Comando non disponibile"));
    } else if(op=="badge.status") badge_.status(data);
    else if(op=="badge.begin") {
        String reason;
        if(!notes_.ready() || !input["data"]["bytes"].is<uint32_t>() || !input["data"]["digest"].is<const char*>()) fail(i18n::text("Badge non valido o archivio non disponibile"));
        else if(!badge_.startUpload(input["data"]["bytes"].as<uint32_t>(),input["data"]["digest"].as<String>(),data,reason)) fail(reason);
    } else if(op=="badge.transfer") {
        noteTransfer_.offerBadge(input["data"]["upload"] | "",data);
    } else if(op=="badge.write") {
        const String encoded=input["data"]["chunk"] | "";
        std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[4096]); size_t count=0;
        if(!input["data"]["offset"].is<uint32_t>() || encoded.isEmpty() || encoded.length()>5464 ||
           !bytes || mbedtls_base64_decode(bytes.get(),4096,&count,reinterpret_cast<const uint8_t*>(encoded.c_str()),encoded.length())!=0 ||
           !badge_.append(input["data"]["upload"] | "",input["data"]["offset"].as<uint32_t>(),bytes.get(),count)) fail(i18n::text("Blocco Badge rifiutato"));
    } else if(op=="badge.commit") {
        String reason;
        if(!badge_.commit(input["data"]["upload"] | "",reason)) fail(reason);
        else badge_.status(data);
    } else if(op=="badge.cancel") {
        badge_.abortUpload();
    } else if (op == "notes.list") {
        auto list = data["notes"].to<JsonArray>();
        if (notes_.ready()) {
            File directory = LittleFS.open("/notes");
            File file = directory.openNextFile();
            while (file && list.size() < policy::maxNotes) {
                if (!file.isDirectory() && policy::noteFilename(file.name())) {
                    auto item = list.add<JsonObject>();
                    item["name"] = file.name();
                    item["bytes"] = file.size();
                    item["seconds"] = file.size() >= 44 ? (file.size() - 44) / float(policy::sampleRate * 2) : 0;
                }
                file = directory.openNextFile();
            }
        }
    } else if (op == "notes.transfer") {
        const String name = input["data"]["name"] | "";
        if (!policy::noteFilename(name.c_str()) || !notes_.ready()) fail(i18n::text("Nota non valida o archivio non disponibile"));
        else noteTransfer_.offer(name, data);
    } else if (op == "notes.read" || op == "notes.delete") {
        const String name = input["data"]["name"] | "";
        if (!notes_.ready() || !policy::noteFilename(name.c_str())) fail(i18n::text("Nota non valida o archivio non disponibile"));
        else if (op == "notes.delete") {
            if (!LittleFS.remove(String("/notes/") + name)) fail(i18n::text("Nota non trovata"));
            else notesChanged_ = true;
        } else if (!input["data"]["offset"].is<uint32_t>()) fail(i18n::text("Offset non valido"));
        else {
            const auto length = input["data"]["length"];
            const size_t requested = length.isNull() ? 768 : length.as<size_t>();
            if ((!length.isNull() && !length.is<unsigned int>()) || !requested || requested > maxBleAudioChunkBytes) {
                fail(i18n::text("Dimensione blocco non valida"));
            } else {
                File file = LittleFS.open(String("/notes/") + name, "r");
                const uint32_t offset = input["data"]["offset"].as<uint32_t>();
                if (!file || offset >= file.size() || !file.seek(offset)) fail(i18n::text("Nota non trovata o offset non valido"));
                else {
                    const size_t capacity = 4 * ((requested + 2) / 3) + 1;
                    std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[requested]);
                    std::unique_ptr<uint8_t[]> encoded(new (std::nothrow) uint8_t[capacity]);
                    const size_t count = bytes && encoded ? file.read(bytes.get(), std::min<size_t>(requested, file.size() - offset)) : 0;
                    size_t size = 0;
                    if (!count || mbedtls_base64_encode(encoded.get(), capacity, &size, bytes.get(), count) != 0) fail(i18n::text("Lettura nota fallita"));
                    else {
                        encoded[size] = 0;
                        data["offset"] = offset;
                        data["total"] = file.size();
                        data["audio"] = String(reinterpret_cast<const char*>(encoded.get()));
                    }
                }
            }
        }
    } else fail(i18n::text("Comando sconosciuto"));
    response_ = "";
    serializeJson(reply, response_);
    response_ += '\n';
    sent_ = 0;
}

bool Companion::hasWork() {
    return pairing() || (linked_ && !authenticated_) || reject_ || advertise_ || noteTransfer_.busy() ||
        (connected() && subscribed_ && (notesChanged_ || !response_.isEmpty() || !line_.value().empty() ||
        (packets_ && uxQueueMessagesWaiting(packets_)) || relay_.pending()));
}

void Companion::update() {
    if (!server_) return;
    if (pairingUntil_ && !pairing()) closePairing();
    const uint32_t session = session_;
    if (session != transferSession_) {
        transferSession_ = session;
        resetTransfer();
        connectedAt_ = millis();
    }
    if (reject_.exchange(false) || (linked_ && !connected() && uint32_t(millis() - connectedAt_) > 45000)) {
        authenticated_ = false;
        server_->disconnect(connection_);
        resetTransfer();
        return;
    }
    if (advertise_.exchange(false)) server_->startAdvertising();
    if (!connected() || !subscribed_) return;
    MousePacket mousePacket;
    while(xQueueReceive(mousePackets_,&mousePacket,0)==pdTRUE)
        if(mousePacket.session==transferSession_) usb_.motion(mousePacket.bytes,sizeof(mousePacket.bytes));
    Packet packet;
    // A new request may arrive while the previous reply's note event is draining.
    while (response_.isEmpty() && xQueueReceive(packets_, &packet, 0) == pdTRUE) {
        if (packet.session != transferSession_) continue;
        for (size_t i = 0; i < packet.size; ++i) {
            if (!response_.isEmpty()) { reject_ = true; return; }
            const auto result = line_.push(char(packet.bytes[i]));
            if (result == BleLine::Result::Invalid) { reject_ = true; return; }
            if (result == BleLine::Result::Complete) { request(line_.value()); line_.clear(); }
        }
    }
    if(response_.isEmpty() && line_.value().empty()) {
        if(auto* job=relay_.next(millis())) {
            JsonDocument event;
            event["id"]=0;event["ok"]=true;event["event"]="relay.request";
            auto data=event["data"].to<JsonObject>();
            data["kind"]=phone::name(job->kind);data["payload"]=job->payload;
            data["age_ms"]=uint32_t(millis()-job->startedAt);
            serializeJson(event,response_);response_+='\n';sent_=0;job->sent=true;
        }
    }
    if (response_.isEmpty() && line_.value().empty() && notesChanged_.exchange(false)) {
        response_ = "{\"id\":0,\"ok\":true,\"event\":\"notes.changed\"}\n";
        sent_ = 0;
    }
    for (unsigned int queued = 0; queued < 4 && !response_.isEmpty(); ++queued) {
        if (!connected() || session_.load() != transferSession_) return;
        const uint16_t mtu = server_->getPeerMTU(connection_);
        const size_t count = std::min(response_.length() - sent_, std::min(size_t(512), size_t(mtu >= 23 ? mtu - 3 : 20)));
        auto* buffer = ble_hs_mbuf_from_flat(response_.c_str() + sent_, count);
        if (!buffer) break;
        // NimBLE consumes the buffer even on failure; advance only when queued.
        const int result = ble_gatts_notify_custom(connection_, tx_->getHandle(), buffer);
        if (result == BLE_HS_ENOMEM || result == BLE_HS_EBUSY || result == BLE_HS_EAGAIN) break;
        if (result != 0) { reject_ = true; break; }
        sent_ += count;
        if (sent_ == response_.length()) { response_ = ""; sent_ = 0; }
    }
}

bool Companion::queueRelay(phone::Kind kind,const String& payload) {
    if(!relayReady()) return false;
    JsonDocument data;
    if(deserializeJson(data,payload) || !data["request"].is<uint32_t>()) return false;
    return relay_.enqueue(kind,data["request"].as<uint32_t>(),payload.c_str(),millis());
}

bool Companion::pollRelay(phone::Kind kind,String& payload) {
    std::string result;
    if(!relay_.take(kind,result)) return false;
    payload=String(result.data(),result.size());return true;
}
