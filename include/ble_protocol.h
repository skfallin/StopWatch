#pragma once

#include <string>
#include "policy.h"

constexpr size_t maxBleAudioChunkBytes = 4096;

// JSON is decoded only after the complete UTF-8 byte sequence has arrived.
class BleLine {
public:
    enum class Result { More, Complete, Invalid };
    Result push(char byte) {
        if (byte == '\n') return value_.empty() ? Result::Invalid : Result::Complete;
        if (byte == '\0' || value_.size() >= policy::maxConfigBytes) return Result::Invalid;
        value_ += byte;
        return Result::More;
    }
    const std::string& value() const { return value_; }
    void clear() { value_.clear(); }
private:
    std::string value_;
};
