#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <M5GFX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "badge_format.h"

class Badge {
public:
    bool begin();
    bool available() const { return info_.count>0; }
    bool animated() const { return info_.count>1; }
    bool uploading();
    bool update(uint32_t now, bool autoRotate, bool animate, bool visible);
    bool draw(M5Canvas& canvas, bool autoRotate);
    void rotateManual() { orientation_.rotateManual(); }
    void suspend();
    uint32_t lastMotion() const { return motionAt_; }
    const String& error() const { return error_; }
    void status(JsonObject out);
    void transferFailed(const String& id, const String& error);
    bool startUpload(size_t bytes, const String& digest, JsonObject out, String& error);
    bool append(const String& id, size_t offset, const uint8_t* bytes, size_t count);
    bool commit(const String& id, String& error);
    void abortUpload();
    size_t uploadSize(const String& id);

private:
    struct Work;
    bool readInfo(File& file, badge::Info& info, String& digest);
    bool decode(File& file, const badge::Frame& frame, const uint8_t* cached=nullptr);
    bool buffers(size_t compressedBytes);
    void clearUpload();
    SemaphoreHandle_t mutex_=nullptr;
    File upload_;
    String uploadId_, expectedDigest_, digest_, error_, uploadError_;
    size_t expected_=0, received_=0;
    uint32_t uploadAt_=0;
    badge::Info info_;
    badge::Orientation orientation_;
    M5Canvas image_;
    Work* work_=nullptr;
    uint8_t* compressed_=nullptr;
    uint8_t* cached_=nullptr;
    size_t capacity_=0;
    uint16_t frame_=UINT16_MAX;
    uint32_t sampledAt_=0, motionAt_=0, animationAt_=0, elapsed_=0;
    float drawnAngle_=10000;
    bool failed_=false;
    bool cacheAttempted_=false;
};
