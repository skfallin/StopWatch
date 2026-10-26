#include "badge.h"
#include "policy.h"
#include <LittleFS.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <miniz.h>
#include <memory>
#include <new>

namespace {
struct Lock {
    SemaphoreHandle_t mutex;
    explicit Lock(SemaphoreHandle_t value):mutex(value) { xSemaphoreTake(mutex,portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
};
String hex(const uint8_t* bytes, size_t count) {
    String result; result.reserve(count*2);
    for(size_t i=0;i<count;++i) { char pair[3]; snprintf(pair,sizeof(pair),"%02x",bytes[i]); result+=pair; }
    return result;
}
}
struct Badge::Work { tinfl_decompressor decoder; uint8_t pixels[badge::frameBytes]; };

bool Badge::begin() {
    if(!mutex_) mutex_=xSemaphoreCreateMutex();
    if(!mutex_) return false;
    abortUpload(); suspend(); info_={}; digest_=""; error_=""; failed_=false;
    File file=LittleFS.open("/badge.bin","r");
    badge::Info loaded; String digest;
    if(file) {
        if(readInfo(file,loaded,digest)) { info_=loaded; digest_=digest; }
        else error_="Badge salvato non valido";
    }
    return true;
}

bool Badge::readInfo(File& file, badge::Info& info, String& digest) {
    std::array<uint8_t,badge::headerBytes+badge::maxFrames*badge::entryBytes> table{};
    uint16_t count;
    if(!file.seek(0) || file.read(table.data(),badge::headerBytes)!=badge::headerBytes ||
       !badge::header(table.data(),badge::headerBytes,file.size(),count)) return false;
    const size_t bytes=count*badge::entryBytes;
    if(file.read(table.data()+badge::headerBytes,bytes)!=bytes || !badge::parse(table.data(),badge::headerBytes+bytes,file.size(),info)) return false;
    uint8_t hash[32];
    if(!file.seek(file.size()-sizeof(hash)) || file.read(hash,sizeof(hash))!=sizeof(hash)) return false;
    digest=hex(hash,sizeof(hash));
    return true;
}

bool Badge::buffers(size_t bytes) {
    if(!work_) work_=static_cast<Work*>(heap_caps_malloc(sizeof(Work),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(bytes>capacity_) {
        auto* next=static_cast<uint8_t*>(heap_caps_realloc(compressed_,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!next) return false;
        compressed_=next; capacity_=bytes;
    }
    if(!image_.getBuffer()) {
        image_.setColorDepth(16); image_.setPsram(true);
        if(!image_.createSprite(badge::side,badge::side)) return false;
        image_.setPivot(233,233);
    }
    return work_ && (bytes==0 || compressed_);
}

bool Badge::decode(File& file, const badge::Frame& frame, const uint8_t* cached) {
    if(!buffers(cached?0:frame.bytes)) return false;
    if(!cached && (!file.seek(frame.offset) || file.read(compressed_,frame.bytes)!=frame.bytes)) return false;
    size_t input=frame.bytes,output=badge::frameBytes;
    tinfl_init(&work_->decoder);
    const auto result=tinfl_decompress(&work_->decoder,cached?cached:compressed_,&input,work_->pixels,work_->pixels,&output,
        TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    return result==TINFL_STATUS_DONE && input==frame.bytes && output==badge::frameBytes;
}

void Badge::suspend() {
    image_.deleteSprite(); free(work_); work_=nullptr; free(compressed_); compressed_=nullptr; capacity_=0;
    free(cached_); cached_=nullptr; cacheAttempted_=false;
    frame_=UINT16_MAX; drawnAngle_=10000; sampledAt_=animationAt_=elapsed_=0; orientation_.reset();
}

bool Badge::update(uint32_t now, bool autoRotate, bool animate, bool visible) {
    if(mutex_) {
        Lock guard(mutex_);
        if(!uploadId_.isEmpty() && badge::uploadExpired(now,uploadAt_)) {
            Serial.printf("[badge-upload] scaduto: ricevuti=%u totale=%u\n",unsigned(received_),unsigned(expected_));
            clearUpload();
        }
    }
    if(!visible || !available()) { animationAt_=now; return false; }
    if(!animationAt_) animationAt_=now;
    if(animate && animated() && !failed_) elapsed_=(elapsed_+uint32_t(now-animationAt_))%info_.milliseconds;
    animationAt_=now;
    if(autoRotate && (!sampledAt_ || uint32_t(now-sampledAt_)>=(uint32_t(now-motionAt_)<2000?25u:100u))) {
        if(M5.Imu.isEnabled() && M5.Imu.update()) {
            const auto imu=M5.Imu.getImuData();
            const float before=orientation_.angle();
            orientation_.update(imu.accel.x,imu.accel.y,imu.accel.z,imu.gyro.z,sampledAt_?uint32_t(now-sampledAt_)/1000.0f:1.0f);
            if(std::abs(badge::wrap(orientation_.angle()-before))>0.6f || std::abs(imu.gyro.z)>3) motionAt_=now;
        }
        sampledAt_=now;
    }
    uint16_t next=0; uint32_t duration=info_.frames[0].milliseconds;
    while(next+1<info_.count && elapsed_>=duration) duration+=info_.frames[++next].milliseconds;
    bool changed=false;
    if(!failed_ && frame_!=next) {
        File file;
        if(animated() && !cacheAttempted_) {
            cacheAttempted_=true; file=LittleFS.open("/badge.bin","r");
            cached_=static_cast<uint8_t*>(heap_caps_malloc(info_.bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
            if(cached_ && (!file || file.read(cached_,info_.bytes)!=info_.bytes)) { free(cached_); cached_=nullptr; }
        }
        if(!cached_ && !file) file=LittleFS.open("/badge.bin","r");
        if((!cached_ && !file) || !decode(file,info_.frames[next],cached_?cached_+info_.frames[next].offset:nullptr)) { failed_=true; error_="Fotogramma Badge non leggibile"; }
        else {
            image_.pushImage(0,0,badge::side,badge::side,reinterpret_cast<const lgfx::rgb565_t*>(work_->pixels));
            frame_=next;
        }
        changed=true;
    }
    const float angle=orientation_.drawAngle(autoRotate);
    return changed || std::abs(badge::wrap(angle-drawnAngle_))>=1.5f;
}

bool Badge::draw(M5Canvas& canvas, bool autoRotate) {
    if(frame_==UINT16_MAX || !image_.getBuffer()) return false;
    const float angle=orientation_.drawAngle(autoRotate);
    if(std::abs(angle)<0.1f) image_.pushSprite(&canvas,0,0);
    else image_.pushRotateZoomWithAA(&canvas,233,233,angle,1.0f,1.0f);
    drawnAngle_=angle;
    return true;
}

void Badge::status(JsonObject out) {
    out["available"]=available(); out["frames"]=info_.count; out["bytes"]=info_.bytes;
    out["duration_ms"]=info_.milliseconds; out["digest"]=digest_; out["error"]=error_;
    if(mutex_) {
        Lock guard(mutex_);
        out["uploading"]=!uploadId_.isEmpty();
        out["upload_received"]=received_; out["upload_error"]=uploadError_;
    }
}
void Badge::transferFailed(const String& id, const String& error) {
    if(!mutex_) return;
    Lock guard(mutex_);
    if(id==uploadId_ && !id.isEmpty()) uploadError_=error;
}
bool Badge::uploading() { if(!mutex_) return false; Lock guard(mutex_); return !uploadId_.isEmpty(); }
size_t Badge::uploadSize(const String& id) {
    if(!mutex_) return 0; Lock guard(mutex_); return id==uploadId_?expected_:0;
}
void Badge::clearUpload() {
    upload_.close(); LittleFS.remove("/badge.upload"); uploadId_=""; expectedDigest_=""; uploadError_=""; expected_=received_=0;
}
void Badge::abortUpload() { if(mutex_) { Lock guard(mutex_); clearUpload(); } }

bool Badge::startUpload(size_t bytes, const String& digest, JsonObject out, String& error) {
    if(!mutex_) { error="Archivio Badge non disponibile"; return false; }
    if(bytes<badge::headerBytes+badge::entryBytes+badge::digestBytes || bytes>badge::maxBytes || digest.length()!=64) {
        error="Badge troppo grande o non valido"; return false;
    }
    for(char c:digest) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) { error="Impronta Badge non valida"; return false; }
    Lock guard(mutex_); clearUpload();
    if(LittleFS.totalBytes()-LittleFS.usedBytes()<bytes+policy::maxSamples*2+131072) {
        error="Spazio insufficiente: conserva e rimuovi alcune note"; return false;
    }
    upload_=LittleFS.open("/badge.upload","w");
    if(!upload_) { error="Impossibile preparare il Badge"; return false; }
    char id[17]; snprintf(id,sizeof(id),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    uploadId_=id; expected_=bytes; expectedDigest_=digest; uploadAt_=millis();
    out["upload"]=uploadId_;
    return true;
}

bool Badge::append(const String& id, size_t offset, const uint8_t* bytes, size_t count) {
    if(!mutex_) return false; Lock guard(mutex_);
    if(id!=uploadId_ || id.isEmpty() || !upload_ || offset!=received_ || !count || count>4096 || count>expected_-received_) {
        Serial.printf("[badge-upload] blocco rifiutato: sessione=%d file=%d offset=%u ricevuti=%u totale=%u blocco=%u\n",
            id==uploadId_ && !id.isEmpty(),bool(upload_),unsigned(offset),unsigned(received_),unsigned(expected_),unsigned(count));
        return false;
    }
    if(upload_.write(bytes,count)!=count) {
        Serial.printf("[badge-upload] scrittura fallita: offset=%u blocco=%u\n",unsigned(offset),unsigned(count));
        clearUpload(); return false;
    }
    received_+=count; uploadAt_=millis(); return true;
}

bool Badge::commit(const String& id, String& error) {
    if(!mutex_) return false; Lock guard(mutex_);
    if(id!=uploadId_ || id.isEmpty() || received_!=expected_) { error="Trasferimento Badge incompleto"; return false; }
    upload_.flush(); upload_.close();
    File file=LittleFS.open("/badge.upload","r");
    badge::Info next; String digest;
    if(!file || !readInfo(file,next,digest) || digest!=expectedDigest_) { clearUpload(); error="Badge non valido"; return false; }
    mbedtls_sha256_context hash; mbedtls_sha256_init(&hash);
    bool ok=mbedtls_sha256_starts(&hash,0)==0 && file.seek(0);
    std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[2048]);
    uint8_t result[32]; size_t left=file.size()-32;
    ok=ok && bool(bytes);
    while(ok && left) {
        const size_t count=file.read(bytes.get(),std::min(left,size_t(2048)));
        ok=count>0 && mbedtls_sha256_update(&hash,bytes.get(),count)==0; left-=count;
    }
    ok=ok && mbedtls_sha256_finish(&hash,result)==0 && hex(result,32)==digest;
    mbedtls_sha256_free(&hash);
    if(ok) { free(cached_); cached_=nullptr; cacheAttempted_=false; }
    if(!ok || !decode(file,next.frames[0])) { file.close(); clearUpload(); error="Verifica Badge fallita: precedente conservato"; return false; }
    file.close();
    if(!LittleFS.rename("/badge.upload","/badge.bin")) { clearUpload(); error="Salvataggio Badge fallito"; return false; }
    info_=next; digest_=digest; error_=""; failed_=false; elapsed_=animationAt_=0; frame_=UINT16_MAX;
    clearUpload();
    suspend();
    return true;
}
