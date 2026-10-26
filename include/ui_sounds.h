#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace sfx {
enum class Cue { Select, Open, Back, Success, Error, Warning, Complete, Volume, Count };
inline constexpr std::array<const char*,13> packIds = {"off", "minimal", "soft", "glass", "arcade",
    "mechanical", "organic", "dreamy", "scifi", "rubber", "cinematic", "studio", "zen"};
inline constexpr std::array<const char*,13> packNames = {"Silenzioso", "Minimal", "Soft", "Glass", "Arcade",
    "Mechanical", "Organic", "Dreamy", "Sci-fi", "Rubber", "Cinematic", "Studio", "Zen"};
inline int packIndex(std::string_view id) {
    for (size_t i=0;i<packIds.size();++i) if (id==packIds[i]) return int(i);
    return -1;
}
bool play(uint8_t pack, Cue cue, uint8_t volume, bool preview=false);
bool begin(uint8_t pack);
bool prepare(uint8_t pack);
bool ready();
void stop();
void update();
}
