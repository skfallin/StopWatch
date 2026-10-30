#include "i18n.h"
#include "productivity_view.h"
#include <filesystem>
#include <fstream>
namespace ui {
void label(M5Canvas& c,const String& value,int x,int y,Font font,uint32_t color,uint32_t bg,int width) { c.text(value,x,y,int(font),color,bg,width); }
#include "drawing_primitives.inc"
}
int main(int argc,char** argv) {
    if(argc<2 || argc>3) return 1;
    i18n::italian = argc==3 && std::string(argv[2])=="it";
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    clockwork::Focus focus;
    for(int state=0;state<4;++state) {
        if(state==1) focus.toggle(0);if(state==2) focus.toggle(10000);if(state==3) { focus.toggle(10000);focus.update(1500000); }
        std::ofstream out(root/("focus-"+std::to_string(state)+".jsonl"));M5Canvas c(out);productivity_ui::focus(c,focus);
    }
    for(bool usb:{false,true}) { std::ofstream out(root/(usb?"slides-connected.jsonl":"slides-offline.jsonl"));M5Canvas c(out);productivity_ui::slides(c,184000,usb); }
}
