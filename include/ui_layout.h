#pragma once
#include "i18n.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "ui_sounds.h"
#include "home_layout.h"

namespace ui {
inline bool confirmationReady(uint32_t now, uint32_t openedAt) { return uint32_t(now-openedAt)>=350; }
enum class Screen { Home, System, SettingsMenu, SettingsSection, Settings, Diagnostics, HomeAssistant, Notes, NoteSaved, Badge,
                    Pairing, Companion, ActionConfirm, FormatConfirm, ForgetConfirm, ClockMenu, Clock, Timer, Stopwatch, Spotify, MusicSources, MusicMenu, MusicPlaylists, UsbControl, SecurityKey, Light, LightConfirm, Focus, Slides };
enum class Action { None, Back, Select, Next, Previous, Open, Record, Play, Stop, Slider, Increase, Decrease, Secondary };
struct Target {
    Action action = Action::None;
    int index = 0;
    bool operator==(Target other) const { return action == other.action && index == other.index; }
};
struct Circle {
    int x, y, radius;
    bool contains(int px, int py) const { return (px-x)*(px-x) + (py-y)*(py-y) <= radius*radius; }
};
struct Rect {
    int x, y, width, height;
    bool contains(int px, int py) const { return px>=x && px<x+width && py>=y && py<y+height; }
};
constexpr Rect casaCard{105,115,256,303};
constexpr Rect noticeCard{84,107,298,84};
constexpr Rect quickPanel{73,87,320,310}, quietToggle{97,211,272,112};
constexpr Circle quickClose{354,125,24};
inline bool quickOrigin(int x,int y) { return x>=167 && x<=299 && y>=0 && y<=64; }
constexpr int homeCount=launcher::appCount;
constexpr int clockActionY=347, clockTabsY=407;
struct HomeMenu {
    int space=1, selected=0;
    bool apps=false;
    std::array<int,launcher::appCount> order=launcher::defaultOrder;
    size_t enabled=launcher::appCount;
    int count(int group) const {
        int result=0;for(size_t i=0;i<enabled;++i) if(launcher::groups[order[i]]==group) ++result;
        return result;
    }
    int count() const { return count(space); }
    int pageCount() const { return int(count(0)>0)+int(count(1)>0)+int(count(2)>0); }
    int visibleCount() const { return std::min(3,count()); }
    int app(int slot=0) const {
        int target=(selected+slot)%std::max(1,count());
        for(size_t i=0;i<enabled;++i) if(launcher::groups[order[i]]==space && target--==0) return order[i];
        return 7;
    }
    void configure(const std::array<int,launcher::appCount>& items,size_t size) {
        const bool changed=order!=items || enabled!=size;
        const int previous=app();order=items;enabled=size;
        if(!count()) nextSpace(1);
        selected=0;if(!changed) for(int i=0;i<count();++i) if(app(i)==previous) { selected=i;break; }
    }
    void nextSpace(int direction) {
        do { space=(space+3+direction)%3; } while(!count());
        selected=0;apps=false;
    }
    void nextApp(int direction) { selected=(selected+count()+direction)%count(); }
    Circle circle(int slot) const {
        if(visibleCount()==1) return {233,235,100};
        if(slot==0) return {233,visibleCount()==2?174:188,visibleCount()==2?79:84};
        return {visibleCount()==2?233:slot==1?130:336,visibleCount()==2?318:309,54};
    }
    Circle arrow(bool next) const { return {next?389:77,visibleCount()==2?206:211,36}; }
};
constexpr Circle settingsPrevious{77,264,36}, settingsNext{389,264,36};
constexpr Circle pagePrevious{154,387,36}, pageNext{312,387,36};
constexpr Rect menuRow(int slot) { return {84,129+slot*112,298,91}; }
constexpr Rect settingsRow(int slot) { return {84,125+slot*91,298,77}; }
constexpr int settingsItems[3][4]={{0,5,0,0},{1,6,2,4},{3,0,0,0}};
inline int settingsSection(int setting) { return setting==0 || setting==5?0:setting==3?2:1; }
inline int settingsCount(int section) { return section==0?2:section==1?4:1; }
inline int settingsPosition(int setting) {
    const int section=settingsSection(setting);
    for(int i=0;i<settingsCount(section);++i) if(settingsItems[section][i]==setting) return i;
    return 0;
}
constexpr Circle backButton{112,84,32}, previousButton{46,264,31}, nextButton{420,264,31};
constexpr Circle recordButton{233,250,68}, stopButton{233,382,44};
constexpr Circle musicPrevious{151,410,28}, musicPlay{233,416,33}, musicNext{315,410,28};
constexpr Circle playlistPrevious{120,399,26},playlistNext{346,399,26};
struct StatusLayout { int dateX, wifiX, batteryX, batteryIconX; };
inline StatusLayout statusLayout(int dateWidth, int batteryWidth) {
    int x=233-(dateWidth+batteryWidth+86)/2;
    const int dateX=x+dateWidth/2;
    x+=dateWidth+18;
    const int wifiX=x+9;
    x+=18+16;
    const int batteryX=x+batteryWidth/2;
    return {dateX,wifiX,batteryX,x+batteryWidth+8};
}
inline bool rectangle(int x, int y, int left, int top, int width, int height) {
    return x >= left && x < left+width && y >= top && y < top+height;
}
inline Target hit(Screen screen, int x, int y, int selected, bool recording = false, bool badgeControls = false, bool clockControls = false, const HomeMenu& menu = {},bool lightControls=false) {
    if (x<0 || y<0 || x>=466 || y>=466) return {};
    if (!Circle{233,233,232}.contains(x,y)) return {};
    if (recording) return stopButton.contains(x,y) ? Target{Action::Stop} : Target{};
    if (screen != Screen::Home && (screen!=Screen::Badge || badgeControls) && (screen!=Screen::Clock || clockControls) && backButton.contains(x,y)) return {Action::Back};
    if(screen==Screen::Timer || screen==Screen::Stopwatch || (screen==Screen::Clock && clockControls)) {
        for(int i=0;i<3;++i) if(Rect{109+i*84,clockTabsY-23,80,46}.contains(x,y)) return {Action::Open,i};
    }
    switch (screen) {
    case Screen::Focus:
        if(Rect{109,330,248,56}.contains(x,y)) return {Action::Select};
        if(Rect{109,389,248,46}.contains(x,y)) return {Action::Secondary};
        break;
    case Screen::Slides:
        if(Circle{149,326,42}.contains(x,y)) return {Action::Previous};
        if(Circle{317,326,42}.contains(x,y)) return {Action::Next};
        if(Rect{118,380,230,56}.contains(x,y)) return {Action::Select};
        break;
    case Screen::Light:
        if(Rect{123,384,220,56}.contains(x,y)) return {Action::Select};
        if(Rect{84,310,298,66}.contains(x,y)) return {Action::Slider};
        break;
    case Screen::LightConfirm:
        if(Rect{100,303,115,56}.contains(x,y)) return {Action::Back};
        if(Rect{252,303,115,56}.contains(x,y)) return {Action::Select};
        break;
    case Screen::SecurityKey:
        break; // Physical B is the only confirmation input.
    case Screen::UsbControl:
        if(Rect{113,342,240,60}.contains(x,y)) return {Action::Select};
        break;
    case Screen::MusicMenu:
        for(int i=0;i<2;++i) if(Rect{85,145+i*92,296,80}.contains(x,y)) return {Action::Open,i};
        break;
    case Screen::MusicPlaylists: {
        const int first=selected>1?selected-1:0;
        if(selected>=0) {
            for(int i=0;i<3;++i) if(Rect{85,140+i*77,296,62}.contains(x,y)) return {Action::Open,first+i};
            if(playlistPrevious.contains(x,y)) return {Action::Decrease};
            if(playlistNext.contains(x,y)) return {Action::Increase};
        }
        if(Rect{163,371,140,56}.contains(x,y)) return {Action::Select};
        break;
    }
    case Screen::Spotify:
        if(Rect{139,111,188,188}.contains(x,y)) return {Action::Select};
        if(musicPrevious.contains(x,y)) return {Action::Previous};
        if(musicPlay.contains(x,y)) return {Action::Select};
        if(musicNext.contains(x,y)) return {Action::Next};
        if(Rect{148,53,190,55}.contains(x,y)) return {Action::Open,0};
        break;
    case Screen::MusicSources: {
        const int first=selected>1?selected-1:0;
        for(int i=0;i<3;++i) if(Rect{85,134+i*77,296,62}.contains(x,y)) return {Action::Open,first+i};
        if(Rect{124,371,218,56}.contains(x,y)) return {Action::Select};
        break;
    }
    case Screen::Home:
        for(int i=0;i<menu.visibleCount();++i) if(menu.circle(i).contains(x,y)) return {Action::Open,i};
        if(menu.pageCount()>1 && menu.arrow(false).contains(x,y)) return {Action::Previous};
        if(menu.pageCount()>1 && menu.arrow(true).contains(x,y)) return {Action::Next};
        break;
    case Screen::ClockMenu:
        for(int i=0;i<3;++i) if(Rect{85,132+i*86,296,72}.contains(x,y)) return {Action::Open,i};
        break;
    case Screen::Clock:
        if(clockControls && Circle{354,84,28}.contains(x,y)) return {Action::Select};
        return {Action::Open,3};
    case Screen::Timer:
        if(selected==0) {
            for(int i=0;i<3;++i) {
                if(Circle{137+i*96,143,25}.contains(x,y)) return {Action::Increase,i};
                if(Circle{137+i*96,283,25}.contains(x,y)) return {Action::Decrease,i};
            }
            if(Rect{133,clockActionY-28,200,56}.contains(x,y)) return {Action::Select};
        } else {
            if(Rect{241,clockActionY-28,130,56}.contains(x,y)) return {Action::Select};
            if(Rect{95,clockActionY-28,130,56}.contains(x,y)) return {Action::Secondary};
        }
        break;
    case Screen::Stopwatch:
        if(Rect{241,clockActionY-28,130,56}.contains(x,y)) return {Action::Select};
        if(Rect{95,clockActionY-28,130,56}.contains(x,y)) return {Action::Secondary};
        break;
    case Screen::Badge:
        if(badgeControls && rectangle(x,y,113,295,240,56)) return {Action::Open,1};
        if(badgeControls && rectangle(x,y,113,361,240,56)) return {Action::Select};
        return {Action::Open,0};
    case Screen::System:
        for(int i=0;i<2;++i) if(menuRow(i).contains(x,y)) return {Action::Open,selected/2*2+i};
        if(pagePrevious.contains(x,y)) return {Action::Previous};
        if(pageNext.contains(x,y)) return {Action::Next};
        break;
    case Screen::SettingsMenu:
        for(int i=0;i<3;++i) if(settingsRow(i).contains(x,y)) return {Action::Open,settingsItems[i][0]};
        break;
    case Screen::SettingsSection: {
        const int section=settingsSection(selected),first=settingsPosition(selected)/2*2;
        for(int i=0;i<2 && first+i<settingsCount(section);++i)
            if(menuRow(i).contains(x,y)) return {Action::Open,settingsItems[section][first+i]};
        if(settingsCount(section)>2) {
            if(pagePrevious.contains(x,y)) return {Action::Previous};
            if(pageNext.contains(x,y)) return {Action::Next};
        }
        break;
    }
    case Screen::HomeAssistant:
        if(lightControls && Rect{145,360,176,58}.contains(x,y)) return {Action::Open,99};
        if (previousButton.contains(x,y)) return {Action::Previous};
        if (nextButton.contains(x,y)) return {Action::Next};
        if (Circle{233,264,110}.contains(x,y)) return {Action::Select};
        break;
    case Screen::Notes:
        if (recordButton.contains(x,y)) return {Action::Record};
        if (rectangle(x,y,118,355,230,63)) return {Action::Play};
        break;
    case Screen::Settings:
        if (rectangle(x,y,84,310,298,66)) return {Action::Slider};
        if (settingsPrevious.contains(x,y)) return {Action::Previous};
        if (settingsNext.contains(x,y)) return {Action::Next};
        break;
    case Screen::Diagnostics:
        if (previousButton.contains(x,y)) return {Action::Previous};
        if (nextButton.contains(x,y)) return {Action::Next};
        if (rectangle(x,y,138,365,190,56)) return {Action::Select};
        break;
    case Screen::Pairing:
        if (rectangle(x,y,85,280,296,62)) return {Action::Open,0};
        if (rectangle(x,y,85,351,296,62)) return {Action::Open,1};
        break;
    case Screen::Companion:
        if (rectangle(x,y,130,129,206,204) || rectangle(x,y,100,350,266,66)) return {Action::Select};
        break;
    case Screen::NoteSaved:
        if (rectangle(x,y,130,354,206,62)) return {Action::Select};
        break;
    case Screen::ActionConfirm:
        if(lightControls && Rect{113,365,240,52}.contains(x,y)) return {Action::Open,99};
        if (rectangle(x,y,100,300,115,62)) return {Action::Back};
        if (rectangle(x,y,251,300,115,62)) return {Action::Select};
        break;
    case Screen::FormatConfirm:
    case Screen::ForgetConfirm:
        // Destructive archive and bonding changes still require the physical B key.
        if (rectangle(x,y,100,350,266,66)) return {Action::Back};
        break;
    }
    return {};
}

inline int sliderValue(int setting, int x) {
    const int minimum = setting == 0 ? 32 : setting == 3 ? 15 : 0;
    const int maximum = (setting == 1 || setting == 6) ? 200 : setting == 3 ? 180 : setting == 4 ? int(sfx::packIds.size())-1 : setting == 5 ? 5 : 255;
    const int step = setting == 3 ? 15 : 1;
    const int value = minimum + ((std::clamp(x,109,357)-109)*(maximum-minimum)+(setting==4?124:0))/248;
    return std::clamp(((value+step/2)/step)*step,minimum,maximum);
}

enum class GestureKind { None, Tap, Left, Right, Drag, Release, QuickOpen, QuickClose, Up, HoldProgress, Hold, Down };
struct Gesture {
    GestureKind kind = GestureKind::None;
    int x=0, y=0, originX=0, originY=0;
    int progress=0;
};
class TouchInput {
public:
    void suppressUntilReleased() { suppressed_ = true; }
    Gesture update(uint32_t now, bool pressed, int x, int y, bool slider, bool quickPanelOpen=false, bool hold=false) {
        if (pressed && (x<0 || y<0 || x>=466 || y>=466)) suppressed_=true;
        if (!active_ && !pressed) { suppressed_=false; return {}; }
        if (!active_) {
            active_=true; started_=now; originX_=lastX_=x; originY_=lastY_=y; slider_=slider; hold_=hold;
        }
        if (pressed) {
            const bool moved = x!=lastX_ || y!=lastY_;
            const bool cancelHold=hold_ && (std::abs(int64_t(x)-originX_)>20 || std::abs(int64_t(y)-originY_)>20 ||
                (x-originX_)*(x-originX_)+(y-originY_)*(y-originY_)>400);
            if(cancelHold) hold_=false;
            lastX_=x; lastY_=y;
            if (slider_ && !suppressed_ && (moved || now==started_)) return {GestureKind::Drag,x,y,originX_,originY_};
            if(cancelHold && !suppressed_) return {GestureKind::HoldProgress,x,y,originX_,originY_,0};
            if(hold_ && !suppressed_) {
                const int progress=int(std::min(uint32_t(650),uint32_t(now-started_))*100/650);
                if(progress==100) { suppressed_=true;return {GestureKind::Hold,x,y,originX_,originY_,100}; }
                return {GestureKind::HoldProgress,x,y,originX_,originY_,progress};
            }
            return {};
        }
        active_=false;
        if (suppressed_) { suppressed_=false; return {}; }
        const int dx=lastX_-originX_, dy=lastY_-originY_;
        Gesture result{GestureKind::None,lastX_,lastY_,originX_,originY_};
        if (slider_) result.kind=GestureKind::Release;
        else if (std::abs(dy)>=52 && std::abs(dy)*10>std::abs(dx)*14) {
            if(dy>0 && quickOrigin(originX_,originY_)) result.kind=GestureKind::QuickOpen;
            else if(dy<0 && quickPanelOpen) result.kind=GestureKind::QuickClose;
            else if(originY_>=110) result.kind=dy<0?GestureKind::Up:GestureKind::Down;
        }
        else if (std::abs(dx)>=52 && std::abs(dx)*10>std::abs(dy)*14) result.kind=dx<0?GestureKind::Left:GestureKind::Right;
        else if (dx*dx+dy*dy<=400 && uint32_t(now-started_)<=900) result.kind=GestureKind::Tap;
        return result;
    }
private:
    bool active_=false, suppressed_=false, slider_=false, hold_=false;
    uint32_t started_=0;
    int originX_=0, originY_=0, lastX_=0, lastY_=0;
};
}
