#include "i18n.h"
#include "i18n.h"
#include "scene_view.h"
#include "ui_gradient.h"
#include <array>
#include <cmath>

namespace scene_ui {
namespace {
uint32_t blend(uint32_t from,uint32_t to,int amount) {
    uint32_t result=0;
    for(int shift:{16,8,0}) result|=uint32_t(((from>>shift&255)*(255-amount)+(to>>shift&255)*amount)/255)<<shift;
    return result;
}

void disc(M5Canvas& canvas,ui::Circle circle,bool hero) {
    const uint32_t base=hero?ui::accent:ui::surface;
    canvas.fillSmoothCircle(circle.x,circle.y,circle.radius,base);
    // Dither before RGB565 conversion; uniform scanlines otherwise form visible bands.
    std::array<lgfx::rgb565_t,466> pixels;
    const int radius=circle.radius-1;
    for(int y=-radius;y<=radius;++y) {
        const int half=int(std::sqrt(float(radius*radius-y*y)));
        const int light=std::max(0,128-(y+radius)*128/(radius*2));
        const uint32_t color=blend(base,hero?0xf5ffd3:0x30392a,hero?light/2:light);
        for(int x=-half;x<=half;++x) pixels[x+half]=ui::dither565(color,circle.x+x,circle.y+y);
        canvas.pushImage(circle.x-half,circle.y+y,half*2+1,1,pixels.data());
    }
}
}

void home(M5Canvas& canvas,const ui::HomeMenu& menu,uint32_t age) {
    const ui::Icon icons[]={ui::Icon::House,ui::Icon::Mic,ui::Icon::Badge,ui::Icon::Clock,ui::Icon::Music,ui::Icon::Usb,ui::Icon::Key,ui::Icon::Sliders,ui::Icon::Focus,ui::Icon::Display};
    const char* names[]={"Casa","Note","Badge","Orologio","Spotify","Ponte","Chiave","Sistema","Focus","Slide"};
    const float progress=std::min(1.0f,age/180.0f);
    const int inset=int(3*(1-progress)*(1-progress));
    auto primary=menu.circle(0);
    const int radius=primary.radius+28;
    std::array<lgfx::rgb565_t,466> pixels;
    for(int y=-radius;y<=radius;++y) {
        const int half=int(std::sqrt(float(radius*radius-y*y)));
        for(int x=-half;x<=half;++x) {
            const float distance=std::sqrt(float(x*x+y*y));
            const int light=std::clamp(int((radius-distance)*13/28),0,13);
            pixels[x+half]=ui::dither565(blend(ui::black,ui::accent,light),primary.x+x,primary.y+5+y);
        }
        canvas.pushImage(primary.x-half,primary.y+5+y,half*2+1,1,pixels.data());
    }
    for(int slot=0;slot<menu.visibleCount();++slot) {
        const int app=menu.app(slot);
        auto circle=menu.circle(slot);
        circle.radius-=inset;
        disc(canvas,circle,slot==0);
        const uint32_t color=slot==0?ui::ink:ui::white;
        ui::icon(canvas,icons[app],circle.x,circle.y-(slot==0?19:13),slot==0?56:35,
            slot==0?ui::ink:ui::navigation,slot==0?ui::accent:ui::surface);
        ui::label(canvas,i18n::text(names[app]),circle.x,circle.y+(slot==0?32:22),slot==0?ui::Font::Label:ui::Font::Small,
            color,color,slot==0?116:80);
    }
    if(menu.pageCount()>1) for(bool next:{false,true})
        ui::circleButton(canvas,menu.arrow(next),next?ui::Icon::Next:ui::Icon::Back,36,ui::surface,ui::navigation);
    if(menu.count()>menu.visibleCount()) ui::dots(canvas,menu.selected,menu.count(),389);
    int selectedPage=0;for(int group=0;group<menu.space;++group) if(menu.count(group)>0) ++selectedPage;
    ui::dots(canvas,selectedPage,menu.pageCount(),424);
}

void row(M5Canvas& canvas,ui::Rect bounds,const String& title,ui::Icon icon,const String& detail,bool selected) {
    const uint32_t fill=selected?ui::accent:ui::surface,color=selected?ui::ink:ui::white;
    const int y=bounds.y+bounds.height/2;
    canvas.fillSmoothRoundRect(bounds.x,bounds.y,bounds.width,bounds.height,32,fill);
    ui::icon(canvas,icon,bounds.x+35,y,35,selected?ui::ink:ui::accent,fill);
    ui::label(canvas,title,247,detail.length()==0?y:y-13,ui::Font::Body,color,fill,204);
    if(detail.length()!=0) ui::label(canvas,detail,247,y+17,ui::Font::Small,selected?ui::ink:ui::muted,fill,204);
    ui::icon(canvas,ui::Icon::Next,bounds.x+bounds.width-19,y,18,selected?ui::ink:ui::muted,fill);
}

void pages(M5Canvas& canvas,int page) {
    ui::circleButton(canvas,ui::pagePrevious,ui::Icon::Back,36,ui::surface,ui::navigation);
    ui::circleButton(canvas,ui::pageNext,ui::Icon::Next,36,ui::surface,ui::navigation);
    ui::label(canvas,page==0?"1 / 2":"2 / 2",233,387,ui::Font::Small,ui::muted,ui::black,70);
}

void notice(M5Canvas& canvas,const String& title,const String& detail,ui::Icon icon) {
    const auto card=ui::noticeCard;
    const uint32_t fill=ui::selectionSurface,shadow=blend(fill,ui::black,160);
    canvas.fillSmoothRoundRect(card.x-3,card.y+5,card.width+6,card.height+6,31,shadow);
    canvas.fillSmoothRoundRect(card.x,card.y,card.width,card.height,28,fill);
    ui::icon(canvas,icon,card.x+36,card.y+card.height/2,37,ui::accent,fill);
    ui::label(canvas,title,card.x+175,card.y+(detail.length()==0?42:29),ui::Font::Body,ui::white,fill,212);
    if(detail.length()!=0) ui::label(canvas,detail,card.x+175,card.y+57,ui::Font::Small,ui::muted,fill,212);
}
}
