#pragma once

#include <cstdint>

namespace firmware {
inline constexpr char version[]="0.18.0-beta.1";
inline constexpr uint32_t loadingDelayMs=400;
inline bool showLoading(uint32_t now, uint32_t startedAt, bool ready) {
    return !ready && uint32_t(now-startedAt)>=loadingDelayMs;
}
}
