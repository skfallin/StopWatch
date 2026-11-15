#include "i18n.h"
#include "scene_view.h"
#include <cstdio>

namespace scene_ui {
namespace {
uint32_t blend(uint32_t from,uint32_t to,int amount) {
    uint32_t result=0;
    for(int shift:{16,8,0}) result|=uint32_t(((from>>shift&255)*(255-amount)+(to>>shift&255)*amount)/255)<<shift;
    return result;
}
}

void home(M5Canvas& canvas,const ui::HomeMenu& menu,uint32_t age) {
    const ui::Icon icons[]={ui::Icon::House,ui::Icon::Mic,ui::Icon::Badge,ui::Icon::Clock,ui::Icon::Music,ui::Icon::Usb,ui::Icon::Sliders,ui::Icon::Focus};
    const char* names[]={"Casa","Note","Badge","Orologio","Spotify","Ponte","Sistema","Focus"};
    const float progress=std::min(1.0f,age/180.0f);
    const int inset=int(3*(1-progress)*(1-progress));
    for(int slot=0;slot<menu.count();++slot) {
        const bool selected=slot==menu.selected;
        const auto circle=menu.circle(slot);
        // Outlines on black: an AMOLED only pays for the pixels it lights.
        canvas.fillSmoothCircle(circle.x,circle.y,circle.radius-(selected?inset:0),selected?ui::accent:ui::line);
        if(!selected) canvas.fillSmoothCircle(circle.x,circle.y,circle.radius-2,ui::black);
        ui::icon(canvas,icons[menu.order[slot]],circle.x,circle.y,38,selected?ui::ink:ui::navigation,selected?ui::accent:ui::black);
    }
    ui::label(canvas,i18n::text(names[menu.app()]),233,288,ui::Font::Body,ui::accent,ui::black,170);
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

void pages(M5Canvas& canvas,int page,int count) {
    ui::circleButton(canvas,ui::pagePrevious,ui::Icon::Back,36,ui::surface,ui::navigation);
    ui::circleButton(canvas,ui::pageNext,ui::Icon::Next,36,ui::surface,ui::navigation);
    char text[8];snprintf(text,sizeof(text),"%d / %d",page+1,count);
    ui::label(canvas,text,233,387,ui::Font::Small,ui::muted,ui::black,70);
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
