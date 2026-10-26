#pragma once

#include <Arduino.h>
#include <FS.h>
#include <atomic>
#include "policy.h"

class VoiceNotes {
public:
    enum class State { Idle, Preparing, Recording, Saving };

    bool begin();
    bool initializeStorage();
    bool warmMicrophone();
    bool start();
    void stop();
    void update();
    bool busy() const { return state_ != State::Idle; }
    bool playing() const { return playback_ != nullptr; }
    bool ready() const { return mounted_; }
    State state() const { return state_; }
    float seconds() const { return completed_.load() / float(policy::sampleRate); }
    uint16_t peak() const { return peak_.load(); }
    size_t count() const;
    String latest() const;
    bool playLatest(uint8_t volume);
    bool takeFinished();
    const String& message() const { return message_; }
    bool succeeded() const { return succeeded_; }

private:
    static constexpr size_t chunkSamples = 256;
    static void onBuffer(void* context, void* data, size_t length);
    bool finishFile(const String& partial);
    void recover();
    void release();
    void fail(const String& error);

    State state_ = State::Idle;
    bool mounted_ = false, finishedEvent_ = false, succeeded_ = false;
    uint32_t micStarted_ = 0, preparedAt_ = 0;
    int16_t* samples_ = nullptr;
    uint8_t* playback_ = nullptr;
    File file_;
    String partial_, message_, lastSaved_;
    size_t written_ = 0;
    std::atomic<size_t> completed_{0};
    std::atomic<uint16_t> peak_{0};
    std::atomic<bool> requested_{false}, captureFinished_{false}, captureError_{false};
};
