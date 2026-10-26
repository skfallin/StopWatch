#pragma once

#include <cstdint>

struct InputEvents {
    bool next = false;
    bool select = false;
    bool back = false;
    bool home = false;
    bool recordStart = false;
    bool recordStop = false;
};

class Controls {
public:
    void suppressTapUntilReleased() { suppressTap_ = true; }

    InputEvents update(uint32_t now, bool a, bool b) {
        InputEvents events;
        if (a && !previousA_) {
            startedA_ = now;
            handledA_ = false;
        }
        if (b && !previousB_) {
            startedB_ = now;
            handledB_ = false;
        }
        if (a && b && !chord_) {
            chord_ = true;
            startedChord_ = now;
            handledA_ = handledB_ = true;
            if (recording_) {
                events.recordStop = true;
                recording_ = false;
            }
        }
        if (chord_) {
            if (a && b && !homeSent_ && uint32_t(now - startedChord_) >= 800) {
                events.home = true;
                homeSent_ = true;
            }
            if (!a && !b) {
                chord_ = homeSent_ = false;
            }
        } else {
            if (a && !handledA_ && uint32_t(now - startedA_) >= 650) {
                events.back = true;
                handledA_ = true;
            }
            if (b && !handledB_ && uint32_t(now - startedB_) >= 650) {
                events.recordStart = true;
                handledB_ = recording_ = true;
            }
            if (!a && previousA_ && !handledA_ && !suppressTap_) {
                events.next = true;
            }
            if (!b && previousB_) {
                if (recording_) {
                    events.recordStop = true;
                    recording_ = false;
                } else if (!handledB_ && !suppressTap_) {
                    events.select = true;
                }
            }
        }
        previousA_ = a;
        previousB_ = b;
        if (!a && !b) suppressTap_ = false;
        return events;
    }

private:
    uint32_t startedA_ = 0, startedB_ = 0, startedChord_ = 0;
    bool previousA_ = false, previousB_ = false;
    bool handledA_ = false, handledB_ = false;
    bool chord_ = false, homeSent_ = false, recording_ = false, suppressTap_ = false;
};
