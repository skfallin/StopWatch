#pragma once
#include "ui_drawing.h"

namespace scene_ui {
void home(M5Canvas& canvas,const ui::HomeMenu& menu,uint32_t age);
void row(M5Canvas& canvas,ui::Rect bounds,const String& title,ui::Icon icon,const String& detail,bool selected);
void pages(M5Canvas& canvas,int page);
void notice(M5Canvas& canvas,const String& title,const String& detail,ui::Icon icon);
}
