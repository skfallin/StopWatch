#pragma once
#include "i18n.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace policy {
constexpr uint32_t sampleRate = 16000;
// shortcut: 60 secondi per nota in PSRAM; passare a un ring buffer per registrazioni piu lunghe.
constexpr size_t noteSeconds = 60;
constexpr size_t maxNotes = 64;
constexpr size_t maxSamples = sampleRate * noteSeconds;
constexpr size_t maxFavorites = 8;
constexpr size_t maxConfigBytes = 12288;
constexpr size_t maxFavoriteMessageBytes = 2048;

inline uint8_t speakerVolume(uint8_t volume) {
    const unsigned level=volume>200?200:volume;
    return uint8_t((level*255+100)/200);
}

inline std::string_view utf8Prefix(std::string_view value, size_t limit) {
    if (value.size() <= limit) return value;
    while (limit && (static_cast<unsigned char>(value[limit]) & 0xc0) == 0x80) --limit;
    return value.substr(0, limit);
}

inline bool brokerHost(std::string_view value) {
    if (value.empty()) return true;
    if (value.size() > 253 || value.front() == '.' || value.back() == '.') return false;
    for (char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
    return true;
}

inline bool identifier(std::string_view value) {
    if (value.empty()) return false;
    for (char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

inline bool entityId(std::string_view value) {
    const auto dot = value.find('.');
    return value.size() <= 96 && dot != std::string_view::npos &&
           identifier(value.substr(0, dot)) && identifier(value.substr(dot + 1));
}

inline std::string_view service(std::string_view entity) {
    const auto domain = entity.substr(0, entity.find('.'));
    if (domain == "light" || domain == "switch" || domain == "input_boolean" || domain == "fan") return "toggle";
    if (domain == "scene" || domain == "script") return "turn_on";
    if (domain == "button" || domain == "input_button") return "press";
    return {};
}

inline bool confirmAction(std::string_view entity) {
    return !service(entity).empty();
}

inline bool usableState(std::string_view entity, std::string_view state) {
    if (service(entity)=="toggle") return state=="on" || state=="off";
    return !state.empty() && state!="unknown" && state!="unavailable";
}

inline bool baseUrl(std::string_view value) {
    if (value.empty()) return true;
    size_t start;
    if (value.substr(0, 7) == "http://") start = 7;
    else if (value.substr(0, 8) == "https://") start = 8;
    else return false;
    if (value.size() > 256 || start == value.size()) return false;
    const auto slash = value.find('/', start);
    const auto authority = value.substr(start, slash == std::string_view::npos ? value.size() - start : slash - start);
    if (authority.empty()) return false;
    for (char c : value) {
        if (static_cast<unsigned char>(c) <= 32 || c == 127 || c == '@' || c == '?' || c == '#' || c == '\\') return false;
    }
    for (char c : authority) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':' || c == '[' || c == ']')) return false;
    }
    return true;
}

inline bool noteFilename(std::string_view name) {
    if (name.size() != 25 || name.substr(0, 5) != "note-" || name.substr(21) != ".wav") return false;
    for (size_t i = 5; i < 21; ++i) {
        const char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

inline std::array<uint8_t, 44> wavHeader(uint32_t pcmBytes) {
    std::array<uint8_t, 44> header{};
    const auto put = [&header](size_t at, uint32_t value, size_t bytes) {
        for (size_t i = 0; i < bytes; ++i) header[at + i] = uint8_t(value >> (8 * i));
    };
    const char riff[] = "RIFF", wave[] = "WAVE", fmt[] = "fmt ", data[] = "data";
    for (size_t i = 0; i < 4; ++i) {
        header[i] = riff[i]; header[8 + i] = wave[i];
        header[12 + i] = fmt[i]; header[36 + i] = data[i];
    }
    put(4, pcmBytes + 36, 4);
    put(16, 16, 4);
    put(20, 1, 2);
    put(22, 1, 2);
    put(24, sampleRate, 4);
    put(28, sampleRate * 2, 4);
    put(32, 2, 2);
    put(34, 16, 2);
    put(40, pcmBytes, 4);
    return header;
}
}
