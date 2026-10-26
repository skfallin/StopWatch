#pragma once
#include "settings.h"
#include "music_model.h"
#include <M5GFX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class Companion;

class MusicArtwork {
public:
    bool update(const music::Playback& playback,const Settings& settings,bool visible,Companion& companion,bool viaPhone);
    bool draw(M5Canvas& canvas);
    uint32_t background() const { return loaded_?background_:0; }
    bool busy() const { return busy_ || relayBusy_; }
private:
    struct Request { String url,ca; };
    struct Result { String url; uint8_t* bytes=nullptr; size_t size=0; ~Result() { free(bytes); } };
    static void worker(void* context);
    bool decode(const uint8_t* bytes,size_t size);
    QueueHandle_t requests_=nullptr,results_=nullptr;
    M5Canvas image_;
    String desired_,cached_;
    String relayDesired_;
    bool relayBusy_=false;
    uint32_t relayRequest_=0,relayAt_=0;
    bool busy_=false,loaded_=false,started_=false;
    uint32_t retryAt_=0;
    uint32_t background_=0;
};
