#pragma once
#include <array>
#include <string_view>

namespace regional {
struct Zone { const char* id; const char* rule; };
// Explicit rules keep the embedded firmware independent of an IANA database.
inline constexpr std::array<Zone,16> zones={{
    {"UTC", "UTC0"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Sao_Paulo", "<-03>3"},
    {"Asia/Dubai", "<+04>-4"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Tokyo", "JST-9"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"}
}};
inline const char* rule(std::string_view id) {
    for(const auto& zone:zones) if(id==zone.id) return zone.rule;
    return nullptr;
}
inline bool language(std::string_view value) { return value=="en" || value=="it"; }
}
