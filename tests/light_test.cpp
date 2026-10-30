#include "light_control.h"
#include "home_state.h"
#include "ui_layout.h"
#include "controls.h"
#include <cassert>
#include <iostream>
int main() {
    casa::LightLevel draft;draft.begin(62);draft.step(-1);assert(draft.value==57);draft.step(1);assert(draft.value==62);
    for(int i=0;i<30;++i) draft.step(-1);assert(draft.value==0);
    for(int i=0;i<30;++i) draft.step(1);assert(draft.value==100);
    draft.point(109,343);assert(draft.value==0);
    draft.point(233,343);assert(draft.value==50);
    draft.point(357,343);assert(draft.value==100);
    draft.point(-999,343);assert(draft.value==0);
    draft.point(999,343);assert(draft.value==100);
    draft.begin(-1);assert(draft.value==100);draft.begin(0);assert(draft.value==0);
    assert(casa::brightnessPercent(255)==100 && casa::brightnessPercent(0)==0 && casa::brightnessPercent(153)==60);
    assert(casa::brightnessPercent(-1)==-1 && casa::brightnessPercent(256)==-1);
    assert(casa::brightnessMode("rgb") && !casa::brightnessMode("onoff") && !casa::brightnessMode("unknown"));
    casa::ToggleVerification pending;pending.begin(0,false,1000,60);
    assert(!pending.observe(0,"on",20) && pending.pending());assert(!pending.observe(1,"on",60));
    assert(!pending.observe(0,"off",60));assert(pending.observe(0,"on",59));
    pending.begin(0,true,1000,0);assert(pending.observe(0,"off"));
    using namespace ui;
    assert(hit(Screen::Light,336,343,0).action==Action::Slider);
    assert(hit(Screen::Light,233,253,0).action==Action::None);
    assert(hit(Screen::Light,233,375,0).action==Action::Slider);
    assert(hit(Screen::Light,233,380,0).action==Action::None);
    assert(hit(Screen::Light,233,398,0).action==Action::Select);
    assert(hit(Screen::HomeAssistant,233,388,0,false,false,false,{},true).action==Action::Open);
    assert(hit(Screen::HomeAssistant,233,388,0).action==Action::None);
    assert(hit(Screen::HomeAssistant,130,388,0,false,false,false,{},true).action==Action::None);
    assert(casaCard.contains(233,388) && casaCard.contains(233,137));
    assert(!casaCard.contains(46,264) && !casaCard.contains(420,264) && !casaCard.contains(112,84));
    TouchInput touch;
    assert(touch.update(1000,true,233,250,false,false,true).kind==GestureKind::HoldProgress);
    assert(touch.update(1325,true,233,250,false,false,true).progress==50);
    assert(touch.update(1650,true,233,250,false,false,true).kind==GestureKind::Hold);
    assert(touch.update(1700,true,233,250,false,false,true).kind==GestureKind::None);
    assert(touch.update(1750,false,233,250,false).kind==GestureKind::None);
    touch.update(2000,true,233,250,false,false,true);
    assert(touch.update(2200,false,233,250,false).kind==GestureKind::Tap);
    touch.update(3000,true,233,250,false,false,true);
    const auto cancelled=touch.update(3200,true,300,250,false,false,true);
    assert(cancelled.kind==GestureKind::HoldProgress && cancelled.progress==0);
    assert(touch.update(3700,true,233,250,false,false,true).kind==GestureKind::None);
    touch.update(3750,false,233,250,false);
    touch.update(4000,true,233,250,false,false,true);touch.suppressUntilReleased();
    assert(touch.update(4650,true,233,250,false,false,true).kind==GestureKind::None);
    assert(touch.update(4700,false,233,250,false).kind==GestureKind::None);
    touch.update(5000,true,233,250,true,false,true);
    assert(touch.update(5650,true,250,250,true,false,true).kind==GestureKind::Drag);
    assert(touch.update(5700,false,250,250,true).kind==GestureKind::Release);
    Controls controls;controls.update(0,true,false);assert(controls.update(650,true,false).back);
    assert(!controls.update(660,false,false).next);
    controls.update(1000,false,true);assert(controls.update(1650,false,true).recordStart);
    assert(controls.update(1660,false,false).recordStop);
    std::cout<<"OK: slider lineare, +/-5%, limiti, capacità, conferma del livello reale e hold invariati\n";
}
