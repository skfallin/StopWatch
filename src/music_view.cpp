#include "music_view.h"
#include "spotify_icon.h"
#include <cstdio>

namespace music_ui {
namespace {
String timeLabel(uint32_t ms) {
    char result[16];
    snprintf(result,sizeof(result),"%lu:%02lu",(unsigned long)(ms/60000),(unsigned long)(ms/1000%60));
    return result;
}
void heading(M5Canvas& c,const String& title,uint32_t background=ui::black) {
    ui::circleButton(c,ui::backButton,ui::Icon::Back,24,ui::surface,ui::navigation);
    ui::label(c,title,245,75,ui::Font::Body,ui::white,background,230);
}
void skip(M5Canvas& c,ui::Circle target,bool next,bool enabled) {
    c.fillSmoothCircle(target.x,target.y,target.radius,ui::surface);
    ui::icon(c,next?ui::Icon::Next:ui::Icon::Back,target.x+(next?-2:2),target.y,24,enabled?ui::white:ui::line,ui::surface);
    c.drawWideLine(target.x+(next?9:-9),target.y-10,target.x+(next?9:-9),target.y+10,1.3,enabled?ui::white:ui::line);
}
}
void player(M5Canvas& c,const music::Playback& p,uint32_t now,bool busy,const String& message,bool artwork,uint32_t background) {
    heading(c,"Spotify",background);
    c.drawBitmap(173,63,spotifyIcon,24,24,ui::white);
    ui::icon(c,ui::Icon::Next,315,75,14,ui::muted,background);
    if(!artwork) {
        c.fillSmoothRoundRect(139,111,188,188,4,ui::selectionSurface);
        ui::icon(c,ui::Icon::Music,233,205,64,ui::accent,ui::selectionSurface);
    }
    const bool fresh=p.fresh(now);
    const String title=p.title.empty()?"La tua musica":String(p.title.c_str());
    ui::label(c,title,233,324,ui::Font::Label,ui::white,background,352);
    ui::label(c,p.artist.empty()?"Apri Spotify su un dispositivo":String(p.artist.c_str()),233,351,ui::Font::Small,ui::muted,background,350);
    if(p.durationMs) {
        c.drawWideLine(132,375,334,375,2,ui::line);
        const int x=132+int(uint64_t(p.position(now))*202/p.durationMs);
        if(x>132) c.drawWideLine(132,375,x,375,2,fresh?ui::accent:ui::muted);
        ui::label(c,timeLabel(p.position(now)),103,375,ui::Font::Small,ui::muted,background,54);
        ui::label(c,timeLabel(p.durationMs),363,375,ui::Font::Small,ui::muted,background,54);
    }
    skip(c,ui::musicPrevious,false,!busy && p.allows(music::Command::Previous,now));
    skip(c,ui::musicNext,true,!busy && p.allows(music::Command::Next,now));
    const bool enabled=!busy && p.allows(p.playing?music::Command::Pause:music::Command::Play,now);
    const uint32_t fill=enabled?ui::accent:ui::surface,ink=enabled?ui::ink:ui::muted;
    c.fillSmoothCircle(ui::musicPlay.x,ui::musicPlay.y,ui::musicPlay.radius,fill);
    if(busy) ui::label(c,"…",233,416,ui::Font::Label,ink,fill,50);
    else if(p.playing) {
        c.drawWideLine(227,407,227,425,2.3,ink);c.drawWideLine(239,407,239,425,2.3,ink);
    } else ui::icon(c,ui::Icon::Play,233,416,25,ink,fill);
    if(message.length()) ui::label(c,message,252,98,ui::Font::Small,ui::muted,background,214);
    else ui::label(c,p.source.empty()?"Nessun lettore attivo":String(p.source.c_str()),252,98,ui::Font::Small,ui::muted,background,214);
}
void sources(M5Canvas& c,const music::Playback& p,size_t selected,bool busy) {
    heading(c,"Dispositivi");
    if(p.sources.empty()) {
        ui::icon(c,ui::Icon::Speaker,233,210,64,ui::accent);
        ui::label(c,"Apri Spotify sul lettore",233,295,ui::Font::Body,ui::white);
        ui::label(c,"Poi torna qui per sceglierlo",233,327,ui::Font::Small,ui::muted);
        return;
    }
    const size_t first=selected>1?selected-1:0;
    for(size_t i=first;i<p.sources.size() && i<first+3;++i) {
        const int y=165+int(i-first)*77;
        const bool active=i==selected;
        const uint32_t bg=active?ui::selectionSurface:ui::surface;
        c.fillSmoothRoundRect(85,y-31,296,62,24,bg);
        ui::icon(c,ui::Icon::Speaker,116,y,23,active?ui::accent:ui::muted,bg);
        ui::label(c,p.sources[i].c_str(),245,y,ui::Font::Body,ui::white,bg,234);
    }
    ui::pill(c,233,399,218,busy?"Attendi…":"B · Scegli lettore");
}
void menu(M5Canvas& c,size_t selected) {
    heading(c,"Spotify");
    c.drawBitmap(173,63,spotifyIcon,24,24,ui::white);
    const char* titles[]={"Playlist","Dispositivi"};
    const ui::Icon icons[]={ui::Icon::Music,ui::Icon::Speaker};
    for(size_t i=0;i<2;++i) {
        const int y=185+int(i)*92;
        const uint32_t bg=i==selected?ui::selectionSurface:ui::surface;
        c.fillSmoothRoundRect(85,y-40,296,80,28,bg);
        ui::icon(c,icons[i],124,y,28,i==selected?ui::accent:ui::muted,bg);
        ui::label(c,titles[i],247,y,ui::Font::Label,ui::white,bg,208);
    }
    ui::pill(c,233,387,220,"A · Scorri  B · Apri");
}
void playlists(M5Canvas& c,const music::PlaylistPage& page,size_t selected,bool busy,const String& message) {
    heading(c,"Playlist");
    c.drawBitmap(173,63,spotifyIcon,24,24,ui::white);
    if(page.items.empty()) {
        ui::icon(c,ui::Icon::Music,233,218,60,ui::accent);
        ui::label(c,busy?"Caricamento…":page.loaded?"Nessuna playlist":"Apri la tua libreria",233,292,ui::Font::Label,ui::white);
        ui::label(c,message.length()?message:String("Le playlist salvate su Spotify"),233,328,ui::Font::Small,ui::muted,ui::black,300);
        ui::pill(c,233,399,140,busy?"Attendi…":"B · Riprova");
        return;
    }
    const size_t first=selected>1?selected-1:0;
    for(size_t i=first;i<page.items.size() && i<first+3;++i) {
        const int y=171+int(i-first)*77;
        const uint32_t bg=i==selected?ui::selectionSurface:ui::surface;
        c.fillSmoothRoundRect(85,y-31,296,62,24,bg);
        ui::icon(c,ui::Icon::Music,116,y,23,i==selected?ui::accent:ui::muted,bg);
        ui::label(c,page.items[i].name.c_str(),245,y,ui::Font::Body,ui::white,bg,234);
    }
    char count[32];snprintf(count,sizeof(count),"%lu / %lu",(unsigned long)(page.offset+selected+1),(unsigned long)page.total);
    ui::label(c,busy?"Caricamento…":count,233,117,ui::Font::Small,ui::muted,ui::black,210);
    ui::circleButton(c,ui::playlistPrevious,ui::Icon::Back,20,ui::surface,page.offset?ui::navigation:ui::line);
    ui::circleButton(c,ui::playlistNext,ui::Icon::Next,20,ui::surface,page.hasNext()?ui::navigation:ui::line);
    ui::pill(c,233,399,140,busy?"Attendi…":"B · Ascolta");
}
}
