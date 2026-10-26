#include "music_view.h"
#include <filesystem>
#include <fstream>
#include <functional>

namespace ui {
void label(M5Canvas& c,const String& value,int x,int y,Font font,uint32_t color,uint32_t bg,int width) {
    c.text(value,x,y,int(font),color,bg,width);
}
#include "drawing_primitives.inc"
}

int main(int argc,char** argv) {
    if(argc!=2) return 1;
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    const auto frame=[&](const String& name,const std::function<void(M5Canvas&)>& draw) {
        std::ofstream output(root/(name+".jsonl"));M5Canvas canvas(output);draw(canvas);
    };
    music::Playback p;
    p.title="Midnight Drive";p.artist="Lofi Chill";p.source="MacBook di Francesco";
    p.sources={"iPhone di Francesco","MacBook di Francesco","Salotto"};
    p.available=p.playing=p.canPlay=p.canPause=p.canNext=p.canPrevious=p.canSource=true;
    p.durationMs=242000;p.positionMs=87000;p.observedAt=1000;
    frame("playing",[&](M5Canvas& c){ music_ui::player(c,p,1000,false,""); });
    p.playing=false;
    frame("paused",[&](M5Canvas& c){ music_ui::player(c,p,1000,false,""); });
    frame("devices",[&](M5Canvas& c){ music_ui::sources(c,p,1,false); });
    frame("offline",[&](M5Canvas& c){ music_ui::player(c,p,30000,false,"MQTT non connesso"); });
    frame("waiting",[&](M5Canvas& c){ music_ui::player(c,p,1000,true,""); });
    p=music::Playback{};
    frame("empty",[&](M5Canvas& c){ music_ui::player(c,p,1000,false,"Configura Spotify in HA"); });
    frame("music-menu",[&](M5Canvas& c){ music_ui::menu(c,0); });
    music::PlaylistPage page;page.loaded=true;page.total=19;
    for(const auto* name:{"Brianstorm","La mia giornata","Focus","In viaggio","Preferiti","Sera","Classici","Weekend"}) page.items.push_back({name,"spotify:playlist:1234567890abcdefghijkl"});
    frame("playlists",[&](M5Canvas& c){ music_ui::playlists(c,page,0,false,""); });
    frame("playlists-scroll",[&](M5Canvas& c){ music_ui::playlists(c,page,4,false,""); });
    page.offset=16;page.items.resize(3);
    frame("playlists-last",[&](M5Canvas& c){ music_ui::playlists(c,page,1,false,""); });
    page=music::PlaylistPage{};
    frame("playlists-loading",[&](M5Canvas& c){ music_ui::playlists(c,page,0,true,""); });
    frame("playlists-empty",[&](M5Canvas& c){ music_ui::playlists(c,page,0,false,"Nessuna playlist salvata"); });
}
