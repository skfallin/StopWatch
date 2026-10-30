#include "ui_gradient.h"
#include "ui_theme.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>

int main() {
    for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
        assert(ui::dither565(0,x,y)==0);
        assert(ui::dither565(0xffffff,x,y)==0xffff);
        assert(ui::dither565(0xff0000,x,y)==0xf800);
        assert(ui::dither565(0x00ff00,x,y)==0x07e0);
        assert(ui::dither565(0x0000ff,x,y)==0x001f);
    }
    for(const auto& theme:ui::themes) {
        for(uint32_t color:{theme.accent,0x181c19u,0x30392au,0x030503u}) {
            std::array<double,3> sum{};
            std::set<uint16_t> shades;
            for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
                const uint16_t p=ui::dither565(color,x,y);
                assert(p==ui::dither565(color,x+8,y+8));
                sum[0]+=(p>>11)*255.0/31;
                sum[1]+=(p>>5&63)*255.0/63;
                sum[2]+=(p&31)*255.0/31;
                shades.insert(p);
            }
            for(int ch=0;ch<3;++ch) assert(std::abs(sum[ch]/64-((color>>(16-ch*8))&255))<0.07);
            assert(shades.size()>1); // Flat RGB565 rows must become a spatial blend.
        }
    }
    std::cout<<"OK: RGB565 primari, neri, sei palette, gradiente senza righe uniformi e colore medio conservato\n";
}
