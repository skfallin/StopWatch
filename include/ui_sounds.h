#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace sfx {
enum class Cue { Select, Open, Back, Success, Error, Warning, Complete, Volume, Count };
inline constexpr std::array<const char*,2> packIds = {"off", "microclick"};
inline constexpr std::array<const char*,2> packNames = {"Silenzioso", "Microclick"};
inline int packIndex(std::string_view id) {
    for (size_t i=0;i<packIds.size();++i) if (id==packIds[i]) return int(i);
    // Stored configurations and older companions must survive the retired packs.
    for (const auto* legacy : {"minimal", "soft", "glass", "arcade", "mechanical", "organic",
                              "dreamy", "scifi", "rubber", "cinematic", "studio", "zen"})
        if (id==legacy) return 1;
    return -1;
}
bool play(uint8_t pack, Cue cue, uint8_t volume, bool preview=false);
bool begin(uint8_t pack);
bool prepare(uint8_t pack);
bool ready();
void stop();
void update();
}
