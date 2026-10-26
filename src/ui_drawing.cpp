#include "ui_drawing.h"
#include "ui_fonts.h"
#include "policy.h"
#include <cmath>

namespace ui {
namespace {
lgfx::PointerWrapper sources[] = {
    {ui_fonts::inter18,sizeof(ui_fonts::inter18)}, {ui_fonts::inter22,sizeof(ui_fonts::inter22)},
    {ui_fonts::inter28,sizeof(ui_fonts::inter28)}, {ui_fonts::inter36,sizeof(ui_fonts::inter36)},
    {ui_fonts::inter68,sizeof(ui_fonts::inter68)}, {ui_fonts::inter96,sizeof(ui_fonts::inter96)}
};
lgfx::VLWfont fonts[6];
bool fontsReady=false;
}
bool beginFonts() {
    fontsReady=true;
    for (size_t i=0;i<6;++i) if (!fonts[i].loadFont(&sources[i])) fontsReady=false;
    return fontsReady;
}
void setTheme(uint8_t index) {
    const auto& theme=themes[index<themes.size()?index:0];
    accent=theme.accent;
    focus=theme.focus;
    focusPanel=theme.focusPanel;
    navigation=theme.navigation;
    selectionSurface=theme.selectionSurface;
}
int textWidth(M5Canvas& canvas, const String& value, Font font) {
    canvas.setTextSize(1);
    canvas.setFont(fontsReady ? static_cast<const lgfx::IFont*>(&fonts[int(font)]) : &lgfx::fonts::FreeSans12pt7b);
    return canvas.textWidth(value);
}
void label(M5Canvas& canvas, const String& value, int x, int y, Font font, uint32_t color, uint32_t background, int maxWidth) {
    canvas.setTextSize(1);
    canvas.setFont(fontsReady ? static_cast<const lgfx::IFont*>(&fonts[int(font)]) : &lgfx::fonts::FreeSans12pt7b);
    canvas.setTextColor(color,background);
    canvas.setTextDatum(lgfx::textdatum_t::middle_center);
    String fitted=value;
    if (canvas.textWidth(fitted)>maxWidth) {
        while (fitted.length() && canvas.textWidth(fitted+"…")>maxWidth) {
            const std::string_view text(fitted.c_str(),fitted.length());
            fitted.remove(policy::utf8Prefix(text,text.size()-1).size());
        }
        fitted+="…";
    }
    canvas.drawString(fitted,x,y);
}
void icon(M5Canvas& c, Icon name, int cx, int cy, int size, uint32_t color, uint32_t bg) {
    const float scale=size/24.0f;
    const auto x=[&](float v){return int(std::lround(cx+(v-12)*scale));};
    const auto y=[&](float v){return int(std::lround(cy+(v-12)*scale));};
    const auto stroke=[&](float ax,float ay,float bx,float by,float width=1.7f){
        c.drawWideLine(x(ax),y(ay),x(bx),y(by),width*scale/2,color);
    };
    const auto arc=[&](float ox,float oy,float radius,float first,float last){
        for (float a=first;a<last;a+=12) {
            const float b=std::min(a+12,last);
            stroke(ox+radius*std::cos(a*M_PI/180),oy+radius*std::sin(a*M_PI/180),
                   ox+radius*std::cos(b*M_PI/180),oy+radius*std::sin(b*M_PI/180));
        }
    };
    switch(name) {
    case Icon::Moon:
        c.fillSmoothCircle(x(12),y(12),9*scale,color);
        c.fillSmoothCircle(x(16),y(8),8*scale,bg);break;
    case Icon::Music:
        stroke(9,16,9,5);stroke(9,5,20,3);stroke(20,3,20,14);
        c.fillSmoothCircle(x(6),y(18),3*scale,color);
        c.fillSmoothCircle(x(17),y(16),3*scale,color);break;
    case Icon::House:
        stroke(3,10,12,3);stroke(12,3,21,10);stroke(4,9,4,21);stroke(20,9,20,21);
        stroke(4,21,9,21);stroke(15,21,20,21);stroke(9,21,9,14);stroke(9,14,15,14);stroke(15,14,15,21);break;
    case Icon::Badge:
        stroke(4,3,20,3);stroke(20,3,20,21);stroke(20,21,4,21);stroke(4,21,4,3);
        stroke(6,17,10,12);stroke(10,12,14,16);stroke(14,16,17,13);stroke(17,13,19,17);
        c.fillSmoothCircle(x(15),y(8),std::max(1,int(scale)),color);break;
    case Icon::Mic:
        c.drawWideLine(x(12),y(5),x(12),y(11),3*scale,color);
        c.drawWideLine(x(12),y(5),x(12),y(11),1.3f*scale,bg);
        arc(12,10,7,0,180);stroke(5,10,5,12);stroke(19,10,19,12);
        stroke(12,17,12,22);stroke(8,22,16,22);break;
    case Icon::Sliders:
        for(int row=0;row<3;++row) {const int yy=6+row*6,xx=row==1?8:16;
            stroke(3,yy,xx-3,yy);stroke(xx+3,yy,21,yy);stroke(xx,yy-3,xx,yy+3);}
        break;
    case Icon::Sun:
        arc(12,12,4,0,360);
        for(int i=0;i<8;++i) {float a=i*M_PI/4;stroke(12+8*std::cos(a),12+8*std::sin(a),12+10.5*std::cos(a),12+10.5*std::sin(a));}
        break;
    case Icon::Activity:
        stroke(2,12,6,12);stroke(6,12,9,5);stroke(9,5,14,20);stroke(14,20,17,12);stroke(17,12,22,12);break;
    case Icon::Bluetooth:
        stroke(7,7,17,17);stroke(17,17,12,22);stroke(12,22,12,2);stroke(12,2,17,7);stroke(17,7,7,17);break;
    case Icon::Wifi:
        arc(12,20,16,233,307);arc(12,20,10,231,309);arc(12,20,4,230,310);
        c.fillSmoothCircle(x(12),y(20),std::max(1,int(scale)),color);break;
    case Icon::Back: stroke(15,5,8,12);stroke(8,12,15,19);break;
    case Icon::Next: stroke(9,5,16,12);stroke(16,12,9,19);break;
    case Icon::Close: stroke(6,6,18,18);stroke(6,18,18,6);break;
    case Icon::Play: stroke(8,4,20,12);stroke(20,12,8,20);stroke(8,20,8,4);break;
    case Icon::Check: stroke(4,12,10,18,2.2f);stroke(10,18,21,6,2.2f);break;
    case Icon::Bulb:
        arc(12,9,7,150,390);stroke(6,13,9,18);stroke(18,13,15,18);
        stroke(9,18,15,18);stroke(9,21,15,21);stroke(11,23,13,23);break;
    case Icon::Speaker:
        stroke(3,9,7,9);stroke(7,9,12,5);stroke(12,5,12,19);stroke(12,19,7,15);stroke(7,15,3,15);stroke(3,15,3,9);
        arc(11,12,7,315,405);arc(11,12,11,315,405);break;
    case Icon::Archive:
        stroke(4,7,20,7);stroke(4,7,4,21);stroke(4,21,20,21);stroke(20,21,20,7);
        stroke(3,3,21,3);stroke(3,3,3,7);stroke(21,3,21,7);stroke(9,12,15,12);break;
    case Icon::Warning:
        stroke(12,3,2,21);stroke(2,21,22,21);stroke(22,21,12,3);stroke(12,9,12,14);
        c.fillSmoothCircle(x(12),y(18),std::max(1,int(scale)),color);break;
    case Icon::Clock:
        arc(12,12,10,0,360);stroke(12,6,12,12);stroke(12,12,17,15);break;
    case Icon::Timer:
        arc(12,13,9,285,610);stroke(12,5,12,13);stroke(12,13,17,13);stroke(11,2,15,5);break;
    case Icon::Stopwatch:
        arc(12,14,8,0,360);stroke(12,1,12,5);stroke(9,1,15,1);stroke(18,5,21,8);stroke(12,9,12,14);break;
    case Icon::Link:
        arc(8,12,5,90,270);arc(16,12,5,270,450);stroke(8,7,12,7);stroke(12,17,16,17);stroke(8,12,16,12);break;
    }
}
void circleButton(M5Canvas& c, Circle target, Icon name, int size, uint32_t fill, uint32_t color) {
    c.fillSmoothCircle(target.x,target.y,target.radius,fill);
    icon(c,name,target.x,target.y,size,color,fill);
}
void pill(M5Canvas& c,int x,int y,int width,const String& value,uint32_t fill,uint32_t color) {
    c.fillSmoothRoundRect(x-width/2,y-28,width,56,28,fill);
    label(c,value,x,y,Font::Body,color,fill,width-24);
}
void dots(M5Canvas& c,int selected,int count,int y,uint32_t active,uint32_t inactive) {
    const int step=count>6?14:18, first=233-(count-1)*step/2;
    for(int i=0;i<count;++i) {
        if(i==selected)c.fillSmoothRoundRect(first+i*step-7,y-3,14,6,3,active);
        else c.fillSmoothCircle(first+i*step,y,3,inactive);
    }
}
}
