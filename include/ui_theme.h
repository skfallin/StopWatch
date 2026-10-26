#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace ui {
struct Theme {
    const char* id;
    const char* name;
    uint32_t accent, focus, focusPanel, navigation, selectionSurface;
};
inline constexpr std::array<Theme,6> themes = {{
    {"lime", "Lime", 0xcbf65e, 0xe1ff67, 0xc5df5a, 0xcbf65e, 0x252c20},
    {"buttons", "Giallo / Blu", 0x78acff, 0x9ac3ff, 0x88abe1, 0xffd34d, 0x202735},
    {"orange", "Arancio", 0xffb36b, 0xffca8a, 0xe0b279, 0xffb36b, 0x30251b},
    {"violet", "Viola", 0xbca6ff, 0xd0c0ff, 0xb7a9e0, 0xbca6ff, 0x292337},
    {"cyan", "Ciano", 0x6ce1e9, 0x9cedf1, 0x87d0d4, 0x6ce1e9, 0x1c2d2f},
    {"rose", "Rosa", 0xff9fc7, 0xffbedb, 0xe0a7c0, 0xff9fc7, 0x30222b}
}};
inline int themeIndex(std::string_view id) {
    for (size_t i=0;i<themes.size();++i) if (id==themes[i].id) return int(i);
    return -1;
}
}
