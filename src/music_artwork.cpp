#include "music_artwork.h"
#include "music_protocol.h"
#include "music_image.h"
#include "ui_drawing.h"
#include "companion.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>
#include <time.h>
#include <mbedtls/base64.h>

extern const uint8_t builtinCaStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t builtinCaEnd[] asm("_binary_x509_crt_bundle_end");

namespace {
constexpr size_t maxImageBytes=384*1024;
class ImageBuffer : public Stream {
public:
    explicit ImageBuffer(uint8_t* bytes):bytes_(bytes) {}
    size_t write(uint8_t byte) override { return write(&byte,1); }
    size_t write(const uint8_t* bytes,size_t count) override {
        if(count>maxImageBytes-size_) return 0;
        memcpy(bytes_+size_,bytes,count);size_+=count;return count;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t size() const { return size_; }
private:
    uint8_t* bytes_;
    size_t size_=0;
};
}

void MusicArtwork::worker(void* context) {
    auto& self=*static_cast<MusicArtwork*>(context);
    for(;;) {
        Request* next;
        if(xQueueReceive(self.requests_,&next,portMAX_DELAY)!=pdTRUE) continue;
        std::unique_ptr<Request> request(next);
        auto* result=new Result;
        result->url=request->url;
        {
            WiFiClient plain;WiFiClientSecure secure;
            WiFiClient* transport=&plain;
            bool valid=WiFi.status()==WL_CONNECTED;
            if(request->url.startsWith("https://")) {
                valid=valid && time(nullptr)>=1704067200;
                if(request->ca.isEmpty()) secure.setCACertBundle(builtinCaStart,builtinCaEnd-builtinCaStart);
                else secure.setCACert(request->ca.c_str());
                secure.setHandshakeTimeout(4);transport=&secure;
            }
            HTTPClient http;
            http.setConnectTimeout(2500);http.setTimeout(3500);
            http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.useHTTP10(true);
            if(valid && http.begin(*transport,request->url) && http.GET()==200 && http.getSize()<=int(maxImageBytes)) {
                result->bytes=static_cast<uint8_t*>(heap_caps_malloc(maxImageBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
                if(result->bytes) {
                    ImageBuffer output(result->bytes);
                    const int written=http.writeToStream(&output);
                    if(written>0 && size_t(written)==output.size()) result->size=output.size();
                }
            }
            http.end();
        }
        xQueueSend(self.results_,&result,portMAX_DELAY);
    }
}

bool MusicArtwork::decode(const uint8_t* bytes,size_t size) {
    const auto format=music::imageFormat(bytes,size);
    image_.setPsram(true);image_.setColorDepth(16);
    if(format==music::ImageFormat::Invalid || !image_.createSprite(188,188)) return false;
    image_.fillScreen(ui::black);
    loaded_=format==music::ImageFormat::Jpeg?image_.drawJpg(bytes,size,0,0,188,188,0,0,0,0,lgfx::datum_t::middle_center):
        image_.drawPng(bytes,size,0,0,188,188,0,0,0,0,lgfx::datum_t::middle_center);
    if(loaded_) {
        cached_=desired_;
        uint32_t r=0,g=0,b=0;
        for(int y=16;y<188;y+=32) for(int x=16;x<188;x+=32) {
            const auto color=image_.readPixel(x,y);
            r+=(color>>16)&255;g+=(color>>8)&255;b+=color&255;
        }
        background_=((r/300)<<16)|((g/300)<<8)|(b/300);
    } else image_.deleteSprite();
    return loaded_;
}

bool MusicArtwork::update(const music::Playback& p,const Settings& settings,bool visible,Companion& companion,bool viaPhone) {
    String url;
    if(!p.image.empty()) {
        if(music::artworkUrl(p.image)) url=p.image.c_str();
        else if(music::artworkPath(p.image)) {
            String base=p.server.empty()?settings.haUrl:String(p.server.c_str());
            while(base.endsWith("/")) base.remove(base.length()-1);
            if(!base.isEmpty() && policy::baseUrl(base.c_str())) url=base+p.image.c_str();
        }
    }
    bool dirty=false;
    if(url!=desired_) {
        desired_=url;cached_="";loaded_=false;image_.deleteSprite();retryAt_=0;dirty=true;
    }
    if(relayBusy_ && uint32_t(millis()-relayAt_)>=phone::timeoutMs) { relayBusy_=false;retryAt_=millis()+30000; }
    String payload;
    if(companion.pollRelay(phone::Kind::Artwork,payload)) {
        JsonDocument result;
        if(!deserializeJson(result,payload) && result["request"].is<uint32_t>() && result["request"].as<uint32_t>()==relayRequest_) {
            relayBusy_=false;
            if(relayDesired_==desired_ && result["success"]==true && result["jpeg"].is<const char*>()) {
                const auto encoded=result["jpeg"].as<JsonString>();
                std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[6000]);size_t size=0;
                if(bytes && encoded.size()<=8000 && !mbedtls_base64_decode(bytes.get(),6000,&size,
                    reinterpret_cast<const uint8_t*>(encoded.c_str()),encoded.size())) dirty=decode(bytes.get(),size) || dirty;
            }
            retryAt_=relayDesired_==desired_?millis()+30000:0;
        }
    }
    Result* finished;
    if(results_ && xQueueReceive(results_,&finished,0)==pdTRUE) {
        std::unique_ptr<Result> result(finished);busy_=false;
        if(result->url==desired_ && result->size) {
            decode(result->bytes,result->size);
        }
        retryAt_=result->url==desired_?millis()+30000:0;dirty=true;
    }
    if(!visible || url.isEmpty() || busy_ || relayBusy_ || loaded_ || (retryAt_ && int32_t(millis()-retryAt_)<0)) return dirty;
    if(viaPhone) {
        if(!relayRequest_) relayRequest_=esp_random();
        JsonDocument request;
        request["request"]=++relayRequest_;request["url"]=url;
        String body;serializeJson(request,body);
        if(companion.queueRelay(phone::Kind::Artwork,body)) { relayBusy_=true;relayAt_=millis();relayDesired_=url; }
        else retryAt_=millis()+1500;
        return dirty;
    }
    if(!started_) {
        started_=true;
        requests_=xQueueCreate(1,sizeof(Request*));results_=xQueueCreate(1,sizeof(Result*));
        if(!requests_ || !results_ || xTaskCreate(worker,"music-artwork",8192,this,1,nullptr)!=pdPASS) {
            if(requests_) vQueueDelete(requests_);if(results_) vQueueDelete(results_);
            requests_=results_=nullptr;return dirty;
        }
    }
    if(!requests_) return dirty;
    const bool privateCA=url.startsWith(settings.haUrl+"/") && !settings.haUrl.isEmpty();
    auto* request=new(std::nothrow) Request{url,privateCA?settings.haCa:String{}};
    if(request) {
        if(xQueueSend(requests_,&request,0)==pdTRUE) busy_=true;else delete request;
    }
    return dirty;
}

bool MusicArtwork::draw(M5Canvas& canvas) {
    if(!loaded_ || cached_!=desired_) return false;
    canvas.fillScreen(background_);
    image_.pushSprite(&canvas,139,111);return true;
}
