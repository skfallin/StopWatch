#pragma once

// Host-only drawing recorder; the production view and vector icons run unchanged.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>
using String=std::string;
namespace lgfx { struct rgb565_t { uint16_t raw=0;rgb565_t& operator=(uint16_t value) { raw=value;return *this; } }; }

class M5Canvas {
public:
    explicit M5Canvas(std::ostream& output):out(output) {}
    void pushImage(int x,int y,int width,int height,const lgfx::rgb565_t* pixels) {
        out<<"[\"pixels\","<<x<<','<<y<<','<<width<<','<<height<<",[";
        for(int i=0;i<width*height;++i) { if(i) out<<',';out<<pixels[i].raw; }
        out<<"]]\n";
    }
    void fillSmoothRoundRect(int x,int y,int w,int h,int r,uint32_t color) {
        out<<"[\"rect\","<<x<<','<<y<<','<<w<<','<<h<<','<<r<<','<<color<<"]\n";
    }
    void fillRect(int x,int y,int w,int h,uint32_t color) { fillSmoothRoundRect(x,y,w,h,0,color); }
    void fillRoundRect(int x,int y,int w,int h,int r,uint32_t color) { fillSmoothRoundRect(x,y,w,h,r,color); }
    void drawRoundRect(int x,int y,int w,int h,int r,uint32_t color) {
        out<<"[\"outline\","<<x<<','<<y<<','<<w<<','<<h<<','<<r<<','<<color<<"]\n";
    }
    void fillSmoothCircle(int x,int y,float r,uint32_t color) {
        out<<"[\"circle\","<<x<<','<<y<<','<<r<<','<<color<<"]\n";
    }
    void drawWideLine(int x0,int y0,int x1,int y1,float r,uint32_t color) {
        out<<"[\"line\","<<x0<<','<<y0<<','<<x1<<','<<y1<<','<<r<<','<<color<<"]\n";
    }
    void text(const String& value,int x,int y,int font,uint32_t color,uint32_t bg,int width) {
        out<<"[\"text\","<<std::quoted(value)<<','<<x<<','<<y<<','<<font<<','<<color<<','<<bg<<','<<width<<"]\n";
    }
    void drawBitmap(int x,int y,const uint8_t* bits,int width,int height,uint32_t color) {
        for(int row=0;row<height;++row) for(int col=0;col<width;++col)
            if(bits[row*((width+7)/8)+col/8]&(0x80>>(col%8))) fillSmoothRoundRect(x+col,y+row,1,1,0,color);
    }
private:
    std::ostream& out;
};
