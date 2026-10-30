"""Run the real Casa action, confirmation and polling gates with deterministic backends."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text()
start = source.index("bool executeFavorite() {")
execute = source[start:source.index("\n}", start) + 2]
start = source.index("    case Screen::ActionConfirm:", source.index("void selectAction("))
confirm = source[start:source.index("    case Screen::Notes:", start)]
start = source.index("if ((homeVisible || homeCommand.pending())") + 4
condition = source[start:source.index(") {", start)]
harness = r'''
#include "policy.h"
#include "home_state.h"
#include "light_control.h"
#include "ui_layout.h"
#include <cassert>
#include <iostream>
#include <string>
using String=std::string;
using Screen=ui::Screen;
struct Settings {
    struct Favorite { String entity="light.camera"; };
    bool favoritesMqtt=true;size_t favoriteCount=1;
    std::array<Favorite,8> favorites{};
} settings;
struct Backend {
    bool occupied=false,accepts=true;int reads=0,actions=0;
    bool busy() const { return occupied; }
    int brightness=-1;
    bool requestFavorite(size_t,bool action,int level=-1) {
        brightness=level;
        if(!accepts) return false;
        occupied=true;if(action) ++actions;else ++reads;return true;
    }
    bool request(const Settings&,size_t index,bool action,int level=-1) { return requestFavorite(index,action,level); }
} mqtt,home;
struct { bool occupied=false;bool busy() const { return occupied; } } notes;
bool homeReady=true,dirty=false,fresh=true,homeVisible=true,homeOnline=true,adjustOption=false,dimmable=true;
casa::LightLevel lightDraft;size_t lightFavorite=0;String lightEntity="light.camera";
size_t favorite=0;
int homeRoute=2;
uint32_t now=1000,lastHaRead=0,transitionAt=0;
std::array<uint32_t,8> stateUpdated{};
std::array<String,8> states{};
casa::ToggleVerification homeCommand;
Screen screen=Screen::ActionConfirm;
String notice;
uint32_t millis() { return now; }
void toast(const String& value) { notice=value; }
bool favoriteFresh() { return fresh; }
bool favoriteCanDim() { return fresh && dimmable; }
void sound(sfx::Cue) {}
void openLight() { screen=Screen::Light; }
void changeScreen(Screen value) { screen=value;lastHaRead=now-6000; }
'''
apply_start=source.index("bool applyLight() {")
apply=source[apply_start:source.index("\n}",apply_start)+2]
harness += execute + "\n" + apply + "\nvoid confirm(bool fromButton=false) { switch(screen) {\n" + confirm + "default:break;\n} }\n"
harness += "bool pollDue() { return " + condition + "; }\n"
harness += r'''
int main() {
    mqtt.occupied=true;
    confirm();assert(screen==Screen::ActionConfirm && !mqtt.actions);
    mqtt.occupied=false;fresh=false;
    confirm();assert(screen==Screen::ActionConfirm && mqtt.reads==1 && !mqtt.actions);
    mqtt.occupied=false;fresh=true;states[0]="off";
    confirm();assert(screen==Screen::HomeAssistant && mqtt.actions==1 && lastHaRead==now);
    assert(homeCommand.pending() && stateUpdated[0]==0);
    screen=Screen::ActionConfirm;
    confirm();assert(screen==Screen::ActionConfirm && mqtt.actions==1);
    assert(homeCommand.observe(0,"on"));
    mqtt.occupied=false;mqtt.accepts=false;
    confirm();assert(screen==Screen::ActionConfirm && mqtt.actions==1);
    mqtt.accepts=true;transitionAt=now;
    confirm();assert(screen==Screen::ActionConfirm && mqtt.actions==1);
    settings.favoriteCount=0;assert(!executeFavorite());settings.favoriteCount=1;
    settings.favoritesMqtt=false;settings.favorites[0].entity="sensor.temperatura";
    assert(executeFavorite() && home.reads==1 && !home.actions && !homeCommand.pending());
    settings.favoritesMqtt=true;mqtt.occupied=false;
    lastHaRead=5000;now=9999;
    assert(!pollDue());now=10000;assert(pollDue());
    homeRoute=1;now=7000;assert(pollDue());
    homeVisible=false;assert(!pollDue());
    homeCommand.begin(0,false,7000);lastHaRead=7000;now=7499;
    assert(!pollDue());now=7500;assert(pollDue());
    notes.occupied=true;assert(!pollDue());notes.occupied=false;
    homeOnline=false;assert(!pollDue());
    notes.occupied=false;homeOnline=true;settings.favorites[0].entity="light.camera";settings.favoritesMqtt=true;
    mqtt.occupied=false;homeCommand.clear();fresh=true;lightDraft.begin(60);
    screen=Screen::Light;confirm(true);assert(lightDraft.value==65 && !mqtt.occupied);
    confirm(false);assert(mqtt.brightness==65 && homeCommand.pending() && screen==Screen::HomeAssistant);
    assert(!homeCommand.observe(0,"on",20));assert(homeCommand.observe(0,"on",65));
    mqtt.occupied=false;lightEntity="light.other";assert(!applyLight() && !mqtt.occupied);
    lightEntity="light.camera";dimmable=false;assert(!applyLight() && !mqtt.occupied);dimmable=true;
    fresh=false;assert(!applyLight() && !mqtt.occupied);fresh=true;
    lightDraft.value=0;assert(applyLight() && mqtt.brightness==0 && homeCommand.observe(0,"off"));
    std::cout<<"OK: conferma conservata durante letture, nessuna azione su dati vecchi, pausa tra aggiornamenti\n";
}
'''
with tempfile.TemporaryDirectory(prefix="stopwatch-casa-interaction-") as temporary:
    output = Path(temporary)
    (output / "test.cpp").write_text(harness)
    binary = output / "test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / "include"),
                    str(output / "test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
