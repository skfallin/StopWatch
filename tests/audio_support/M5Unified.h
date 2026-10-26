#pragma once
#include "Arduino.h"
#include <vector>
struct TestSpeaker {
    bool playing=false;
    unsigned volume=0,tones=0,sampleRate=0;
    std::vector<int16_t> pcm;
    bool begin() { return true; }
    void end() { playing=false; }
    bool isPlaying() const { return playing; }
    void setVolume(unsigned value) { volume=value; }
    void tone(unsigned,unsigned) { ++tones;playing=true; }
    bool playRaw(const int16_t* samples,size_t count,unsigned rate,bool,unsigned,int,bool) {
        pcm.assign(samples,samples+count);sampleRate=rate;playing=true;return true;
    }
};
struct TestM5 { TestSpeaker Speaker;struct { void end() {} } Mic; };
inline TestM5 M5;
