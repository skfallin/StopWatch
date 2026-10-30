#include "focus_model.h"
#include "ui_layout.h"
#include "usb_input.h"
#include <cassert>
#include <iostream>
int main() {
    clockwork::Focus focus;
    uint32_t now=1000;
    for(unsigned session=1;session<=4;++session) {
        focus.toggle(now);assert(focus.timer().state()==clockwork::Timer::State::Running);
        assert(!focus.reset());
        now+=1500000;assert(focus.update(now));assert(focus.sessions()==session);
        assert(!focus.update(now+1));assert(focus.sessions()==session);
        assert(focus.alert(now,false)==1);
        focus.toggle(++now);
        assert(focus.phase()==(session==4?clockwork::Focus::Phase::LongBreak:clockwork::Focus::Phase::ShortBreak));
        now+=session==4?900000:300000;assert(focus.update(now));assert(focus.sessions()==session);
        // Leave the break completed; the next tap starts work.
    }
    focus.toggle(++now);focus.toggle(++now);assert(focus.timer().state()==clockwork::Timer::State::Paused);
    assert(focus.reset() && focus.sessions()==0 && focus.timer().remainingSeconds()==1500);
    ui::HomeMenu home;
    std::array<int,launcher::appCount> order{7,4,0,8,1,2,3,5,6,9};
    home.configure(order,1);assert(home.space==2 && home.app()==7 && home.visibleCount()==1);
    home.nextSpace(1);assert(home.space==2);home.nextApp(1);assert(home.app()==7);
    home.configure(order,order.size());home.space=0;home.selected=0;assert(home.app()==4);
    home.nextApp(1);assert(home.app()==0);
    home.space=1;home.selected=0;assert(home.app()==8 && home.count()==4 && home.visibleCount()==3);
    bool seen[launcher::appCount]={};for(int i=0;i<home.count();++i) { seen[home.app()]=true;home.nextApp(1); }
    assert(seen[1] && seen[2] && seen[3] && seen[8]);
    ui::TouchInput touch;
    touch.update(0,true,233,200,false);touch.update(100,true,233,300,false);
    assert(touch.update(120,false,233,300,false).kind==ui::GestureKind::Down);
    touch.update(200,true,233,20,false);touch.update(300,true,233,200,false);
    assert(touch.update(320,false,233,200,false).kind==ui::GestureKind::QuickOpen);
    usb_input::Queue queue;assert(!queue.press({0x4f,0},0));
    queue.state(true,true,true,0);usb_input::Report report;while(queue.busy()) { assert(queue.next(report,0));queue.sent(0); }
    assert(queue.press({0x4f,0},10));assert(queue.next(report,10) && report.id==1 && report.bytes[2]==0x4f);
    queue.sent(10);assert(queue.next(report,10) && report.bytes[2]==0);queue.sent(10);
    queue.state(false,true,false,20);assert(!queue.press({0x05,0},20));
    std::cout<<"OK: Pomodoro 25/5/15, pausa/reset, Home personalizzata, gesti e rilascio tasti USB\n";
}
