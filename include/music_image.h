#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace music {
enum class ImageFormat { Invalid, Jpeg, Png };
inline ImageFormat imageFormat(const uint8_t* data,size_t size) {
    const auto fits=[](uint32_t w,uint32_t h){ return w && h && w<=2048 && h<=2048; };
    const uint8_t png[]={137,80,78,71,13,10,26,10};
    if(size>=33 && !memcmp(data,png,8) && !memcmp(data+12,"IHDR",4)) {
        const auto u32=[&](size_t at) { return uint32_t(data[at])<<24 | uint32_t(data[at+1])<<16 | uint32_t(data[at+2])<<8 | data[at+3]; };
        return u32(8)==13 && fits(u32(16),u32(20))?ImageFormat::Png:ImageFormat::Invalid;
    }
    if(size<4 || data[0]!=0xff || data[1]!=0xd8) return ImageFormat::Invalid;
    size_t at=2;
    while(at+4<=size) {
        if(data[at++]!=0xff) break;
        while(at<size && data[at]==0xff) ++at;
        if(at+3>size) break;
        const uint8_t marker=data[at++];
        if(marker==0xda || marker==0xd9) break;
        const size_t length=(size_t(data[at])<<8)|data[at+1];
        if(length<2 || length>size-at) break;
        if(marker==0xc0) {
            if(length<8 || data[at+2]!=8) break;
            return fits((uint32_t(data[at+5])<<8)|data[at+6],(uint32_t(data[at+3])<<8)|data[at+4])?ImageFormat::Jpeg:ImageFormat::Invalid;
        }
        at+=length;
    }
    return ImageFormat::Invalid;
}
}
