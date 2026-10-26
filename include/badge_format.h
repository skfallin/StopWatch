#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace badge {
constexpr size_t side=466, frameBytes=side*side*2, maxFrames=120, maxBytes=3*1024*1024;
constexpr size_t headerBytes=16, entryBytes=12, digestBytes=32;
// HTTPS may refresh lastData after the UI sampled now; a newer write is not expired.
inline bool uploadExpired(uint32_t now, uint32_t lastData) { return int32_t(now-lastData)>120000; }
struct Frame { uint32_t offset=0, bytes=0, milliseconds=0; };
struct Info {
    uint32_t bytes=0, milliseconds=0;
    uint16_t count=0;
    std::array<Frame,maxFrames> frames{};
};
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
inline uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
inline bool header(const uint8_t* p, size_t n, size_t total, uint16_t& count) {
    if(n<headerBytes || total>maxBytes || total<headerBytes+entryBytes+digestBytes || std::memcmp(p,"SWB1",4) ||
       u16(p+4)!=side || u16(p+6)!=side || u16(p+10)!=0 || u32(p+12)!=total) return false;
    count=u16(p+8);
    return count>0 && count<=maxFrames && headerBytes+count*entryBytes+digestBytes<total;
}
inline bool parse(const uint8_t* p, size_t n, size_t total, Info& info) {
    Info next;
    if(!header(p,n,total,next.count) || n<headerBytes+next.count*entryBytes) return false;
    uint64_t end=headerBytes+next.count*entryBytes, duration=0;
    for(size_t i=0;i<next.count;++i) {
        const auto* row=p+headerBytes+i*entryBytes;
        auto& frame=next.frames[i];
        frame={u32(row),u32(row+4),u32(row+8)};
        if(frame.offset!=end || !frame.bytes || frame.bytes>frameBytes+65536 || frame.milliseconds<100 || frame.milliseconds>15000) return false;
        end+=frame.bytes; duration+=frame.milliseconds;
        if(end>total-digestBytes || duration>15000) return false;
    }
    if(end!=total-digestBytes) return false;
    next.bytes=total; next.milliseconds=duration; info=next;
    return true;
}
inline float wrap(float degrees) {
    const float value=std::remainder(degrees,360.0f);
    return value<=-180?value+360:value;
}
class Orientation {
public:
    float angle() const { return degrees_; }
    float drawAngle(bool autoRotate) const { return autoRotate?-degrees_:manualDegrees_; }
    void rotateManual() { manualDegrees_=(manualDegrees_+45)%360; }
    void reset() { initialized_=false; }
    void update(float x, float y, float z, float gyroZ, float seconds) {
        if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(gyroZ) || !std::isfinite(seconds) || seconds<=0) return;
        if(std::abs(x)>32 || std::abs(y)>32 || std::abs(z)>32 || std::abs(gyroZ)>4000) return;
        const float projection=std::sqrt(x*x+y*y), gravity=std::sqrt(x*x+y*y+z*z);
        const bool upright=projection>0.25f && gravity>0.7f && gravity<1.3f;
        // StopWatch upright (buttons above the display) has gravity along -X.
        const float target=std::atan2(-y,-x)*180.0f/3.14159265358979323846f;
        if(!initialized_ || seconds>0.5f) {
            if(upright) degrees_=target;
            initialized_=true; return;
        }
        if(std::abs(gyroZ)>0.8f) degrees_=wrap(degrees_-gyroZ*seconds);
        if(upright) degrees_=wrap(degrees_+wrap(target-degrees_)*(1-std::exp(-seconds/0.18f)));
    }
private:
    float degrees_=0;
    uint16_t manualDegrees_=0;
    bool initialized_=false;
};
}
