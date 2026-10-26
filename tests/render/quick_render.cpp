#include "quick_settings_view.h"
#include <filesystem>
#include <fstream>

namespace ui {
void label(M5Canvas& c,const String& value,int x,int y,Font font,uint32_t color,uint32_t bg,int width) {
    c.text(value,x,y,int(font),color,bg,width);
}
#include "drawing_primitives.inc"
}

int main(int argc,char** argv) {
    if(argc!=2) return 1;
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    const auto frame=[&](const char* name,bool quiet,bool pending=false,bool failed=false) {
        std::ofstream output(root/(String(name)+".jsonl"));M5Canvas canvas(output);
        ui::quickSettings(canvas,quiet,pending,failed);
    };
    frame("quick-off",false);frame("quick-on",true);
    frame("quick-pending",true,true);frame("quick-failed",true,false,true);
    ui::accent=ui::themes[1].accent;ui::selectionSurface=ui::themes[1].selectionSurface;
    frame("quick-buttons",true);
    ui::accent=ui::themes[3].accent;ui::selectionSurface=ui::themes[3].selectionSurface;
    frame("quick-violet",true);
}
