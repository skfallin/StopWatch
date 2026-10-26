#include "clock_model.h"
#include "ui_layout.h"
#include "controls.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using clockwork::Timer;
    Timer timer;
    assert(timer.duration()==300000 && timer.remainingSeconds()==300);
    assert(!timer.setSeconds(0) && !timer.setSeconds(86400));
    assert(timer.adjust(0,-1) && timer.duration()==300000);
    assert(timer.adjust(1,1) && timer.duration()==360000);
    assert(timer.adjust(0,1) && timer.duration()==3960000);
    assert(!timer.adjust(3,1) && !timer.adjust(0,2));
    assert(timer.setSeconds(86399) && timer.duration()==86399000);
    for(unsigned field=0;field<3;++field) assert(timer.adjust(field,1) && timer.duration()==86399000);
    timer.setSeconds(1);assert(timer.adjust(2,-1) && timer.remainingSeconds()==1);
    timer.setSeconds(10);timer.toggle(1000);
    assert(timer.state()==Timer::State::Running);
    assert(!timer.setSeconds(5) && !timer.adjust(0,-1));
    assert(!timer.update(1001) && timer.remainingSeconds()==10);
    timer.toggle(4500);
    assert(timer.state()==Timer::State::Paused && timer.remaining()==6500);
    timer.update(100000);assert(timer.remaining()==6500);
    timer.toggle(100000);
    assert(!timer.update(106499) && timer.remainingSeconds()==1);
    assert(timer.update(106500) && timer.state()==Timer::State::Done && timer.remaining()==0);
    assert(!timer.update(200000));
    assert(timer.alert(200000,true)==0);
    assert(timer.alert(300000,false)==1 && timer.alert(301199,false)==0);
    assert(timer.alert(301200,false)==2);
    assert(timer.alert(400000,true)==0 && timer.alert(400000,false)==3);
    assert(timer.alert(401200,false)==4 && timer.alert(999999,false)==0);
    timer.toggle(999999);assert(timer.state()==Timer::State::Idle && timer.remaining()==10000);
    timer.restart(1000000);assert(timer.state()==Timer::State::Running);
    timer.update(1020000);assert(timer.alert(1020000,false)==1);
    timer.reset();assert(timer.alert(2000000,false)==0);
    constexpr uint32_t end=std::numeric_limits<uint32_t>::max();
    timer.setSeconds(2);timer.toggle(end-500);
    assert(!timer.update(498) && timer.remaining()==1001);
    assert(timer.update(1499));
    timer.reset();timer.setSeconds(1);timer.toggle(end-2020);timer.update(end-1020);
    assert(timer.alert(end-20,false)==1);
    assert(timer.alert(1178,false)==0 && timer.alert(1179,false)==2);
    timer.reset();timer.setSeconds(1);timer.toggle(2000);timer.toggle(3000);
    assert(timer.state()==Timer::State::Done); // A pause at the deadline cannot restart the timer.

    clockwork::Stopwatch watch;
    watch.update(50000);assert(watch.elapsed()==0 && !watch.mark(50000));
    watch.toggle(50000);watch.update(51234);assert(watch.elapsed()==1234 && !watch.reset());
    assert(watch.mark(52500) && watch.lap(0).number==1 && watch.lap(0).split==2500);
    assert(!watch.mark(52500));
    assert(watch.mark(53100) && watch.lap(0).split==600 && watch.elapsed()==3100);
    watch.mark(54000);watch.mark(55000);
    assert(watch.lapCount()==3 && watch.lap(0).number==4 && watch.lap(2).number==2);
    watch.toggle(56000);assert(!watch.running() && watch.elapsed()==6000);
    watch.update(90000);assert(watch.elapsed()==6000 && !watch.mark(90000));
    watch.toggle(90000);watch.toggle(90123);assert(watch.elapsed()==6123);
    assert(watch.reset() && watch.elapsed()==0 && watch.lapCount()==0);
    watch.toggle(end-500);watch.update(499);assert(watch.elapsed()==1000);
    watch.toggle(500);assert(watch.elapsed()==1001);
    watch.reset();watch.toggle(0);watch.update(clockwork::Stopwatch::maxElapsed);
    assert(!watch.running() && watch.elapsed()==clockwork::Stopwatch::maxElapsed);
    watch.toggle(clockwork::Stopwatch::maxElapsed+1);assert(!watch.running());
    assert(watch.reset());

    using namespace ui;
    for(int page=0;page<3;++page) {
        assert((hit(Screen::ClockMenu,233,168+page*86,0)==Target{Action::Open,page}));
        for(auto screen:{Screen::Clock,Screen::Timer,Screen::Stopwatch}) {
            assert((hit(screen,149+page*84,clockTabsY,0,false,false,true)==Target{Action::Open,page}));
            assert(hit(screen,233,382,0,true).action==Action::Stop);
        }
    }
    assert((hit(Screen::Clock,112,84,0)==Target{Action::Open,3}));
    assert(hit(Screen::Clock,112,84,0,false,false,true).action==Action::Back);
    assert(hit(Screen::Clock,354,84,0,false,false,true).action==Action::Select);
    for(int field=0;field<3;++field) {
        assert((hit(Screen::Timer,137+field*96,143,0)==Target{Action::Increase,field}));
        assert((hit(Screen::Timer,137+field*96,283,0)==Target{Action::Decrease,field}));
        assert(hit(Screen::Timer,137+field*96,143,1).action==Action::None);
    }
    assert(hit(Screen::Timer,233,clockActionY,0).action==Action::Select);
    assert(hit(Screen::Timer,160,clockActionY,1).action==Action::Secondary);
    assert(hit(Screen::Timer,306,clockActionY,1).action==Action::Select);
    assert(hit(Screen::Stopwatch,160,clockActionY,0).action==Action::Secondary);
    assert(hit(Screen::Stopwatch,306,clockActionY,0).action==Action::Select);
    for(int selected=0;selected<homeCount;++selected) {
        assert((hit(Screen::Home,120,351,selected)==Target{Action::Open,(selected+1)%homeCount}));
        assert((hit(Screen::Home,346,351,selected)==Target{Action::Open,(selected+homeCount-1)%homeCount}));
    }
    for(size_t i=0;i<clockwork::faceIds.size();++i) assert(clockwork::faceIndex(clockwork::faceIds[i])==int(i));
    assert(clockwork::faceIndex("unknown")==-1 && clockwork::faceIndex(std::string_view("halo\0bad",8))==-1);
    Controls buttons;
    buttons.update(0,false,true);
    buttons.suppressTapUntilReleased();
    assert(!buttons.update(50,false,false).select);
    buttons.update(100,false,true);
    assert(buttons.update(150,false,false).select);
    std::cout<<"OK: timer, pausa/ripresa, avviso differito e limitato, cronometro, giri, rollover, quadranti e touch\n";
}
