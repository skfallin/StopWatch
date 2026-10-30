#pragma once
#include "i18n.h"
#include <array>
#include <cstddef>
#include <string_view>

namespace launcher {
inline constexpr size_t appCount=10;
inline constexpr std::array<const char*,appCount> ids={"home","notes","badge","clock","spotify","usb","key","system","focus","slides"};
inline constexpr std::array<const char*,appCount> names={"Casa","Note","Badge","Orologio","Spotify","Ponte","Chiave","Sistema","Focus","Slide"};
inline constexpr std::array<int,appCount> groups={0,1,1,1,0,2,2,2,1,2};
inline constexpr std::array<int,appCount> defaultOrder={0,4,1,2,3,8,7,5,6,9};
inline int index(const char* id) {
    if(!id) return -1;
    for(size_t i=0;i<ids.size();++i) if(std::string_view(id)==ids[i]) return int(i);
    return -1;
}
}
