#pragma once
#include <M5GFX.h>
#include <ctime>
#include "clock_model.h"

namespace clock_ui {
void menu(M5Canvas& canvas, int selected);
void watch(M5Canvas& canvas, clockwork::Face face, const tm& local, bool valid, bool controls);
void timer(M5Canvas& canvas, const clockwork::Timer& timer);
void stopwatch(M5Canvas& canvas, const clockwork::Stopwatch& stopwatch);
}
