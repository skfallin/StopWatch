#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace casa {
constexpr uint32_t refreshMs=2000, freshMs=10000, verifyMs=12000;

class ToggleVerification {
public:
    void begin(size_t favorite, bool currentlyOn, uint32_t now,int brightness=-1) {
        favorite_=favorite; expectedOn_=brightness>=0?brightness>0:!currentlyOn; expectedBrightness_=brightness; started_=now; active_=true;
    }
    bool pending() const { return active_; }
    bool pending(size_t favorite) const { return active_ && favorite_==favorite; }
    size_t favorite() const { return favorite_; }
    bool observe(size_t favorite, std::string_view state,int brightness=-1) {
        if(!pending(favorite) || state!=(expectedOn_?"on":"off")) return false;
        if(expectedOn_ && expectedBrightness_>=0 && (brightness<expectedBrightness_-2 || brightness>expectedBrightness_+2)) return false;
        active_=false;
        return true;
    }
    bool expired(uint32_t now) const { return active_ && uint32_t(now-started_)>=verifyMs; }
    void clear() { active_=false; }
private:
    size_t favorite_=0;
    int expectedBrightness_=-1;
    uint32_t started_=0;
    bool expectedOn_=false, active_=false;
};
}
