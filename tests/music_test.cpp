#include "music_protocol.h"
#include "music_image.h"
#include "ui_layout.h"
#include <cassert>
#include <iostream>
#include <utility>

int main() {
    music::Playback p;
    assert(!p.fresh(0) && !p.allows(music::Command::Play,0));
    p.available=true;p.canPlay=p.canPause=p.canNext=true;
    p.observedAt=1000;p.positionMs=5000;p.durationMs=20000;
    assert(p.position(3000)==5000);
    p.playing=true;assert(p.position(3000)==7000);
    assert(p.position(50000)==20000);
    assert(p.allows(music::Command::Next,15999));
    assert(!p.allows(music::Command::Next,16000));
    assert(!p.allows(music::Command::Previous,1000));
    p.observedAt=UINT32_MAX-500;
    assert(p.position(499)==6000 && p.fresh(499));
    p.durationMs=0;assert(p.position(499)==0);
    using namespace ui;
    for(auto target:{musicPrevious,musicPlay,musicNext})
        assert((Circle{233,233,232-target.radius}.contains(target.x,target.y)));
    assert(hit(Screen::Spotify,233,416,0).action==Action::Select);
    assert(hit(Screen::Spotify,151,410,0).action==Action::Previous);
    assert(hit(Screen::Spotify,315,410,0).action==Action::Next);
    assert(hit(Screen::Spotify,245,79,0).action==Action::Open);
    assert(hit(Screen::Spotify,112,84,0).action==Action::Back);
    assert(hit(Screen::Spotify,0,0,0).action==Action::None);
    for(auto point:{std::pair{233,200},std::pair{139,111},std::pair{326,298}})
        assert(hit(Screen::Spotify,point.first,point.second,0).action==Action::Select);
    for(auto point:{std::pair{138,200},std::pair{327,200},std::pair{233,110},std::pair{233,299}})
        assert(hit(Screen::Spotify,point.first,point.second,0).action==Action::None);
    assert(hit(Screen::Spotify,233,200,0,true).action==Action::None);
    assert((hit(Screen::MusicSources,200,242,4)==Target{Action::Open,4}));
    assert((hit(Screen::MusicMenu,233,185,0)==Target{Action::Open,0}));
    assert((hit(Screen::MusicMenu,233,277,0)==Target{Action::Open,1}));
    assert((hit(Screen::MusicPlaylists,233,248,4)==Target{Action::Open,4}));
    assert(hit(Screen::MusicPlaylists,233,248,-1).action==Action::None);
    assert(hit(Screen::MusicPlaylists,233,399,-1).action==Action::Select);
    assert(hit(Screen::MusicPlaylists,346,399,0).action==Action::Increase);
    assert(hit(Screen::MusicPlaylists,120,399,0).action==Action::Decrease);
    assert(music::playlistUri("spotify:playlist:1234567890abcdefghijkl"));
    for(auto uri:{"https://evil/playlist","spotify:track:1234567890abcdefghijkl","spotify:playlist:1234567890abcdefghijk/","spotify:playlist:short"}) assert(!music::playlistUri(uri));
    JsonDocument catalog;
    auto items=catalog["items"].to<JsonArray>();
    catalog["offset"]=0;catalog["total"]=9;
    for(int i=0;i<8;++i) { auto item=items.add<JsonObject>();item["name"]="Caffè";item["uri"]="spotify:playlist:1234567890abcdefghijkl"; }
    music::PlaylistPage page;
    assert(music::parsePlaylists(catalog.as<JsonObjectConst>(),page,0) && page.loaded && page.hasNext());
    assert(!music::parsePlaylists(catalog.as<JsonObjectConst>(),page,8));
    catalog["offset"]=8;catalog["items"].to<JsonArray>();
    auto last=catalog["items"].add<JsonObject>();last["name"]="Ultima";last["uri"]="spotify:playlist:1234567890abcdefghijkl";
    assert(music::parsePlaylists(catalog.as<JsonObjectConst>(),page,8) && page.items.size()==1 && !page.hasNext());
    last["uri"]="http://evil/stream";
    assert(!music::parsePlaylists(catalog.as<JsonObjectConst>(),page,8));
    catalog["offset"]=0;catalog["total"]=0;catalog["items"].to<JsonArray>();
    assert(music::parsePlaylists(catalog.as<JsonObjectConst>(),page,0) && page.loaded && page.items.empty());
    for(auto url:{"https://i.scdn.co/image/abc","https://image-cdn-ak.spotifycdn.com/image/abc"}) assert(music::artworkUrl(url));
    for(auto url:{"http://i.scdn.co/image/abc","https://i.scdn.co.evil/image/a","https://i.scdn.co@evil/image/a","https://i.scdn.co/image/","https://i.scdn.co/image/a\r\nHeader:x"}) assert(!music::artworkUrl(url));
    assert(music::artworkPath("/api/media_player_proxy/media_player.spotify_fra?token=abc&cache=123"));
    for(auto path:{"/api/media_player_proxy/light.room?token=x","/api/media_player_proxy/media_player.a/../../?token=x","//evil/api/media_player_proxy/media_player.a?token=x","/api/media_player_proxy/media_player.a?token=x#fragment"}) assert(!music::artworkPath(path));
    JsonDocument data;
    assert(!deserializeJson(data,R"({"state":"playing","title":"Caffè","artist":"Artista","source":"iPhone","image":"/api/media_player_proxy/media_player.spotify?token=x","ha_url":"http://192.168.0.150:8123","position_ms":42000,"duration_ms":180000,"features":18481,"sources":["iPhone","Mac"]})"));
    assert(music::parse(data.as<JsonObjectConst>(),p,1000));
    assert(p.playing && p.canNext && p.canPrevious && p.canPlay && p.canPause && p.canSource);
    assert(!p.canPlaylist);
    data["features"]=18481+512;
    assert(music::parse(data.as<JsonObjectConst>(),p,1000) && p.allows(music::Command::Playlist,1000));
    assert(p.title=="Caffè" && p.sources.size()==2 && p.position(2000)==43000);
    data["image"]="https://evil.example/image";
    assert(music::parse(data.as<JsonObjectConst>(),p,1000) && p.image.empty());
    data["ha_url"]="https://user:password@example.com";
    assert(!music::parse(data.as<JsonObjectConst>(),p,1000));
    data["ha_url"]="";data["position_ms"]=-1;
    assert(!music::parse(data.as<JsonObjectConst>(),p,1000));
    data["position_ms"]=0;data["title"]=std::string(257,'a');
    assert(!music::parse(data.as<JsonObjectConst>(),p,1000));
    data["title"]="Idle";data["state"]="idle";
    assert(music::parse(data.as<JsonObjectConst>(),p,1000) && p.allows(music::Command::Source,1000));
    data["sources"].to<JsonArray>().add(std::string(129,'a'));
    assert(!music::parse(data.as<JsonObjectConst>(),p,1000));
    uint8_t jpeg[]={0xff,0xd8,0xff,0xc0,0,8,8,1,44,1,44,3};
    assert(music::imageFormat(jpeg,sizeof(jpeg))==music::ImageFormat::Jpeg);
    for(size_t n=0;n<sizeof(jpeg);++n) assert(music::imageFormat(jpeg,n)==music::ImageFormat::Invalid);
    jpeg[9]=9;assert(music::imageFormat(jpeg,sizeof(jpeg))==music::ImageFormat::Invalid);
    uint8_t png[33]={137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,1,44,0,0,1,44};
    assert(music::imageFormat(png,sizeof(png))==music::ImageFormat::Png);
    png[16]=0xff;assert(music::imageFormat(png,sizeof(png))==music::ImageFormat::Invalid);
    uint32_t random=123;
    uint8_t noise[127];
    for(unsigned i=0;i<2000;++i) {
        for(auto& byte:noise) { random=random*1664525+1013904223;byte=uint8_t(random>>24); }
        for(size_t n=0;n<sizeof(noise);++n) music::imageFormat(noise,n);
    }
    std::cout<<"Music state and touch tests passed\n";
}
