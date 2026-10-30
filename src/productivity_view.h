#pragma once
#include "ui_drawing.h"
#include "focus_model.h"
namespace productivity_ui {
void focus(M5Canvas& canvas,const clockwork::Focus& model);
void slides(M5Canvas& canvas,uint64_t elapsed,bool usb);
}
