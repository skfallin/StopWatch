#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace music {
enum class Command { Refresh, Play, Pause, Next, Previous, Source, Playlists, Playlist };
inline constexpr uint32_t playlistPageSize=8;
struct Playlist { std::string name,uri; };
struct PlaylistPage {
    std::vector<Playlist> items;
    uint32_t offset=0,total=0;
    bool loaded=false;
    bool hasNext() const { return offset+items.size()<total; }
};
struct Playback {
    std::string title, artist, source, image, server;
    std::vector<std::string> sources;
    uint32_t positionMs=0, durationMs=0, observedAt=0;
    bool playing=false, available=false;
    bool canPlay=false, canPause=false, canNext=false, canPrevious=false, canSource=false,canPlaylist=false;

    uint32_t position(uint32_t now) const {
        const uint64_t elapsed=playing?uint32_t(now-observedAt):0;
        return uint32_t(std::min(uint64_t(positionMs)+elapsed,uint64_t(durationMs)));
    }
    bool fresh(uint32_t now) const { return available && uint32_t(now-observedAt)<15000; }
    bool allows(Command command,uint32_t now) const {
        if(!fresh(now)) return false;
        switch(command) {
        case Command::Play: return canPlay;
        case Command::Pause: return canPause;
        case Command::Next: return canNext;
        case Command::Previous: return canPrevious;
        case Command::Source: return canSource;
        case Command::Playlist: return canPlaylist;
        case Command::Playlists: return true;
        case Command::Refresh: return true;
        }
        return false;
    }
};
}
