#include "controls.h"
#include "ble_protocol.h"
#include "policy.h"
#include "home_state.h"
#include "firmware.h"
#include "ui_layout.h"
#include "ui_sounds.h"
#include "ui_theme.h"
#include <cassert>
#include <iostream>
#include <limits>

void tapsAndHolds() {
    Controls input;
    assert(!input.update(0, true, false).next);
    assert(input.update(50, false, false).next);
    assert(!input.update(51, false, false).next);
    input.update(100, false, true);
    assert(input.update(200, false, false).select);
    input.update(300, true, false);
    assert(input.update(950, true, false).back);
    assert(!input.update(1000, true, false).back);
    assert(!input.update(1001, false, false).next);
    input.update(2000, false, true);
    assert(!input.update(2649, false, true).recordStart);
    assert(input.update(2650, false, true).recordStart);
    assert(!input.update(2651, false, true).recordStart);
    auto release = input.update(3000, false, false);
    assert(release.recordStop && !release.select);
    assert(!input.update(3001, false, false).recordStop);
}

void chordsAndWake() {
    Controls input;
    input.update(0, true, true);
    auto held = input.update(800, true, true);
    assert(held.home && !held.recordStart && !held.back);
    assert(!input.update(900, true, true).home);
    auto oneReleased = input.update(950, false, true);
    assert(!oneReleased.next && !oneReleased.recordStart);
    auto bothReleased = input.update(1000, false, false);
    assert(!bothReleased.select && !bothReleased.next);
    input.update(2000, false, true);
    assert(input.update(2700, false, true).recordStart);
    auto chord = input.update(2800, true, true);
    assert(chord.recordStop && !chord.recordStart);
    assert(input.update(3600, true, true).home);
    input.update(3700, false, false);
    input.suppressTapUntilReleased();
    assert(!input.update(4000, false, true).recordStart);
    assert(input.update(6000, false, true).recordStart);
    auto releaseAfterWake = input.update(6100, false, false);
    assert(!releaseAfterWake.select && releaseAfterWake.recordStop);
    input.suppressTapUntilReleased();
    input.update(6200, true, false);
    assert(!input.update(6250, false, false).next);
    input.update(7000, false, true);
    assert(input.update(7100, false, false).select);
    Controls rollover;
    constexpr uint32_t end = std::numeric_limits<uint32_t>::max();
    rollover.update(end - 100, false, true);
    assert(rollover.update(600, false, true).recordStart);
}

void boundaryValidation() {
    assert(policy::utf8Prefix("Caffè", 5) == "Caff");
    assert(policy::utf8Prefix("Caffè", 6) == "Caffè");
    assert(policy::utf8Prefix("☀️", 2).empty());
    assert(policy::utf8Prefix("🌙x", 4) == "🌙");
    assert(policy::utf8Prefix("x", 0).empty());
    assert(policy::brokerHost("homeassistant.local"));
    assert(policy::brokerHost("192.168.1.2"));
    for (const auto value : {"mqtt://ha.local", "ha.local:1883", "ha/#", "user@ha", "ha\n", ".ha", "ha."})
        assert(!policy::brokerHost(value));
    assert(policy::entityId("light.cucina"));
    assert(policy::entityId("sensor.soggiorno_2"));
    for (const auto value : {"", "light.", ".cucina", "light.cucina.altra", "light/../../", "light.a\n", "Light.a"}) {
        assert(!policy::entityId(value));
    }
    assert(policy::service("light.cucina") == "toggle");
    assert(policy::service("scene.cinema") == "turn_on");
    assert(policy::service("input_button.campanello") == "press");
    assert(policy::service("sensor.temperatura").empty());
    assert(policy::service("lock.porta").empty());
    for (const auto value : {"http://192.168.1.2:8123", "https://ha.example.org", "http://homeassistant.local:8123/ha"}) {
        assert(policy::baseUrl(value));
    }
    for (const auto value : {"ftp://ha.local", "http://", "https:///path", "http://user:pass@host", "http://ha/\r\nAuthorization:x", "http://ha/?token=x", "http://ha/#fragment", "http://ha\\path"}) {
        assert(!policy::baseUrl(value));
    }
    assert(policy::noteFilename("note-0123456789abcdef.wav"));
    for (const auto value : {"../config.json", "note-0123456789abcdef.wav.part", "note-0123456789abcdef.wav/..", "note-0123456789abcdeg.wav", "/notes/note-0123456789abcdef.wav"}) {
        assert(!policy::noteFilename(value));
    }
}

void bluetoothFrames() {
    BleLine input;
    const std::string json = "{\"name\":\"Caffè\"}";
    for (char c : json) assert(input.push(c) == BleLine::Result::More);
    assert(input.push('\n') == BleLine::Result::Complete);
    assert(input.value() == json);
    input.clear();
    assert(input.push('\n') == BleLine::Result::Invalid);
    assert(input.push('\0') == BleLine::Result::Invalid);
    for (size_t i = 0; i < policy::maxConfigBytes; ++i) assert(input.push('x') == BleLine::Result::More);
    assert(input.push('\n') == BleLine::Result::Complete);
    assert(input.push('x') == BleLine::Result::Invalid);
    input.clear();
    assert(input.push('{') == BleLine::Result::More);
}

void waveHeader() {
    const auto header = policy::wavHeader(32000);
    const auto read = [&header](size_t offset, size_t bytes) {
        uint32_t value = 0;
        for (size_t i = 0; i < bytes; ++i) value |= uint32_t(header[offset + i]) << (8 * i);
        return value;
    };
    assert(header[0] == 'R' && header[8] == 'W' && header[36] == 'd');
    assert(read(4, 4) == 32036);
    assert(read(20, 2) == 1 && read(22, 2) == 1);
    assert(read(24, 4) == 16000 && read(28, 4) == 32000);
    assert(read(32, 2) == 2 && read(34, 2) == 16);
    assert(read(40, 4) == 32000);
    assert(policy::wavHeader(0)[40] == 0);
    assert(policy::maxSamples % 256 == 0);
}

void touchNavigation() {
    using namespace ui;
    assert(policy::speakerVolume(0)==0 && policy::speakerVolume(100)==128);
    assert(policy::speakerVolume(200)==255 && policy::speakerVolume(255)==255);
    for (int v=1;v<=200;++v) assert(policy::speakerVolume(v)>policy::speakerVolume(v-1));
    for (int x : {noticeCard.x,noticeCard.x+noticeCard.width-1})
        for (int y : {noticeCard.y,noticeCard.y+noticeCard.height-1})
            assert((Circle{233,233,216}.contains(x,y)));
    assert(noticeCard.contains(355,357) && !noticeCard.contains(233,310));
    assert((hit(Screen::Home,233,233,0)==Target{Action::Open,0}));
    assert((hit(Screen::Home,120,351,0)==Target{Action::Open,1}));
    assert((hit(Screen::Home,346,351,2)==Target{Action::Open,1}));
    assert(hit(Screen::Home,0,0,0).action==Action::None);
    assert(hit(Screen::Home,-32768,-32768,0).action==Action::None);
    assert(hit(Screen::Notes,233,250,0).action==Action::Record);
    assert(hit(Screen::Notes,233,382,0,true).action==Action::Stop);
    assert(hit(Screen::Notes,233,250,0,true).action==Action::None);
    assert(hit(Screen::Settings,233,343,0).action==Action::Slider);
    for(auto screen:{Screen::FormatConfirm,Screen::ForgetConfirm}) {
        for(int y=0;y<466;y+=3) for(int x=0;x<466;x+=3) {
            const auto action=hit(screen,x,y,0).action;
            assert(action==Action::None || action==Action::Back);
        }
    }
    assert(hit(Screen::ActionConfirm,309,331,0).action==Action::Select);
    assert(hit(Screen::ActionConfirm,157,331,0).action==Action::Back);
    assert(policy::confirmAction("switch.cancello"));
    assert(policy::confirmAction("input_boolean.lascia_cancello_aperto"));
    assert(policy::confirmAction("light.camera_da_letto"));
    assert(policy::confirmAction("fan.studio"));
    assert(policy::confirmAction("scene.cinema"));
    assert(policy::confirmAction("script.buonanotte"));
    assert(policy::confirmAction("button.riavvio"));
    assert(policy::confirmAction("input_button.campanello"));
    assert(!policy::confirmAction("sensor.temperatura"));
    assert(!ui::confirmationReady(1000,1000));
    assert(!ui::confirmationReady(1349,1000));
    assert(ui::confirmationReady(1350,1000));
    assert(ui::confirmationReady(100,std::numeric_limits<uint32_t>::max()-300));
    assert(sliderValue(0,-100)==32 && sliderValue(0,999)==255);
    assert(sliderValue(1,109)==0 && sliderValue(1,357)==200);
    assert(sliderValue(6,109)==0 && sliderValue(6,357)==200);
    for(int x=-10;x<480;++x) assert(sliderValue(6,x)==sliderValue(1,x));
    assert(sliderValue(3,109)==15 && sliderValue(3,357)==180);
    for(int x=-10;x<480;++x) assert(sliderValue(3,x)%15==0);
    assert(sliderValue(4,109)==0 && sliderValue(4,357)==12);
    for (size_t i=0;i<sfx::packIds.size();++i) assert(sfx::packIndex(sfx::packIds[i])==int(i));
    assert(sfx::packIndex("unknown")==-1 && sfx::packIndex("")==-1);
    assert(sfx::packIndex(std::string_view("minimal\0other",13))==-1);
    assert(sliderValue(5,109)==0 && sliderValue(5,357)==int(ui::themes.size())-1);
    for (size_t i=0;i<ui::themes.size();++i) assert(ui::themeIndex(ui::themes[i].id)==int(i));
    assert(ui::themeIndex("unknown")==-1 && ui::themeIndex("")==-1);
    assert(ui::themeIndex(std::string_view("lime\0other",10))==-1);
    for (int dateWidth : {35,58,65,105}) for (int batteryWidth : {18,33,46,58}) {
        const auto row=ui::statusLayout(dateWidth,batteryWidth);
        const int left=row.dateX-dateWidth/2, right=row.batteryIconX+26;
        assert(left+right>=466 && left+right<=467);
        assert(row.dateX+dateWidth/2<row.wifiX-9);
        assert(row.wifiX+9<row.batteryX-batteryWidth/2);
        assert(row.batteryX+batteryWidth/2<row.batteryIconX);
    }
}

void touchGestures() {
    using namespace ui;
    TouchInput input;
    assert(input.update(0,true,233,250,false).kind==GestureKind::None);
    assert(input.update(60,false,233,250,false).kind==GestureKind::Tap);
    assert(input.update(70,false,233,250,false).kind==GestureKind::None);
    input.update(100,true,380,240,false);
    input.update(150,true,150,245,false);
    assert(input.update(200,false,150,245,false).kind==GestureKind::Left);
    input.update(300,true,233,250,false);
    assert(input.update(1300,false,233,250,false).kind==GestureKind::None);
    input.suppressUntilReleased();
    input.update(2000,true,233,250,false);
    assert(input.update(2100,false,233,250,false).kind==GestureKind::None);
    input.update(2200,true,233,250,false);
    assert(input.update(2300,false,233,250,false).kind==GestureKind::Tap);
    assert(input.update(3000,true,109,343,true).kind==GestureKind::Drag);
    assert(input.update(3050,true,357,343,true).kind==GestureKind::Drag);
    const auto end=input.update(3100,false,0,0,false);
    assert(end.kind==GestureKind::Release && end.x==357);
    input.update(4000,true,233,250,false);
    input.update(4050,true,-32768,-32768,false);
    assert(input.update(4100,false,0,0,false).kind==GestureKind::None);
    const uint32_t boundary=std::numeric_limits<uint32_t>::max();
    input.update(boundary-20,true,233,250,false);
    assert(input.update(50,false,233,250,false).kind==GestureKind::Tap);
}

void quickControls() {
    using namespace ui;
    for(const auto panel:{quickPanel,quietToggle})
        for(const int x:{panel.x,panel.x+panel.width-1})
            for(const int y:{panel.y,panel.y+panel.height-1})
                assert((Circle{233,233,232}.contains(x,y)));
    assert(quietToggle.contains(233,207) && quietToggle.contains(233,300));
    assert(!quietToggle.contains(quickClose.x,quickClose.y));
    const auto swipe=[](int x,int y,int dx,int dy,bool panel=false,bool slider=false) {
        TouchInput input;
        input.update(0,true,x,y,slider,panel);
        input.update(100,true,x+dx,y+dy,slider,panel);
        return input.update(150,false,x+dx,y+dy,slider,panel).kind;
    };
    assert(swipe(233,24,0,100)==GestureKind::QuickOpen);
    assert(swipe(167,40,20,52)==GestureKind::QuickOpen);
    assert(swipe(299,40,-20,52)==GestureKind::QuickOpen);
    assert(swipe(233,64,0,52)==GestureKind::QuickOpen);
    for(const auto point:{std::pair{166,40},std::pair{300,40},std::pair{233,65},std::pair{233,250}})
        assert(swipe(point.first,point.second,0,100)==GestureKind::None);
    assert(swipe(233,40,0,51)==GestureKind::None);
    assert(swipe(233,40,90,60)==GestureKind::Right);
    assert(swipe(233,250,0,-70)==GestureKind::None);
    assert(swipe(233,250,0,-70,true)==GestureKind::QuickClose);
    assert(swipe(233,250,-100,0,true)==GestureKind::Left);
    assert(swipe(233,343,50,-100,false,true)==GestureKind::Release);
    TouchInput wake;
    wake.suppressUntilReleased();
    wake.update(0,true,233,24,false);
    wake.update(50,true,233,180,false);
    assert(wake.update(100,false,233,180,false).kind==GestureKind::None);
    wake.update(150,true,233,24,false);
    wake.update(200,true,233,180,false);
    assert(wake.update(250,false,233,180,false).kind==GestureKind::QuickOpen);
}

void homeStateVerification() {
    for(const auto entity:{"light.camera","fan.studio","switch.presa","input_boolean.casa"}) {
        assert(policy::usableState(entity,"on") && policy::usableState(entity,"off"));
        for(const auto invalid:{"","unknown","unavailable","turning_on","false","0"})
            assert(!policy::usableState(entity,invalid));
    }
    assert(policy::usableState("sensor.temperatura","0"));
    assert(!policy::usableState("sensor.temperatura","unavailable"));
    casa::ToggleVerification command;
    command.begin(2,false,1000);
    assert(command.pending() && command.pending(2) && !command.pending(1));
    assert(!command.observe(1,"on"));
    assert(!command.observe(2,"off"));
    assert(!command.observe(2,"unknown"));
    assert(command.pending() && !command.expired(12999));
    assert(command.observe(2,"on") && !command.pending());
    command.begin(2,true,20000);
    assert(!command.observe(2,"on"));
    assert(command.observe(2,"off") && !command.expired(40000));
    command.begin(7,false,std::numeric_limits<uint32_t>::max()-1000);
    assert(!command.expired(10998) && command.expired(10999));
    assert(command.pending() && command.favorite()==7);
    command.clear(); assert(!command.pending() && !command.observe(7,"on"));
}

void startupLoading() {
    assert(!firmware::showLoading(1000,1000,false));
    assert(!firmware::showLoading(1399,1000,false));
    assert(firmware::showLoading(1400,1000,false));
    assert(!firmware::showLoading(1400,1000,true));
    assert(!firmware::showLoading(9000,1000,true));
    assert(firmware::showLoading(100,std::numeric_limits<uint32_t>::max()-300,false));
    assert(!firmware::showLoading(100,std::numeric_limits<uint32_t>::max()-300,true));
}

int main() {
    tapsAndHolds();
    chordsAndWake();
    boundaryValidation();
    bluetoothFrames();
    waveHeader();
    touchNavigation();
    touchGestures();
    quickControls();
    homeStateVerification();
    startupLoading();
    std::cout << "OK: pulsanti, touch, conferme, stato Casa, avvio condizionale, rollover, WAV\n";
}
