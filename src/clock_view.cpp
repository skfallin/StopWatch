#include "clock_view.h"
#include "ui_drawing.h"
#include <cmath>
#include <cstdio>

namespace clock_ui {
namespace {
constexpr float radians=3.14159265358979323846f/180;
const char* days[]={"DOM","LUN","MAR","MER","GIO","VEN","SAB"};
const char* months[]={"GEN","FEB","MAR","APR","MAG","GIU","LUG","AGO","SET","OTT","NOV","DIC"};
String two(unsigned value) { char text[12];std::snprintf(text,sizeof(text),"%02u",value);return text; }
String date(const tm& local) {
    char text[32];std::snprintf(text,sizeof(text),"%s %d %s",days[local.tm_wday],local.tm_mday,months[local.tm_mon]);return text;
}
String duration(uint64_t milliseconds) {
    const unsigned seconds=milliseconds/1000, hours=seconds/3600;
    char text[24];
    if(hours) std::snprintf(text,sizeof(text),"%02u:%02u:%02u",hours,seconds/60%60,seconds%60);
    else std::snprintf(text,sizeof(text),"%02u:%02u",seconds/60,seconds%60);
    return text;
}
uint32_t shade(uint32_t color, float strength) {
    return (uint32_t(((color>>16)&255)*strength)<<16) | (uint32_t(((color>>8)&255)*strength)<<8) | uint32_t((color&255)*strength);
}
void radial(M5Canvas& c,float degrees,float inner,float outer,uint32_t color,float width=1,int cy=233) {
    const float a=(degrees-90)*radians;
    c.drawWideLine(233+inner*std::cos(a),cy+inner*std::sin(a),233+outer*std::cos(a),cy+outer*std::sin(a),width,color);
}
void arc(M5Canvas& c,int radius,float progress,uint32_t color,int cy=233,float width=3) {
    const float end=std::clamp(progress,0.0f,1.0f)*360;
    for(float degrees=0;degrees<end;degrees+=3) {
        const float a=(degrees-90)*radians,b=(std::min(degrees+3,end)-90)*radians;
        c.drawWideLine(233+radius*std::cos(a),cy+radius*std::sin(a),233+radius*std::cos(b),cy+radius*std::sin(b),width,color);
    }
}
void header(M5Canvas& c,const char* title) {
    ui::circleButton(c,ui::backButton,ui::Icon::Back,23,ui::surface,ui::white);
    ui::label(c,title,233,84,ui::Font::Label,ui::white,ui::black,164);
}
void tabs(M5Canvas& c,int selected) {
    c.fillSmoothRoundRect(105,ui::clockTabsY-29,256,58,29,ui::surface);
    const char* names[]={"Ora","Timer","Crono"};
    for(int i=0;i<3;++i) {
        if(i==selected) c.fillSmoothRoundRect(109+i*84,ui::clockTabsY-23,80,46,23,ui::accent);
        ui::label(c,names[i],149+i*84,ui::clockTabsY,ui::Font::Small,i==selected?ui::ink:ui::white,i==selected?ui::accent:ui::surface,76);
    }
}
void action(M5Canvas& c,int x,const char* label,bool primary) {
    ui::pill(c,x,ui::clockActionY,130,label,primary?ui::accent:ui::surface,primary?ui::ink:ui::white);
}
void hands(M5Canvas& c,const tm& t,bool glow) {
    const float hour=(t.tm_hour%12+t.tm_min/60.0f)*30, minute=(t.tm_min+t.tm_sec/60.0f)*6;
    if(glow) {
        radial(c,hour,-8,94,shade(ui::accent,0.15f),12);
        radial(c,minute,-12,142,shade(ui::accent,0.15f),8);
    }
    radial(c,hour,-8,94,ui::white,5);
    radial(c,minute,-12,142,glow?ui::accent:ui::white,3);
    radial(c,t.tm_sec*6,-24,159,ui::navigation,1.2f);
    c.fillSmoothCircle(233,233,6,ui::accent);
    c.fillSmoothCircle(233,233,2,ui::ink);
}
void digital(M5Canvas& c,const tm& t) {
    ui::label(c,"ORA LOCALE",233,119,ui::Font::Small,ui::muted);
    ui::label(c,two(t.tm_hour)+":"+two(t.tm_min),233,204,ui::Font::Number,ui::white,ui::black,366);
    c.fillSmoothRoundRect(179,264,108,56,28,ui::selectionSurface);
    ui::label(c,two(t.tm_sec),233,292,ui::Font::Title,ui::accent,ui::selectionSurface,96);
    ui::label(c,date(t),233,350,ui::Font::Small,ui::muted);
}
void analog(M5Canvas& c,const tm& t,bool glow) {
    if(glow) {
        for(int i=0;i<9;++i) arc(c,211-i,1,shade(ui::accent,0.04f+i*0.025f),233,1.5f);
        arc(c,201,1,ui::accent,233,1.4f);
        ui::label(c,t.tm_hour<12?"AM":"PM",233,157,ui::Font::Small,ui::muted);
    }
    for(int i=0;i<60;++i) radial(c,i*6,i%5?174:167,180,i%5?ui::line:ui::muted,i%5?0.7f:1.5f);
    if(!glow) {
        ui::label(c,"12",233,91,ui::Font::Label,ui::white);
        ui::label(c,"3",377,233,ui::Font::Label,ui::white);
        ui::label(c,"6",233,376,ui::Font::Label,ui::white);
        ui::label(c,"9",89,233,ui::Font::Label,ui::white);
    }
    ui::label(c,date(t),233,310,ui::Font::Small,ui::muted,ui::black,180);
    hands(c,t,glow);
}
void duo(M5Canvas& c,const tm& t) {
    ui::label(c,two(t.tm_hour),203,158,ui::Font::Number,ui::accent,ui::black,210);
    ui::label(c,two(t.tm_min),263,269,ui::Font::Number,ui::white,ui::black,210);
    c.fillSmoothCircle(345,170,28,ui::surface);
    ui::label(c,two(t.tm_sec),345,170,ui::Font::Body,ui::white,ui::surface,52);
    ui::label(c,date(t),233,350,ui::Font::Small,ui::muted);
}
void orbit(M5Canvas& c,const tm& t) {
    const float values[]={(t.tm_hour%12+t.tm_min/60.0f)/12.0f,(t.tm_min+t.tm_sec/60.0f)/60.0f,t.tm_sec/60.0f};
    for(int i=0;i<3;++i) {
        const int radius=190-i*30;
        arc(c,radius,1,ui::surface,233,4);
        arc(c,radius,values[i],i==1?ui::white:ui::accent,233,4);
    }
    ui::label(c,two(t.tm_hour)+":"+two(t.tm_min),233,220,ui::Font::Title,ui::white,ui::black,230);
    ui::label(c,two(t.tm_sec)+" s",233,266,ui::Font::Body,ui::accent);
    ui::label(c,date(t),233,308,ui::Font::Small,ui::muted,ui::black,190);
}
void segment(M5Canvas& c,int x,int value) {
    constexpr uint8_t masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    const int lines[7][4]={{8,0,44,0},{49,6,49,44},{49,56,49,94},{8,100,44,100},{3,56,3,94},{3,6,3,44},{8,50,44,50}};
    for(int i=0;i<7;++i) c.drawWideLine(x+lines[i][0],173+lines[i][1],x+lines[i][2],173+lines[i][3],3.5f,masks[value]&(1<<i)?ui::accent:ui::surface);
}
void segments(M5Canvas& c,const tm& t) {
    ui::label(c,"STOPWATCH / 24H",233,111,ui::Font::Small,ui::muted);
    segment(c,74,t.tm_hour/10);segment(c,141,t.tm_hour%10);
    segment(c,273,t.tm_min/10);segment(c,340,t.tm_min%10);
    c.fillSmoothCircle(233,205,4,ui::white);c.fillSmoothCircle(233,244,4,ui::white);
    ui::label(c,two(t.tm_sec),233,324,ui::Font::Title,ui::white);
    ui::label(c,date(t),233,371,ui::Font::Small,ui::muted);
}
}
void menu(M5Canvas& c,int selected) {
    header(c,"Orologio");
    const char* titles[]={"Ora","Timer","Cronometro"};
    const ui::Icon icons[]={ui::Icon::Clock,ui::Icon::Timer,ui::Icon::Stopwatch};
    for(int i=0;i<3;++i) {
        const int y=132+i*86;
        const uint32_t bg=i==selected?ui::selectionSurface:ui::surface;
        c.fillSmoothRoundRect(85,y,296,72,30,bg);
        ui::icon(c,icons[i],124,y+36,28,i==selected?ui::accent:ui::white,bg);
        ui::label(c,titles[i],245,y+36,ui::Font::Label,ui::white,bg,218);
    }
    ui::label(c,"A scegli · B apri",233,413,ui::Font::Small,ui::muted,ui::black,240);
}
void watch(M5Canvas& c,clockwork::Face face,const tm& t,bool valid,bool controls) {
    if(!valid) {
        ui::icon(c,ui::Icon::Clock,233,185,72,ui::accent);
        ui::label(c,"Ora non impostata",233,265,ui::Font::Label);
        ui::label(c,"Collega iPhone o Wi-Fi",233,305,ui::Font::Small,ui::muted);
    } else switch(face) {
    case clockwork::Face::Digital: digital(c,t);break;
    case clockwork::Face::Analog: analog(c,t,false);break;
    case clockwork::Face::Halo: analog(c,t,true);break;
    case clockwork::Face::Duo: duo(c,t);break;
    case clockwork::Face::Orbit: orbit(c,t);break;
    case clockwork::Face::Segments: segments(c,t);break;
    }
    if(controls || !valid) {
        c.fillSmoothRoundRect(154,57,158,54,27,ui::black);
        header(c,clockwork::faceNames[int(face)]);
        ui::circleButton(c,{354,84,28},ui::Icon::Next,20,ui::surface,ui::white);
        tabs(c,0);
    }
}
void timer(M5Canvas& c,const clockwork::Timer& timer) {
    using State=clockwork::Timer::State;
    header(c,"Timer");
    if(timer.state()==State::Idle) {
        const unsigned seconds=timer.duration()/1000;
        const unsigned values[]={seconds/3600,seconds/60%60,seconds%60};
        const char* units[]={"ORE","MIN","SEC"};
        for(int i=0;i<3;++i) {
            const int x=137+i*96;
            for(int y:{143,283}) {
                c.fillSmoothCircle(x,y,23,ui::surface);
                c.drawWideLine(x-7,y,x+7,y,1.3f,ui::white);
                if(y==143) c.drawWideLine(x,y-7,x,y+7,1.3f,ui::white);
            }
            ui::label(c,two(values[i]),x,207,ui::Font::Clock,ui::white,ui::black,90);
            ui::label(c,units[i],x,252,ui::Font::Small,ui::muted,ui::black,80);
        }
        ui::pill(c,233,ui::clockActionY,200,"Avvia",ui::accent,ui::ink);
    } else {
        const bool done=timer.state()==State::Done;
        if(done) {
            c.fillSmoothCircle(233,208,102,ui::focus);
            ui::label(c,"Tempo",233,187,ui::Font::Title,ui::ink,ui::focus,190);
            ui::label(c,"scaduto",233,233,ui::Font::Title,ui::ink,ui::focus,190);
        } else {
            arc(c,102,1,ui::surface,208,4);
            arc(c,102,float(timer.remaining())/timer.duration(),ui::accent,208,4);
            const auto font=timer.remainingSeconds()>=3600?ui::Font::Title:ui::Font::Clock;
            ui::label(c,duration(uint64_t(timer.remainingSeconds())*1000),233,197,font,ui::white,ui::black,192);
            ui::label(c,timer.state()==State::Running?"IN CORSO":"IN PAUSA",233,253,ui::Font::Small,ui::muted);
        }
        action(c,160,done?"Ripeti":timer.state()==State::Running?"Menu":"Azzera",false);
        action(c,306,done?"OK":timer.state()==State::Running?"Pausa":"Riprendi",true);
    }
    tabs(c,1);
}
void stopwatch(M5Canvas& c,const clockwork::Stopwatch& watch) {
    const bool limit=watch.elapsed()==clockwork::Stopwatch::maxElapsed;
    header(c,"Cronometro");
    ui::label(c,duration(watch.elapsed()),233,170,ui::Font::Clock,ui::white,ui::black,358);
    ui::label(c,"."+two(watch.elapsed()/10%100),233,226,ui::Font::Title,ui::accent);
    if(watch.lapCount() && !limit) {
        for(size_t i=0;i<watch.lapCount();++i) {
            const auto& lap=watch.lap(i);
            char name[24];std::snprintf(name,sizeof(name),"GIRO %02lu   ",static_cast<unsigned long>(lap.number));
            ui::label(c,String(name)+duration(lap.split)+"."+two(lap.split/10%100),233,261+i*21,ui::Font::Small,i?ui::muted:ui::white,ui::black,310);
        }
    } else ui::label(c,limit?"LIMITE 100 ORE":watch.running()?"IN CORSO":watch.elapsed()?"IN PAUSA":"PRONTO",233,289,ui::Font::Small,ui::muted);
    action(c,160,watch.running()?"Giro":"Azzera",false);
    action(c,306,limit?"Menu":watch.running()?"Pausa":watch.elapsed()?"Riprendi":"Avvia",true);
    tabs(c,2);
}
}
