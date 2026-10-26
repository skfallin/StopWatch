#pragma once
#include "music_model.h"
#include "ui_drawing.h"

namespace music_ui {
void player(M5Canvas& canvas,const music::Playback& playback,uint32_t now,bool busy,
            const String& message,bool artwork=false,uint32_t background=ui::black);
void sources(M5Canvas& canvas,const music::Playback& playback,size_t selected,bool busy);
void menu(M5Canvas& canvas,size_t selected);
void playlists(M5Canvas& canvas,const music::PlaylistPage& page,size_t selected,bool busy,const String& message);
}
