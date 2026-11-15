#pragma once
#include "i18n.h"
#include <array>
#include <cstddef>
#include <string_view>

namespace launcher {
inline constexpr size_t appCount=8;
inline constexpr std::array<const char*,appCount> ids={"home","notes","badge","clock","spotify","usb","system","focus"};
inline constexpr std::array<const char*,appCount> names={"Casa","Note","Badge","Orologio","Spotify","Ponte","Sistema","Focus"};
inline constexpr std::array<int,appCount> defaultOrder={0,4,1,2,3,7,6,5};
inline constexpr int system=6;
// Apps removed from the launcher; saved or Companion-sent orders may still name them.
inline bool retired(std::string_view id) { return id=="key" || id=="slides"; }
inline int index(const char* id) {
    if(!id) return -1;
    for(size_t i=0;i<ids.size();++i) if(std::string_view(id)==ids[i]) return int(i);
    return -1;
}
}
