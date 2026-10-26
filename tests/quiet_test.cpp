#include "controls.h"
#include "ui_layout.h"
#include "policy.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

struct String:std::string {
    using std::string::string;
    using std::string::operator=;
    bool isEmpty() const { return empty(); }
};
uint32_t now=1000;
uint32_t millis() { return now; }
struct {
    struct {
        int volume=0, tones=0, stops=0;bool active=false;
        bool begin() { return true; }
        void setVolume(int value) { volume=value; }
        void tone(int,int) { ++tones;active=true; }
        void stop() { ++stops;active=false; }
        bool isPlaying() const { return active; }
    } Speaker;
    struct {
        int motor=0,led=0,pulses=0;
        void setVibration(int value) { motor=value;if(value) ++pulses; }
        void setLed(int value) { led=value; }
    } Power;
    struct { int level=0;void setBrightness(int value) { level=value; } } Display;
    struct { int ends=0;void end() { ++ends; } } Mic;
    struct { void begin() {} bool scanID(int) { return false; } } Ex_I2C;
} M5;
struct {
    bool doNotDisturb=false,saveOk=true;
    uint8_t soundPack=1,colorTheme=0,brightness=160,volume=200,noteVolume=70,vibration=120;
    int saves=0;
    bool save() { ++saves;return saveOk; }
} settings;
struct {
    bool busyFlag=false,playingFlag=false;int captures=0,plays=0;
    bool busy() const { return busyFlag; }
    bool playing() const { return playingFlag; }
    bool start() { busyFlag=true;++captures;return true; }
    bool playLatest(uint8_t volume) { ++plays;playingFlag=true;M5.Speaker.active=true;M5.Speaker.volume=policy::speakerVolume(volume);return true; }
    void update() { if(!M5.Speaker.active) playingFlag=false; }
} notes;
struct { bool busyFlag=false;bool busy() const { return busyFlag; } } home;
struct { void abortUpload() {} void suspend() {} } badgeApp;
struct {
    std::vector<std::string> events;
    void changed() {}
    void button(const String& key,const String& event) { events.push_back(key+":"+event); }
} mqtt;
namespace sfx {
enum class Cue { Select, Complete, Error };
int plays=0;
bool ready() { return true; }
void prepare(int) {}
void play(int,Cue,int volume,bool) { ++plays;M5.Speaker.active=true;M5.Speaker.volume=policy::speakerVolume(volume); }
void stop() {}
}
namespace ui { void setTheme(int) {} }
using Screen=ui::Screen;
Screen screen=Screen::Home;
Controls controls;
ui::TouchInput touchInput;
std::array<uint16_t,24> waveform{};
bool dirty=false,screenOff=false,displayDimmed=false,ledOn=false,touchRecording=false,noticeTouch=false;
bool quickPanelOpen=false,quietSavePending=false,quietSaveFailed=false,homeAfterNote=false;
uint32_t vibrationUntil=0,lastInteraction=0,noticeAt=0;
size_t diagnostic=0;
int settingPreview=-1,displayColor=0,underlyingTouches=0,backCalls=0,nextCalls=0,selectCalls=0,stopCalls=0;
String notice,groveResult;
void changeScreen(Screen next) { screen=next; }
void backAction() { ++backCalls; }
void nextAction() { ++nextCalls; }
void selectAction(bool) { ++selectCalls; }
void stopRecording() { ++stopCalls;notes.busyFlag=false; }

#include "quiet_functions.inc"

int main() {
    JsonDocument input,output;String error;
    settings.doNotDisturb=true;
    assert(updateQuiet(input.to<JsonObject>(),error) && settings.doNotDisturb);
    for(const auto* invalid:{"null","1","0","\"true\"","{}","[]"}) {
        assert(!deserializeJson(input,String("{\"do_not_disturb\":")+invalid+"}"));
        assert(!updateQuiet(input.as<JsonObjectConst>(),error) && settings.doNotDisturb);
    }
    input["do_not_disturb"]=false;
    assert(updateQuiet(input.as<JsonObjectConst>(),error) && !settings.doNotDisturb);
    input["do_not_disturb"]=true;
    assert(updateQuiet(input.as<JsonObjectConst>(),error) && settings.doNotDisturb);
    encodeQuiet(output.to<JsonObject>(),settings.doNotDisturb);
    assert(output["do_not_disturb"].is<bool>() && output["do_not_disturb"].as<bool>());
    settings.doNotDisturb=false;
    // Activation cuts an existing note/tone and motor/LED, without changing levels.
    notes.playingFlag=true;M5.Speaker.active=true;M5.Power.motor=120;M5.Power.led=255;
    vibrationUntil=now+500;ledOn=true;
    toggleQuiet();
    assert(settings.doNotDisturb && quietSavePending);
    assert(!M5.Speaker.active && M5.Speaker.volume==0 && !M5.Power.motor && !M5.Power.led && !vibrationUntil && !ledOn);
    assert(settings.volume==200 && settings.noteVolume==70 && settings.vibration==120);
    notes.update();
    for(const auto cue:{sfx::Cue::Select,sfx::Cue::Complete,sfx::Cue::Error}) sound(cue,true);
    vibrate();vibrate(250,true);playNote();
    for(const auto* action:{"beep","vibrate","led"}) assert(!remoteOutput(action));
    for(const auto index:{3,4,5,13}) { diagnostic=index;diagnosticAction(); }
    assert(!sfx::plays && !M5.Speaker.tones && !notes.plays && !M5.Power.pulses && !M5.Power.led);
    assert(remoteOutput("badge.show") && screen==Screen::Badge);

    // Capturing still works, but neither the start pulse nor flash writes run during it.
    startRecording(false);
    assert(notes.captures==1 && notes.busy() && !M5.Power.pulses);
    flushQuiet();assert(!settings.saves && quietSavePending);
    notes.busyFlag=false;home.busyFlag=true;
    flushQuiet();assert(!settings.saves && quietSavePending);
    home.busyFlag=false;flushQuiet();flushQuiet();
    assert(settings.saves==1 && !quietSavePending && !quietSaveFailed);
    settings.saveOk=false;toggleQuiet();flushQuiet();flushQuiet();
    assert(settings.saves==2 && quietSaveFailed && !quietSavePending);
    assert(!settings.doNotDisturb && M5.Speaker.volume==255);
    settings.saveOk=true;
    sound(sfx::Cue::Select);assert(sfx::plays==1);
    M5.Speaker.stop();
    playNote();assert(notes.plays==1 && M5.Speaker.volume==policy::speakerVolume(70));
    notes.playingFlag=false;M5.Speaker.active=false;
    assert(remoteOutput("beep") && M5.Speaker.tones==1);
    assert(remoteOutput("vibrate") && M5.Power.pulses==1);
    assert(remoteOutput("led") && M5.Power.led==255);

    // Opening over a recording/card cannot stop it, toggle on release, or hit the app below.
    notes.busyFlag=true;noticeTouch=true;
    handleTouch({ui::GestureKind::QuickOpen,233,200,233,24});
    assert(quickPanelOpen && !underlyingTouches && notes.busy());
    handleTouch({ui::GestureKind::Tap,233,207,233,207});
    assert(settings.doNotDisturb && notes.busy());
    handleTouch({ui::GestureKind::Tap,233,207,90,207});
    handleTouch({ui::GestureKind::Left,130,250,350,250});
    handleTouch({ui::GestureKind::Tap,10,200,10,200});
    assert(settings.doNotDisturb && quickPanelOpen && !underlyingTouches);
    handleTouch({ui::GestureKind::QuickClose,233,100,233,250});
    assert(!quickPanelOpen && notes.busy() && !underlyingTouches);

    // Short B toggles; A closes. Neither reaches the app nor sends MQTT button actions.
    showQuickPanel(true);
    InputEvents b;b.select=true;buttons(b);
    assert(!settings.doNotDisturb && notes.busy() && !stopCalls && mqtt.events.empty());
    InputEvents a;a.next=true;buttons(a);
    assert(!quickPanelOpen && !nextCalls && !backCalls && mqtt.events.empty());
    notes.busyFlag=false;showQuickPanel(true);
    InputEvents held;held.recordStart=true;buttons(held);
    assert(!quickPanelOpen && notes.busy() && notes.captures==2);
    held={};held.recordStop=true;buttons(held);
    assert(!notes.busy() && stopCalls==1 && !selectCalls);
    showQuickPanel(true);InputEvents chord;chord.home=true;buttons(chord);
    assert(!quickPanelOpen && screen==Screen::Home && !backCalls && !nextCalls);
    std::cout<<"OK: silenzio globale, timer, note, diagnostica, MQTT/BLE, pannello e registrazione\n";
}
