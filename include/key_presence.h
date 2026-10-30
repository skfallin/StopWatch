#pragma once
#include <cstdint>

namespace keylab {
// A request never inherits a button already held before its prompt became visible.
class Presence {
public:
    void begin(uint32_t id,uint32_t now) { id_=id;started_=now;armed_=pressed_=false; }
    enum class Result { Waiting, Confirm, Cancel, Timeout };
    Result update(uint32_t now,bool a,bool b,bool allowed) {
        if(!id_ || !allowed || a) return Result::Cancel;
        if(uint32_t(now-started_)>=25000) return Result::Timeout;
        if(!armed_) { if(!a && !b) armed_=true;return Result::Waiting; }
        if(b) pressed_=true;
        if(pressed_ && !b) { id_=0;return Result::Confirm; }
        return Result::Waiting;
    }
private:
    uint32_t id_=0,started_=0;
    bool armed_=false,pressed_=false;
};
}
