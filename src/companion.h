#pragma once

#include <BLEDevice.h>
#include <BLESecurity.h>
#include <atomic>
#include <freertos/queue.h>
#include "ble_protocol.h"
#include "settings.h"
#include "voice_notes.h"
#include "note_transfer.h"
#include "badge.h"
#include "phone_relay.h"
#include "usb_control.h"

class Companion : private BLEServerCallbacks, private BLECharacteristicCallbacks, private BLESecurityCallbacks {
public:
    Companion(Settings& settings, VoiceNotes& notes, Badge& badge, UsbControl& usb, bool (*canSave)(), void (*onSaved)(),
              bool (*output)(const String&), void (*extraStatus)(JsonObject))
        : settings_(settings), notes_(notes), badge_(badge), usb_(usb), noteTransfer_(badge), canSave_(canSave), onSaved_(onSaved),
          output_(output), extraStatus_(extraStatus) {}
    bool begin();
    void update();
    bool hasWork();
    void notesChanged() { notesChanged_ = true; }
    void openPairing();
    void closePairing();
    void forgetPhones();
    bool pairing() const;
    bool connected() const { return authenticated_.load(); }
    bool ready() const { return server_ != nullptr; }
    uint32_t passkey() const { return passkey_.load(); }
    int bondedPhones() const;
    const String& name() const { return name_; }
    void status(JsonObject out) const;
    bool relayReady() const { return connected() && subscribed_ && relay_.enabled(); }
    bool queueRelay(phone::Kind kind,const String& payload);
    bool pollRelay(phone::Kind kind,String& payload);

private:
    struct Packet { uint32_t session; uint16_t size; uint8_t bytes[512]; };
    struct MousePacket { uint32_t session; uint8_t bytes[8]; };
    void onConnect(BLEServer*, ble_gap_conn_desc*) override;
    void onDisconnect(BLEServer*, ble_gap_conn_desc*) override;
    void onWrite(BLECharacteristic*, ble_gap_conn_desc*) override;
    void onSubscribe(BLECharacteristic*, ble_gap_conn_desc*, uint16_t) override;
    uint32_t onPassKeyRequest() override { return passkey(); }
    void onPassKeyNotify(uint32_t code) override;
    bool onSecurityRequest() override;
    bool onConfirmPIN(uint32_t) override { return false; }
    bool onAuthorizationRequest(uint16_t, uint16_t, bool) override { return connected(); }
    void onAuthenticationComplete(ble_gap_conn_desc* result) override;
    void request(const std::string& line);
    bool knownPhone(const ble_addr_t& address) const;
    void resetTransfer();

    Settings& settings_;
    VoiceNotes& notes_;
    Badge& badge_;
    UsbControl& usb_;
    NoteTransfer noteTransfer_;
    bool (*canSave_)();
    void (*onSaved_)();
    bool (*output_)(const String&);
    void (*extraStatus_)(JsonObject);
    BLEServer* server_ = nullptr;
    BLECharacteristic* tx_ = nullptr;
    BLECharacteristic* mouseRx_ = nullptr;
    QueueHandle_t packets_ = nullptr;
    QueueHandle_t mousePackets_ = nullptr;
    String name_, response_;
    BleLine line_;
    phone::Relay relay_;
    size_t sent_ = 0;
    uint32_t connectedAt_ = 0, transferSession_ = 0;
    std::atomic<uint32_t> session_{0}, pairingUntil_{0}, passkey_{0};
    std::atomic<uint16_t> connection_{0};
    std::atomic<bool> linked_{false}, authenticated_{false}, reject_{false}, advertise_{false};
    std::atomic<bool> knownPeer_{false};
    std::atomic<bool> subscribed_{false}, notesChanged_{false};
};
