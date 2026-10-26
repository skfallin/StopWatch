#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace ui {
inline bool confirmationReady(uint32_t now, uint32_t openedAt) { return uint32_t(now-openedAt)>=350; }
enum class Screen { Home, System, Settings, Diagnostics, HomeAssistant, Notes, NoteSaved, Badge,
                    Pairing, Companion, ActionConfirm, FormatConfirm, ForgetConfirm, ClockMenu, Clock, Timer, Stopwatch, Spotify, MusicSources, MusicMenu, MusicPlaylists };
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
constexpr Rect noticeCard{91,325,284,64};
constexpr Rect quickPanel{73,87,320,292}, quietToggle{97,160,272,164};
constexpr Circle quickClose{354,125,24};
inline bool quickOrigin(int x,int y) { return x>=167 && x<=299 && y>=0 && y<=64; }
constexpr int homeCount=6;
constexpr int clockActionY=347, clockTabsY=407;
constexpr Circle homeMain{233,233,88}, homeLeft{120,351,50}, homeRight{346,351,50};
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
inline Target hit(Screen screen, int x, int y, int selected, bool recording = false, bool badgeControls = false, bool clockControls = false) {
    if (x<0 || y<0 || x>=466 || y>=466) return {};
    if (!Circle{233,233,232}.contains(x,y)) return {};
    if (recording) return stopButton.contains(x,y) ? Target{Action::Stop} : Target{};
    if (screen != Screen::Home && (screen!=Screen::Badge || badgeControls) && (screen!=Screen::Clock || clockControls) && backButton.contains(x,y)) return {Action::Back};
    if(screen==Screen::Timer || screen==Screen::Stopwatch || (screen==Screen::Clock && clockControls)) {
        for(int i=0;i<3;++i) if(Rect{109+i*84,clockTabsY-23,80,46}.contains(x,y)) return {Action::Open,i};
    }
    switch (screen) {
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
        if (homeMain.contains(x,y)) return {Action::Open, selected};
        if (homeLeft.contains(x,y)) return {Action::Open, (selected+1)%homeCount};
        if (homeRight.contains(x,y)) return {Action::Open, (selected+homeCount-1)%homeCount};
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
        for (int i=0;i<4;++i) if (rectangle(x,y,85,121+i*71,296,62)) return {Action::Open,i};
        break;
    case Screen::HomeAssistant:
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
        if (previousButton.contains(x,y)) return {Action::Previous};
        if (nextButton.contains(x,y)) return {Action::Next};
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
    const int maximum = (setting == 1 || setting == 6) ? 200 : setting == 3 ? 180 : setting == 4 ? 12 : setting == 5 ? 5 : 255;
    const int step = setting == 3 ? 15 : 1;
    const int value = minimum + (std::clamp(x,109,357)-109)*(maximum-minimum)/248;
    return std::clamp(((value+step/2)/step)*step,minimum,maximum);
}

enum class GestureKind { None, Tap, Left, Right, Drag, Release, QuickOpen, QuickClose };
struct Gesture {
    GestureKind kind = GestureKind::None;
    int x=0, y=0, originX=0, originY=0;
};
class TouchInput {
public:
    void suppressUntilReleased() { suppressed_ = true; }
    Gesture update(uint32_t now, bool pressed, int x, int y, bool slider, bool quickPanelOpen=false) {
        if (pressed && (x<0 || y<0 || x>=466 || y>=466)) suppressed_=true;
        if (!active_ && !pressed) { suppressed_=false; return {}; }
        if (!active_) {
            active_=true; started_=now; originX_=lastX_=x; originY_=lastY_=y; slider_=slider;
        }
        if (pressed) {
            const bool moved = x!=lastX_ || y!=lastY_;
            lastX_=x; lastY_=y;
            if (slider_ && !suppressed_ && (moved || now==started_)) return {GestureKind::Drag,x,y,originX_,originY_};
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
        }
        else if (std::abs(dx)>=52 && std::abs(dx)*10>std::abs(dy)*14) result.kind=dx<0?GestureKind::Left:GestureKind::Right;
        else if (dx*dx+dy*dy<=400 && uint32_t(now-started_)<=900) result.kind=GestureKind::Tap;
        return result;
    }
private:
    bool active_=false, suppressed_=false, slider_=false;
    uint32_t started_=0;
    int originX_=0, originY_=0, lastX_=0, lastY_=0;
};
}
