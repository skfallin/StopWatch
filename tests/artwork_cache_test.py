"""Run the actual cover update across screen changes, late responses and changed tracks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/music_artwork.cpp").read_text()
start = source.index("bool MusicArtwork::update(")
update = source[start:source.index("\n}", start) + 2]
harness = r'''
#include "music_protocol.h"
#include "phone_relay.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
struct String:std::string {
    using std::string::string;
    using std::string::operator=;
    String(const std::string& value):std::string(value) {}
    bool isEmpty() const { return empty(); }
    bool endsWith(const String& suffix) const { return size()>=suffix.size() && compare(size()-suffix.size(),suffix.size(),suffix)==0; }
    bool startsWith(const String& prefix) const { return compare(0,prefix.size(),prefix)==0; }
    void remove(size_t offset) { erase(offset); }
    size_t length() const { return size(); }
};
void serializeJson(JsonDocument& doc,String& out) { ArduinoJson::serializeJson(doc,static_cast<std::string&>(out)); }
struct Settings { String haUrl="https://home.example",haCa; };
uint32_t now=1000;
uint32_t millis() { return now; }
uint32_t esp_random() { return 42; }
int mbedtls_base64_decode(uint8_t*,size_t,size_t*,const uint8_t*,size_t) { return -1; }
using QueueHandle_t=void*;
constexpr int pdTRUE=1,pdPASS=1;
void* queuedResult=nullptr;
int xQueueReceive(QueueHandle_t,void* target,int) {
    if(!queuedResult) return 0;
    *static_cast<void**>(target)=queuedResult;queuedResult=nullptr;return pdTRUE;
}
int xQueueSend(QueueHandle_t,void*,int) { return 0; }
QueueHandle_t xQueueCreate(int,size_t) { return nullptr; }
int xTaskCreate(void (*)(void*),const char*,int,void*,int,void*) { return 0; }
void vQueueDelete(QueueHandle_t) {}
struct Companion {
    int requests=0;
    bool pollRelay(phone::Kind,String&) { return false; }
    bool queueRelay(phone::Kind,const String&) { ++requests;return true; }
};
struct MusicArtwork {
    struct Request { String url,ca; };
    struct Result { String url;uint8_t* bytes=nullptr;size_t size=0;~Result() { free(bytes); } };
    QueueHandle_t requests_=nullptr,results_=nullptr;
    int decodes=0;
    struct { int deletes=0;void deleteSprite() { ++deletes; } } image_;
    String desired_,cached_,relayDesired_;
    bool relayBusy_=false,busy_=false,loaded_=false,started_=false;
    uint32_t relayAt_=0,relayRequest_=0,retryAt_=0;
    static void worker(void*) {}
    bool decode(const uint8_t*,size_t) { ++decodes;cached_=desired_;loaded_=true;return true; }
    bool update(const music::Playback&,const Settings&,bool,Companion&,bool);
};
'''
harness += update + r'''
int main() {
    Settings settings;Companion phone;MusicArtwork artwork;music::Playback track;
    track.image="https://i.scdn.co/image/first";
    artwork.update(track,settings,false,phone,true);
    assert(phone.requests==0 && !artwork.loaded_);
    artwork.update(track,settings,true,phone,true);assert(phone.requests==1);
    // A Wi-Fi response completes after the screen has closed: keep it in the cache.
    int resultQueue=0;
    artwork.relayBusy_=false;artwork.busy_=true;artwork.results_=&resultQueue;
    queuedResult=new MusicArtwork::Result{track.image.c_str(),nullptr,123};
    artwork.update(track,settings,false,phone,true);
    assert(artwork.loaded_ && artwork.decodes==1 && !artwork.busy_);
    const int deletes=artwork.image_.deletes;
    for(int i=0;i<4;++i) artwork.update(track,settings,i%2==0,phone,true);
    assert(phone.requests==1 && artwork.decodes==1 && artwork.image_.deletes==deletes);
    const auto previous=track.image;
    track.image="https://i.scdn.co/image/second";
    artwork.update(track,settings,false,phone,true);
    assert(!artwork.loaded_ && phone.requests==1);
    queuedResult=new MusicArtwork::Result{previous.c_str(),nullptr,123};
    artwork.update(track,settings,false,phone,true);
    assert(!artwork.loaded_ && artwork.decodes==1); // Ignore old artwork.
    artwork.update(track,settings,true,phone,true);assert(phone.requests==2);
    artwork.relayBusy_=false;track.image="http://untrusted.example/image";
    artwork.update(track,settings,true,phone,true);
    assert(artwork.desired_.isEmpty() && phone.requests==2);
    track.image="/api/media_player_proxy/media_player.spotify?token=example";
    artwork.update(track,settings,false,phone,true);
    assert(artwork.desired_.startsWith(settings.haUrl) && phone.requests==2);
    std::cout<<"OK: cache copertina, risposta tardiva, cambio brano e validazione URL\n";
}
'''
with tempfile.TemporaryDirectory(prefix="stopwatch-artwork-cache-") as temporary:
    source = Path(temporary) / "test.cpp"
    source.write_text(harness)
    binary = Path(temporary) / "test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / "include"),
                    "-I" + str(root / ".pio/libdeps/stopwatch/ArduinoJson/src"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
