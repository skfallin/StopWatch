#pragma once
#include "music_model.h"
#include "policy.h"
#include <ArduinoJson.h>
#include <cstring>

namespace music {
inline bool playlistUri(std::string_view uri) {
    constexpr std::string_view prefix="spotify:playlist:";
    if(uri.size()!=prefix.size()+22 || uri.substr(0,prefix.size())!=prefix) return false;
    for(char c:uri.substr(prefix.size()))
        if(!((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z'))) return false;
    return true;
}
inline bool parsePlaylists(JsonObjectConst data,PlaylistPage& output,uint32_t offset) {
    if(!data["offset"].is<uint32_t>() || data["offset"].as<uint32_t>()!=offset ||
       !data["total"].is<uint32_t>() || data["total"].as<uint32_t>()>10000 ||
       !data["items"].is<JsonArrayConst>()) return false;
    PlaylistPage page;page.offset=offset;page.total=data["total"].as<uint32_t>();
    const auto items=data["items"].as<JsonArrayConst>();
    if(offset>page.total || items.size()>playlistPageSize || offset+items.size()>page.total ||
       (offset<page.total && items.size()!=std::min(playlistPageSize,page.total-offset))) return false;
    for(auto item:items) {
        if(!item["name"].is<const char*>() || !item["uri"].is<const char*>()) return false;
        const auto name=item["name"].as<JsonString>(),uri=item["uri"].as<JsonString>();
        if(!name.size() || name.size()>256 || std::strlen(name.c_str())!=name.size() ||
           std::strlen(uri.c_str())!=uri.size() || !playlistUri({uri.c_str(),uri.size()})) return false;
        page.items.push_back({std::string(name.c_str(),name.size()),std::string(uri.c_str(),uri.size())});
    }
    page.loaded=true;output=std::move(page);return true;
}
inline bool artworkPath(std::string_view path) {
    constexpr std::string_view prefix="/api/media_player_proxy/";
    if(path.substr(0,prefix.size())!=prefix || path.size()>1024 || path.find_first_of("\r\n\t #\\")!=std::string_view::npos) return false;
    const auto query=path.find('?');
    return query!=std::string_view::npos && path.substr(prefix.size(),query-prefix.size()).substr(0,13)=="media_player." &&
        policy::entityId(path.substr(prefix.size(),query-prefix.size()));
}
inline bool artworkUrl(std::string_view url) {
    for(auto prefix:{"https://i.scdn.co/image/", "https://image-cdn-ak.spotifycdn.com/image/", "https://image-cdn-fa.spotifycdn.com/image/"}) {
        if(url.substr(0,std::strlen(prefix))!=prefix || url.size()>512 || url.size()==std::strlen(prefix)) continue;
        if(url.find_first_of("\r\n\t #\\")!=std::string_view::npos) return false;
        return true;
    }
    return false;
}
inline const char* commandName(Command c) {
    switch(c) {
    case Command::Refresh:return "refresh";
    case Command::Play:return "play";
    case Command::Pause:return "pause";
    case Command::Next:return "next";
    case Command::Previous:return "previous";
    case Command::Source:return "source";
    case Command::Playlists:return "playlists";
    case Command::Playlist:return "playlist";
    }
    return "";
}
inline bool parse(JsonObjectConst data,Playback& output,uint32_t now) {
    Playback p;
    const auto text=[&](const char* key,std::string& target,size_t max) {
        if(!data[key].is<const char*>()) return false;
        const auto s=data[key].as<JsonString>();
        if(s.size()>max || std::strlen(s.c_str())!=s.size()) return false;
        target.assign(s.c_str(),s.size());return true;
    };
    std::string state;
    if(!text("state",state,16) || !text("title",p.title,256) || !text("artist",p.artist,256) ||
       !text("source",p.source,512) || !text("image",p.image,1024) || !text("ha_url",p.server,256) ||
       !data["position_ms"].is<uint32_t>() || !data["duration_ms"].is<uint32_t>() ||
       !data["features"].is<uint32_t>() || !data["sources"].is<JsonArrayConst>()) return false;
    if(!policy::baseUrl(p.server)) return false;
    if(!p.image.empty() && !artworkUrl(p.image) && !artworkPath(p.image)) p.image.clear();
    p.durationMs=data["duration_ms"].as<uint32_t>();
    if(p.durationMs>604800000) return false;
    p.positionMs=std::min(data["position_ms"].as<uint32_t>(),p.durationMs);
    p.playing=state=="playing";
    p.available=p.playing || state=="paused";
    const uint32_t features=data["features"].as<uint32_t>();
    p.canPause=(features&1)!=0;p.canPrevious=(features&16)!=0;p.canNext=(features&32)!=0;
    p.canPlay=(features&16384)!=0;p.canSource=(features&2048)!=0;
    p.canPlaylist=(features&512)!=0;
    const auto list=data["sources"].as<JsonArrayConst>();
    if(list.size()>12) return false;
    for(auto item:list) {
        if(!item.is<const char*>()) return false;
        const auto s=item.as<JsonString>();
        if(!s.size() || s.size()>128 || std::strlen(s.c_str())!=s.size()) return false;
        p.sources.emplace_back(s.c_str(),s.size());
    }
    // Spotify can have available players even when no track is loaded.
    if(!p.sources.empty() && state=="idle") p.available=true;
    p.observedAt=now;output=std::move(p);return true;
}
}
