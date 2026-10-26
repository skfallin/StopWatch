#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace clockwork {
enum class Face { Digital, Analog, Halo, Duo, Orbit, Segments };
inline constexpr std::array<const char*,6> faceIds={"digital","analog","halo","duo","orbit","segments"};
inline constexpr std::array<const char*,6> faceNames={"Digitale","Analogico","Halo","Doppio","Orbita","Segmenti"};
inline int faceIndex(std::string_view id) {
    for(size_t i=0;i<faceIds.size();++i) if(id==faceIds[i]) return int(i);
    return -1;
}
inline constexpr std::array<uint32_t,6> presets={60,180,300,600,900,1500};
inline constexpr uint32_t maxTimerSeconds=86399;

class Timer {
public:
    enum class State { Idle, Running, Paused, Done };
    State state() const { return state_; }
    uint32_t duration() const { return duration_; }
    uint32_t remaining() const { return remaining_; }
    uint32_t remainingSeconds() const { return (remaining_+999)/1000; }
    bool setSeconds(uint32_t seconds) {
        if(state_==State::Running || state_==State::Paused || !seconds || seconds>maxTimerSeconds) return false;
        duration_=seconds*1000;reset();return true;
    }
    bool adjust(unsigned field,int direction) {
        if(state_!=State::Idle || field>2 || (direction!=1 && direction!=-1)) return false;
        const int units[]={3600,60,1};
        const int seconds=duration_/1000;
        const int current=field==0?seconds/3600:(seconds/units[field])%60;
        const int next=std::clamp(current+direction,0,field==0?23:59);
        return setSeconds(std::max(1,seconds+(next-current)*units[field]));
    }
    bool update(uint32_t now) {
        const uint32_t delta=now-last_; last_=now;
        if(state_!=State::Running) return false;
        if(delta>=remaining_) { remaining_=0;state_=State::Done;return true; }
        remaining_-=delta;return false;
    }
    void toggle(uint32_t now) {
        if(update(now)) return;
        if(state_==State::Done) { reset();return; }
        if(state_==State::Idle) remaining_=duration_;
        state_=state_==State::Running?State::Paused:State::Running; last_=now;
    }
    void reset() { remaining_=duration_;state_=State::Idle;alerts_=0; }
    void restart(uint32_t now) { reset();toggle(now); }
    unsigned alert(uint32_t now,bool audioBusy) {
        if(state_!=State::Done || audioBusy || alerts_>=4) return 0;
        if(alerts_ && uint32_t(now-alertAt_)<1200) return 0;
        alertAt_=now;return ++alerts_;
    }
private:
    State state_=State::Idle;
    uint32_t duration_=300000,remaining_=300000,last_=0;
    uint32_t alertAt_=0;
    unsigned alerts_=0;
};

class Stopwatch {
public:
    struct Lap { uint32_t number=0; uint64_t split=0; };
    static constexpr uint64_t maxElapsed=100ull*60*60*1000-10;
    bool running() const { return running_; }
    uint64_t elapsed() const { return elapsed_; }
    size_t lapCount() const { return lapCount_; }
    const Lap& lap(size_t index) const { return laps_[index]; }
    void update(uint32_t now) {
        const uint32_t delta=now-last_;last_=now;
        if(!running_) return;
        elapsed_=std::min(maxElapsed,elapsed_+delta);
        if(elapsed_==maxElapsed) running_=false;
    }
    void toggle(uint32_t now) {
        update(now);
        if(elapsed_==maxElapsed) return;
        running_=!running_;last_=now;
    }
    bool mark(uint32_t now) {
        update(now);
        if(!running_ || elapsed_==lastLap_) return false;
        for(size_t i=laps_.size()-1;i>0;--i) laps_[i]=laps_[i-1];
        laps_[0]={++number_,elapsed_-lastLap_};lastLap_=elapsed_;
        lapCount_=std::min(lapCount_+1,laps_.size());return true;
    }
    bool reset() {
        if(running_) return false;
        elapsed_=lastLap_=0;lapCount_=number_=0;return true;
    }
private:
    bool running_=false;
    uint32_t last_=0,number_=0;
    uint64_t elapsed_=0,lastLap_=0;
    size_t lapCount_=0;
    std::array<Lap,3> laps_{};
};
}
