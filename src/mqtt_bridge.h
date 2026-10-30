#pragma once

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>
#include "settings.h"
#include "home_assistant.h"
#include "music_protocol.h"
#include "phone_relay.h"

class Companion;

class MqttBridge {
public:
    MqttBridge(Settings& settings, bool (*canSave)(), void (*onSaved)(),
               bool (*output)(const String&), void (*status)(JsonObject))
        : settings_(settings), canSave_(canSave), onSaved_(onSaved), output_(output), status_(status) {}
    void begin();
    void configure();
    void update(bool screenOff);
    bool hasWork() const {
        return busy() || musicBusy() || reconfigure_ || discover_ ||
            (commands_ && uxQueueMessagesWaiting(commands_)) || (messages_ && uxQueueMessagesWaiting(messages_));
    }
    void changed() { stateDirty_ = true; }
    void button(const char* key, const char* event);
    bool connected() const { return connected_.load(); }
    void setRelay(Companion* relay) { relay_=relay; }
    bool available() const;
    const String& deviceId() const { return id_; }
    bool requestFavorite(size_t favorite, bool action,int brightness=-1);
    bool pollFavorite(HomeResult& result);
    bool busy() const { return favoritePending_ || favoriteReady_; }
    bool requestMusic(music::Command command=music::Command::Refresh,const String& value="",uint32_t offset=0);
    bool pollMusic();
    bool musicBusy() const { return musicPending_; }
    const music::Playback& playback() const { return playback_; }
    const String& musicMessage() const { return musicMessage_; }
    const music::PlaylistPage& playlists() const { return playlists_; }

private:
    struct Command { char key[24], value[17]; };
    enum class MessageKind { Favorites, FavoriteResult, Music };
    static constexpr size_t maxMessageBytes=4096;
    struct Message { MessageKind kind; uint16_t length; bool relay=false; char body[maxMessageBytes + 1]; };
    static void event(void* context, esp_event_base_t, int32_t id, void* data);
    void discovery();
    void publishState();
    bool publish(const String& topic, const String& body, bool retain = true, int qos = 1);
    bool dispatch(phone::Kind kind,const String& body,bool& viaRelay);
    Companion* relay_=nullptr;
    Settings& settings_;
    bool (*canSave_)();
    void (*onSaved_)();
    bool (*output_)(const String&);
    void (*status_)(JsonObject);
    esp_mqtt_client_handle_t client_ = nullptr;
    QueueHandle_t commands_ = nullptr;
    QueueHandle_t messages_ = nullptr;
    String queuedFavorites_;
    HomeResult favoriteResult_;
    uint32_t favoriteRequest_ = 0, favoriteAt_ = 0;
    bool favoritePending_ = false, favoriteReady_ = false;
    bool favoriteViaRelay_=false,musicViaRelay_=false;
    music::Playback playback_;
    music::PlaylistPage playlists_;
    music::Command musicCommand_=music::Command::Refresh;
    uint32_t playlistOffset_=0;
    String musicMessage_;
    uint32_t musicRequest_=0,musicAt_=0;
    bool musicPending_=false,musicDirty_=false;
    String id_, base_, host_, user_, password_, ca_;
    uint16_t port_ = 0;
    bool tls_ = false, reconfigure_ = false, stateDirty_ = true;
    uint32_t lastState_ = 0;
    std::atomic<bool> connected_{false}, discover_{false};
};
