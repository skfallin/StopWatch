#pragma once
#include <cstdint>

namespace ui {
inline uint16_t dither565(uint32_t rgb,int x,int y) {
    constexpr uint8_t bayer[8][8]={
        {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
        {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
        {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
        {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}
    };
    const int threshold=bayer[y&7][x&7]*2+1;
    const auto channel=[&](int shift,int levels) {
        return (((rgb>>shift)&255)*levels*128+threshold*255)/(255*128);
    };
    return uint16_t((channel(16,31)<<11)|(channel(8,63)<<5)|channel(0,31));
}
}
