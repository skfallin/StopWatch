#include "i18n.h"
#include "scene_view.h"
#include "ui_sounds.h"
#include "light_control.h"
#include "policy.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include "scene_widths.inc"

namespace ui {
void label(M5Canvas& c,const String& value,int x,int y,Font font,uint32_t color,uint32_t bg,int width) {
    c.text(value,x,y,int(font),color,bg,width);
}
int textWidth(M5Canvas&,const String& text,Font) {
    int width=0;for(unsigned char ch:text) if(ch<128) width+=sceneWidths[ch];return width;
}
#include "drawing_primitives.inc"
void dots(M5Canvas& c,int selected,int count,int y,uint32_t active,uint32_t inactive) {
    const int step=count>6?14:18, first=233-(count-1)*step/2;
    for(int i=0;i<count;++i) {
        if(i==selected)c.fillSmoothRoundRect(first+i*step-7,y-3,14,6,3,active);
        else c.fillSmoothCircle(first+i*step,y,3,inactive);
    }
}
}

std::ofstream output;
M5Canvas canvas(output);
struct {
    int brightness=192,volume=140,vibration=128,screenTimeout=30,soundPack=1,colorTheme=0,noteVolume=120;
    bool doNotDisturb=false,favoritesMqtt=true;
    int favoriteCount=1;
    struct Favorite { String label="Camera da letto",entity="light.camera"; };
    std::array<Favorite,8> favorites{};
} settings;
int setting=0,settingPreview=-1,systemItem=0,batteryLevel=82;
bool charging=false;
constexpr int WL_CONNECTED=1;
struct { int status() { return WL_CONNECTED; } } WiFi;
struct { bool relayReady() { return false; } } companion;
uint32_t surfaceColor=ui::black,foregroundColor=ui::white;
size_t favorite=0;int casaHoldProgress=0;
std::array<String,8> states{"on"},units{};
std::array<int,8> lightLevels{62};
bool fresh=true;
bool favoriteFresh() { return fresh; }
bool favoriteCanDim() { return fresh; }
struct { bool pending(size_t) { return false; } } homeCommand;
struct { bool linked=true;bool connected() { return linked; } bool busy() { return false; } bool available() { return linked; } } mqtt,home;
void text(const String& value,int y,uint32_t color,int size) { ui::label(canvas,value,233,y,size==2?ui::Font::Body:ui::Font::Small,color,surfaceColor,310); }
casa::LightLevel lightDraft;String lightEntity="light.camera";size_t lightFavorite=0;
// Only the numeric Arduino String constructor is used by these extracted views.
#define String(value) std::to_string(value)
#include "scene_settings.inc"
#undef String

int main(int argc,char** argv) {
    if(argc<2 || argc>3) return 1;
    i18n::italian = argc==3 && std::string(argv[2])=="it";
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    const auto frame=[&](const String& name,const std::function<void()>& draw) {
        output.open(root/(name+".jsonl"));draw();output.close();
    };
    ui::HomeMenu ring;
    for(ring.selected=0;ring.selected<ring.count();++ring.selected)
        frame("scene-"+std::to_string(ring.selected),[&] { scene_ui::home(canvas,ring,180);drawHomeClock(); });
    for(int direction:{-1,1}) {
        ui::HomeMenu swipe;swipe.nextApp(direction);
        for(int age:{0,90,179,180,220}) {
            frame("home-swipe-"+std::to_string(direction)+"-"+std::to_string(age),[&] { scene_ui::home(canvas,swipe,age);drawHomeClock(); });
        }
    }
    ui::HomeMenu custom;std::array<int,launcher::appCount> order{launcher::system};custom.configure(order,1);
    frame("home-only-system",[&] { scene_ui::home(canvas,custom,180);drawHomeClock(); });
    frame("system",[] { drawSystem(); });
    systemItem=2;frame("system-2",[] { drawSystem(); });
    systemItem=4;frame("system-4",[] { drawSystem(); });
    for(setting=0;setting<7;++setting) frame("slider-"+std::to_string(setting),[] { drawSettings(); });
    ui::HomeMenu menu;charging=true;
    frame("charging",[&] { scene_ui::home(canvas,menu,180);drawHomeClock();scene_ui::notice(canvas,i18n::text("In carica"),std::string("82")+i18n::text("% · USB collegata"),ui::Icon::Charging); });
    frame("notice",[&] { scene_ui::home(canvas,menu,180);drawHomeClock();scene_ui::notice(canvas,"Configurazione","salvata",ui::Icon::Activity); });
    for(int theme=0;theme<6;++theme) {
        ui::accent=ui::themes[theme].accent;ui::navigation=ui::themes[theme].navigation;
        ui::focus=ui::themes[theme].focus;surfaceColor=ui::focus;foregroundColor=ui::ink;
        ui::selectionSurface=ui::themes[theme].selectionSurface;
        frame("notice-theme-"+std::to_string(theme),[&] {
            canvas.fillRect(0,0,466,466,ui::themes[theme].focus);drawStatus();
            scene_ui::notice(canvas,"Stato aggiornato","Camera da letto",ui::Icon::Check);
        });
    }
    surfaceColor=ui::black;foregroundColor=ui::white;
    for(int theme=1;theme<6;++theme) {
        ui::accent=ui::themes[theme].accent;ui::navigation=ui::themes[theme].navigation;
        ui::selectionSurface=ui::themes[theme].selectionSurface;
        frame("scene-theme-"+std::to_string(theme),[&] { scene_ui::home(canvas,menu,180);drawHomeClock(); });
    }
    ui::accent=ui::themes[2].accent;ui::navigation=ui::themes[2].navigation;
    surfaceColor=ui::focus;foregroundColor=ui::ink;
    frame("casa-light",[] { canvas.fillRect(0,0,466,466,surfaceColor);drawCasa(); });
    fresh=false;mqtt.linked=false;
    frame("casa-cached-offline",[] { canvas.fillRect(0,0,466,466,surfaceColor);drawCasa(); });
    fresh=true;mqtt.linked=true;
    casaHoldProgress=50;frame("casa-light-hold",[] { canvas.fillRect(0,0,466,466,surfaceColor);drawCasa(); });
    casaHoldProgress=0;states[0]="off";surfaceColor=ui::black;foregroundColor=ui::white;
    frame("casa-light-off",[] { canvas.fillRect(0,0,466,466,surfaceColor);drawCasa(); });
    for(int level:{0,62,100}) {
        lightDraft.begin(level);
        frame("light-"+std::to_string(level),[] { drawStatus();drawLight(); });
    }

}
