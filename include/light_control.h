#pragma once
#include <algorithm>
#include <string_view>

namespace casa {
inline bool brightnessMode(std::string_view mode) {
    return mode=="brightness" || mode=="color_temp" || mode=="hs" || mode=="xy" ||
        mode=="rgb" || mode=="rgbw" || mode=="rgbww" || mode=="white";
}
inline int brightnessPercent(int value) { return value>=0 && value<=255?(value*100+127)/255:-1; }
struct LightLevel {
    int value=100,initial=100;
    void begin(int level) { value=initial=level>=0 && level<=100?level:100; }
    void step(int direction) { value=std::clamp(value+direction*5,0,100); }
    void point(int x,int) {
        value=((std::clamp(x,109,357)-109)*100+124)/248;
    }
};
}
