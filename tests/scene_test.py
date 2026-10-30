"""Exercise the real Scene navigation branches and charger polling without USB hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text()

def function(signature):
    start = source.index(signature + " {")
    return source[start:source.index("\n}", start) + 2]

select = source[source.index("    case Screen::Home: {", source.index("void selectAction(")):]
select = select[:select.index("    case Screen::Settings:\n")]
next_action = function("void nextAction(int direction=1,bool page=false)")
next_home = next_action[next_action.index("    case Screen::Home:"):next_action.index("    case Screen::ClockMenu:")]
next_settings = next_action[next_action.index("    case Screen::System:"):next_action.index("    case Screen::Diagnostics:")]
harness = r'''
#include "ui_layout.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
struct String:std::string {
    using std::string::string;
    String(const std::string& s):std::string(s) {}
    String(int value):std::string(std::to_string(value)) {}
    bool isEmpty() const { return empty(); }
};
using Screen=ui::Screen;
ui::HomeMenu homeMenu;
Screen screen=Screen::Home;
size_t systemItem=0,setting=0;
int settingPreview=-1;
bool homeFramePending=false;
int canvas=0;
namespace scene_ui { uint32_t age=0;void home(int&,const ui::HomeMenu&,uint32_t value) { age=value; } }
void drawStatus() {}
bool dirty=false,quickPanelOpen=false,screenOff=false,displayDimmed=false;
uint32_t now=1000,transitionAt=0,noticeAt=0,noticeDuration=4500,lastChargeRead=0,lastInteraction=0;
bool charging=false,chargeNotified=false,chargeNoticePending=false;
int chargeCandidate=-1,batteryLevel=82;
namespace ui { enum class Icon { Activity,Charging }; }
ui::Icon noticeIcon=ui::Icon::Activity;
String notice,noticeDetail;
struct { int brightness=160; } settings;
struct { bool occupied=false;bool busy() const { return occupied; } } notes;
namespace security_key { bool waiting=false;bool awaiting() { return waiting; } }
namespace m5 { struct Power_Class { enum is_charging_t { is_discharging,is_charging,charge_unknown }; }; }
struct {
    struct {
        int vbus=0,reads=0;
        m5::Power_Class::is_charging_t state=m5::Power_Class::is_discharging;
        int getVBUSVoltage() { ++reads;return vbus; }
        auto isCharging() { return state; }
    } Power;
    struct { int brightness=160;void setBrightness(int b) { brightness=b; } } Display;
} M5;
void sound(sfx::Cue) {}
void applySettings() {}
uint32_t millis() { return now; }
void changeScreen(Screen next) { screen=next;dirty=true; }
'''
harness += function("void drawHome()")
animation=next(line for line in source.splitlines() if "const bool animating=" in line)
harness += "bool animates() {\n" + animation + "\nreturn animating;\n}\n"
harness += function("void toast(const String& text)")
harness += function("void updateCharging(uint32_t now)")
harness += "void selectAction(bool fromButton=false) { switch(screen) {\n" + select + "default:break;\n} dirty=true; }\n"
harness += function("void backAction()")
harness += "void nextAction(int direction=1,bool page=false) { switch(screen) {\n" + next_home + next_settings + "default:break;\n} }\n"
harness += r'''
void poll() { now+=1000;updateCharging(now); }
int main() {
    transitionAt=1000;now=1000;drawHome();assert(homeFramePending && animates());
    now=1170;drawHome();assert(homeFramePending && scene_ui::age==170);
    now=1220;assert(animates()); // The previous draw may have finished after the deadline.
    drawHome();assert(!homeFramePending && !animates() && scene_ui::age==220);
    screen=Screen::Notes;homeFramePending=true;assert(!animates());screen=Screen::Home;
    transitionAt=std::numeric_limits<uint32_t>::max()-100;now=transitionAt+170;drawHome();assert(homeFramePending);
    now=transitionAt+220;assert(animates());drawHome();assert(!homeFramePending && !animates());
    transitionAt=now;
    constexpr Screen pages[]={Screen::HomeAssistant,Screen::Notes,Screen::Badge,Screen::ClockMenu,Screen::Spotify,Screen::UsbControl,Screen::SecurityKey,Screen::System,Screen::Focus,Screen::Slides};
    bool reached[ui::homeCount]={};
    for(int space=0;space<3;++space) {
        screen=Screen::Home;homeMenu.space=space;homeMenu.selected=0;homeMenu.apps=false;
        selectAction(true);assert(screen==Screen::Home && homeMenu.apps);
        for(int i=0;i<homeMenu.count();++i) {
            const int app=homeMenu.app();selectAction(true);assert(screen==pages[app]);reached[app]=true;
            backAction();assert(screen==Screen::Home && homeMenu.apps && homeMenu.app()==app);
            nextAction();
        }
        assert(homeMenu.selected==0);backAction();assert(!homeMenu.apps);
        nextAction();assert(homeMenu.space==(space+1)%3 && homeMenu.selected==0);
    }
    for(bool app:reached) assert(app);
    screen=Screen::Home;homeMenu.space=1;homeMenu.selected=2;homeMenu.apps=true;
    nextAction(-1,true);assert(homeMenu.space==0 && !homeMenu.apps && homeMenu.selected==0);
    screen=Screen::System;systemItem=1;nextAction(1,true);assert(systemItem==2);
    selectAction();assert(screen==Screen::Pairing);backAction();assert(screen==Screen::System && systemItem==2);
    nextAction();assert(systemItem==3);nextAction();assert(systemItem==0);
    selectAction();assert(screen==Screen::SettingsMenu);
    bool settingsReached[7]={};
    for(int section=0;section<3;++section) {
        screen=Screen::SettingsMenu;setting=ui::settingsItems[section][0];selectAction();
        assert(screen==(section==2?Screen::Settings:Screen::SettingsSection));
        if(section==2) { settingsReached[setting]=true;backAction();assert(screen==Screen::SettingsMenu);continue; }
        for(int i=0;i<ui::settingsCount(section);++i) {
            const auto item=setting;selectAction();assert(screen==Screen::Settings);settingsReached[item]=true;
            backAction();assert(screen==Screen::SettingsSection && setting==item);nextAction();
        }
        backAction();assert(screen==Screen::SettingsMenu);
    }
    for(bool value:settingsReached) assert(value);
    screen=Screen::SettingsSection;setting=1;nextAction(1,true);assert(setting==2);
    nextAction(-1,true);assert(setting==1);
    screen=Screen::Settings;setting=6;nextAction();assert(setting==0);
    nextAction(-1);assert(setting==6);
    // Two stable hardware samples; no repeated wake/notice on charger top-off cycles.
    screen=Screen::Home;M5.Power.vbus=5000;
    poll();poll();assert(!charging && notice.isEmpty()); // USB power alone is not charging.
    M5.Power.state=m5::Power_Class::is_charging;
    screenOff=displayDimmed=true;M5.Display.brightness=0;
    poll();assert(!charging && notice.isEmpty());poll();assert(charging && notice=="Charging");
    assert(!screenOff && !displayDimmed && M5.Display.brightness==160 && noticeDuration==3200);
    assert(noticeDetail=="82% · USB connected");
    const auto firstNotice=noticeAt;const auto reads=M5.Power.reads;
    now+=100;updateCharging(now);assert(M5.Power.reads==reads);
    poll();poll();assert(noticeAt==firstNotice);
    notice="";M5.Power.state=m5::Power_Class::is_discharging;poll();poll();assert(!charging);
    M5.Power.state=m5::Power_Class::is_charging;poll();poll();assert(charging && notice.isEmpty());
    M5.Power.vbus=0;poll();poll();assert(!charging && !chargeNotified);
    notes.occupied=true;M5.Power.vbus=5000;poll();poll();assert(charging && notice.isEmpty() && chargeNoticePending);
    notes.occupied=false;security_key::waiting=true;poll();assert(notice.isEmpty());
    security_key::waiting=false;quickPanelOpen=true;poll();assert(notice.isEmpty());
    quickPanelOpen=false;poll();assert(notice=="Charging" && !chargeNoticePending);
    toast("Configurazione salvata");assert(noticeDuration==4500 && noticeDetail.isEmpty());
    M5.Power.state=m5::Power_Class::charge_unknown;poll();assert(charging);
    M5.Power.vbus=0;M5.Power.state=m5::Power_Class::is_discharging;
    poll();poll();notice="";
    now=std::numeric_limits<uint32_t>::max()-1200;
    M5.Power.vbus=5000;M5.Power.state=m5::Power_Class::is_charging;
    poll();poll();assert(charging && notice=="Charging");
    M5.Power.vbus=0;poll();poll();assert(!charging && notice.isEmpty());
    toast("Attendi la risposta");M5.Power.vbus=5000;poll();poll();
    assert(chargeNoticePending && notice=="Attendi la risposta");
    now+=4500;updateCharging(now);assert(notice=="Charging" && !chargeNoticePending);
    std::cout<<"OK: tutte le app e impostazioni, A/B, pagine, ritorno e ricarica senza avvisi ripetuti\n";
}
'''
with tempfile.TemporaryDirectory(prefix="stopwatch-scene-test-") as temporary:
    output = Path(temporary)
    (output / "test.cpp").write_text(harness)
    binary = output / "test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / "include"),
                    str(output / "test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
