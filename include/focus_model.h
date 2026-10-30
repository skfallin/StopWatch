#pragma once
#include "clock_model.h"

namespace clockwork {
class Focus {
public:
    enum class Phase { Work, ShortBreak, LongBreak };
    const Timer& timer() const { return timer_; }
    Phase phase() const { return phase_; }
    unsigned sessions() const { return sessions_; }
    bool update(uint32_t now) {
        if(!timer_.update(now)) return false;
        if(phase_==Phase::Work) ++sessions_;
        return true;
    }
    void toggle(uint32_t now) {
        // Account for a deadline reached between the last frame and this tap.
        update(now);
        if(timer_.state()==Timer::State::Done) {
            phase_=phase_==Phase::Work?(sessions_%4==0?Phase::LongBreak:Phase::ShortBreak):Phase::Work;
            timer_.setSeconds(phase_==Phase::Work?1500:phase_==Phase::LongBreak?900:300);
        }
        timer_.toggle(now);
    }
    bool reset() {
        if(timer_.state()==Timer::State::Running) return false;
        sessions_=0;phase_=Phase::Work;timer_.reset();timer_.setSeconds(1500);return true;
    }
    unsigned alert(uint32_t now,bool busy) { return timer_.alert(now,busy); }
    Focus() { timer_.setSeconds(1500); }
private:
    Timer timer_;
    Phase phase_=Phase::Work;
    unsigned sessions_=0;
};
}
