#pragma once
#include <M5GFX.h>
#include "ui_layout.h"
#include "ui_theme.h"

namespace ui {
constexpr uint32_t black=0x000000, white=0xf4f6f0, muted=0xa4aaa4, surface=0x181c19;
constexpr uint32_t ink=0x10150a, line=0x343b34;
inline uint32_t accent=themes[0].accent, focus=themes[0].focus;
inline uint32_t focusPanel=themes[0].focusPanel, navigation=themes[0].navigation;
inline uint32_t selectionSurface=themes[0].selectionSurface;
enum class Font { Small, Body, Label, Title, Clock, Number };
enum class Icon { House, Mic, Sliders, Sun, Activity, Bluetooth, Wifi, Back, Next,
                  Play, Check, Bulb, Speaker, Archive, Warning, Clock, Link, Close, Badge, Timer, Stopwatch, Music, Moon };
bool beginFonts();
void setTheme(uint8_t index);
int textWidth(M5Canvas& canvas, const String& value, Font font);
void label(M5Canvas& canvas, const String& value, int x, int y, Font font=Font::Body,
           uint32_t color=white, uint32_t background=black, int maxWidth=360);
void icon(M5Canvas& canvas, Icon name, int cx, int cy, int size, uint32_t color, uint32_t background=black);
void circleButton(M5Canvas& canvas, Circle target, Icon name, int iconSize, uint32_t fill, uint32_t color);
void pill(M5Canvas& canvas, int x, int y, int width, const String& value, uint32_t fill=surface, uint32_t color=white);
void dots(M5Canvas& canvas, int selected, int count, int y=419, uint32_t active=navigation, uint32_t inactive=line);
}
