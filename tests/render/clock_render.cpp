#include "i18n.h"
#include "clock_view.h"
#include "ui_drawing.h"
#include <filesystem>
#include <fstream>
#include <functional>

namespace ui {
void label(M5Canvas& c,const String& value,int x,int y,Font font,uint32_t color,uint32_t bg,int width) {
    c.text(value,x,y,int(font),color,bg,width);
}
#include "drawing_primitives.inc"
}

int main(int argc,char** argv) {
    if(argc<2 || argc>3) return 1;
    i18n::italian = argc==3 && std::string(argv[2])=="it";
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    const auto frame=[&](const String& name,const std::function<void(M5Canvas&)>& draw) {
        std::ofstream output(root/(name+".jsonl"));M5Canvas canvas(output);draw(canvas);
    };
    tm local{};local.tm_hour=10;local.tm_min=8;local.tm_sec=36;local.tm_wday=6;local.tm_mday=24;local.tm_mon=9;
    for(int theme:{0,3}) {
        ui::accent=ui::themes[theme].accent;ui::navigation=ui::themes[theme].navigation;
        ui::focus=ui::themes[theme].focus;ui::selectionSurface=ui::themes[theme].selectionSurface;
        for(int i=0;i<6;++i) frame((theme?"violet-":"")+String("face-")+std::to_string(i),[&](M5Canvas& c) {
            clock_ui::watch(c,clockwork::Face(i),local,true,false);
        });
    }
    ui::accent=ui::themes[0].accent;ui::navigation=ui::themes[0].navigation;
    ui::focus=ui::themes[0].focus;ui::selectionSurface=ui::themes[0].selectionSurface;
    frame("menu",[](M5Canvas& c) { clock_ui::menu(c,0); });
    frame("controls",[&](M5Canvas& c) { clock_ui::watch(c,clockwork::Face::Halo,local,true,true); });
    frame("no-time",[&](M5Canvas& c) { clock_ui::watch(c,clockwork::Face::Digital,local,false,false); });
    clockwork::Timer timer;
    frame("timer-edit",[&](M5Canvas& c) { clock_ui::timer(c,timer); });
    timer.toggle(0);timer.update(83000);
    frame("timer-running",[&](M5Canvas& c) { clock_ui::timer(c,timer); });
    timer.toggle(83000);
    frame("timer-paused",[&](M5Canvas& c) { clock_ui::timer(c,timer); });
    timer.reset();timer.setSeconds(86399);timer.toggle(0);
    frame("timer-long",[&](M5Canvas& c) { clock_ui::timer(c,timer); });
    timer.update(86399000);
    frame("timer-done",[&](M5Canvas& c) { clock_ui::timer(c,timer); });
    clockwork::Stopwatch watch;watch.toggle(0);watch.mark(23450);watch.mark(54320);watch.mark(94380);
    frame("stopwatch-running",[&](M5Canvas& c) { clock_ui::stopwatch(c,watch); });
    watch.toggle(94450);
    frame("stopwatch-paused",[&](M5Canvas& c) { clock_ui::stopwatch(c,watch); });
    watch.reset();
    frame("stopwatch-idle",[&](M5Canvas& c) { clock_ui::stopwatch(c,watch); });
    watch.toggle(0);watch.update(clockwork::Stopwatch::maxElapsed);
    frame("stopwatch-long",[&](M5Canvas& c) { clock_ui::stopwatch(c,watch); });
}
