#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <time.h>
#include "controls.h"
#include "companion.h"
#include "home_assistant.h"
#include "home_state.h"
#include "firmware.h"
#include "mqtt_bridge.h"
#include "portal.h"
#include "settings.h"
#include "voice_notes.h"
#include "ui_drawing.h"
#include "badge.h"
#include "clock_view.h"
#include "music_view.h"
#include "music_artwork.h"
#include "quick_settings_view.h"

namespace {
constexpr uint32_t muted = 0x9aaebd;
constexpr uint32_t accent = 0xa5ecc2, blue = 0x8cc8ff, yellow = 0xffd377, red = 0xff8993;
constexpr const char* timezoneRome = "CET-1CEST,M3.5.0,M10.5.0/3";
using Screen = ui::Screen;
const char* diagnostics[] = {"Pulsanti A e B", "Touch", "Display AMOLED", "Vibrazione", "Altoparlante",
    "Microfono", "Movimento / IMU", "Orologio RTC", "Batteria / USB", "Wi-Fi", "Bluetooth LE", "Archivio / RAM", "Porta Grove / GPIO", "LED verde"};
const char* settingNames[] = {"Luminosita", "Interazioni", "Vibrazione", "Display spento dopo", "Pacchetto suoni", "Colori", "Volume note"};
Settings settings;
VoiceNotes notes;
Badge badgeApp;
clockwork::Timer clockTimer;
clockwork::Stopwatch chronometer;
size_t clockItem=0;
size_t musicSource=0;
size_t musicMenu=0,playlistItem=0;
bool playlistRequested=false;
uint32_t lastMusicRead=0;
MusicArtwork musicArtwork;
uint32_t clockControlsUntil=0;
time_t clockDrawnSecond=-1;
HomeAssistant home;
Controls controls;
ui::TouchInput touchInput;
M5Canvas canvas(&M5.Display);
Screen screen = Screen::Home;
size_t selected = 0, systemItem = 0, diagnostic = 0, setting = 0, favorite = 0;
int settingPreview = -1;
bool touchRecording = false, noticeTouch = false, noticeDisplayed = false;
bool displayDimmed=false;
bool quickPanelOpen=false, quietSavePending=false, quietSaveFailed=false;
uint32_t badgeControlsUntil=0;
uint32_t transitionAt = 0, lastWave = 0;
uint32_t surfaceColor = ui::black, foregroundColor = ui::white;
std::array<uint16_t, 24> waveform{};
bool dirty = true, screenOff = false, memoryOk = false, settingsOk = true, homeReady = false;
bool ledOn = false, showPageQr = false, forgetOption = false;
bool homeAfterNote = false;
uint32_t lastInteraction = 0, lastDraw = 0, lastTick = 0, vibrationUntil = 0;
uint32_t lastBatteryRead=0,lastCpuWork=0;
bool cpuScaling=true;
uint32_t noticeAt = 0, reconnectAt = 0, lastHaRead = 0, lastRtcSync = 0;
uint32_t pressesA = 0, pressesB = 0;
uint8_t displayColor = 0;
int batteryLevel = -1, batteryMv = -1;
int touchX = -1, touchY = -1;
String notice, groveResult;
String activeSsid, activeWifiPassword;
m5::imu_data_t imu{};
std::array<String, policy::maxFavorites> states{}, units{};
std::array<uint32_t, policy::maxFavorites> stateUpdated{};
casa::ToggleVerification homeCommand;
uint32_t stateResetAt=0;

bool canSave() { return !home.busy() && !notes.busy(); }
void onSettingsSaved();
void changeScreen(Screen next);
void backAction();
bool remoteOutput(const String& action);
void extraStatus(JsonObject out);
void deviceStatus(JsonObject out);
Portal portal(settings, notes, canSave, onSettingsSaved);
Companion companion(settings, notes, badgeApp, canSave, onSettingsSaved, remoteOutput, extraStatus);
MqttBridge mqtt(settings, canSave, onSettingsSaved, remoteOutput, deviceStatus);

void updateCpu(uint32_t now,bool working,bool drawing=false) {
    if(working) lastCpuWork=now;
    // Both CPU frequencies retain the 80 MHz peripheral clock; radios stay connected.
    const uint32_t mhz=working || drawing || uint32_t(now-lastCpuWork)<2000?240:80;
    if(!cpuScaling || getCpuFrequencyMhz()==mhz) return;
    if(!setCpuFrequencyMhz(mhz)) {
        setCpuFrequencyMhz(240);cpuScaling=false;
        Serial.println("[power] frequenza variabile non disponibile");return;
    }
    Serial.printf("[power] cpu=%lu screen_off=%d at=%lu\n",(unsigned long)mhz,screenOff,(unsigned long)millis());
}

bool cpuWorkPending() {
    const bool idleBadge=screen==Screen::Badge && badgeApp.available() &&
        (!badgeApp.animated() || !settings.badgeAnimate) &&
        (!settings.badgeAutoRotate || uint32_t(millis()-badgeApp.lastMotion())>=2000);
    const bool passiveDisplay=!quickPanelOpen && (screen==Screen::Clock || idleBadge);
    return (!screenOff && !passiveDisplay) || M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.Touch.getCount()>0 ||
        notes.busy() || notes.playing() || M5.Speaker.isPlaying() ||
        home.busy() || homeCommand.pending() || portal.active() || badgeApp.uploading() ||
        companion.hasWork() || mqtt.hasWork() || musicArtwork.busy();
}

void sound(sfx::Cue cue, bool preview=false) {
    if (!settings.doNotDisturb && !notes.busy()) sfx::play(settings.soundPack,cue,settings.volume,preview);
}

void toast(const String& text) {
    notice = text;
    noticeAt = millis();
    dirty = true;
}

void vibrate(uint32_t duration = 35, bool test = false) {
    if (settings.doNotDisturb || notes.busy()) return;
    M5.Power.setVibration(test ? std::max(uint8_t(100), settings.vibration) : settings.vibration);
    vibrationUntil = millis() + duration;
}

void applySettings() {
    ui::setTheme(settings.colorTheme);
    if(sfx::ready()) sfx::prepare(settings.soundPack);
    if (!settings.soundPack || settings.doNotDisturb) sfx::stop();
    if (!screenOff) M5.Display.setBrightness(displayDimmed?std::min(settings.brightness,uint8_t(48)):settings.brightness);
    if(settings.doNotDisturb) {
        M5.Speaker.setVolume(0);
        M5.Speaker.stop();
        M5.Power.setVibration(0); vibrationUntil=0;
        M5.Power.setLed(0); ledOn=false;
    } else M5.Speaker.setVolume(policy::speakerVolume(notes.playing()?settings.noteVolume:settings.volume));
    dirty = true;
}

void showQuickPanel(bool open) {
    quickPanelOpen=open;
    noticeTouch=false;
    if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    if(displayDimmed) { displayDimmed=false; M5.Display.setBrightness(settings.brightness); }
    controls.suppressTapUntilReleased();
    touchInput.suppressUntilReleased();
    lastInteraction=millis();dirty=true;
}

void toggleQuiet() {
    settings.doNotDisturb=!settings.doNotDisturb;
    quietSavePending=true;quietSaveFailed=false;
    applySettings();
    mqtt.changed();
}

void playNote() {
    sfx::stop();
    if(notes.playing()) M5.Speaker.stop();
    else if(settings.doNotDisturb) toast("Non disturbare attivo");
    else if(!notes.playLatest(settings.noteVolume)) toast("Nessuna nota riproducibile");
}

void onSettingsSaved() {
    quietSavePending=quietSaveFailed=false;
    settingPreview=-1;
    touchInput.suppressUntilReleased();
    if(screen==Screen::ActionConfirm) changeScreen(Screen::HomeAssistant);
    applySettings();
    if(clockTimer.state()==clockwork::Timer::State::Idle) clockTimer.setSeconds(settings.timerSeconds);
    states.fill(""); units.fill(""); stateUpdated.fill(0);
    homeCommand.clear(); stateResetAt=millis();
    favorite = 0;
    if (activeSsid != settings.ssid || activeWifiPassword != settings.wifiPassword) reconnectAt = millis() + 600;
    mqtt.configure();
    mqtt.changed();
    toast("Configurazione salvata");
}

void connectWifi() {
    WiFi.disconnect(false, false);
    if (!settings.ssid.isEmpty()) WiFi.begin(settings.ssid.c_str(), settings.wifiPassword.c_str());
    activeSsid = settings.ssid;
    activeWifiPassword = settings.wifiPassword;
}

bool remoteOutput(const String& action) {
    if (notes.busy()) return false;
    if(action=="badge.show") { changeScreen(Screen::Badge); return true; }
    if(settings.doNotDisturb) return false;
    if (action == "vibrate") vibrate(250, true);
    else if (action == "beep") {
        if(notes.playing()) return false;
        sfx::stop();
        M5.Mic.end();
        if (!M5.Speaker.begin()) return false;
        M5.Speaker.setVolume(policy::speakerVolume(settings.volume));
        M5.Speaker.tone(880, 180);
    } else if (action == "led") { ledOn = !ledOn; M5.Power.setLed(ledOn ? 255 : 0); }
    else return false;
    return true;
}

void extraStatus(JsonObject out) {
    out["mqtt_connected"] = mqtt.connected();
    out["mqtt_device_id"] = mqtt.deviceId();
    out["connection_route"]=mqtt.connected()?"wifi":mqtt.available()?"iphone":"offline";
    if(M5.Imu.isEnabled() && (screen==Screen::Badge || M5.Imu.update())) imu=M5.Imu.getImuData();
    auto acceleration = out["acceleration"].to<JsonArray>();
    acceleration.add(imu.accel.x); acceleration.add(imu.accel.y); acceleration.add(imu.accel.z);
}

void deviceStatus(JsonObject out) { companion.status(out); }

void changeScreen(Screen next) {
    if(screen==Screen::Badge && next!=Screen::Badge) badgeApp.suspend();
    if(displayDimmed && screen!=next) { displayDimmed=false; if(!screenOff) M5.Display.setBrightness(settings.brightness); }
    if(next==Screen::Badge) { badgeControlsUntil=0; notice=""; }
    if(next==Screen::Clock) { clockControlsUntil=millis()+4000;notice=""; }
    if(next==Screen::Spotify || next==Screen::MusicSources || next==Screen::MusicMenu || next==Screen::MusicPlaylists) { lastMusicRead=millis()-5000;notice=""; }
    if(next==Screen::MusicPlaylists) { playlistRequested=false;playlistItem=0; }
    if(next==Screen::ActionConfirm) notice="";
    if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    touchInput.suppressUntilReleased();
    transitionAt=millis();
    if(next==Screen::HomeAssistant) lastHaRead=millis()-6000;
    if (screen == Screen::Companion && next != Screen::Companion) portal.stop();
    if (screen == Screen::Pairing && next != Screen::Pairing) companion.closePairing();
    if (screen == Screen::Diagnostics) {
        M5.Power.setLed(0); ledOn = false;
    }
    screen = next;
    if (next == Screen::Companion && !portal.start()) toast("Avvio Wi-Fi companion fallito");
    dirty = true;
}

void text(const String& value, int y, uint32_t color=ui::white, int size=2, int maxWidth=350) {
    const ui::Font fonts[] = {ui::Font::Small,ui::Font::Label,ui::Font::Title,ui::Font::Clock};
    ui::label(canvas,value,233,y,fonts[std::clamp(size,1,4)-1],color,surfaceColor,maxWidth);
}

void header(const char* title) {
    ui::circleButton(canvas,ui::backButton,ui::Icon::Back,24,
        surfaceColor==ui::focus ? ui::focusPanel : ui::surface,
        surfaceColor==ui::focus ? foregroundColor : ui::navigation);
    ui::label(canvas,title,244,79,ui::Font::Body,foregroundColor,surfaceColor,260);
}

void arrows() {
    const uint32_t color=surfaceColor==ui::focus?foregroundColor:ui::navigation;
    ui::icon(canvas,ui::Icon::Back,46,264,22,color,surfaceColor);
    ui::icon(canvas,ui::Icon::Next,420,264,22,color,surfaceColor);
}

void footer(const char*,const char* action) {
    arrows();
    ui::pill(canvas,233,393,190,action);
}

bool favoriteFresh() {
    return favorite<settings.favoriteCount && stateUpdated[favorite] &&
        !homeCommand.pending(favorite) &&
        policy::usableState(settings.favorites[favorite].entity.c_str(),states[favorite].c_str()) &&
        uint32_t(millis()-stateUpdated[favorite])<=casa::freshMs &&
        (settings.favoritesMqtt?mqtt.available():WiFi.status()==WL_CONNECTED);
}

int settingValue() {
    const int values[]={settings.brightness,settings.volume,settings.vibration,settings.screenTimeout,settings.soundPack,settings.colorTheme,settings.noteVolume};
    return settingPreview>=0 ? settingPreview : values[setting];
}

void drawHome() {
    if(settings.doNotDisturb) ui::icon(canvas,ui::Icon::Moon,233,24,16,ui::accent);
    tm local{};
    const time_t now=time(nullptr);
    localtime_r(&now,&local);
    char clock[8]="--:--",date[18]="STOPWATCH";
    if(now>=1704067200) {
        strftime(clock,sizeof(clock),"%H:%M",&local);
        const char* days[]={"DOM","LUN","MAR","MER","GIO","VEN","SAB"};
        snprintf(date,sizeof(date),"%s %d",days[local.tm_wday],local.tm_mday);
    }
    const String battery=batteryLevel<0?"--":String(batteryLevel)+"%";
    const auto status=ui::statusLayout(ui::textWidth(canvas,date,ui::Font::Small),ui::textWidth(canvas,battery,ui::Font::Small));
    ui::label(canvas,date,status.dateX,51,ui::Font::Small,ui::muted);
    ui::icon(canvas,WiFi.status()!=WL_CONNECTED && companion.relayReady()?ui::Icon::Bluetooth:ui::Icon::Wifi,
        status.wifiX,51,18,WiFi.status()==WL_CONNECTED || companion.relayReady()?ui::navigation:ui::muted);
    ui::label(canvas,battery,status.batteryX,51,ui::Font::Small,ui::muted,ui::black,58);
    canvas.drawRoundRect(status.batteryIconX,44,24,14,3,ui::muted);
    canvas.fillRect(status.batteryIconX+24,49,2,4,ui::muted);
    if(batteryLevel>0) canvas.fillRoundRect(status.batteryIconX+3,47,std::max(1,18*std::min(batteryLevel,100)/100),8,1,ui::muted);
    ui::label(canvas,clock,233,100,ui::Font::Clock,ui::white,ui::black,310);
    const ui::Icon icons[]={ui::Icon::House,ui::Icon::Mic,ui::Icon::Badge,ui::Icon::Clock,ui::Icon::Music,ui::Icon::Sliders};
    const char* names[]={"Casa","Note","Badge","Orologio","Spotify","Sistema"};
    const float age=std::min(1.0f,uint32_t(millis()-transitionAt)/160.0f);
    const int inset=int(3*(1-age)*(1-age));
    const ui::Circle primary=ui::homeMain;
    canvas.fillSmoothCircle(primary.x,primary.y,primary.radius-inset,ui::accent);
    ui::icon(canvas,icons[selected],primary.x,primary.y-22,56,ui::ink,ui::accent);
    ui::label(canvas,names[selected],primary.x,primary.y+41,ui::Font::Label,ui::ink,ui::accent,164);
    const ui::Circle sides[]={ui::homeLeft,ui::homeRight};
    for(int i=0;i<2;++i) {
        const size_t index=(selected+(i==0?1:ui::homeCount-1))%ui::homeCount;
        canvas.fillSmoothCircle(sides[i].x,sides[i].y,sides[i].radius,ui::surface);
        ui::icon(canvas,icons[index],sides[i].x,sides[i].y-13,30,ui::navigation,ui::surface);
        ui::label(canvas,names[index],sides[i].x,sides[i].y+26,ui::Font::Small,ui::white,ui::surface,90);
    }
    ui::dots(canvas,selected,ui::homeCount);
}

bool badgeControls() { return !badgeApp.available() || !badgeApp.error().isEmpty() || (badgeControlsUntil && int32_t(badgeControlsUntil-millis())>0); }
bool clockControls() { return time(nullptr)<1704067200 || (clockControlsUntil && int32_t(clockControlsUntil-millis())>0); }
int touchSelection() {
    if(screen==Screen::MusicSources) return int(musicSource);
    if(screen==Screen::MusicPlaylists) return mqtt.playlists().items.empty()?-1:int(playlistItem);
    return screen==Screen::Timer?int(clockTimer.state()!=clockwork::Timer::State::Idle):int(selected);
}
void clockPage(int index) {
    const Screen pages[]={Screen::Clock,Screen::Timer,Screen::Stopwatch};
    clockItem=index;changeScreen(pages[index]);
}
void nextClockFace() {
    Settings next=settings;next.clockFace=(next.clockFace+1)%clockwork::faceIds.size();
    if(next.save()) settings=next;else toast("Quadrante non salvato");
    clockControlsUntil=millis()+4000;dirty=true;sound(sfx::Cue::Select);
}
void timerSecondaryAction() {
    using State=clockwork::Timer::State;
    switch(clockTimer.state()) {
    case State::Idle: {
        const uint32_t current=clockTimer.duration()/1000;
        const auto next=std::upper_bound(clockwork::presets.begin(),clockwork::presets.end(),current);
        clockTimer.setSeconds(next==clockwork::presets.end()?clockwork::presets.front():*next);break;
    }
    case State::Running: changeScreen(Screen::ClockMenu);break;
    case State::Paused: clockTimer.reset();break;
    case State::Done: clockTimer.restart(millis());break;
    }
    dirty=true;sound(sfx::Cue::Select);
}
void stopwatchSecondaryAction() {
    if(chronometer.running()) chronometer.mark(millis());else chronometer.reset();
    dirty=true;sound(sfx::Cue::Select);
}

void drawBadge() {
    if(!badgeApp.draw(canvas,settings.badgeAutoRotate)) {
        header("Badge");
        ui::icon(canvas,ui::Icon::Badge,233,200,78,ui::accent);
        text("Il tuo Badge.",281,ui::white,2);
        text(badgeApp.error().isEmpty()?"Carica foto o GIF dall'iPhone":badgeApp.error(),331,ui::muted,1,310);
        return;
    }
    if(badgeControls()) {
        canvas.fillSmoothRoundRect(155,51,177,56,28,ui::surface);
        header("Badge");
        ui::pill(canvas,233,323,240,settings.badgeAutoRotate?"Rotazione: auto":"Fissa · B ruota 45°");
        ui::pill(canvas,233,389,240,badgeApp.animated()?(settings.badgeAnimate?"Pausa GIF":"Riprendi GIF"):"Torna alla home");
    }
}

void drawSystem() {
    header("Sistema");
    const char* titles[]={"Impostazioni","Diagnostica","iPhone","Browser Wi-Fi"};
    const ui::Icon icons[]={ui::Icon::Sliders,ui::Icon::Activity,ui::Icon::Bluetooth,ui::Icon::Link};
    for(int i=0;i<4;++i) {
        const int y=152+i*71;
        const uint32_t fill=i==systemItem?ui::selectionSurface:ui::surface;
        canvas.fillSmoothRoundRect(85,y-31,296,62,24,fill);
        if(i==systemItem) canvas.fillSmoothCircle(96,y,3,ui::accent);
        ui::icon(canvas,icons[i],119,y,25,ui::accent,fill);
        ui::label(canvas,titles[i],238,y,ui::Font::Body,ui::white,fill,200);
        ui::icon(canvas,ui::Icon::Next,355,y,17,ui::muted,fill);
    }
}

void drawSettings() {
    header("Impostazioni");
    const ui::Icon icons[]={ui::Icon::Sun,ui::Icon::Speaker,ui::Icon::Activity,ui::Icon::Clock,ui::Icon::Speaker,ui::Icon::Sliders,ui::Icon::Mic};
    const int value=settingValue();
    ui::label(canvas,settingNames[setting],233,140,ui::Font::Label,ui::white);
    ui::icon(canvas,icons[setting],233,187,40,ui::accent);
    if (setting==4 || setting==5) ui::label(canvas,setting==4?sfx::packNames[value]:ui::themes[value].name,233,264,ui::Font::Title,ui::white);
    else {
        const int displayValue=setting==3?value:value*100/((setting==1 || setting==6)?200:255);
        ui::label(canvas,String(displayValue)+(setting==3?" s":"%"),233,264,ui::Font::Clock,ui::white);
    }
    arrows();
    const int minimum=setting==0?32:setting==3?15:0,maximum=(setting==1 || setting==6)?200:setting==3?180:setting==4?12:setting==5?5:255;
    const int position=109+(std::clamp(value,minimum,maximum)-minimum)*248/(maximum-minimum);
    canvas.fillSmoothRoundRect(109,339,248,8,4,ui::line);
    canvas.fillSmoothRoundRect(109,339,std::max(8,position-109),8,4,ui::accent);
    canvas.fillSmoothCircle(position,343,15,ui::accent);
    if (setting==4) ui::label(canvas,"Trascina o premi B · ascolta",233,386,ui::Font::Small,ui::muted);
    else if(setting==5) ui::label(canvas,"Trascina o premi B · anteprima",233,386,ui::Font::Small,ui::muted);
    else {
        ui::label(canvas,setting==3?"15 s":"Min",114,386,ui::Font::Small,ui::muted,ui::black,65);
        ui::label(canvas,setting==3?"180 s":"Max",350,386,ui::Font::Small,ui::muted,ui::black,65);
    }
    ui::dots(canvas,setting,7);
}

void drawCasa() {
    header("Casa");
    if(!settings.favoriteCount) {
        ui::icon(canvas,ui::Icon::House,233,210,76,ui::accent);
        text("Scegli i preferiti",290,ui::white,2);
        text("da Home Assistant",332,ui::muted,1);
        return;
    }
    const bool fresh=favoriteFresh(),on=fresh&&states[favorite]=="on";
    const auto service=policy::service(settings.favorites[favorite].entity.c_str());
    const bool toggle=service=="toggle";
    const uint32_t secondary=surfaceColor==ui::focus?ui::ink:ui::muted;
    const String& entity=settings.favorites[favorite].entity;
    const bool light=entity.startsWith("light.");
    ui::label(canvas,settings.favorites[favorite].label,233,137,ui::Font::Label,foregroundColor,surfaceColor,330);
    ui::icon(canvas,light?ui::Icon::Bulb:service.empty()?ui::Icon::Activity:ui::Icon::House,233,194,50,foregroundColor,surfaceColor);
    String value,detail;
    ui::Font font=ui::Font::Number;
    if(toggle) {
        value=fresh?(on?"ON":"OFF"):"--";
        detail=fresh?(light?(on?"Accesa":"Spenta"):(on?"Attivo":"Disattivo")):"Stato non aggiornato";
    } else if(!service.empty()) {
        value=service=="press"?"Premi":"Avvia"; font=ui::Font::Title;
        detail="Tocca per confermare";
    } else {
        value=fresh?states[favorite]:"--";
        char* end=nullptr; strtof(value.c_str(),&end);
        if(end==value.c_str() || *end!='\0') font=ui::Font::Title;
        else font=ui::Font::Clock;
        detail=fresh?units[favorite]:"Stato non aggiornato";
    }
    ui::label(canvas,value,233,277,font,foregroundColor,surfaceColor,310);
    if(homeCommand.pending(favorite)) detail="Verifica stato…";
    else if(settings.favoritesMqtt && !mqtt.available()) detail="Collega Wi-Fi o iPhone";
    else if(!settings.favoritesMqtt && WiFi.status()!=WL_CONNECTED) detail="Wi-Fi non connesso";
    else if(!fresh && (settings.favoritesMqtt?mqtt.busy():home.busy())) detail="Aggiornamento…";
    else if(states[favorite]=="unavailable" || states[favorite]=="unknown") detail="Entita non disponibile";
    ui::label(canvas,detail,233,386,ui::Font::Body,secondary,surfaceColor,310);
    arrows();
    ui::dots(canvas,favorite,settings.favoriteCount,419,foregroundColor,surfaceColor==ui::focus?ui::focusPanel:ui::line);
}

void drawNotes() {
    header("Note");
    text("Un'idea al volo.",143,ui::white,2);
    ui::circleButton(canvas,ui::recordButton,ui::Icon::Mic,57,ui::accent,ui::ink);
    ui::label(canvas,String(notes.count())+" note sul dispositivo",233,335,ui::Font::Small,ui::muted);
    ui::pill(canvas,233,386,230,notes.playing()?"Ferma ascolto":"Ultima nota");
    if(!notes.playing()) ui::icon(canvas,ui::Icon::Play,143,386,20,ui::white,ui::surface);
}

void drawRecording() {
    const auto state=notes.state();
    if(state==VoiceNotes::State::Preparing) {
        ui::icon(canvas,ui::Icon::Mic,233,207,86,ui::accent);
        text("Preparazione",119,ui::white,2);
        text("Attendi REC",299,ui::accent,2);
        text("Il microfono si sta attivando",342,ui::muted,1);
    } else if(state==VoiceNotes::State::Saving) {
        ui::icon(canvas,ui::Icon::Archive,233,222,76,ui::accent);
        text("Salvataggio…",315,ui::white,2);
    } else {
        canvas.fillSmoothCircle(144,86,5,red);
        ui::label(canvas,"REGISTRAZIONE",245,86,ui::Font::Small,ui::ink,surfaceColor,245);
        const int seconds=int(notes.seconds());
        char duration[12]; snprintf(duration,sizeof(duration),"%d:%02d",seconds/60,seconds%60);
        ui::label(canvas,duration,233,191,ui::Font::Number,ui::ink,surfaceColor,330);
        for(size_t i=0;i<waveform.size();++i) {
            const int height=std::clamp(int(waveform[i])*66/16000,6,70);
            canvas.fillSmoothRoundRect(95+i*12,280-height/2,7,height,3,ui::ink);
        }
    }
    canvas.fillSmoothCircle(233,382,44,state==VoiceNotes::State::Recording?ui::ink:ui::surface);
    canvas.fillSmoothRoundRect(220,369,26,26,5,state==VoiceNotes::State::Recording?ui::focus:ui::white);
    ui::label(canvas,touchRecording?"Tocca per salvare":"Rilascia B per salvare",233,435,ui::Font::Small,
        state==VoiceNotes::State::Recording?ui::ink:ui::muted,surfaceColor,216);
}

void drawSaved() {
    ui::icon(canvas,ui::Icon::Check,233,161,87,ui::ink,ui::focus);
    ui::label(canvas,"Idea salvata.",233,256,ui::Font::Title,ui::ink,ui::focus);
    ui::label(canvas,"Sul dispositivo",233,313,ui::Font::Body,ui::ink,ui::focus);
    ui::pill(canvas,233,386,206,"Fine",ui::ink,ui::focus);
}

void drawPairing() {
    header("iPhone");
    if(companion.pairing()) {
        char code[7]; snprintf(code,sizeof(code),"%06lu",(unsigned long)companion.passkey());
        ui::label(canvas,code,233,224,ui::Font::Clock,ui::white,ui::black,340);
        text("Inserisci il codice su iPhone",296,ui::muted,1);
        text("Abbinamento aperto per 90 s",330,ui::muted,1);
        ui::pill(canvas,233,386,240,"Nuovo codice");
    } else {
        ui::icon(canvas,ui::Icon::Bluetooth,233,159,55,ui::accent);
        text(companion.connected()?"iPhone connesso":"Pronto per abbinare",219,ui::white,2);
        text(String(companion.bondedPhones())+" abbinamenti salvati",256,ui::muted,1);
        ui::pill(canvas,233,311,296,"Abbina iPhone",forgetOption?ui::surface:ui::accent,forgetOption?ui::white:ui::ink);
        ui::pill(canvas,233,382,296,"Rimuovi abbinamenti",forgetOption?ui::accent:ui::surface,forgetOption?ui::ink:ui::white);
    }
}

void drawConfirmation() {
    header(screen==Screen::ActionConfirm?"Conferma":"Attenzione");
    ui::icon(canvas,ui::Icon::Warning,233,157,53,screen==Screen::ActionConfirm?ui::accent:red);
    if(screen==Screen::ActionConfirm) {
        text("Eseguire il comando?",215,ui::white,2);
        text(favorite<settings.favoriteCount?settings.favorites[favorite].label:"Preferito modificato",260,ui::muted,1);
        ui::pill(canvas,157,331,115,"Annulla");
        ui::pill(canvas,309,331,115,"Esegui",ui::accent,ui::ink);
    } else {
        text(screen==Screen::FormatConfirm?"Eliminare note e Badge?":"Rimuovere gli iPhone?",223,ui::white,2);
        text(screen==Screen::FormatConfirm?"L'archivio sara cancellato":"Tutti gli abbinamenti saranno rimossi",270,ui::muted,1);
        text("Conferma con il pulsante B",320,red,1);
        ui::pill(canvas,233,386,266,"Annulla");
    }
}

void drawPortal() {
    header("Browser Wi-Fi");
    if(portal.active()) {
        canvas.fillRoundRect(132,129,202,202,10,ui::white);
        const String qr=showPageQr?portal.url():portal.wifiQr();
        canvas.qrcode(qr.c_str(),141,138,184,showPageQr?3:4,true);
        text(showPageQr?"2 · Apri la pagina":"1 · Unisci il Wi-Fi",350,ui::white,1);
        text(String("stopwatch / ")+portal.password(),385,ui::accent,1,305);
        text("Tocca per cambiare QR",421,ui::muted,1,230);
    } else {
        text("Sessione terminata",223,ui::white,2);
        ui::pill(canvas,233,386,266,"Riapri");
    }
}

void drawNotice() {
    noticeDisplayed=!notice.isEmpty() && uint32_t(millis()-noticeAt)<4500 && !notes.busy() &&
        screen!=Screen::Companion && screen!=Screen::NoteSaved;
    if(!noticeDisplayed) return;
    const auto card=ui::noticeCard;
    canvas.fillSmoothRoundRect(card.x,card.y,card.width,card.height,22,ui::line);
    canvas.fillSmoothRoundRect(card.x+1,card.y+1,card.width-2,card.height-2,21,ui::surface);
    ui::icon(canvas,ui::Icon::Close,card.x+card.width-20,card.y+card.height/2,16,ui::muted,ui::surface);
    const int contentWidth=card.width-60, textX=card.x+20+contentWidth/2, textY=card.y+card.height/2;
    String first=notice,second;
    while(first.length()>1) {
        if(ui::textWidth(canvas,first,ui::Font::Small)<=contentWidth) break;
        const int space=first.lastIndexOf(' ');
        if(space<=0) break;
        second=first.substring(space+1)+(second.isEmpty()?"":" "+second);
        first=first.substring(0,space);
    }
    ui::label(canvas,first,textX,second.isEmpty()?textY:textY-11,ui::Font::Small,ui::white,ui::surface,contentWidth);
    if(!second.isEmpty()) ui::label(canvas,second,textX,textY+13,ui::Font::Small,ui::white,ui::surface,contentWidth);
}

void drawDiagnostics() {
    header("Diagnostica");
    text(diagnostics[diagnostic],135,ui::white,2);
        switch (diagnostic) {
        case 0:
            text(String("A: ") + (M5.BtnA.isPressed() ? "PREMUTO" : "rilasciato"), 207, yellow);
            text(String("B: ") + (M5.BtnB.isPressed() ? "PREMUTO" : "rilasciato"), 253, blue);
            text(String("Pressioni A / B: ") + pressesA + " / " + pressesB, 305, muted, 1); break;
        case 1:
            text(M5.Touch.isEnabled() ? "Tocca lo schermo" : "Touch non rilevato", 215, blue);
            text(String("X ") + touchX + "   Y " + touchY, 273);
            if (touchX >= 0) { canvas.drawCircle(touchX, touchY, 12, accent); } break;
        case 2: {
            const uint32_t colors[] = {red, accent, blue, 0xffffff, 0x000000};
            canvas.fillRoundRect(95, 177, 276, 132, 15, colors[displayColor]);
            canvas.drawRoundRect(95, 177, 276, 132, 15, muted);
            text("B cambia colore · verifica i pixel", 332, muted, 1); break;
        }
        case 3: text("B: impulso di 250 ms", 225, blue); text("Verifica manuale della vibrazione", 285, muted, 1); break;
        case 4: text("B: tono breve", 216, blue); text(String("Volume: ") + settings.volume*100/200 + "%", 270); break;
        case 5:
            text(M5.Mic.isEnabled() ? "Microfono configurato" : "Microfono assente", 209, blue);
            text("Tieni B per una nota di prova", 258, muted, 1);
            text("B breve riascolta l'ultima nota", 302, muted, 1); break;
        case 6:
            text(M5.Imu.isEnabled() ? "BMI270 rilevato" : "IMU non rilevata", 191, blue);
            text(String("Accel ") + String(imu.accel.x, 2) + " " + String(imu.accel.y, 2) + " " + String(imu.accel.z, 2), 242, 0xffffff, 1);
            text(String("Gyro ") + String(imu.gyro.x, 1) + " " + String(imu.gyro.y, 1) + " " + String(imu.gyro.z, 1), 283, 0xffffff, 1); break;
        case 7: {
            m5::rtc_datetime_t rtc{};
            const bool valid = M5.Rtc.getDateTime(&rtc);
            text(valid ? "RTC rilevato · UTC" : "RTC non leggibile", 198, blue);
            char stamp[40];
            snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d  %02d:%02d:%02d", rtc.date.year, rtc.date.month, rtc.date.date, rtc.time.hours, rtc.time.minutes, rtc.time.seconds);
            text(stamp, 248, 0xffffff, 1);
            text(M5.Rtc.getVoltLow() ? "Ora da sincronizzare" : "Sincronizza da Wi-Fi o iPhone", 300, muted, 1); break;
        }
        case 8:
            text(String(batteryMv) + " mV", 201, blue, 3);
            text(String("Carica stimata: ") + batteryLevel + "%", 260, 0xffffff, 1);
            text(String("USB: ") + M5.Power.getVBUSVoltage() + " mV", 293, muted, 1);
            text(M5.Power.isCharging() == m5::Power_Class::is_charging ? "In carica" : "Carica non attiva / non rilevata", 325, muted, 1); break;
        case 9:
            text(WiFi.status() == WL_CONNECTED ? "CONNESSO" : "NON CONNESSO", 198, blue);
            text(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "B apre la configurazione", 247, 0xffffff, 1);
            text(String("Segnale: ") + (WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "--"), 294, muted, 1); break;
        case 10:
            text(companion.connected() ? "iPhone connesso e autenticato" : companion.ready() ? "Companion BLE attivo" : "BLE non disponibile", 204, blue, 1);
            text(companion.name(), 253, 0xffffff, 1);
            text(String(companion.bondedPhones()) + " abbinamenti salvati", 295, muted, 1);
            text("B: abbinamento iPhone", 327, muted, 1); break;
        case 11:
            text(memoryOk ? "PSRAM: prova superata" : "PSRAM: prova fallita", 190, memoryOk ? accent : red, 1);
            text(String("PSRAM libera: ") + ESP.getFreePsram() / 1024 + " KB", 230, muted, 1);
            text(notes.ready() ? String("Archivio: ") + LittleFS.usedBytes() / 1024 + " / " + LittleFS.totalBytes() / 1024 + " KB" : "Archivio non inizializzato", 270, blue, 1);
            text("B: inizializza / cancella archivio", 312, muted, 1); break;
        case 12:
            text("Grove I2C: SDA 10 / SCL 11", 196, blue, 1);
            text(groveResult.isEmpty() ? "B: scansione indirizzi I2C" : groveResult, 244, 0xffffff, 1);
            text("Uscita 5 V disattivata", 285, muted, 1);
            text("GPIO posteriori: serve un fixture", 323, muted, 1); break;
        case 13: text(ledOn ? "LED acceso" : "LED spento", 222, blue); text("B cambia · verifica visiva", 281, muted, 1); break;
        }
    footer("Test dopo","Prova");
}

void draw() {
    const bool lit=screen==Screen::HomeAssistant && favoriteFresh() && states[favorite]=="on";
    const bool recording=notes.state()==VoiceNotes::State::Recording;
    surfaceColor=!quickPanelOpen && (recording || (!notes.busy() && (lit || screen==Screen::NoteSaved)))?ui::focus:ui::black;
    foregroundColor=surfaceColor==ui::focus?ui::ink:ui::white;
    canvas.fillScreen(surfaceColor);
    if(quickPanelOpen) ui::quickSettings(canvas,settings.doNotDisturb,quietSavePending,quietSaveFailed);
    else if(notes.busy()) drawRecording();
    else switch(screen) {
    case Screen::Home: drawHome(); break;
    case Screen::System: drawSystem(); break;
    case Screen::Settings: drawSettings(); break;
    case Screen::HomeAssistant: drawCasa(); break;
    case Screen::Notes: drawNotes(); break;
    case Screen::Badge: drawBadge(); break;
    case Screen::Spotify: {
        const bool artwork=musicArtwork.draw(canvas);
        const String message=!mqtt.available()?"Collega Wi-Fi o iPhone":mqtt.musicMessage();
        music_ui::player(canvas,mqtt.playback(),millis(),mqtt.musicBusy(),message,artwork,musicArtwork.background());
        break;
    }
    case Screen::MusicSources: music_ui::sources(canvas,mqtt.playback(),musicSource,mqtt.musicBusy());break;
    case Screen::MusicMenu: music_ui::menu(canvas,musicMenu);break;
    case Screen::MusicPlaylists: music_ui::playlists(canvas,mqtt.playlists(),playlistItem,mqtt.musicBusy(),mqtt.musicMessage());break;
    case Screen::ClockMenu: clock_ui::menu(canvas,clockItem);break;
    case Screen::Clock: {
        tm local{};const time_t epoch=time(nullptr);localtime_r(&epoch,&local);
        clock_ui::watch(canvas,clockwork::Face(settings.clockFace),local,epoch>=1704067200,clockControls());
        clockDrawnSecond=epoch;break;
    }
    case Screen::Timer: clock_ui::timer(canvas,clockTimer);break;
    case Screen::Stopwatch: clock_ui::stopwatch(canvas,chronometer);break;
    case Screen::NoteSaved: drawSaved(); break;
    case Screen::Pairing: drawPairing(); break;
    case Screen::Companion: drawPortal(); break;
    case Screen::ActionConfirm:
    case Screen::FormatConfirm:
    case Screen::ForgetConfirm: drawConfirmation(); break;
    case Screen::Diagnostics: drawDiagnostics(); break;
    }
    if(quickPanelOpen) noticeDisplayed=false;
    else drawNotice();
    canvas.pushSprite(0,0);
    dirty=false;
    lastDraw=millis();
}

void diagnosticAction() {
    sfx::stop();
    if(settings.doNotDisturb && (diagnostic==3 || diagnostic==4 || diagnostic==5 || diagnostic==13)) {
        toast("Non disturbare attivo");return;
    }
    switch (diagnostic) {
    case 2: displayColor = (displayColor + 1) % 5; break;
    case 3: vibrate(250, true); break;
    case 4:
        if(notes.playing()) { toast("Ferma prima l'ascolto della nota"); break; }
        M5.Mic.end();
        if (M5.Speaker.begin()) { M5.Speaker.setVolume(policy::speakerVolume(settings.volume)); M5.Speaker.tone(880, 180); }
        else toast("Avvio altoparlante fallito");
        break;
    case 5: playNote(); break;
    case 9: changeScreen(Screen::Companion); break;
    case 10: changeScreen(Screen::Pairing); break;
    case 11: changeScreen(Screen::FormatConfirm); break;
    case 12: {
        groveResult = "";
        M5.Ex_I2C.begin();
        for (uint8_t address = 8; address < 0x78; ++address) {
            if (M5.Ex_I2C.scanID(address)) {
                char hex[8]; snprintf(hex, sizeof(hex), "0x%02x ", address); groveResult += hex;
            }
        }
        if (groveResult.isEmpty()) groveResult = "Nessun dispositivo alimentato rilevato";
        break;
    }
    case 13: ledOn = !ledOn; M5.Power.setLed(ledOn ? 255 : 0); break;
    default: break;
    }
}

void commitSetting(int value) {
    Settings next=settings;
    switch(setting) {
    case 0: next.brightness=value; break;
    case 1: next.volume=value; break;
    case 2: next.vibration=value; break;
    case 3: next.screenTimeout=value; break;
    case 4: next.soundPack=value; break;
    case 5: next.colorTheme=value; break;
    case 6: next.noteVolume=value; break;
    }
    settingPreview=-1;
    if(!next.save()) { applySettings(); toast("Impostazioni non salvate"); return; }
    settings=next;
    applySettings();
    mqtt.changed();
    if(setting==2) vibrate(120);
    sound(setting==4?sfx::Cue::Success:setting==1?sfx::Cue::Volume:sfx::Cue::Select,true);
    dirty=true;
}

bool executeFavorite() {
    if(favorite>=settings.favoriteCount) { toast("Scegli i preferiti in Home Assistant"); return false; }
    if(settings.favoritesMqtt?mqtt.busy():home.busy()) { toast("Attendi la risposta di Home Assistant"); return false; }
    const auto service=policy::service(settings.favorites[favorite].entity.c_str());
    const bool action=!service.empty();
    if(action && homeCommand.pending()) { toast("Attendi la verifica del comando"); return false; }
    if(service=="toggle" && !favoriteFresh()) {
        if(settings.favoritesMqtt) mqtt.requestFavorite(favorite,false);
        else if(homeReady) home.request(settings,favorite,false);
        toast("Aggiorno lo stato: riprova"); return false;
    }
    if(settings.favoritesMqtt?mqtt.requestFavorite(favorite,action):homeReady&&home.request(settings,favorite,action)) {
        lastHaRead=millis();
        if(service=="toggle") homeCommand.begin(favorite,states[favorite]=="on",millis());
        if(action) stateUpdated[favorite]=0;
        if(action) toast("Invio comando…");
        else dirty=true;
        return true;
    } else toast("Controlla rete e orologio");
    return false;
}

bool musicCommand(music::Command command,const String& source="") {
    if(mqtt.requestMusic(command,source)) { lastMusicRead=millis();dirty=true;sound(sfx::Cue::Select);return true; }
    else if(!mqtt.available()) toast("Collega Wi-Fi o iPhone");
    else if(mqtt.musicBusy()) toast("Attendi Spotify");
    else if(!mqtt.playback().fresh(millis())) { mqtt.requestMusic();toast("Aggiorno Spotify…"); }
    else toast("Comando non disponibile sul lettore");
    return false;
}

void playlistPage(uint32_t offset) {
    if(mqtt.requestMusic(music::Command::Playlists,"",offset)) {
        playlistRequested=true;playlistItem=0;lastMusicRead=millis();dirty=true;
    } else toast(mqtt.available()?"Attendi Spotify":"Collega Wi-Fi o iPhone");
}

void selectAction(bool fromButton=false) {
    switch(screen) {
    case Screen::Home: {
        const Screen pages[]={Screen::HomeAssistant,Screen::Notes,Screen::Badge,Screen::ClockMenu,Screen::Spotify,Screen::System};
        changeScreen(pages[selected]); sound(sfx::Cue::Open); break;
    }
    case Screen::System: {
        const Screen pages[]={Screen::Settings,Screen::Diagnostics,Screen::Pairing,Screen::Companion};
        changeScreen(pages[systemItem]); sound(sfx::Cue::Open); break;
    }
    case Screen::Settings:
        switch(setting) {
        case 0: commitSetting(settings.brightness>=240?32:std::min(255,int(settings.brightness)+32)); break;
        case 1: commitSetting(settings.volume>=200?0:std::min(200,int(settings.volume)+20)); break;
        case 2: commitSetting(settings.vibration>=240?0:std::min(255,int(settings.vibration)+40)); break;
        case 3: commitSetting(settings.screenTimeout>=180?15:settings.screenTimeout+15); break;
        case 4: commitSetting((settings.soundPack+1)%sfx::packIds.size()); break;
        case 5: commitSetting((settings.colorTheme+1)%ui::themes.size()); break;
        case 6: commitSetting(settings.noteVolume>=200?0:std::min(200,int(settings.noteVolume)+20)); break;
        }
        break;
    case Screen::Diagnostics: diagnosticAction(); break;
    case Screen::Spotify: musicCommand(mqtt.playback().playing?music::Command::Pause:music::Command::Play);break;
    case Screen::MusicMenu:
        if(musicMenu==0) changeScreen(Screen::MusicPlaylists);
        else {
            musicSource=0;const auto& p=mqtt.playback();
            for(size_t i=0;i<p.sources.size();++i) if(p.sources[i]==p.source) musicSource=i;
            changeScreen(Screen::MusicSources);
        }
        sound(sfx::Cue::Open);break;
    case Screen::MusicPlaylists:
        if(mqtt.playlists().items.empty()) { playlistRequested=false;dirty=true; }
        else if(playlistItem<mqtt.playlists().items.size() && musicCommand(music::Command::Playlist,mqtt.playlists().items[playlistItem].uri.c_str())) changeScreen(Screen::Spotify);
        break;
    case Screen::MusicSources:
        if(musicSource<mqtt.playback().sources.size()) {
            if(musicCommand(music::Command::Source,mqtt.playback().sources[musicSource].c_str())) changeScreen(Screen::Spotify);
        }
        break;
    case Screen::ClockMenu: clockPage(clockItem);sound(sfx::Cue::Open);break;
    case Screen::Clock:
        if(fromButton) backAction();else nextClockFace();
        break;
    case Screen::Timer:
        if(clockTimer.state()==clockwork::Timer::State::Idle && settings.timerSeconds!=clockTimer.duration()/1000) {
            Settings next=settings;next.timerSeconds=clockTimer.duration()/1000;
            if(next.save()) settings=next;else toast("Durata non salvata");
        }
        clockTimer.toggle(millis());sound(sfx::Cue::Select);break;
    case Screen::Stopwatch:
        if(chronometer.elapsed()==clockwork::Stopwatch::maxElapsed) backAction();else chronometer.toggle(millis());
        sound(sfx::Cue::Select);break;
    case Screen::Badge:
        if(!badgeApp.available()) { toast("Apri Badge nell'app iPhone"); break; }
        if(fromButton && !settings.badgeAutoRotate) {
            badgeApp.rotateManual(); badgeControlsUntil=0;
            sound(sfx::Cue::Select);
        } else if(!badgeControls()) badgeControlsUntil=millis()+4000;
        else if(badgeApp.animated()) {
            Settings next=settings; next.badgeAnimate=!next.badgeAnimate;
            if(next.save()) settings=next; else toast("Impostazioni Badge non salvate");
            badgeControlsUntil=millis()+4000;
        } else backAction();
        break;
    case Screen::HomeAssistant:
        if(favorite<settings.favoriteCount && policy::confirmAction(settings.favorites[favorite].entity.c_str())) {
            if(homeCommand.pending()) { toast("Attendi la verifica del comando"); break; }
            changeScreen(Screen::ActionConfirm);
            sound(sfx::Cue::Warning);
        } else executeFavorite();
        break;
    case Screen::ActionConfirm:
        if(!ui::confirmationReady(millis(),transitionAt)) break;
        if(executeFavorite()) { changeScreen(Screen::HomeAssistant);lastHaRead=millis(); }
        break;
    case Screen::Notes:
        playNote();
        break;
    case Screen::NoteSaved: changeScreen(Screen::Notes); break;
    case Screen::Pairing:
        if(forgetOption && !companion.pairing()) changeScreen(Screen::ForgetConfirm);
        else if(companion.ready()) { companion.openPairing(); forgetOption=false; }
        else toast("Companion BLE non disponibile");
        break;
    case Screen::ForgetConfirm:
        companion.forgetPhones(); forgetOption=false; changeScreen(Screen::Pairing); toast("Abbinamenti rimossi"); break;
    case Screen::Companion:
        if(!portal.active()) portal.start(); else showPageQr=!showPageQr;
        break;
    case Screen::FormatConfirm:
        sfx::stop();
        badgeApp.abortUpload();
        { const bool initialized=notes.initializeStorage(); badgeApp.begin();
          if(initialized) sfx::begin(settings.soundPack);
          toast(initialized?"Archivio inizializzato":"Inizializzazione archivio fallita"); }
        changeScreen(Screen::Diagnostics); break;
    }
    dirty=true;
}

void backAction() {
    switch(screen) {
    case Screen::Settings:
    case Screen::Diagnostics:
    case Screen::Pairing:
    case Screen::Companion: changeScreen(Screen::System); break;
    case Screen::FormatConfirm: changeScreen(Screen::Diagnostics); break;
    case Screen::ForgetConfirm: changeScreen(Screen::Pairing); break;
    case Screen::ActionConfirm: changeScreen(Screen::HomeAssistant); break;
    case Screen::MusicSources:
    case Screen::MusicPlaylists: changeScreen(Screen::MusicMenu);break;
    case Screen::MusicMenu: changeScreen(Screen::Spotify);break;
    case Screen::Clock:
    case Screen::Timer:
    case Screen::Stopwatch: changeScreen(Screen::ClockMenu);break;
    default: changeScreen(Screen::Home); break;
    }
    sound(sfx::Cue::Back);
}

void nextAction(int direction=1) {
    if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    switch(screen) {
    case Screen::Home: selected=(selected+ui::homeCount+direction)%ui::homeCount; transitionAt=millis(); break;
    case Screen::ClockMenu: clockItem=(clockItem+3+direction)%3;break;
    case Screen::Spotify: musicCommand(direction>0?music::Command::Next:music::Command::Previous);return;
    case Screen::MusicMenu: musicMenu=(musicMenu+1)%2;break;
    case Screen::MusicPlaylists: {
        const auto& page=mqtt.playlists();
        if(page.items.empty()) { playlistRequested=false;break; }
        if(direction>0 && playlistItem+1==page.items.size() && (page.hasNext() || page.offset)) {
            playlistPage(page.hasNext()?page.offset+music::playlistPageSize:0);return;
        }
        playlistItem=(playlistItem+page.items.size()+direction)%page.items.size();break;
    }
    case Screen::MusicSources:
        if(!mqtt.playback().sources.empty()) musicSource=(musicSource+mqtt.playback().sources.size()+direction)%mqtt.playback().sources.size();
        break;
    case Screen::Clock: nextClockFace();return;
    case Screen::Timer: timerSecondaryAction();return;
    case Screen::Stopwatch: stopwatchSecondaryAction();return;
    case Screen::System: systemItem=(systemItem+4+direction)%4; break;
    case Screen::Settings: setting=(setting+7+direction)%7; break;
    case Screen::Diagnostics:
        M5.Power.setLed(0); ledOn=false; diagnostic=(diagnostic+14+direction)%14; break;
    case Screen::HomeAssistant:
        if(settings.favoriteCount) favorite=(favorite+settings.favoriteCount+direction)%settings.favoriteCount;
        lastHaRead=millis()-6000; break;
    case Screen::Pairing: companion.closePairing(); forgetOption=!forgetOption; break;
    default: backAction(); return;
    }
    sound(sfx::Cue::Select);
    dirty=true;
}

void startRecording(bool fromTouch) {
    if(notes.busy()) return;
    badgeApp.abortUpload();
    if(screen==Screen::Badge) badgeApp.suspend();
    if(displayDimmed) { displayDimmed=false; M5.Display.setBrightness(settings.brightness); }
    sfx::stop();
    if(M5.Speaker.isPlaying()) { M5.Speaker.stop(); notes.update(); }
    M5.Power.setVibration(0); vibrationUntil=0;
    touchRecording=fromTouch;
    waveform.fill(0);
    if(notes.start()) {
        if(!settings.doNotDisturb) { M5.Power.setVibration(settings.vibration); vibrationUntil=millis()+65; }
        mqtt.changed();
    } else touchRecording=false;
    dirty=true;
}

void stopRecording() {
    const bool preparing=notes.state()==VoiceNotes::State::Preparing;
    notes.stop();
    if(!notes.busy()) {
        touchRecording=false;
        if(preparing) toast(notes.message());
    }
    dirty=true;
}

void handleTouch(const ui::Gesture& gesture) {
    using ui::Action;
    using ui::GestureKind;
    if(gesture.kind==GestureKind::None) return;
    lastInteraction=millis();
    if(gesture.kind==GestureKind::QuickOpen) { showQuickPanel(true);return; }
    if(quickPanelOpen) {
        if(gesture.kind==GestureKind::QuickClose) showQuickPanel(false);
        else if(gesture.kind==GestureKind::Tap) {
            if(ui::quickClose.contains(gesture.x,gesture.y) && ui::quickClose.contains(gesture.originX,gesture.originY)) showQuickPanel(false);
            else if(ui::quietToggle.contains(gesture.x,gesture.y) && ui::quietToggle.contains(gesture.originX,gesture.originY)) toggleQuiet();
        }
        return;
    }
    if(noticeTouch) {
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
        if(gesture.kind==GestureKind::Tap) { notice=""; dirty=true; }
        return;
    }
    if(gesture.kind==GestureKind::Drag || gesture.kind==GestureKind::Release) {
        if(screen!=Screen::Settings || notes.busy()) return;
        if(gesture.kind==GestureKind::Drag) {
            settingPreview=ui::sliderValue(setting,gesture.x);
            if(setting==0) M5.Display.setBrightness(settingPreview);
            if(setting==5) ui::setTheme(settingPreview);
            dirty=true;
        } else if(settingPreview>=0) commitSetting(settingPreview);
        return;
    }
    const auto target=ui::hit(screen,gesture.x,gesture.y,touchSelection(),notes.busy(),badgeControls(),clockControls());
    const auto origin=ui::hit(screen,gesture.originX,gesture.originY,touchSelection(),notes.busy(),badgeControls(),clockControls());
    if(screen==Screen::Diagnostics && diagnostic==1 && origin.action==Action::None) {
        touchX=gesture.x; touchY=gesture.y; dirty=true; return;
    }
    if(gesture.kind==GestureKind::Left || gesture.kind==GestureKind::Right) {
        if(!notes.busy() && (screen==Screen::Clock || screen==Screen::Timer || screen==Screen::Stopwatch)) {
            const int page=screen==Screen::Clock?0:screen==Screen::Timer?1:2;
            clockPage((page+3+(gesture.kind==GestureKind::Left?1:-1))%3);vibrate();sound(sfx::Cue::Open);return;
        }
        if(!notes.busy()) { vibrate(); nextAction(gesture.kind==GestureKind::Left?1:-1); }
        return;
    }
    if(!(target==origin)) return;
    if(notes.busy()) {
        if(target.action==Action::Stop) stopRecording();
        return;
    }
    if(target.action==Action::None) return;
    vibrate();
    switch(target.action) {
    case Action::Back: backAction(); break;
    case Action::Next: nextAction(); break;
    case Action::Previous: nextAction(-1); break;
    case Action::Select:
    case Action::Play: selectAction(); break;
    case Action::Record: startRecording(true); break;
    case Action::Increase:
    case Action::Decrease:
        if(screen==Screen::MusicPlaylists) {
            const auto& page=mqtt.playlists();
            if(target.action==Action::Increase && page.hasNext()) playlistPage(page.offset+music::playlistPageSize);
            else if(target.action==Action::Decrease && page.offset>=music::playlistPageSize) playlistPage(page.offset-music::playlistPageSize);
            break;
        }
        if(screen==Screen::Timer && clockTimer.state()==clockwork::Timer::State::Idle) {
            clockTimer.adjust(target.index,target.action==Action::Increase?1:-1);dirty=true;sound(sfx::Cue::Select);
        }
        break;
    case Action::Secondary:
        if(screen==Screen::Timer) timerSecondaryAction();else if(screen==Screen::Stopwatch) stopwatchSecondaryAction();
        break;
    case Action::Open:
        if(screen==Screen::Spotify) {
            musicMenu=0;changeScreen(Screen::MusicMenu);sound(sfx::Cue::Open);break;
        }
        if(screen==Screen::MusicMenu) { musicMenu=target.index;selectAction();break; }
        if(screen==Screen::MusicPlaylists) {
            if(target.index>=0 && size_t(target.index)<mqtt.playlists().items.size()) { playlistItem=target.index;selectAction(); }
            break;
        }
        if(screen==Screen::MusicSources) {
            if(target.index>=0 && size_t(target.index)<mqtt.playback().sources.size()) {
                musicSource=target.index;selectAction();
            }
            break;
        }
        if(screen==Screen::ClockMenu || screen==Screen::Clock || screen==Screen::Timer || screen==Screen::Stopwatch) {
            if(target.index<3) { clockPage(target.index);sound(sfx::Cue::Open); }
            else { clockControlsUntil=clockControls()?0:millis()+4000;dirty=true; }
            break;
        }
        if(screen==Screen::Badge) {
            if(target.index==1 && badgeApp.available()) {
                Settings next=settings; next.badgeAutoRotate=!next.badgeAutoRotate;
                if(next.save()) settings=next; else toast("Impostazioni Badge non salvate");
            }
            badgeControlsUntil=millis()+4000; dirty=true; break;
        }
        if(screen==Screen::Home) selected=target.index;
        else if(screen==Screen::System) systemItem=target.index;
        else if(screen==Screen::Pairing) forgetOption=!companion.pairing() && target.index==1;
        selectAction(); break;
    default: break;
    }
}

bool checkMemory() {
    constexpr size_t size = 65536;
    auto* block = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!block) return false;
    for (size_t i = 0; i < size; ++i) block[i] = uint8_t(i ^ (i >> 8));
    bool ok = true;
    for (size_t i = 0; i < size; ++i) if (block[i] != uint8_t(i ^ (i >> 8))) { ok = false; break; }
    free(block);
    return ok;
}

struct BootIndicator {
    const uint32_t startedAt=millis();
    std::atomic<bool> ready{false}, stopped{false};
};

void showBootLoading(void* context) {
    auto& boot=*static_cast<BootIndicator*>(context);
    while(!boot.ready.load(std::memory_order_acquire)) {
        if(firmware::showLoading(millis(),boot.startedAt,boot.ready.load(std::memory_order_acquire))) {
            canvas.fillScreen(ui::black);
            ui::label(canvas,"Caricamento…",233,215,ui::Font::Label,ui::white);
            ui::label(canvas,String("v")+firmware::version,233,262,ui::Font::Body,ui::muted);
            canvas.pushSprite(0,0);
            break;
        }
        vTaskDelay(1);
    }
    boot.stopped.store(true,std::memory_order_release);
    vTaskDelete(nullptr);
}
}

void setup() {
    auto config = M5.config();
    config.output_power = false;
    config.fallback_board = m5::board_t::board_M5StopWatch;
    config.serial_baudrate = 115200;
    M5.begin(config);
    Serial.printf("[boot] hardware=%d heap_ok=%d\n", int(M5.getBoard()), heap_caps_check_integrity_all(true));
    if (M5.getBoard() != m5::board_t::board_M5StopWatch) {
        Serial.println("Hardware diverso da M5Stack StopWatch: avvio interrotto.");
        for (;;) delay(1000);
    }
    settingsOk = settings.load();
    clockTimer.setSeconds(settings.timerSeconds);
    applySettings();
    M5.Power.setVibration(0);
    M5.Power.setLed(0);
    memoryOk = checkMemory();
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(466, 466)) {
        M5.Display.setTextColor(0xffffff, 0x000000);
        M5.Display.drawString("PSRAM: impossibile avviare UI", 50, 233);
        for (;;) delay(1000);
    }
    if(!ui::beginFonts()) toast("Caratteri UI non disponibili");
    BootIndicator boot;
    // Setup leaves the canvas to this task until it has acknowledged completion.
    const bool bootIndicatorStarted=xTaskCreate(showBootLoading,"boot-display",4096,&boot,1,nullptr)==pdPASS;
    notes.begin();
    if(notes.ready()) badgeApp.begin();
    if(notes.ready()) sfx::begin(settings.soundPack);
    Serial.printf("[boot] display pronto, archivio=%d heap_ok=%d\n", notes.ready(), heap_caps_check_integrity_all(true));
    setenv("TZ", "UTC0", 1); tzset();
    if (M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow()) M5.Rtc.setSystemTimeFromRtc();
    setenv("TZ", timezoneRome, 1); tzset();
    Serial.printf("[boot] RTC pronto, heap_ok=%d\n", heap_caps_check_integrity_all(true));
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    Serial.println("[boot] Wi-Fi STA pronto");
    WiFi.setHostname("stopwatch");
    WiFi.setAutoReconnect(true);
    connectWifi();
    Serial.println("[boot] rete inizializzata");
    homeReady = home.begin();
    Serial.println("[boot] task Home Assistant pronto");
    if (!companion.begin()) toast("Avvio companion BLE fallito");
    mqtt.setRelay(&companion);mqtt.begin();
    if (settings.ssid.isEmpty()) changeScreen(Screen::Pairing);
    Serial.println("[boot] companion pronto");
    if (!settingsOk) toast("Configurazione illeggibile: valori predefiniti");
    else if (!notes.ready()) toast("Archivio da inizializzare in Diagnostica");
    else if(!sfx::ready() && settings.soundPack) toast("Pacchetti audio da installare via USB");
    notes.warmMicrophone();
    boot.ready.store(true,std::memory_order_release);
    if(bootIndicatorStarted) while(!boot.stopped.load(std::memory_order_acquire)) delay(1);
    lastInteraction = millis();
    if(!notice.isEmpty()) noticeAt=lastInteraction;
    draw();
    Serial.println("[boot] interfaccia avviata");
}

void loop() {
    M5.update();
    const uint32_t now = millis();
    if(clockTimer.update(now) && !notes.busy() && !notes.playing()) {
        // A press begun before expiry must not restart or dismiss the new alert.
        controls.suppressTapUntilReleased();touchInput.suppressUntilReleased();
    }
    const bool stopwatchWasRunning=chronometer.running();chronometer.update(now);
    if(stopwatchWasRunning && !chronometer.running() && screen==Screen::Stopwatch) dirty=true;
    sfx::update();
    if (M5.BtnA.wasPressed()) ++pressesA;
    if (M5.BtnB.wasPressed()) ++pressesB;
    const auto touch=M5.Touch.getDetail();
    // Keep the visible card's tap until release, even if it expires in the meantime.
    if(touch.wasPressed()) noticeTouch=noticeDisplayed && ui::noticeCard.contains(touch.x,touch.y);
    const bool interaction=M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || touch.wasPressed();
    updateCpu(now,cpuWorkPending() || interaction);
    if(touch.isPressed()) lastInteraction=now;
    if(interaction) {
        lastInteraction=now;
        if(screenOff) {
            screenOff=false;
            M5.Display.setBrightness(settings.brightness);
            controls.suppressTapUntilReleased();
            if(touch.wasPressed()) touchInput.suppressUntilReleased();
            lastBatteryRead=now-30000;
            if(screen!=Screen::Badge && screen!=Screen::Clock && screen!=Screen::Spotify && screen!=Screen::MusicSources && screen!=Screen::MusicMenu && screen!=Screen::MusicPlaylists)
                notes.warmMicrophone();
            dirty=true;
        }
    }
    const auto events=controls.update(now,M5.BtnA.isPressed(),M5.BtnB.isPressed());
    if(notes.busy() || events.recordStop || events.next || events.select || events.home || events.back) lastInteraction=now;
    if(events.next || events.select || events.back || events.home || events.recordStart) {
        touchInput.suppressUntilReleased();
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    }
    if(events.recordStop) stopRecording();
    const bool panelButtons=quickPanelOpen;
    if(panelButtons) {
        if(events.next || events.back) showQuickPanel(false);
        else if(events.select) toggleQuiet();
    } else {
        if(events.next) mqtt.button("a","press");
        if(events.select) mqtt.button("b","press");
        if(events.back) mqtt.button("a","hold");
    }
    if(events.recordStart) mqtt.button("b","hold");
    if(events.recordStop) mqtt.button("b","release");
    if(events.home || events.recordStart) { if(quickPanelOpen) showQuickPanel(false); }
    if(events.home || (!panelButtons && events.back)) {
        stopRecording();
        if(!notes.busy()) {
            if(events.home) changeScreen(Screen::Home);
            else backAction();
        } else homeAfterNote=true;
    }
    if(events.recordStart) {
        if(notes.busy()) stopRecording();
        else startRecording(false);
    } else if(!panelButtons && events.select && notes.busy() && touchRecording) stopRecording();
    else if(!panelButtons && !notes.busy()) {
        if(events.next) { vibrate(); nextAction(); }
        if(events.select) { vibrate(); selectAction(true); }
    }
    if(M5.Touch.getCount()>1) {
        touchInput.suppressUntilReleased();
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    }
    if(!screenOff) {
        const bool slider=!quickPanelOpen && screen==Screen::Settings && !notes.busy() &&
            !noticeTouch &&
            ui::hit(screen,touch.x,touch.y,touchSelection()).action==ui::Action::Slider;
        handleTouch(touchInput.update(now,touch.isPressed(),touch.x,touch.y,slider,quickPanelOpen));
        if(!touch.isPressed()) noticeTouch=false;
        if(!quickPanelOpen && screen==Screen::Diagnostics && diagnostic==1 && touch.isPressed()) {
            touchX=touch.x;touchY=touch.y;dirty=true;
        }
    }
    if (vibrationUntil && int32_t(now - vibrationUntil) >= 0) { M5.Power.setVibration(0); vibrationUntil = 0; }
    const bool noteWasPlaying=notes.playing();
    notes.update();
    if(noteWasPlaying!=notes.playing()) dirty=true;
    if(notes.state()==VoiceNotes::State::Recording && uint32_t(now-lastWave)>=60) {
        for(size_t i=0;i+1<waveform.size();++i) waveform[i]=waveform[i+1];
        waveform.back()=notes.peak(); lastWave=now;
    }
    if(notes.takeFinished()) {
        touchRecording=false;
        if(notes.succeeded()) { notice=""; changeScreen(homeAfterNote?Screen::Home:Screen::NoteSaved); }
        else toast(notes.message());
        vibrate(notes.succeeded()?140:350);
        sound(notes.succeeded()?sfx::Cue::Complete:sfx::Cue::Error,true);
        mqtt.changed(); companion.notesChanged();
    }
    if(homeAfterNote && !notes.busy()) { homeAfterNote=false; changeScreen(Screen::Home); }
    const unsigned timerAlert=clockTimer.alert(now,notes.busy() || notes.playing());
    if(timerAlert) {
        updateCpu(now,true);
        if(timerAlert==1) {
            controls.suppressTapUntilReleased();
            screenOff=false;M5.Display.setBrightness(settings.brightness);lastInteraction=now;
            changeScreen(Screen::Timer);
        }
        vibrate(180);sound(sfx::Cue::Complete,true);
    }
    if (reconnectAt && int32_t(now - reconnectAt) >= 0) { reconnectAt = 0; connectWifi(); }
    static bool wasConnected = false;
    static int wasHomeRoute=-1;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const int homeRoute=settings.favoritesMqtt?(mqtt.connected()?1:mqtt.available()?2:0):(connected?1:0);
    const bool homeOnline=homeRoute!=0;
    if(homeRoute!=wasHomeRoute) {
        states.fill(""); units.fill(""); stateUpdated.fill(0);
        stateResetAt=now; lastHaRead=now-6000; dirty=true;
        wasHomeRoute=homeRoute;
    }
    if (connected && !wasConnected) configTzTime(timezoneRome, "pool.ntp.org", "time.cloudflare.com");
    if(wasConnected!=connected) dirty=true;
    wasConnected = connected;
    HomeResult result;
    if (home.poll(result) || mqtt.pollFavorite(result)) {
        if (homeOnline && result.favorite < settings.favoriteCount && result.viaMqtt==settings.favoritesMqtt &&
            result.entity==settings.favorites[result.favorite].entity && int32_t(result.requestedAt-stateResetAt)>=0) {
            const bool verifying=homeCommand.pending(result.favorite);
            states[result.favorite] = result.state;
            units[result.favorite] = result.unit;
            stateUpdated[result.favorite] = result.success && policy::usableState(result.entity.c_str(),result.state.c_str())?now:0;
            const bool confirmed=result.success && homeCommand.observe(result.favorite,result.state.c_str());
            lastHaRead=(result.favorite==favorite || homeCommand.pending())?now:now-6000;
            if(confirmed) {
                toast("Stato confermato"); vibrate(100); sound(sfx::Cue::Success,true);
            } else if(result.action || (!result.success && !verifying)) toast(result.message);
            if(result.action && !verifying) {
                vibrate(result.success ? 100 : 250);
                sound(result.success?sfx::Cue::Success:sfx::Cue::Error,true);
            }
            dirty=true;
        }
    }
    if(homeCommand.expired(now)) {
        stateUpdated[homeCommand.favorite()]=0; homeCommand.clear();
        toast("Stato non confermato: verifica in Home Assistant");
    }
    const bool homeVisible=!quickPanelOpen && !screenOff && screen==Screen::HomeAssistant;
    const bool musicScreen=screen==Screen::Spotify || screen==Screen::MusicSources || screen==Screen::MusicMenu || screen==Screen::MusicPlaylists;
    const bool musicActive=!screenOff && !notes.busy() && musicScreen;
    const bool musicVisible=!quickPanelOpen && musicActive;
    if(mqtt.pollMusic()) {
        lastMusicRead=now;
        if(musicSource>=mqtt.playback().sources.size()) musicSource=0;
        if(playlistItem>=mqtt.playlists().items.size()) playlistItem=0;
        if(musicVisible) dirty=true;
    }
    const uint32_t musicRefresh=mqtt.musicMessage().isEmpty()?5000:30000;
    if(musicVisible && screen==Screen::MusicPlaylists && !playlistRequested && !mqtt.musicBusy()) {
        if(mqtt.requestMusic(music::Command::Playlists)) { playlistRequested=true;lastMusicRead=now; }
    } else if(musicVisible && !mqtt.musicBusy() && uint32_t(now-lastMusicRead)>=musicRefresh) {
        if(mqtt.requestMusic()) lastMusicRead=now;
    }
    if(musicArtwork.update(mqtt.playback(),settings,musicActive,companion,!mqtt.connected() && companion.relayReady()) && musicVisible) dirty=true;
    if(musicVisible && uint32_t(now-lastDraw)>=1000) dirty=true;
    if ((homeVisible || homeCommand.pending()) && settings.favoriteCount && homeOnline &&
        !(settings.favoritesMqtt ? mqtt.busy() : home.busy()) && !notes.busy() &&
        uint32_t(now-lastHaRead)>=(homeCommand.pending()?500u:homeRoute==2?5000u:casa::refreshMs)) {
        const size_t target=homeCommand.pending()?homeCommand.favorite():favorite;
        if (settings.favoritesMqtt) mqtt.requestFavorite(target, false);
        else if (homeReady) home.request(settings, target, false);
        lastHaRead = now;
    }
    if (uint32_t(now - lastTick) >= 500) {
        lastTick = now;
        const bool batteryDiagnostic=!screenOff && !quickPanelOpen && screen==Screen::Diagnostics && diagnostic==8;
        if(batteryLevel<0 || uint32_t(now-lastBatteryRead)>=(batteryDiagnostic?500u:screenOff?30000u:5000u)) {
            const int level=M5.Power.getBatteryLevel();
            if(level!=batteryLevel) dirty=true;
            batteryLevel=level;batteryMv=M5.Power.getBatteryVoltage();lastBatteryRead=now;
        }
        if(!screenOff && !quickPanelOpen && screen==Screen::Diagnostics && diagnostic==6 && M5.Imu.isEnabled() && M5.Imu.update()) imu=M5.Imu.getImuData();
        if (connected && time(nullptr) >= 1704067200 && uint32_t(now - lastRtcSync) > 60000) {
            const time_t epoch = time(nullptr);
            tm utc{}; gmtime_r(&epoch, &utc);
            if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&utc);
            lastRtcSync = now;
        }
        static time_t homeMinute=-1;
        static bool relayReady=false;
        static size_t noteCount=0;
        const time_t minute=time(nullptr)/60;
        if(screen==Screen::Home && (homeMinute!=minute || relayReady!=companion.relayReady())) dirty=true;
        homeMinute=minute;relayReady=companion.relayReady();
        if(!quickPanelOpen && screen==Screen::Notes) {
            const size_t count=notes.count();
            if(count!=noteCount) dirty=true;
            noteCount=count;
        }
        if(!quickPanelOpen && (screen==Screen::HomeAssistant || screen==Screen::Diagnostics || screen==Screen::Pairing || screen==Screen::Companion)) dirty=true;
    }
    if(noticeDisplayed && uint32_t(now-noticeAt)>=4500) dirty=true;
    if(badgeApp.update(now,settings.badgeAutoRotate,settings.badgeAnimate,!quickPanelOpen && screen==Screen::Badge && !screenOff && !notes.busy())) dirty=true;
    if(!quickPanelOpen && screen==Screen::Clock) {
        if(time(nullptr)!=clockDrawnSecond) dirty=true;
        if(clockControlsUntil && int32_t(now-clockControlsUntil)>=0) { clockControlsUntil=0;dirty=true; }
    }
    if(!quickPanelOpen && ((screen==Screen::Timer && clockTimer.state()==clockwork::Timer::State::Running && uint32_t(now-lastDraw)>=100) ||
       (screen==Screen::Stopwatch && chronometer.running() && uint32_t(now-lastDraw)>=50))) dirty=true;
    if(screen==Screen::Badge) {
        if(badgeControlsUntil && int32_t(now-badgeControlsUntil)>=0) { badgeControlsUntil=0; dirty=true; }
    }
    const bool keepDisplayOn=(screen==Screen::Clock && settings.clockKeepAwake) ||
        (screen==Screen::Badge && badgeApp.available() && settings.badgeKeepAwake);
    const bool dim=keepDisplayOn && !quickPanelOpen && !notes.busy() && uint32_t(now-lastInteraction)>20000 &&
        (screen!=Screen::Badge || uint32_t(now-badgeApp.lastMotion())>20000);
    if(dim!=displayDimmed) { displayDimmed=dim; if(!screenOff) M5.Display.setBrightness(dim?std::min(settings.brightness,uint8_t(48)):settings.brightness); }
    if (!screenOff && !keepDisplayOn && !notes.busy() && !portal.active() && !companion.pairing() && !M5.BtnA.isPressed() && !M5.BtnB.isPressed() && !touch.isPressed() &&
        uint32_t(millis() - lastInteraction) > uint32_t(settings.screenTimeout) * 1000) {
        M5.Display.setBrightness(0);
        sfx::stop();
        M5.Mic.end();
        screenOff = true;
    }
    if (!notes.busy() && !M5.Speaker.isPlaying() && !screenOff) {
        if(screen==Screen::Badge || screen==Screen::Clock || musicScreen || badgeApp.uploading()) { if(M5.Mic.isRunning()) M5.Mic.end(); }
        else notes.warmMicrophone();
    }
    if(quietSavePending && canSave()) {
        quietSaveFailed=!settings.save();quietSavePending=false;dirty=true;
    }
    const bool animating=screen==Screen::Home && uint32_t(now-transitionAt)<160;
    if(!screenOff && (dirty || (!quickPanelOpen && (notes.busy() || animating))) && uint32_t(now-lastDraw)>=(notes.busy()?50u:screen==Screen::Badge?40u:33u)) {
        // A periodic clock frame must not extend the two-second interaction boost.
        updateCpu(now,cpuWorkPending(),true);
        draw();
    }
    portal.update();
    companion.update();
    mqtt.update(screenOff);
    updateCpu(millis(),cpuWorkPending());
    delay(5);
}
