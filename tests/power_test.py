"""Exercise the firmware's CPU policy and asynchronous work guards without hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature + " {")
    return source[start:source.index("\n}", start) + 2]

harness = r'''
#include <atomic>
#include <algorithm>
#include <ArduinoJson.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using String=std::string;
uint32_t now=1000,lastCpuWork=0,frequency=240;
bool screenOff=false,cpuScaling=true,clockWorks=true;
enum class Screen { Home,Diagnostics,Clock,Badge,Timer,Stopwatch };
Screen screen=Screen::Home;
bool quickPanelOpen=false,dirty=false;
size_t diagnostic=0;
int batteryLevel=-1,batteryMv=-1;
uint32_t lastBatteryRead=0;
uint32_t lastInteraction=0;
bool displayDimmed=false;
struct {
    bool badgeAnimate=true,badgeAutoRotate=true,badgeKeepAwake=false,clockKeepAwake=false;
    uint8_t brightness=160;
} settings;
uint32_t millis() { return now; }
uint32_t getCpuFrequencyMhz() { return frequency; }
std::vector<uint32_t> changes;
bool setCpuFrequencyMhz(uint32_t value) {
    changes.push_back(value);
    if(!clockWorks && value==80) return false;
    frequency=value;return true;
}
struct { template<typename... T> void printf(const char*,T...) {} void println(const char*) {} } Serial;
struct Work {
    bool work=false;
    bool busy() const { return work; }
    bool hasWork() const { return work; }
    bool pending() const { return work; }
    bool active() const { return work; }
    bool uploading() const { return work; }
} home,homeCommand,portal,mqtt,musicArtwork;
struct : Work {
    bool availableFlag=true,animatedFlag=false;
    uint32_t motionAt=0;
    bool available() const { return availableFlag; }
    bool animated() const { return animatedFlag; }
    uint32_t lastMotion() const { return motionAt; }
} badgeApp;
struct : Work { bool playingFlag=false;bool playing() const { return playingFlag; } } notes;
struct {
    struct { bool playing=false;bool isPlaying() const { return playing; } } Speaker;
    struct Button { bool pressed=false;bool isPressed() const { return pressed; } } BtnA,BtnB;
    struct { int count=0;int getCount() const { return count; } } Touch;
    struct { int brightness=160;void setBrightness(int value) { brightness=value; } } Display;
    struct {
        int reads=0;
        int getBatteryLevel() { ++reads;return 80; }
        int getBatteryVoltage() const { return 3900; }
    } Power;
} M5;
struct Text:std::string { bool isEmpty() const { return empty(); } };
using QueueHandle_t=int*;
unsigned uxQueueMessagesWaiting(QueueHandle_t queue) { return *queue; }
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
constexpr int ESP_OK=0;
struct httpd_req_t { const char* authorization; };
int httpd_req_get_hdr_value_str(httpd_req_t* request,const char*,char* out,size_t bytes) {
    return std::snprintf(out,bytes,"%s",request->authorization)>=int(bytes)?-1:ESP_OK;
}
struct NoteTransfer {
    int lock_=0;
    uint32_t expires_=0,bytes_=123;
    bool upload_=false;
    char name_[26]="note.wav",ticket_[65]="valid";
    std::atomic<bool> transferring_{false};
    bool busy();
    bool authorize(httpd_req_t*,bool,char*,uint32_t&);
};
struct Companion {
    bool pairingFlag=false,linked_=false,authenticated_=false,reject_=false,advertise_=false;
    bool subscribed_=false,notesChanged_=false;
    Text response_;
    struct { std::string data;const std::string& value() const { return data; } } line_;
    Work relay_;
    NoteTransfer noteTransfer_;
    QueueHandle_t packets_=nullptr;
    bool pairing() const { return pairingFlag; }
    bool connected() const { return authenticated_; }
    bool hasWork();
} companion;
'''
harness += function("src/note_transfer.cpp", "bool NoteTransfer::busy()")
harness += function("src/note_transfer.cpp", "bool NoteTransfer::authorize(httpd_req_t* request, bool upload, char* reference, uint32_t& bytes)")
harness += function("src/companion.cpp", "bool Companion::hasWork()")
harness += function("src/main.cpp", "void updateCpu(uint32_t now,bool working,bool drawing=false)")
harness += function("src/main.cpp", "bool cpuWorkPending()")
source = (root / "src/main.cpp").read_text()
start = source.index("        const bool batteryDiagnostic=")
end = source.index("        if(!screenOff", start + 8)
harness += "void batteryTick() {\n" + source[start:end] + "}\n"
start = source.index("    const bool keepDisplayOn=")
end = source.index("    if (!screenOff && !keepDisplayOn", start)
harness += "bool dimTick() {\n" + source[start:end] + "    return keepDisplayOn;\n}\n"
source = (root / "src/settings.cpp").read_text()
start = source.index('    if(!input["clock_keep_awake"].isUnbound())')
end = source.index('    if(!input["timer_seconds"].isUnbound())', start)
harness += "bool updateClock(JsonObjectConst input,String& error) {\n    auto next=settings;\n" + source[start:end] + "    settings=next;return true;\n}\n"
line = next(line for line in source.splitlines() if 'out["clock_keep_awake"]' in line)
harness += "void encodeClock(JsonObject out,bool clockKeepAwake) {\n" + line + "\n}\n"
source = (root / "src/note_transfer.cpp").read_text()
start = source.index("struct TransferActivity {")
harness += source[start:source.index("\n};", start) + 3]
harness += r'''
void idle() {
    now+=2000;updateCpu(now,cpuWorkPending());
    assert(frequency==80);
}
int main() {
    updateCpu(now,cpuWorkPending());assert(frequency==240 && changes.empty());
    screenOff=true;now=2999;updateCpu(now,cpuWorkPending());assert(frequency==240);
    now=3000;updateCpu(now,cpuWorkPending());assert(frequency==80 && changes.size()==1);
    updateCpu(now,false);assert(changes.size()==1);
    screenOff=false;updateCpu(now,cpuWorkPending());assert(frequency==240);
    screenOff=true;idle();
    for(Work* work:{&home,&homeCommand,&portal,static_cast<Work*>(&badgeApp),&mqtt,&musicArtwork,static_cast<Work*>(&notes),&companion.relay_}) {
        companion.authenticated_=companion.subscribed_=true;
        work->work=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
        work->work=false;idle();
    }
    notes.playingFlag=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    notes.playingFlag=false;idle();
    M5.Speaker.playing=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    M5.Speaker.playing=false;idle();
    int packets=1;companion.packets_=&packets;
    updateCpu(++now,cpuWorkPending());assert(frequency==240);
    packets=0;idle();
    companion.notesChanged_=true;assert(companion.hasWork());
    companion.authenticated_=companion.subscribed_=false;
    assert(!companion.hasWork()); // A queued note with no phone must not prevent idle.
    companion.linked_=true;assert(companion.hasWork());companion.linked_=false;
    companion.pairingFlag=true;assert(companion.hasWork());companion.pairingFlag=false;
    auto& transfer=companion.noteTransfer_;
    transfer.expires_=now+90000;assert(cpuWorkPending());
    httpd_req_t bad{"Bearer wrong"},good{"Bearer valid"};char reference[26]{};uint32_t bytes=0;
    assert(!transfer.authorize(&bad,false,reference,bytes) && transfer.busy());
    assert(!transfer.authorize(&good,true,reference,bytes) && transfer.busy());
    assert(transfer.authorize(&good,false,reference,bytes) && !transfer.expires_ && transfer.busy());
    assert(bytes==123 && std::string(reference)=="note.wav");
    assert(!transfer.authorize(&good,false,reference,bytes)); // Ticket remains single-use.
    { const TransferActivity activity{transfer.transferring_};assert(transfer.busy()); }
    assert(!transfer.busy());
    transfer.expires_=now+10;now+=10;assert(!transfer.busy());
    constexpr auto boundary=std::numeric_limits<uint32_t>::max();
    now=boundary-1000;updateCpu(now,true);now=998;updateCpu(now,false);assert(frequency==240);
    now=999;updateCpu(now,false);assert(frequency==80);
    updateCpu(now,true);clockWorks=false;now+=2000;updateCpu(now,false);
    assert(!cpuScaling && frequency==240);
    const auto attempts=changes.size();updateCpu(++now,false);assert(changes.size()==attempts);
    screenOff=false;now=1000;batteryTick();assert(M5.Power.reads==1 && dirty);
    dirty=false;now=5999;batteryTick();assert(M5.Power.reads==1 && !dirty);
    now=6000;batteryTick();assert(M5.Power.reads==2 && !dirty);
    screenOff=true;now=35999;batteryTick();assert(M5.Power.reads==2);
    now=36000;batteryTick();assert(M5.Power.reads==3);
    screenOff=false;screen=Screen::Diagnostics;diagnostic=8;
    now=36499;batteryTick();assert(M5.Power.reads==3);
    now=36500;batteryTick();assert(M5.Power.reads==4);
    cpuScaling=clockWorks=true;frequency=240;now=100000;lastCpuWork=now;
    screen=Screen::Clock;idle();
    const auto lastInteractionBoost=lastCpuWork;
    for(int frame=0;frame<120;++frame) {
        now+=1000;updateCpu(now,cpuWorkPending(),true);assert(frequency==240);
        now+=20;updateCpu(now,cpuWorkPending());assert(frequency==80);
        assert(lastCpuWork==lastInteractionBoost); // Second ticks never hold the CPU high.
    }
    M5.BtnA.pressed=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    M5.BtnA.pressed=false;idle();
    M5.Touch.count=1;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    M5.Touch.count=0;idle();
    quickPanelOpen=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    quickPanelOpen=false;idle();
    screen=Screen::Badge;badgeApp.motionAt=now-5000;idle();
    badgeApp.animatedFlag=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    settings.badgeAnimate=false;idle();
    badgeApp.motionAt=now;updateCpu(now,cpuWorkPending());assert(frequency==240);
    now+=1999;updateCpu(now,cpuWorkPending());assert(frequency==240);
    now+=1;updateCpu(now,cpuWorkPending());assert(frequency==240);
    now+=1999;updateCpu(now,cpuWorkPending());assert(frequency==80);
    settings.badgeAutoRotate=false;badgeApp.motionAt=now;
    updateCpu(++now,cpuWorkPending());assert(frequency==80);
    badgeApp.availableFlag=false;updateCpu(++now,cpuWorkPending());assert(frequency==240);
    badgeApp.availableFlag=true;
    for(Work* work:{&home,&homeCommand,&portal,&mqtt,&musicArtwork,static_cast<Work*>(&notes),static_cast<Work*>(&badgeApp),&companion.relay_}) {
        companion.authenticated_=companion.subscribed_=true;
        work->work=true;updateCpu(++now,cpuWorkPending());assert(frequency==240);
        work->work=false;companion.notesChanged_=false;idle();
    }
    screen=Screen::Clock;settings.clockKeepAwake=true;lastInteraction=now;
    now+=20000;assert(dimTick() && !displayDimmed);
    ++now;assert(dimTick() && displayDimmed && M5.Display.brightness==48);
    lastInteraction=now;assert(dimTick() && !displayDimmed && M5.Display.brightness==160);
    now+=20001;settings.brightness=32;
    assert(dimTick() && displayDimmed && M5.Display.brightness==32);
    settings.clockKeepAwake=false;assert(!dimTick() && !displayDimmed);
    settings.clockKeepAwake=true;
    for(auto page:{Screen::Timer,Screen::Stopwatch,Screen::Home}) { screen=page;assert(!dimTick()); }
    screen=Screen::Badge;settings.brightness=160;settings.badgeKeepAwake=true;badgeApp.motionAt=now;
    assert(dimTick() && !displayDimmed);
    now+=20001;assert(dimTick() && displayDimmed);
    quickPanelOpen=true;assert(dimTick() && !displayDimmed);quickPanelOpen=false;
    JsonDocument input,output;String error;
    settings.clockKeepAwake=true;assert(updateClock(input.to<JsonObject>(),error) && settings.clockKeepAwake);
    for(const auto* invalid:{"null","0","1","\"true\"","{}","[]"}) {
        assert(!deserializeJson(input,String("{\"clock_keep_awake\":")+invalid+"}"));
        assert(!updateClock(input.as<JsonObjectConst>(),error) && settings.clockKeepAwake);
    }
    input["clock_keep_awake"]=false;assert(updateClock(input.as<JsonObjectConst>(),error) && !settings.clockKeepAwake);
    input["clock_keep_awake"]=true;assert(updateClock(input.as<JsonObjectConst>(),error) && settings.clockKeepAwake);
    encodeClock(output.to<JsonObject>(),settings.clockKeepAwake);assert(output["clock_keep_awake"].as<bool>());
    std::cout<<"OK: CPU adattiva, quadranti, Badge fermo/GIF/movimento, ponte, dimming e configurazione compatibile\n";
}
'''
with tempfile.TemporaryDirectory(prefix="stopwatch-power-") as temporary:
    source = Path(temporary) / "test.cpp"
    source.write_text(harness)
    binary = Path(temporary) / "test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / ".pio/libdeps/stopwatch/ArduinoJson/src"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
