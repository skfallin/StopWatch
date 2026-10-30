#include "i18n.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <utility/M5IOE1_Class.hpp>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <time.h>
#include "controls.h"
#include "companion.h"
#include "home_assistant.h"
#include "home_state.h"
#include "light_control.h"
#include "firmware.h"
#include "mqtt_bridge.h"
#include "portal.h"
#include "settings.h"
#include "voice_notes.h"
#include "ui_drawing.h"
#include "badge.h"
#include "clock_view.h"
#include "productivity_view.h"
#include "music_view.h"
#include "music_artwork.h"
#include "quick_settings_view.h"
#include "scene_view.h"
#include "security_key.h"

namespace {
constexpr uint32_t muted = 0x9aaebd;
constexpr uint32_t accent = 0xa5ecc2, blue = 0x8cc8ff, yellow = 0xffd377, red = 0xff8993;
void applyRegion(const Settings& value) {
    i18n::italian = value.language == "it";
    setenv("TZ", regional::rule(value.timeZone.c_str()), 1);
    tzset();
}
using Screen = ui::Screen;
const char* diagnostics[] = {"Pulsanti A e B", "Touch", "Display AMOLED", "Vibrazione", "Altoparlante",
    "Microfono", "Movimento / IMU", "Orologio RTC", "Batteria / USB", "Wi-Fi", "Bluetooth LE", "Archivio / RAM", "Porta Grove / GPIO", "LED verde"};
const char* settingNames[] = {"Luminosità", "Interazioni", "Vibrazione", "Spegni display", "Pacchetto suoni", "Colori", "Volume note"};
const char* settingsSections[]={"Display","Audio","Standby"};
Settings settings;
VoiceNotes notes;
Badge badgeApp;
clockwork::Timer clockTimer;
clockwork::Stopwatch chronometer;
clockwork::Focus focusSession;
clockwork::Stopwatch presentationTime;
size_t clockItem=0;
size_t musicSource=0;
size_t musicMenu=0,playlistItem=0;
bool playlistRequested=false;
uint32_t lastMusicRead=0;
MusicArtwork musicArtwork;
UsbControl usbControl;
bool usbPaused=false;
uint32_t clockControlsUntil=0;
time_t clockDrawnSecond=-1;
HomeAssistant home;
Controls controls;
ui::TouchInput touchInput;
M5Canvas canvas(&M5.Display);
Screen screen = Screen::Home;
ui::HomeMenu homeMenu;
size_t systemItem = 0, diagnostic = 0, setting = 0, favorite = 0;
int settingPreview = -1;
bool touchRecording = false, noticeTouch = false, noticeDisplayed = false;
bool displayDimmed=false;
bool quickPanelOpen=false, quietSavePending=false, quietSaveFailed=false;
int quickPage=0,quickPreview=-1;
uint32_t badgeControlsUntil=0;
uint32_t transitionAt = 0, lastWave = 0;
uint32_t surfaceColor = ui::black, foregroundColor = ui::white;
std::array<uint16_t, 24> waveform{};
bool homeFramePending=false;
bool dirty = true, screenOff = false, memoryOk = false, settingsOk = true, homeReady = false;
bool ledOn = false, showPageQr = false, forgetOption = false;
bool homeAfterNote = false;
uint32_t lastInteraction = 0, lastDraw = 0, lastTick = 0, vibrationUntil = 0;
uint32_t lastBatteryRead=0,lastCpuWork=0;
bool cpuScaling=true;
uint32_t noticeAt = 0, reconnectAt = 0, lastHaRead = 0, lastRtcSync = 0;
uint32_t noticeDuration=4500,lastChargeRead=0;
bool charging=false,chargeNotified=false,chargeNoticePending=false;
int chargeCandidate=-1;
ui::Icon noticeIcon=ui::Icon::Activity;
uint32_t pressesA = 0, pressesB = 0;
uint8_t displayColor = 0;
int batteryLevel = -1, batteryMv = -1;
int touchX = -1, touchY = -1;
String notice, noticeDetail, groveResult;
String activeSsid, activeWifiPassword;
m5::imu_data_t imu{};
std::array<String, policy::maxFavorites> states{}, units{};
std::array<uint32_t, policy::maxFavorites> stateUpdated{};
casa::ToggleVerification homeCommand;
casa::LightLevel lightDraft;
std::array<bool,policy::maxFavorites> lightCapable{};
std::array<int,policy::maxFavorites> lightLevels{};
String lightEntity;
size_t lightFavorite=0;
bool adjustOption=false;
int casaHoldProgress=0;
uint32_t stateResetAt=0;

bool canSave() { return !home.busy() && !notes.busy(); }
void onSettingsSaved();
void changeScreen(Screen next);
void backAction();
bool remoteOutput(const String& action);
void extraStatus(JsonObject out);
void deviceStatus(JsonObject out);
Portal portal(settings, notes, canSave, onSettingsSaved);
Companion companion(settings, notes, badgeApp, usbControl, canSave, onSettingsSaved, remoteOutput, extraStatus);
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
    const bool passiveDisplay=!quickPanelOpen && (screen==Screen::Clock || screen==Screen::Focus || screen==Screen::Slides || idleBadge);
    return (!screenOff && !passiveDisplay) || M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.Touch.getCount()>0 ||
        notes.busy() || notes.playing() || M5.Speaker.isPlaying() ||
        home.busy() || homeCommand.pending() || portal.active() || badgeApp.uploading() ||
        companion.hasWork() || mqtt.hasWork() || musicArtwork.busy() || usbControl.busy() || security_key::busy();
}

void sound(sfx::Cue cue, bool preview=false) {
    if (!settings.doNotDisturb && !notes.busy()) sfx::play(settings.soundPack,cue,settings.volume,preview);
}

void toast(const String& text) {
    notice = text;
    noticeDetail="";noticeIcon=ui::Icon::Activity;noticeDuration=4500;
    noticeAt = millis();
    dirty = true;
}

void updateCharging(uint32_t now) {
    if(chargeCandidate>=0 && uint32_t(now-lastChargeRead)<1000) return;
    lastChargeRead=now;
    const int vbus=M5.Power.getVBUSVoltage();
    const auto state=M5.Power.isCharging();
    if(vbus<0 || state==m5::Power_Class::charge_unknown) return;
    const bool usb=vbus>=4000;
    const int candidate=(usb?1:0)+(usb && state==m5::Power_Class::is_charging?2:0);
    if(candidate!=chargeCandidate) { chargeCandidate=candidate;return; }
    const bool next=(candidate&2)!=0;
    if(charging!=next) { charging=next;dirty=true; }
    if(!usb) {
        chargeNotified=false;chargeNoticePending=false;
        if(noticeIcon==ui::Icon::Charging) { notice="";dirty=true; }
    }
    if(charging && !chargeNotified) { chargeNotified=true;chargeNoticePending=true; }
    if(!charging) chargeNoticePending=false;
    if(!chargeNoticePending || notes.busy() || quickPanelOpen || security_key::awaiting() ||
        screen==Screen::Companion || screen==Screen::NoteSaved ||
        (!notice.isEmpty() && uint32_t(now-noticeAt)<noticeDuration)) return;
    chargeNoticePending=false;
    toast(i18n::text("In carica"));
    noticeIcon=ui::Icon::Charging;
    noticeDetail=(batteryLevel<0?String(i18n::text("USB collegata")):String(batteryLevel)+i18n::text("% · USB collegata"));
    noticeDuration=3200;
    screenOff=false;displayDimmed=false;lastInteraction=now;
    M5.Display.setBrightness(settings.brightness);
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
    if(quickPreview>=0) M5.Display.setBrightness(settings.brightness);
    quickPreview=-1;
    if(open) quickPage=0;
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
    else if(settings.doNotDisturb) toast(i18n::text("Non disturbare attivo"));
    else if(!notes.playLatest(settings.noteVolume)) toast(i18n::text("Nessuna nota riproducibile"));
}

void onSettingsSaved() {
    applyRegion(settings);
    quietSavePending=quietSaveFailed=false;
    settingPreview=-1;
    touchInput.suppressUntilReleased();
    if(screen==Screen::ActionConfirm || screen==Screen::Light || screen==Screen::LightConfirm) changeScreen(Screen::HomeAssistant);
    applySettings();
    homeMenu.configure(settings.homeApps,settings.homeAppCount);
    if(clockTimer.state()==clockwork::Timer::State::Idle) clockTimer.setSeconds(settings.timerSeconds);
    states.fill(""); units.fill(""); stateUpdated.fill(0);
    homeCommand.clear(); stateResetAt=millis();
    favorite = 0;
    if (activeSsid != settings.ssid || activeWifiPassword != settings.wifiPassword) reconnectAt = millis() + 600;
    mqtt.configure();
    mqtt.changed();
    toast(i18n::text("Configurazione salvata"));
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
    if(screen==Screen::Slides && next!=Screen::Slides && presentationTime.running()) presentationTime.toggle(millis());
    if(next==Screen::Slides && screen!=next) { presentationTime.reset();presentationTime.toggle(millis()); }
    if(screen==Screen::Badge && next!=Screen::Badge) badgeApp.suspend();
    if(next==Screen::UsbControl && screen!=next) usbPaused=false;
    if(displayDimmed && screen!=next) { displayDimmed=false; if(!screenOff) M5.Display.setBrightness(settings.brightness); }
    if(next==Screen::Badge) { badgeControlsUntil=0; notice=""; }
    if(next==Screen::Clock) { clockControlsUntil=millis()+4000;notice=""; }
    if(next==Screen::Spotify || next==Screen::MusicSources || next==Screen::MusicMenu || next==Screen::MusicPlaylists) { lastMusicRead=millis()-5000;notice=""; }
    if(next==Screen::MusicPlaylists) { playlistRequested=false;playlistItem=0; }
    if(next==Screen::ActionConfirm || next==Screen::Light || next==Screen::LightConfirm) { notice="";adjustOption=false; }
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
    security_key::update(screen==Screen::SecurityKey && !quickPanelOpen && !notes.busy(),M5.BtnA.isPressed(),M5.BtnB.isPressed());
    usbControl.update(((screen==Screen::UsbControl && !usbPaused) || screen==Screen::Slides) && !quickPanelOpen && !notes.busy(),companion.connected(),millis(),screen==Screen::Slides);
    if (next == Screen::Companion && !portal.start()) toast(i18n::text("Avvio Wi-Fi companion fallito"));
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

void drawStatus() {
    if(settings.doNotDisturb) ui::icon(canvas,ui::Icon::Moon,233,14,14,ui::accent);
    tm local{};
    const time_t now=time(nullptr);
    localtime_r(&now,&local);
    char date[8]="--:--";
    if(now>=1704067200) strftime(date,sizeof(date),"%H:%M",&local);
    const String battery=batteryLevel<0?"--":String(batteryLevel)+"%";
    const auto status=ui::statusLayout(ui::textWidth(canvas,date,ui::Font::Small),ui::textWidth(canvas,battery,ui::Font::Small));
    ui::label(canvas,date,status.dateX,36,ui::Font::Small,ui::muted);
    ui::connectionIcon(canvas,status.wifiX,36,WiFi.status()==WL_CONNECTED,companion.relayReady());
    ui::label(canvas,battery,status.batteryX,36,ui::Font::Small,charging?ui::accent:ui::muted,ui::black,58);
    if(charging) ui::icon(canvas,ui::Icon::Charging,status.batteryIconX+12,36,27,ui::accent);
    else {
        canvas.drawRoundRect(status.batteryIconX,29,24,14,3,ui::muted);
        canvas.fillRect(status.batteryIconX+24,34,2,4,ui::muted);
        if(batteryLevel>0) canvas.fillRoundRect(status.batteryIconX+3,32,std::max(1,18*std::min(batteryLevel,100)/100),8,1,ui::muted);
    }
}

void drawHome() {
    const uint32_t age=uint32_t(millis()-transitionAt);
    scene_ui::home(canvas,homeMenu,age);
    // Rendering itself can cross the deadline; present one frame at the final geometry.
    homeFramePending=age<180;
    drawStatus();
}

bool badgeControls() { return !badgeApp.available() || !badgeApp.error().isEmpty() || (badgeControlsUntil && int32_t(badgeControlsUntil-millis())>0); }
bool clockControls() { return time(nullptr)<1704067200 || (clockControlsUntil && int32_t(clockControlsUntil-millis())>0); }
int touchSelection() {
    if(screen==Screen::MusicSources) return int(musicSource);
    if(screen==Screen::MusicPlaylists) return mqtt.playlists().items.empty()?-1:int(playlistItem);
    if(screen==Screen::System) return int(systemItem);
    if(screen==Screen::SettingsMenu || screen==Screen::SettingsSection) return int(setting);
    return screen==Screen::Timer?int(clockTimer.state()!=clockwork::Timer::State::Idle):0;
}
void clockPage(int index) {
    const Screen pages[]={Screen::Clock,Screen::Timer,Screen::Stopwatch};
    clockItem=index;changeScreen(pages[index]);
}
void nextClockFace() {
    Settings next=settings;next.clockFace=(next.clockFace+1)%clockwork::faceIds.size();
    if(next.save()) settings=next;else toast(i18n::text("Quadrante non salvato"));
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
        text(i18n::text("Il tuo Badge."),281,ui::white,2);
        text(badgeApp.error().isEmpty()?i18n::text("Carica foto o GIF dall'iPhone"):badgeApp.error(),331,ui::muted,1,310);
        return;
    }
    if(badgeControls()) {
        canvas.fillSmoothRoundRect(155,51,177,56,28,ui::surface);
        header("Badge");
        ui::pill(canvas,233,323,240,settings.badgeAutoRotate?i18n::text("Rotazione: auto"):i18n::text("Fissa · B ruota 45°"));
        ui::pill(canvas,233,389,240,badgeApp.animated()?(settings.badgeAnimate?i18n::text("Pausa GIF"):i18n::text("Riprendi GIF")):i18n::text("Torna alla home"));
    }
}

void drawSystem() {
    drawStatus();
    header(i18n::text("Sistema"));
    const char* titles[]={"Impostazioni","Diagnostica","iPhone","Browser Wi-Fi"};
    const char* details[]={"Display, audio, standby","Verifica hardware","Abbinamento e ponte","Configurazione web"};
    const ui::Icon icons[]={ui::Icon::Sliders,ui::Icon::Activity,ui::Icon::Bluetooth,ui::Icon::Link};
    const int page=systemItem/2;
    for(int slot=0;slot<2;++slot) {
        const int item=page*2+slot;
        scene_ui::row(canvas,ui::menuRow(slot),i18n::text(titles[item]),icons[item],i18n::text(details[item]),item==int(systemItem));
    }
    scene_ui::pages(canvas,page);
}

String settingSummary(int item) {
    const int values[]={settings.brightness,settings.volume,settings.vibration,settings.screenTimeout,settings.soundPack,settings.colorTheme,settings.noteVolume};
    if(item==4) return i18n::text(sfx::packNames[values[item]]);
    if(item==5) return i18n::text(ui::themes[values[item]].name);
    if(item==3) return String(values[item])+" s";
    return String(values[item]*100/((item==1 || item==6)?200:255))+"%";
}

void drawSettingsMenu() {
    drawStatus();header(i18n::text("Impostazioni"));
    const ui::Icon icons[]={ui::Icon::Sun,ui::Icon::Speaker,ui::Icon::Timer};
    for(int section=0;section<3;++section)
        scene_ui::row(canvas,ui::settingsRow(section),i18n::text(settingsSections[section]),icons[section],"",section==ui::settingsSection(setting));
}

void drawSettingsSection() {
    const int section=ui::settingsSection(setting),page=ui::settingsPosition(setting)/2;
    drawStatus();header(i18n::text(settingsSections[section]));
    const ui::Icon icons[]={ui::Icon::Sun,ui::Icon::Speaker,ui::Icon::Activity,ui::Icon::Clock,ui::Icon::Speaker,ui::Icon::Sliders,ui::Icon::Mic};
    for(int slot=0;slot<2 && page*2+slot<ui::settingsCount(section);++slot) {
        const int item=ui::settingsItems[section][page*2+slot];
        scene_ui::row(canvas,ui::menuRow(slot),i18n::text(settingNames[item]),icons[item],settingSummary(item),item==int(setting));
    }
    if(ui::settingsCount(section)>2) scene_ui::pages(canvas,page);
}

void drawSettings() {
    drawStatus();header(i18n::text(settingsSections[ui::settingsSection(setting)]));
    const ui::Icon icons[]={ui::Icon::Sun,ui::Icon::Speaker,ui::Icon::Activity,ui::Icon::Clock,ui::Icon::Speaker,ui::Icon::Sliders,ui::Icon::Mic};
    const int value=settingValue();
    ui::label(canvas,i18n::text(settingNames[setting]),233,140,ui::Font::Label,ui::white);
    ui::icon(canvas,icons[setting],233,187,40,ui::accent);
    if (setting==4 || setting==5) ui::label(canvas,setting==4?i18n::text(sfx::packNames[value]):i18n::text(ui::themes[value].name),233,264,ui::Font::Title,ui::white);
    else {
        const int displayValue=setting==3?value:value*100/((setting==1 || setting==6)?200:255);
        ui::label(canvas,String(displayValue)+(setting==3?" s":"%"),233,264,ui::Font::Clock,ui::white);
    }
    ui::circleButton(canvas,ui::settingsPrevious,ui::Icon::Back,36,ui::surface,ui::navigation);
    ui::circleButton(canvas,ui::settingsNext,ui::Icon::Next,36,ui::surface,ui::navigation);
    const int minimum=setting==0?32:setting==3?15:0,maximum=(setting==1 || setting==6)?200:setting==3?180:setting==4?int(sfx::packIds.size())-1:setting==5?5:255;
    const int position=109+(std::clamp(value,minimum,maximum)-minimum)*248/(maximum-minimum);
    canvas.fillSmoothRoundRect(109,339,248,8,4,ui::line);
    canvas.fillSmoothRoundRect(109,339,std::max(8,position-109),8,4,ui::accent);
    canvas.fillSmoothCircle(position,343,15,ui::accent);
    if (setting==4) ui::label(canvas,i18n::text("Trascina o premi B · ascolta"),233,386,ui::Font::Small,ui::muted);
    else if(setting==5) ui::label(canvas,i18n::text("Trascina o premi B · anteprima"),233,386,ui::Font::Small,ui::muted);
    else {
        ui::label(canvas,setting==3?"15 s":"Min",114,386,ui::Font::Small,ui::muted,ui::black,65);
        ui::label(canvas,setting==3?"180 s":"Max",350,386,ui::Font::Small,ui::muted,ui::black,65);
    }
    ui::dots(canvas,setting,7);
}

bool favoriteCanDim() {
    return favoriteFresh() && lightCapable[favorite] && settings.favorites[favorite].entity.startsWith("light.");
}

void openLight() {
    if(!favoriteCanDim()) { toast(i18n::text("Intensità non disponibile: aggiorna lo stato"));return; }
    lightEntity=settings.favorites[favorite].entity;lightFavorite=favorite;
    lightDraft.begin(lightLevels[favorite]);
    changeScreen(Screen::Light);sound(sfx::Cue::Open);
}

void drawLight() {
    header(i18n::text("Luce"));
    ui::label(canvas,lightEntity==settings.favorites[lightFavorite].entity?settings.favorites[lightFavorite].label:i18n::text("Luce modificata"),233,121,ui::Font::Body,ui::white);
    ui::icon(canvas,ui::Icon::Sun,233,194,50,ui::accent);
    ui::label(canvas,String(lightDraft.value)+"%",233,267,ui::Font::Clock,ui::white);
    ui::label(canvas,i18n::text("Luminosità"),233,306,ui::Font::Small,ui::muted);
    const int position=109+lightDraft.value*248/100;
    canvas.fillSmoothRoundRect(109,339,248,8,4,ui::line);
    canvas.fillSmoothRoundRect(109,339,std::max(8,position-109),8,4,ui::accent);
    canvas.fillSmoothCircle(position,343,15,ui::accent);
    ui::pill(canvas,233,412,220,i18n::text("Applica"),ui::accent,ui::ink);
}

void drawCasa() {
    header(i18n::text("Casa"));
    if(!settings.favoriteCount) {
        ui::icon(canvas,ui::Icon::House,233,210,76,ui::accent);
        text(i18n::text("Scegli i preferiti"),290,ui::white,2);
        text(i18n::text("da Home Assistant"),332,ui::muted,1);
        return;
    }
    const bool fresh=favoriteFresh();
    const bool cached=policy::usableState(settings.favorites[favorite].entity.c_str(),states[favorite].c_str());
    const bool on=cached && states[favorite]=="on";
    const bool direct=settings.favoritesMqtt?mqtt.connected():WiFi.status()==WL_CONNECTED;
    const bool phone=settings.favoritesMqtt && !direct && mqtt.available();
    ui::connectionIcon(canvas,354,79,direct,phone,surfaceColor);
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
        value=cached?(on?"ON":"OFF"):"--";
        detail=fresh?(light?(on?i18n::text("Accesa"):i18n::text("Spenta")):(on?i18n::text("Attivo"):i18n::text("Disattivo"))):i18n::text("Stato non aggiornato");
    } else if(!service.empty()) {
        value=service=="press"?i18n::text("Premi"):i18n::text("Avvia"); font=ui::Font::Title;
        detail=i18n::text("Tocca per confermare");
    } else {
        value=cached?states[favorite]:"--";
        char* end=nullptr; strtof(value.c_str(),&end);
        if(end==value.c_str() || *end!='\0') font=ui::Font::Title;
        else font=ui::Font::Clock;
        detail=fresh?units[favorite]:i18n::text("Stato non aggiornato");
    }
    ui::label(canvas,value,233,277,font,foregroundColor,surfaceColor,310);
    if(homeCommand.pending(favorite)) detail=i18n::text("Verifica stato…");
    else if(settings.favoritesMqtt && !mqtt.available()) detail=i18n::text("Collega Wi-Fi o iPhone");
    else if(!settings.favoritesMqtt && WiFi.status()!=WL_CONNECTED) detail=i18n::text("Wi-Fi non connesso");
    else if(!fresh && (settings.favoritesMqtt?mqtt.busy():home.busy())) detail=i18n::text("Aggiornamento…");
    else if(states[favorite]=="unavailable" || states[favorite]=="unknown") detail=i18n::text("Entita non disponibile");
    if(!fresh && cached) detail=homeCommand.pending(favorite)?i18n::text("Ultimo stato · verifica…"):direct || phone?i18n::text("Ultimo stato · aggiorno…"):i18n::text("Ultimo stato · offline");
    ui::label(canvas,detail,233,favoriteCanDim()?341:386,ui::Font::Body,secondary,surfaceColor,310);
    arrows();
    if(favoriteCanDim()) {
        ui::icon(canvas,ui::Icon::Sun,194,388,24,foregroundColor,surfaceColor);
        const String level=lightLevels[favorite]>=0?String(lightLevels[favorite])+"%":"--%";
        ui::label(canvas,level,250,388,ui::Font::Body,foregroundColor,surfaceColor,100);
        if(casaHoldProgress>0) {
            canvas.fillRoundRect(183,416,100,4,2,surfaceColor==ui::focus?ui::focusPanel:ui::line);
            canvas.fillRoundRect(183,416,std::max(4,casaHoldProgress),4,2,foregroundColor);
        }
    }
    ui::dots(canvas,favorite,settings.favoriteCount,434,foregroundColor,surfaceColor==ui::focus?ui::focusPanel:ui::line);
}

void drawNotes() {
    header(i18n::text("Note"));
    text(i18n::text("Un'idea al volo."),143,ui::white,2);
    ui::circleButton(canvas,ui::recordButton,ui::Icon::Mic,57,ui::accent,ui::ink);
    ui::label(canvas,String(notes.count())+i18n::text(" note sul dispositivo"),233,335,ui::Font::Small,ui::muted);
    ui::pill(canvas,233,386,230,notes.playing()?i18n::text("Ferma ascolto"):i18n::text("Ultima nota"));
    if(!notes.playing()) ui::icon(canvas,ui::Icon::Play,143,386,20,ui::white,ui::surface);
}

void drawRecording() {
    const auto state=notes.state();
    if(state==VoiceNotes::State::Preparing) {
        ui::icon(canvas,ui::Icon::Mic,233,207,86,ui::accent);
        text(i18n::text("Preparazione"),119,ui::white,2);
        text(i18n::text("Attendi REC"),299,ui::accent,2);
        text(i18n::text("Il microfono si sta attivando"),342,ui::muted,1);
    } else if(state==VoiceNotes::State::Saving) {
        ui::icon(canvas,ui::Icon::Archive,233,222,76,ui::accent);
        text(i18n::text("Salvataggio…"),315,ui::white,2);
    } else {
        canvas.fillSmoothCircle(144,86,5,red);
        ui::label(canvas,i18n::text("REGISTRAZIONE"),245,86,ui::Font::Small,ui::ink,surfaceColor,245);
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
    ui::label(canvas,touchRecording?i18n::text("Tocca per salvare"):i18n::text("Rilascia B per salvare"),233,435,ui::Font::Small,
        state==VoiceNotes::State::Recording?ui::ink:ui::muted,surfaceColor,216);
}

void drawSaved() {
    ui::icon(canvas,ui::Icon::Check,233,161,87,ui::ink,ui::focus);
    ui::label(canvas,i18n::text("Idea salvata."),233,256,ui::Font::Title,ui::ink,ui::focus);
    ui::label(canvas,i18n::text("Sul dispositivo"),233,313,ui::Font::Body,ui::ink,ui::focus);
    ui::pill(canvas,233,386,206,i18n::text("Fine"),ui::ink,ui::focus);
}

void drawPairing() {
    header("iPhone");
    if(companion.pairing()) {
        char code[7]; snprintf(code,sizeof(code),"%06lu",(unsigned long)companion.passkey());
        ui::label(canvas,code,233,224,ui::Font::Clock,ui::white,ui::black,340);
        text(i18n::text("Inserisci il codice su iPhone"),296,ui::muted,1);
        text(i18n::text("Abbinamento aperto per 90 s"),330,ui::muted,1);
        ui::pill(canvas,233,386,240,i18n::text("Nuovo codice"));
    } else {
        ui::icon(canvas,ui::Icon::Bluetooth,233,159,55,ui::accent);
        text(companion.connected()?i18n::text("iPhone connesso"):i18n::text("Pronto per abbinare"),219,ui::white,2);
        text(String(companion.bondedPhones())+i18n::text(" abbinamenti salvati"),256,ui::muted,1);
        ui::pill(canvas,233,311,296,i18n::text("Abbina iPhone"),forgetOption?ui::surface:ui::accent,forgetOption?ui::white:ui::ink);
        ui::pill(canvas,233,382,296,i18n::text("Rimuovi abbinamenti"),forgetOption?ui::accent:ui::surface,forgetOption?ui::ink:ui::white);
    }
}

void drawConfirmation() {
    const bool ordinary=screen==Screen::ActionConfirm || screen==Screen::LightConfirm;
    header(ordinary?i18n::text("Conferma"):i18n::text("Attenzione"));
    ui::icon(canvas,ui::Icon::Warning,233,157,53,ordinary?ui::accent:red);
    if(screen==Screen::LightConfirm) {
        text(i18n::text("Applico il nuovo livello?"),210,ui::white,2);
        text(String(lightDraft.value)+"%",262,ui::accent,2);
        ui::pill(canvas,157,331,115,i18n::text("Annulla"));
        ui::pill(canvas,309,331,115,i18n::text("Applica"),ui::accent,ui::ink);
    } else if(screen==Screen::ActionConfirm) {
        text(i18n::text("Eseguire il comando?"),215,ui::white,2);
        text(favorite<settings.favoriteCount?settings.favorites[favorite].label:i18n::text("Preferito modificato"),260,ui::muted,1);
        ui::pill(canvas,157,331,115,i18n::text("Annulla"));
        ui::pill(canvas,309,331,115,i18n::text("Esegui"),adjustOption?ui::surface:ui::accent,adjustOption?ui::white:ui::ink);
        if(favoriteCanDim()) ui::pill(canvas,233,391,240,i18n::text("Regola"),adjustOption?ui::accent:ui::surface,adjustOption?ui::ink:ui::white);
    } else {
        text(screen==Screen::FormatConfirm?i18n::text("Eliminare note e Badge?"):i18n::text("Rimuovere gli iPhone?"),223,ui::white,2);
        text(screen==Screen::FormatConfirm?i18n::text("L'archivio sara cancellato"):i18n::text("Tutti gli abbinamenti saranno rimossi"),270,ui::muted,1);
        text(i18n::text("Conferma con il pulsante B"),320,red,1);
        ui::pill(canvas,233,386,266,i18n::text("Annulla"));
    }
}

void drawPortal() {
    header(i18n::text("Browser Wi-Fi"));
    if(portal.active()) {
        canvas.fillRoundRect(132,129,202,202,10,ui::white);
        const String qr=showPageQr?portal.url():portal.wifiQr();
        canvas.qrcode(qr.c_str(),141,138,184,showPageQr?3:4,true);
        text(showPageQr?i18n::text("2 · Apri la pagina"):i18n::text("1 · Unisci il Wi-Fi"),350,ui::white,1);
        text(String("stopwatch / ")+portal.password(),385,ui::accent,1,305);
        text(i18n::text("Tocca per cambiare QR"),421,ui::muted,1,230);
    } else {
        text(i18n::text("Sessione terminata"),223,ui::white,2);
        ui::pill(canvas,233,386,266,i18n::text("Riapri"));
    }
}

void drawNotice() {
    noticeDisplayed=!notice.isEmpty() && uint32_t(millis()-noticeAt)<noticeDuration && !notes.busy() &&
        screen!=Screen::Companion && screen!=Screen::NoteSaved;
    if(!noticeDisplayed) return;
    String first=notice,second=noticeDetail;
    while(noticeDetail.isEmpty() && first.length()>1) {
        if(ui::textWidth(canvas,first,ui::Font::Body)<=212) break;
        const int space=first.lastIndexOf(' ');
        if(space<=0) break;
        second=first.substring(space+1)+(second.isEmpty()?"":" "+second);
        first=first.substring(0,space);
    }
    scene_ui::notice(canvas,first,second,noticeIcon);
}

void drawDiagnostics() {
    header(i18n::text("Diagnostica"));
    text(i18n::text(diagnostics[diagnostic]),135,ui::white,2);
        switch (diagnostic) {
        case 0:
            text(String("A: ") + (M5.BtnA.isPressed() ? i18n::text("PREMUTO") : "rilasciato"), 207, yellow);
            text(String("B: ") + (M5.BtnB.isPressed() ? i18n::text("PREMUTO") : "rilasciato"), 253, blue);
            text(String(i18n::text("Pressioni A / B: ")) + pressesA + " / " + pressesB, 305, muted, 1); break;
        case 1:
            text(M5.Touch.isEnabled() ? i18n::text("Tocca lo schermo") : i18n::text("Touch non rilevato"), 215, blue);
            text(String("X ") + touchX + "   Y " + touchY, 273);
            if (touchX >= 0) { canvas.drawCircle(touchX, touchY, 12, accent); } break;
        case 2: {
            const uint32_t colors[] = {red, accent, blue, 0xffffff, 0x000000};
            canvas.fillRoundRect(95, 177, 276, 132, 15, colors[displayColor]);
            canvas.drawRoundRect(95, 177, 276, 132, 15, muted);
            text(i18n::text("B cambia colore · verifica i pixel"), 332, muted, 1); break;
        }
        case 3: text(i18n::text("B: impulso di 250 ms"), 225, blue); text(i18n::text("Verifica manuale della vibrazione"), 285, muted, 1); break;
        case 4: text(i18n::text("B: tono breve"), 216, blue); text(String("Volume: ") + settings.volume*100/200 + "%", 270); break;
        case 5:
            text(M5.Mic.isEnabled() ? i18n::text("Microfono configurato") : i18n::text("Microfono assente"), 209, blue);
            text(i18n::text("Tieni B per una nota di prova"), 258, muted, 1);
            text(i18n::text("B breve riascolta l'ultima nota"), 302, muted, 1); break;
        case 6:
            text(M5.Imu.isEnabled() ? i18n::text("BMI270 rilevato") : i18n::text("IMU non rilevata"), 191, blue);
            text(String("Accel ") + String(imu.accel.x, 2) + " " + String(imu.accel.y, 2) + " " + String(imu.accel.z, 2), 242, 0xffffff, 1);
            text(String("Gyro ") + String(imu.gyro.x, 1) + " " + String(imu.gyro.y, 1) + " " + String(imu.gyro.z, 1), 283, 0xffffff, 1); break;
        case 7: {
            m5::rtc_datetime_t rtc{};
            const bool valid = M5.Rtc.getDateTime(&rtc);
            text(valid ? i18n::text("RTC rilevato · UTC") : i18n::text("RTC non leggibile"), 198, blue);
            char stamp[40];
            snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d  %02d:%02d:%02d", rtc.date.year, rtc.date.month, rtc.date.date, rtc.time.hours, rtc.time.minutes, rtc.time.seconds);
            text(stamp, 248, 0xffffff, 1);
            text(M5.Rtc.getVoltLow() ? i18n::text("Ora da sincronizzare") : i18n::text("Sincronizza da Wi-Fi o iPhone"), 300, muted, 1); break;
        }
        case 8:
            text(String(batteryMv) + " mV", 201, blue, 3);
            text(String(i18n::text("Carica stimata: ")) + batteryLevel + "%", 260, 0xffffff, 1);
            text(String("USB: ") + M5.Power.getVBUSVoltage() + " mV", 293, muted, 1);
            text(M5.Power.isCharging() == m5::Power_Class::is_charging ? i18n::text("In carica") : i18n::text("Carica non attiva / non rilevata"), 325, muted, 1); break;
        case 9:
            text(WiFi.status() == WL_CONNECTED ? i18n::text("CONNESSO") : i18n::text("NON CONNESSO"), 198, blue);
            text(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : i18n::text("B apre la configurazione"), 247, 0xffffff, 1);
            text(String(i18n::text("Segnale: ")) + (WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "--"), 294, muted, 1); break;
        case 10:
            text(companion.connected() ? i18n::text("iPhone connesso e autenticato") : companion.ready() ? i18n::text("Companion BLE attivo") : i18n::text("BLE non disponibile"), 204, blue, 1);
            text(companion.name(), 253, 0xffffff, 1);
            text(String(companion.bondedPhones()) + i18n::text(" abbinamenti salvati"), 295, muted, 1);
            text(i18n::text("B: abbinamento iPhone"), 327, muted, 1); break;
        case 11:
            text(memoryOk ? i18n::text("PSRAM: prova superata") : i18n::text("PSRAM: prova fallita"), 190, memoryOk ? accent : red, 1);
            text(String(i18n::text("PSRAM libera: ")) + ESP.getFreePsram() / 1024 + " KB", 230, muted, 1);
            text(notes.ready() ? String(i18n::text("Archivio: ")) + LittleFS.usedBytes() / 1024 + " / " + LittleFS.totalBytes() / 1024 + " KB" : i18n::text("Archivio non inizializzato"), 270, blue, 1);
            text(i18n::text("B: inizializza / cancella archivio"), 312, muted, 1); break;
        case 12:
            text("Grove I2C: SDA 10 / SCL 11", 196, blue, 1);
            text(groveResult.isEmpty() ? i18n::text("B: scansione indirizzi I2C") : groveResult, 244, 0xffffff, 1);
            text(i18n::text("Uscita 5 V disattivata"), 285, muted, 1);
            text(i18n::text("GPIO posteriori: serve un fixture"), 323, muted, 1); break;
        case 13: text(ledOn ? i18n::text("LED acceso") : i18n::text("LED spento"), 222, blue); text(i18n::text("B cambia · verifica visiva"), 281, muted, 1); break;
        }
    footer(i18n::text("Test dopo"),i18n::text("Prova"));
}

void draw() {
    const uint32_t keyPrompt=security_key::promptId();
    const bool lit=screen==Screen::HomeAssistant && favorite<settings.favoriteCount && states[favorite]=="on";
    const bool recording=notes.state()==VoiceNotes::State::Recording;
    surfaceColor=!quickPanelOpen && (recording || (!notes.busy() && (lit || screen==Screen::NoteSaved)))?ui::focus:ui::black;
    foregroundColor=surfaceColor==ui::focus?ui::ink:ui::white;
    canvas.fillScreen(surfaceColor);
    if(quickPanelOpen) ui::quickSettings(canvas,settings.doNotDisturb,quietSavePending,quietSaveFailed,quickPage,quickPreview>=0?quickPreview:quickPage==1?settings.brightness:settings.volume);
    else if(notes.busy()) drawRecording();
    else switch(screen) {
    case Screen::Home: drawHome(); break;
    case Screen::SecurityKey: {
        header(i18n::text("Chiave · LAB"));
        ui::icon(canvas,ui::Icon::Key,233,145,58,security_key::awaiting()?ui::focus:ui::accent);
        const String rp=security_key::awaiting()?security_key::relyingParty():String();
        const int lines=(rp.length()+19)/20;
        if(!rp.length()) ui::label(canvas,i18n::text("Prototipo reversibile"),233,230,ui::Font::Label,ui::white,ui::black,320);
        else for(int i=0;i<lines;++i)
            ui::label(canvas,rp.substring(i*20,std::min(int(rp.length()),(i+1)*20)),233,230-(lines-1)*12+i*24,ui::Font::Small,ui::white,ui::black,340);
        ui::label(canvas,security_key::message(),233,lines>2?302:279,ui::Font::Small,ui::muted,ui::black,330);
        if(security_key::awaiting()) {
            ui::pill(canvas,233,342,272,i18n::text("B blu · Conferma"),ui::focusPanel,ui::focus);
            ui::label(canvas,i18n::text("A · Annulla"),233,396,ui::Font::Small,ui::muted);
        } else {
            ui::label(canvas,i18n::text("Solo account di prova"),233,352,ui::Font::Body,ui::muted);
            ui::label(canvas,i18n::text("A lungo · Esci"),233,398,ui::Font::Small,ui::muted);
        }
        break;
    }
    case Screen::UsbControl:
        header(i18n::text("Ponte"));
        ui::icon(canvas,ui::Icon::Usb,233,183,85,usbControl.connected()?ui::accent:ui::muted);
        text(usbControl.fault()?i18n::text("Connessione interrotta"):usbPaused?i18n::text("In pausa"):usbControl.connected()?i18n::text("Mouse e tastiera"):i18n::text("Collega il cavo USB-C"),270,ui::white,2);
        text(companion.connected()?i18n::text("Controlla dal Companion"):i18n::text("Apri il Companion sull'iPhone"),310,ui::muted,1);
        ui::pill(canvas,233,372,240,usbPaused || usbControl.fault()?i18n::text("Riprendi"):i18n::text("Sospendi"));
        break;
    case Screen::System: drawSystem(); break;
    case Screen::SettingsMenu: drawSettingsMenu(); break;
    case Screen::SettingsSection: drawSettingsSection(); break;
    case Screen::Settings: drawSettings(); break;
    case Screen::HomeAssistant: drawCasa(); break;
    case Screen::Light: drawLight();break;
    case Screen::Notes: drawNotes(); break;
    case Screen::Badge: drawBadge(); break;
    case Screen::Spotify: {
        const bool artwork=musicArtwork.draw(canvas);
        const String message=!mqtt.available()?i18n::text("Offline · ultimo brano"):!mqtt.playback().fresh(millis()) && !mqtt.playback().title.empty()?i18n::text("Ultimo stato · aggiorno…"):mqtt.musicMessage();
        music_ui::player(canvas,mqtt.playback(),millis(),mqtt.musicBusy(),message,artwork,musicArtwork.background());
        ui::connectionIcon(canvas,365,75,mqtt.connected(),!mqtt.connected() && mqtt.available(),musicArtwork.background());
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
    case Screen::Focus: productivity_ui::focus(canvas,focusSession);break;
    case Screen::Slides: productivity_ui::slides(canvas,presentationTime.elapsed(),usbControl.connected());break;
    case Screen::Timer: clock_ui::timer(canvas,clockTimer);break;
    case Screen::Stopwatch: clock_ui::stopwatch(canvas,chronometer);break;
    case Screen::NoteSaved: drawSaved(); break;
    case Screen::Pairing: drawPairing(); break;
    case Screen::Companion: drawPortal(); break;
    case Screen::ActionConfirm:
    case Screen::LightConfirm:
    case Screen::FormatConfirm:
    case Screen::ForgetConfirm: drawConfirmation(); break;
    case Screen::Diagnostics: drawDiagnostics(); break;
    }
    if(quickPanelOpen) noticeDisplayed=false;
    else drawNotice();
    canvas.pushSprite(0,0);
    if(screen==Screen::SecurityKey && !quickPanelOpen && !screenOff) security_key::presented(keyPrompt);
    dirty=false;
    lastDraw=millis();
}

void diagnosticAction() {
    sfx::stop();
    if(settings.doNotDisturb && (diagnostic==3 || diagnostic==4 || diagnostic==5 || diagnostic==13)) {
        toast(i18n::text("Non disturbare attivo"));return;
    }
    switch (diagnostic) {
    case 2: displayColor = (displayColor + 1) % 5; break;
    case 3: vibrate(250, true); break;
    case 4:
        if(notes.playing()) { toast(i18n::text("Ferma prima l'ascolto della nota")); break; }
        M5.Mic.end();
        if (M5.Speaker.begin()) { M5.Speaker.setVolume(policy::speakerVolume(settings.volume)); M5.Speaker.tone(880, 180); }
        else toast(i18n::text("Avvio altoparlante fallito"));
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
        if (groveResult.isEmpty()) groveResult = i18n::text("Nessun dispositivo alimentato rilevato");
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
    if(!next.save()) { applySettings(); toast(i18n::text("Impostazioni non salvate")); return; }
    settings=next;
    applySettings();
    mqtt.changed();
    if(setting==2) vibrate(120);
    sound(setting==4?sfx::Cue::Success:setting==1?sfx::Cue::Volume:sfx::Cue::Select,true);
    dirty=true;
}

bool executeFavorite() {
    if(favorite>=settings.favoriteCount) { toast(i18n::text("Scegli i preferiti in Home Assistant")); return false; }
    if(settings.favoritesMqtt?mqtt.busy():home.busy()) { toast(i18n::text("Attendi la risposta di Home Assistant")); return false; }
    const auto service=policy::service(settings.favorites[favorite].entity.c_str());
    const bool action=!service.empty();
    if(action && homeCommand.pending()) { toast(i18n::text("Attendi la verifica del comando")); return false; }
    if(service=="toggle" && !favoriteFresh()) {
        if(settings.favoritesMqtt) mqtt.requestFavorite(favorite,false);
        else if(homeReady) home.request(settings,favorite,false);
        toast(i18n::text("Aggiorno lo stato: riprova")); return false;
    }
    if(settings.favoritesMqtt?mqtt.requestFavorite(favorite,action):homeReady&&home.request(settings,favorite,action)) {
        lastHaRead=millis();
        if(service=="toggle") homeCommand.begin(favorite,states[favorite]=="on",millis());
        if(action) stateUpdated[favorite]=0;
        if(action) toast(i18n::text("Invio comando…"));
        else dirty=true;
        return true;
    } else toast(i18n::text("Controlla rete e orologio"));
    return false;
}

bool applyLight() {
    if(favorite!=lightFavorite || favorite>=settings.favoriteCount || settings.favorites[favorite].entity!=lightEntity) {
        toast(i18n::text("Preferito modificato: riapri la luce"));return false;
    }
    if(!favoriteCanDim() || homeCommand.pending() || (settings.favoritesMqtt?mqtt.busy():home.busy())) {
        toast(i18n::text("Attendi lo stato aggiornato della luce"));return false;
    }
    if(!(settings.favoritesMqtt?mqtt.requestFavorite(favorite,true,lightDraft.value):homeReady&&home.request(settings,favorite,true,lightDraft.value))) {
        toast(i18n::text("Invio non riuscito: controlla la connessione"));return false;
    }
    homeCommand.begin(favorite,states[favorite]=="on",millis(),lightDraft.value);
    stateUpdated[favorite]=0;lastHaRead=millis();
    changeScreen(Screen::HomeAssistant);toast(i18n::text("Invio intensità…"));return true;
}

bool musicCommand(music::Command command,const String& source="") {
    if(mqtt.requestMusic(command,source)) { lastMusicRead=millis();dirty=true;sound(sfx::Cue::Select);return true; }
    else if(!mqtt.available()) toast(i18n::text("Collega Wi-Fi o iPhone"));
    else if(mqtt.musicBusy()) toast(i18n::text("Attendi Spotify"));
    else if(!mqtt.playback().fresh(millis())) { mqtt.requestMusic();toast(i18n::text("Aggiorno Spotify…")); }
    else toast(i18n::text("Comando non disponibile sul lettore"));
    return false;
}

void playlistPage(uint32_t offset) {
    if(mqtt.requestMusic(music::Command::Playlists,"",offset)) {
        playlistRequested=true;playlistItem=0;lastMusicRead=millis();dirty=true;
    } else toast(mqtt.available()?i18n::text("Attendi Spotify"):i18n::text("Collega Wi-Fi o iPhone"));
}

void slideKey(bool next,bool blackout=false) {
    if(!usbControl.connected()) { toast(i18n::text("Collega il cavo USB dati"));return; }
    const uint8_t key=blackout?0x05:settings.presentationPageKeys?(next?0x4e:0x4b):(next?0x4f:0x50);
    if(!usbControl.presentationKey({key,0})) { toast(i18n::text("USB occupata: riprova"));return; }
    sound(sfx::Cue::Select);dirty=true;
}

void selectAction(bool fromButton=false) {
    if(screen==Screen::Slides) { slideKey(true,!fromButton);return; }
    if(screen==Screen::Focus) { focusSession.toggle(millis());dirty=true;sound(sfx::Cue::Select);return; }
    switch(screen) {
    case Screen::UsbControl:
        if(usbControl.fault()) { usbPaused=true;usbControl.update(false,companion.connected(),millis()); }
        usbPaused=!usbPaused;dirty=true;break;
    case Screen::Home: {
        if(fromButton && !homeMenu.apps) { homeMenu.apps=true;homeMenu.selected=0;sound(sfx::Cue::Select);break; }
        const Screen pages[]={Screen::HomeAssistant,Screen::Notes,Screen::Badge,Screen::ClockMenu,Screen::Spotify,Screen::UsbControl,Screen::SecurityKey,Screen::System,Screen::Focus,Screen::Slides};
        homeMenu.apps=true;changeScreen(pages[homeMenu.app()]); sound(sfx::Cue::Open); break;
    }
    case Screen::System: {
        const Screen pages[]={Screen::SettingsMenu,Screen::Diagnostics,Screen::Pairing,Screen::Companion};
        changeScreen(pages[systemItem]); sound(sfx::Cue::Open); break;
    }
    case Screen::SettingsMenu:
        changeScreen(ui::settingsSection(setting)==2?Screen::Settings:Screen::SettingsSection);
        sound(sfx::Cue::Open);break;
    case Screen::SettingsSection:
        changeScreen(Screen::Settings);sound(sfx::Cue::Open);break;
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
            if(next.save()) settings=next;else toast(i18n::text("Durata non salvata"));
        }
        clockTimer.toggle(millis());sound(sfx::Cue::Select);break;
    case Screen::Stopwatch:
        if(chronometer.elapsed()==clockwork::Stopwatch::maxElapsed) backAction();else chronometer.toggle(millis());
        sound(sfx::Cue::Select);break;
    case Screen::Badge:
        if(!badgeApp.available()) { toast(i18n::text("Apri Badge nell'app iPhone")); break; }
        if(fromButton && !settings.badgeAutoRotate) {
            badgeApp.rotateManual(); badgeControlsUntil=0;
            sound(sfx::Cue::Select);
        } else if(!badgeControls()) badgeControlsUntil=millis()+4000;
        else if(badgeApp.animated()) {
            Settings next=settings; next.badgeAnimate=!next.badgeAnimate;
            if(next.save()) settings=next; else toast(i18n::text("Impostazioni Badge non salvate"));
            badgeControlsUntil=millis()+4000;
        } else backAction();
        break;
    case Screen::HomeAssistant:
        if(favorite<settings.favoriteCount && policy::confirmAction(settings.favorites[favorite].entity.c_str())) {
            if(homeCommand.pending()) { toast(i18n::text("Attendi la verifica del comando")); break; }
            changeScreen(Screen::ActionConfirm);
            sound(sfx::Cue::Warning);
        } else executeFavorite();
        break;
    case Screen::ActionConfirm:
        if(adjustOption) { openLight();break; }
        if(!ui::confirmationReady(millis(),transitionAt)) break;
        if(executeFavorite()) { changeScreen(Screen::HomeAssistant);lastHaRead=millis(); }
        break;
    case Screen::Light:
        if(fromButton) { lightDraft.step(1);dirty=true;sound(sfx::Cue::Select); }
        else applyLight();
        break;
    case Screen::LightConfirm:
        if(ui::confirmationReady(millis(),transitionAt)) applyLight();
        break;
    case Screen::Notes:
        playNote();
        break;
    case Screen::NoteSaved: changeScreen(Screen::Notes); break;
    case Screen::Pairing:
        if(forgetOption && !companion.pairing()) changeScreen(Screen::ForgetConfirm);
        else if(companion.ready()) { companion.openPairing(); forgetOption=false; }
        else toast(i18n::text("Companion BLE non disponibile"));
        break;
    case Screen::ForgetConfirm:
        companion.forgetPhones(); forgetOption=false; changeScreen(Screen::Pairing); toast(i18n::text("Abbinamenti rimossi")); break;
    case Screen::Companion:
        if(!portal.active()) portal.start(); else showPageQr=!showPageQr;
        break;
    case Screen::FormatConfirm:
        sfx::stop();
        badgeApp.abortUpload();
        { const bool initialized=notes.initializeStorage(); badgeApp.begin();
          if(initialized) sfx::begin(settings.soundPack);
          toast(initialized?i18n::text("Archivio inizializzato"):i18n::text("Inizializzazione archivio fallita")); }
        changeScreen(Screen::Diagnostics); break;
    }
    dirty=true;
}

void backAction() {
    switch(screen) {
    case Screen::Home: homeMenu.apps=false;dirty=true;break;
    case Screen::Settings: changeScreen(ui::settingsSection(setting)==2?Screen::SettingsMenu:Screen::SettingsSection);break;
    case Screen::SettingsSection: changeScreen(Screen::SettingsMenu);break;
    case Screen::SettingsMenu:
    case Screen::Diagnostics:
    case Screen::Pairing:
    case Screen::Companion: changeScreen(Screen::System); break;
    case Screen::FormatConfirm: changeScreen(Screen::Diagnostics); break;
    case Screen::ForgetConfirm: changeScreen(Screen::Pairing); break;
    case Screen::ActionConfirm:
    case Screen::Light:
    case Screen::LightConfirm: changeScreen(Screen::HomeAssistant); break;
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

void nextAction(int direction=1,bool page=false) {
    if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    if(screen==Screen::Slides) { slideKey(page?direction>0:false);return; }
    if(screen==Screen::Focus) { if(focusSession.reset()) { dirty=true;sound(sfx::Cue::Select); }return; }
    switch(screen) {
    case Screen::Home:
        if(page || !homeMenu.apps) homeMenu.nextSpace(direction);else homeMenu.nextApp(direction);
        transitionAt=millis();break;
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
    case Screen::System: systemItem=page?((systemItem/2+2+direction)%2)*2:(systemItem+4+direction)%4; break;
    case Screen::SettingsMenu: setting=ui::settingsItems[(ui::settingsSection(setting)+3+direction)%3][0];break;
    case Screen::SettingsSection: {
        const int section=ui::settingsSection(setting),count=ui::settingsCount(section),position=ui::settingsPosition(setting);
        const int pages=(count+1)/2;
        const int next=page?((position/2+pages+direction)%pages)*2:(position+count+direction)%count;
        setting=ui::settingsItems[section][next];
        break;
    }
    case Screen::Settings: setting=(setting+7+direction)%7; break;
    case Screen::Diagnostics:
        M5.Power.setLed(0); ledOn=false; diagnostic=(diagnostic+14+direction)%14; break;
    case Screen::ActionConfirm:
        if(favoriteCanDim()) adjustOption=!adjustOption;break;
    case Screen::Light:
        lightDraft.step(-1);break;
    case Screen::LightConfirm:
        backAction();return;
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
    if(screen==Screen::Light || screen==Screen::LightConfirm) changeScreen(Screen::HomeAssistant);
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
    if(gesture.kind==GestureKind::HoldProgress) {
        if(screen==Screen::HomeAssistant && !notes.busy() && !quickPanelOpen && !noticeTouch) { casaHoldProgress=gesture.progress;dirty=true; }
        return;
    }
    if(gesture.kind!=GestureKind::None && casaHoldProgress) { casaHoldProgress=0;dirty=true; }
    if(gesture.kind==GestureKind::None) return;
    lastInteraction=millis();
    if(gesture.kind==GestureKind::QuickOpen) { showQuickPanel(true);return; }
    if(quickPanelOpen) {
        if(gesture.kind==GestureKind::QuickClose) showQuickPanel(false);
        else if(gesture.kind==GestureKind::Left || gesture.kind==GestureKind::Right) {
            quickPage=(quickPage+3+(gesture.kind==GestureKind::Left?1:-1))%3;quickPreview=-1;dirty=true;
        } else if(quickPage>0 && (gesture.kind==GestureKind::Drag || gesture.kind==GestureKind::Release)) {
            if(gesture.kind==GestureKind::Drag) {
                quickPreview=ui::sliderValue(quickPage==1?0:1,gesture.x);
                if(quickPage==1) M5.Display.setBrightness(quickPreview);
            } else if(quickPreview>=0) {
                if(quickPage==1) settings.brightness=quickPreview;else settings.volume=quickPreview;
                quickPreview=-1;quietSavePending=true;quietSaveFailed=false;mqtt.changed();
            }
            dirty=true;
        } else if(gesture.kind==GestureKind::Tap) {
            if(ui::quickClose.contains(gesture.x,gesture.y) && ui::quickClose.contains(gesture.originX,gesture.originY)) showQuickPanel(false);
            else {
                for(int i=0;i<3;++i) if(ui::Circle{151+i*82,176,25}.contains(gesture.x,gesture.y) && ui::Circle{151+i*82,176,25}.contains(gesture.originX,gesture.originY)) { quickPage=i;quickPreview=-1;dirty=true;return; }
                if(quickPage==0 && ui::quietToggle.contains(gesture.x,gesture.y) && ui::quietToggle.contains(gesture.originX,gesture.originY)) toggleQuiet();
            }
        }
        return;
    }
    if(noticeTouch) {
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
        if(gesture.kind==GestureKind::Tap) { notice=""; dirty=true; }
        return;
    }
    if(gesture.kind==GestureKind::Hold) {
        if(screen==Screen::HomeAssistant && !notes.busy() && favoriteCanDim()) { vibrate();openLight(); }
        return;
    }
    if(screen==Screen::Home && (gesture.kind==GestureKind::Up || gesture.kind==GestureKind::Down)) {
        homeMenu.apps=true;homeMenu.nextApp(gesture.kind==GestureKind::Up?1:-1);
        transitionAt=millis();dirty=true;vibrate();sound(sfx::Cue::Select);return;
    }
    if(gesture.kind==GestureKind::Up || gesture.kind==GestureKind::Down) {
        if(gesture.kind==GestureKind::Up && screen==Screen::HomeAssistant && !notes.busy()) openLight();
        return;
    }
    if(screen==Screen::Light && !notes.busy() && (gesture.kind==GestureKind::Drag || gesture.kind==GestureKind::Release)) {
        lightDraft.point(gesture.x,gesture.y);dirty=true;return;
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
    const auto target=ui::hit(screen,gesture.x,gesture.y,touchSelection(),notes.busy(),badgeControls(),clockControls(),homeMenu,favoriteCanDim());
    const auto origin=ui::hit(screen,gesture.originX,gesture.originY,touchSelection(),notes.busy(),badgeControls(),clockControls(),homeMenu,favoriteCanDim());
    if(screen==Screen::Diagnostics && diagnostic==1 && origin.action==Action::None) {
        touchX=gesture.x; touchY=gesture.y; dirty=true; return;
    }
    if(gesture.kind==GestureKind::Left || gesture.kind==GestureKind::Right) {
        if(screen==Screen::Light || screen==Screen::LightConfirm) return;
        if(!notes.busy() && (screen==Screen::Clock || screen==Screen::Timer || screen==Screen::Stopwatch)) {
            const int page=screen==Screen::Clock?0:screen==Screen::Timer?1:2;
            clockPage((page+3+(gesture.kind==GestureKind::Left?1:-1))%3);vibrate();sound(sfx::Cue::Open);return;
        }
        if(!notes.busy()) { vibrate(); nextAction(gesture.kind==GestureKind::Left?1:-1,true); }
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
    case Action::Next: nextAction(1,true); break;
    case Action::Previous: nextAction(-1,true); break;
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
        if(screen==Screen::Focus) { if(focusSession.reset()) { dirty=true;sound(sfx::Cue::Select); } }
        else if(screen==Screen::Timer) timerSecondaryAction();else if(screen==Screen::Stopwatch) stopwatchSecondaryAction();
        break;
    case Action::Open:
        if(target.index==99 && (screen==Screen::HomeAssistant || screen==Screen::ActionConfirm)) { openLight();break; }
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
                if(next.save()) settings=next; else toast(i18n::text("Impostazioni Badge non salvate"));
            }
            badgeControlsUntil=millis()+4000; dirty=true; break;
        }
        if(screen==Screen::Home) homeMenu.nextApp(target.index);
        else if(screen==Screen::SettingsMenu || screen==Screen::SettingsSection) setting=target.index;
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
            ui::label(canvas,i18n::text("Caricamento…"),233,215,ui::Font::Label,ui::white);
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
    usbControl.begin();
    auto config = M5.config();
    config.output_power = false;
    config.fallback_board = m5::board_t::board_M5StopWatch;
    config.serial_baudrate = 115200;
    M5.begin(config);
    Serial.printf("[boot] hardware=%d heap_ok=%d\n", int(M5.getBoard()), heap_caps_check_integrity_all(true));
    if (M5.getBoard() != m5::board_t::board_M5StopWatch) {
        Serial.println(i18n::text("Hardware diverso da M5Stack StopWatch: avvio interrotto."));
        for (;;) delay(1000);
    }
    settingsOk = settings.load();
    applyRegion(settings);
    clockTimer.setSeconds(settings.timerSeconds);
    homeMenu.configure(settings.homeApps,settings.homeAppCount);
    applySettings();
    M5.Power.setVibration(0);
    // The default 2 kHz PWM is audible; preserve duty and move the carrier above hearing.
    auto& motorController=static_cast<m5::M5IOE1_Class&>(M5.getIOExpander(0));
    if(!motorController.setPwmFrequency(24000)) Serial.println("[haptics] frequenza silenziosa non impostata");
    M5.Power.setLed(0);
    memoryOk = checkMemory();
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(466, 466)) {
        M5.Display.setTextColor(0xffffff, 0x000000);
        M5.Display.drawString(i18n::text("PSRAM: impossibile avviare UI"), 50, 233);
        for (;;) delay(1000);
    }
    if(!ui::beginFonts()) toast(i18n::text("Caratteri UI non disponibili"));
    BootIndicator boot;
    // Setup leaves the canvas to this task until it has acknowledged completion.
    const bool bootIndicatorStarted=xTaskCreate(showBootLoading,"boot-display",4096,&boot,1,nullptr)==pdPASS;
    notes.begin();
    security_key::begin();
    if(notes.ready()) badgeApp.begin();
    if(notes.ready()) sfx::begin(settings.soundPack);
    Serial.printf("[boot] display pronto, archivio=%d heap_ok=%d\n", notes.ready(), heap_caps_check_integrity_all(true));
    setenv("TZ", "UTC0", 1); tzset();
    if (M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow()) M5.Rtc.setSystemTimeFromRtc();
    applyRegion(settings);
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
    if (!companion.begin()) toast(i18n::text("Avvio companion BLE fallito"));
    mqtt.setRelay(&companion);mqtt.begin();
    if (settings.ssid.isEmpty()) changeScreen(Screen::Pairing);
    Serial.println("[boot] companion pronto");
    if (!settingsOk) toast(i18n::text("Configurazione illeggibile: valori predefiniti"));
    else if (!notes.ready()) toast(i18n::text("Archivio da inizializzare in Diagnostica"));
    else if(!sfx::ready() && settings.soundPack) toast(i18n::text("Pacchetti audio da installare via USB"));
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
    presentationTime.update(now);
    focusSession.update(now);
    const unsigned focusAlert=focusSession.alert(now,notes.busy() || notes.playing() || security_key::awaiting());
    if(focusAlert==1) {
        if(screenOff) { screenOff=false;displayDimmed=false;M5.Display.setBrightness(settings.brightness);lastInteraction=now; }
        toast(focusSession.phase()==clockwork::Focus::Phase::Work?i18n::text("Focus completato"):i18n::text("Pausa terminata"));
        vibrate(180);sound(sfx::Cue::Complete,true);
    }
    const bool stopwatchWasRunning=chronometer.running();chronometer.update(now);
    if(stopwatchWasRunning && !chronometer.running() && screen==Screen::Stopwatch) dirty=true;
    sfx::update();
    if(usbControl.update(((screen==Screen::UsbControl && !usbPaused) || screen==Screen::Slides) && !quickPanelOpen && !notes.busy(),companion.connected(),now,screen==Screen::Slides)) dirty=true;
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
    if(security_key::update(screen==Screen::SecurityKey && !quickPanelOpen && !notes.busy() && !screenOff,M5.BtnA.isPressed(),M5.BtnB.isPressed())) dirty=true;
    auto events=controls.update(now,M5.BtnA.isPressed(),M5.BtnB.isPressed());
    if(screen==Screen::SecurityKey && !quickPanelOpen) {
        // Only the physical presence gate consumes B; never start recording or confirm through touch/BLE.
        events.next=events.select=events.recordStart=events.recordStop=false;
    }
    if(notes.busy() || events.recordStop || events.next || events.select || events.home || events.back) lastInteraction=now;
    if(events.next || events.select || events.back || events.home || events.recordStart) {
        touchInput.suppressUntilReleased();
        if(quickPreview>=0) { quickPreview=-1;applySettings(); }
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    }
    if(events.recordStop) stopRecording();
    const bool panelButtons=quickPanelOpen;
    if(panelButtons) {
        if(events.next || events.back) showQuickPanel(false);
        else if(events.select) {
            if(quickPage==0) toggleQuiet();
            else {
                if(quickPage==1) settings.brightness=settings.brightness>=240?32:std::min(255,int(settings.brightness)+32);
                else settings.volume=settings.volume>=200?0:std::min(200,int(settings.volume)+20);
                quietSavePending=true;quietSaveFailed=false;applySettings();mqtt.changed();dirty=true;
            }
        }
    } else {
        if(events.next) mqtt.button("a","press");
        if(events.select) mqtt.button("b","press");
        if(events.back) mqtt.button("a","hold");
    }
    if(events.recordStart) mqtt.button("b","hold");
    if(events.recordStop) mqtt.button("b","release");
    if(events.home || events.recordStart) { if(quickPanelOpen) showQuickPanel(false); }
    if(events.home || (!panelButtons && events.back)) {
        if(events.home) homeMenu.apps=false;
        stopRecording();
        if(!notes.busy()) {
            if(events.home) changeScreen(Screen::Home);
            else if((screen==Screen::HomeAssistant || screen==Screen::ActionConfirm) && favoriteCanDim()) openLight();
            else if(screen==Screen::Light && lightDraft.value!=lightDraft.initial) changeScreen(Screen::LightConfirm);
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
        if(quickPreview>=0) { quickPreview=-1;applySettings(); }
        if(settingPreview>=0) { settingPreview=-1; applySettings(); }
    }
    if(!screenOff) {
        const bool slider=!notes.busy() && !noticeTouch &&
            ((quickPanelOpen && quickPage>0 && ui::Rect{84,310,298,66}.contains(touch.x,touch.y)) ||
             (!quickPanelOpen && (screen==Screen::Settings || screen==Screen::Light) && ui::hit(screen,touch.x,touch.y,touchSelection()).action==ui::Action::Slider));
        const bool hold=!quickPanelOpen && !noticeTouch && !notes.busy() && screen==Screen::HomeAssistant && favoriteCanDim() && ui::casaCard.contains(touch.x,touch.y);
        handleTouch(touchInput.update(now,touch.isPressed(),touch.x,touch.y,slider,quickPanelOpen,hold));
        if(casaHoldProgress && (!touch.isPressed() || !hold)) { casaHoldProgress=0;dirty=true; }
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
        stateUpdated.fill(0);
        stateResetAt=now; lastHaRead=now-6000; dirty=true;
        wasHomeRoute=homeRoute;
    }
    if (connected && !wasConnected) configTzTime(regional::rule(settings.timeZone.c_str()), "pool.ntp.org", "time.cloudflare.com");
    if(wasConnected!=connected) dirty=true;
    wasConnected = connected;
    HomeResult result;
    if (home.poll(result) || mqtt.pollFavorite(result)) {
        if (homeOnline && result.favorite < settings.favoriteCount && result.viaMqtt==settings.favoritesMqtt &&
            result.entity==settings.favorites[result.favorite].entity && int32_t(result.requestedAt-stateResetAt)>=0) {
            const bool verifying=homeCommand.pending(result.favorite);
            if(result.success && policy::usableState(result.entity.c_str(),result.state.c_str())) {
                states[result.favorite] = result.state;
                units[result.favorite] = result.unit;
            }
            lightCapable[result.favorite]=result.success && result.dimmable;
            lightLevels[result.favorite]=result.brightness;
            stateUpdated[result.favorite] = result.success && policy::usableState(result.entity.c_str(),result.state.c_str())?now:0;
            const bool confirmed=result.success && homeCommand.observe(result.favorite,result.state.c_str(),result.brightness);
            lastHaRead=(result.favorite==favorite || homeCommand.pending())?now:now-6000;
            if(confirmed) {
                toast(i18n::text("Stato confermato")); vibrate(100); sound(sfx::Cue::Success,true);
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
        toast(i18n::text("Stato non confermato: verifica in Home Assistant"));
    }
    const bool homeVisible=!quickPanelOpen && !screenOff && (screen==Screen::HomeAssistant || screen==Screen::ActionConfirm || screen==Screen::Light || screen==Screen::LightConfirm);
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
        updateCharging(now);
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
        if((screen==Screen::Home || screen==Screen::System || screen==Screen::SettingsMenu || screen==Screen::SettingsSection || screen==Screen::Settings) &&
            (homeMinute!=minute || relayReady!=companion.relayReady())) dirty=true;
        homeMinute=minute;relayReady=companion.relayReady();
        if(!quickPanelOpen && screen==Screen::Notes) {
            const size_t count=notes.count();
            if(count!=noteCount) dirty=true;
            noteCount=count;
        }
        if(!quickPanelOpen && (screen==Screen::HomeAssistant || screen==Screen::Diagnostics || screen==Screen::Pairing || screen==Screen::Companion)) dirty=true;
    }
    if(noticeDisplayed && uint32_t(now-noticeAt)>=noticeDuration) dirty=true;
    if(badgeApp.update(now,settings.badgeAutoRotate,settings.badgeAnimate,!quickPanelOpen && screen==Screen::Badge && !screenOff && !notes.busy())) dirty=true;
    if(!quickPanelOpen && screen==Screen::Clock) {
        if(time(nullptr)!=clockDrawnSecond) dirty=true;
        if(clockControlsUntil && int32_t(now-clockControlsUntil)>=0) { clockControlsUntil=0;dirty=true; }
    }
    if(!quickPanelOpen && (screen==Screen::Focus || screen==Screen::Slides) && uint32_t(now-lastDraw)>=1000) dirty=true;
    if(!quickPanelOpen && ((screen==Screen::Timer && clockTimer.state()==clockwork::Timer::State::Running && uint32_t(now-lastDraw)>=100) ||
       (screen==Screen::Stopwatch && chronometer.running() && uint32_t(now-lastDraw)>=50))) dirty=true;
    if(screen==Screen::Badge) {
        if(badgeControlsUntil && int32_t(now-badgeControlsUntil)>=0) { badgeControlsUntil=0; dirty=true; }
    }
    const bool keepDisplayOn=(screen==Screen::Slides && usbControl.connected()) || (screen==Screen::SecurityKey && security_key::connected()) || security_key::awaiting() || (screen==Screen::Clock && settings.clockKeepAwake) ||
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
        if(screen==Screen::Badge || screen==Screen::Clock || screen==Screen::Focus || screen==Screen::Slides || screen==Screen::SecurityKey || musicScreen || badgeApp.uploading()) { if(M5.Mic.isRunning()) M5.Mic.end(); }
        else notes.warmMicrophone();
    }
    if(quietSavePending && canSave()) {
        quietSaveFailed=!settings.save();quietSavePending=false;dirty=true;
    }
    const bool animating=screen==Screen::Home && (uint32_t(now-transitionAt)<180 || homeFramePending);
    if(!screenOff && (dirty || (!quickPanelOpen && (notes.busy() || animating))) && uint32_t(now-lastDraw)>=(notes.busy()?50u:screen==Screen::Badge?40u:33u)) {
        // A periodic clock frame must not extend the two-second interaction boost.
        updateCpu(now,cpuWorkPending(),true);
        draw();
    }
    portal.update();
    if(usbControl.update(((screen==Screen::UsbControl && !usbPaused) || screen==Screen::Slides) && !quickPanelOpen && !notes.busy(),companion.connected(),millis(),screen==Screen::Slides)) dirty=true;
    companion.update();
    mqtt.update(screenOff);
    updateCpu(millis(),cpuWorkPending());
    delay(5);
}
