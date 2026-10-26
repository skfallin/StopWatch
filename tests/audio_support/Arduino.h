#pragma once
#include <cstdint>
#include <cstdio>
#define PROGMEM
inline uint32_t fakeMillis=0;
inline uint32_t millis() { return fakeMillis; }
struct TestSerial {
    template<class... Args> void printf(const char* format,Args... args) { std::printf(format,args...); }
};
inline TestSerial Serial;
