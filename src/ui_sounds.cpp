#include "ui_sounds.h"
#include "policy.h"
#include "ui_sound_catalog.h"
#ifdef SFX_SEED_ARCHIVE
#include "ui_sound_data.h"
#endif
#include <LittleFS.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <miniz.h>
#include <mbedtls/sha256.h>
#include <cstring>

namespace sfx {
namespace {
constexpr const char* archivePath="/ui-sounds.bin";
struct PlaybackBuffer { tinfl_decompressor decoder; int16_t pcm[catalog::maxDecodedBytes/2]; };
PlaybackBuffer* buffer=nullptr;
uint8_t* packCache=nullptr;
uint8_t cachedPack=0;
bool archiveReady=false, active=false, played=false;
uint32_t lastPlayed=0;

bool archiveMatches(const char* path) {
    File file=LittleFS.open(path,"r");
    if(!file || file.size()!=catalog::archiveBytes) return false;
    mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);
    bool ok=mbedtls_sha256_starts(&hash,0)==0;
    uint8_t bytes[1024],digest[32];
    size_t remaining=catalog::archiveBytes;
    while(ok && remaining) {
        const size_t count=file.read(bytes,std::min(remaining,sizeof(bytes)));
        ok=count && mbedtls_sha256_update(&hash,bytes,count)==0;remaining-=count;
    }
    ok=ok && mbedtls_sha256_finish(&hash,digest)==0 && std::memcmp(digest,catalog::digest,sizeof(digest))==0;
    mbedtls_sha256_free(&hash);return ok;
}

#ifdef SFX_SEED_ARCHIVE
bool installArchive() {
    constexpr const char* stagingPath="/ui-sounds.tmp";
    // Only our temporary file is written; user files are never reformatted.
    LittleFS.remove(stagingPath);
    const size_t total=LittleFS.totalBytes(),used=LittleFS.usedBytes();
    Serial.printf("[audio-store] installazione: totale=%u usati=%u audio=%u\n",unsigned(total),unsigned(used),unsigned(catalog::archiveBytes));
    if(total<used || total-used<catalog::archiveBytes+policy::maxSamples*2+131072) return false;
    File file=LittleFS.open(stagingPath,"w");
    if(!file) return false;
    uint8_t bytes[1024];bool ok=true;
    for(const auto& pack:data::packs) for(const auto& asset:pack) {
        for(size_t offset=0;ok && offset<asset.size;) {
            const size_t count=std::min(sizeof(bytes),size_t(asset.size)-offset);
            std::memcpy(bytes,asset.bytes+offset,count);
            ok=file.write(bytes,count)==count;offset+=count;
        }
    }
    file.flush();file.close();
    ok=ok && archiveMatches(stagingPath) && LittleFS.rename(stagingPath,archivePath);
    if(!ok) LittleFS.remove(stagingPath);
    return ok;
}
#endif

bool fallback(Cue cue,uint8_t volume) {
    M5.Mic.end();
    if(!M5.Speaker.begin()) return false;
    M5.Speaker.setVolume(policy::speakerVolume(volume));
    M5.Speaker.tone(cue==Cue::Error?440:cue==Cue::Complete?1320:880,60);
    active=played=true;lastPlayed=millis();return true;
}
}

void stop() {
    if (!active) return;
    M5.Speaker.end();
    active=false;
}

bool ready() { return archiveReady; }

bool prepare(uint8_t pack) {
    if(pack>=packIds.size()) return false;
    if(!pack) { stop();free(packCache);packCache=nullptr;cachedPack=0;return true; }
    if(cachedPack==pack && packCache) return true;
    if(!archiveReady) return false;
    const uint32_t started=millis();
    const auto& assets=catalog::packs[pack-1];
    const auto& last=assets[unsigned(Cue::Count)-1];
    const size_t base=assets[0].offset,size=last.offset+last.size-base;
    auto* next=static_cast<uint8_t*>(heap_caps_malloc(size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(!next) return false;
    File file=LittleFS.open(archivePath,"r");
    if(!file || file.size()!=catalog::archiveBytes || !file.seek(base) || file.read(next,size)!=size) { free(next);return false; }
    stop();free(packCache);packCache=next;cachedPack=pack;
    Serial.printf("[audio-store] pacchetto=%u cache_byte=%u ms=%u\n",unsigned(pack),unsigned(size),unsigned(millis()-started));
    return true;
}

bool begin(uint8_t pack) {
    const uint32_t started=millis();
    stop();free(packCache);packCache=nullptr;cachedPack=0;
    archiveReady=archiveMatches(archivePath);
#ifdef SFX_SEED_ARCHIVE
    if(!archiveReady) archiveReady=installArchive();
#endif
    const bool cached=archiveReady && prepare(pack);
    Serial.printf("[audio-store] pronto=%d cache=%d byte=%u usati=%u totale=%u ms=%u\n",archiveReady,cached,unsigned(catalog::archiveBytes),unsigned(LittleFS.usedBytes()),unsigned(LittleFS.totalBytes()),unsigned(millis()-started));
    return cached;
}

void update() {
    if (active && !M5.Speaker.isPlaying()) stop();
}

bool play(uint8_t pack, Cue cue, uint8_t volume, bool preview) {
    if (!pack || pack>=packIds.size() || unsigned(cue)>=unsigned(Cue::Count) || !volume) return false;
    const uint32_t now=millis();
    if (!preview && played && uint32_t(now-lastPlayed)<90) return false;
    if (M5.Speaker.isPlaying() && !active) return false;
    stop();
    const auto& asset=catalog::packs[pack-1][unsigned(cue)];
    const uint8_t* encoded=nullptr;
    if(prepare(pack)) encoded=packCache+asset.offset-catalog::packs[pack-1][0].offset;
#ifdef SFX_SEED_ARCHIVE
    if(!encoded) encoded=data::packs[pack-1][unsigned(cue)].bytes;
#endif
    if(!encoded) return fallback(cue,volume);
    // One compressed pack and the current PCM cue stay in PSRAM.
    if (!buffer) buffer=static_cast<PlaybackBuffer*>(heap_caps_malloc(sizeof(PlaybackBuffer),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) return false;
    size_t inputBytes=asset.size, outputBytes=asset.decodedBytes;
    auto* pcm=reinterpret_cast<mz_uint8*>(buffer->pcm);
    tinfl_init(&buffer->decoder);
    const auto status=tinfl_decompress(&buffer->decoder,encoded,&inputBytes,pcm,pcm,&outputBytes,
        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (status!=TINFL_STATUS_DONE || inputBytes!=asset.size || outputBytes!=asset.decodedBytes) return false;
    M5.Mic.end();
    if (!M5.Speaker.begin()) return false;
    M5.Speaker.setVolume(policy::speakerVolume(volume));
    if (!M5.Speaker.playRaw(buffer->pcm,asset.decodedBytes/2,catalog::sampleRate,false,1,0,true)) {
        M5.Speaker.end();
        return false;
    }
    active=played=true;
    lastPlayed=now;
    return true;
}
}
