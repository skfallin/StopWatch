#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace phone {
enum class Kind { Favorite, Music, Artwork, Invalid };
inline Kind kind(std::string_view value) {
    return value=="favorite"?Kind::Favorite:value=="music"?Kind::Music:value=="artwork"?Kind::Artwork:Kind::Invalid;
}
inline const char* name(Kind value) {
    return value==Kind::Favorite?"favorite":value==Kind::Music?"music":value==Kind::Artwork?"artwork":"";
}
constexpr uint32_t timeoutMs=12000;
inline size_t resultLimit(Kind value) { return value==Kind::Artwork?10240:value==Kind::Music?4096:2048; }
struct Job {
    Kind kind=Kind::Invalid;
    uint32_t request=0,startedAt=0;
    std::string payload;
    bool sent=false,complete=false;
};
class Relay {
public:
    void enable(bool enabled) { enabled_=enabled;if(!enabled) jobs_={}; }
    bool enabled() const { return enabled_; }
    bool pending() const {
        for(const auto& job:jobs_) if(!job.payload.empty()) return true;
        return false;
    }
    bool enqueue(Kind kind,uint32_t request,std::string payload,uint32_t now) {
        expire(now);
        if(!enabled_ || kind==Kind::Invalid || payload.empty() || payload.size()>2048) return false;
        auto& job=jobs_[size_t(kind)];
        if(!job.payload.empty()) return false;
        job={kind,request,now,std::move(payload),false,false};return true;
    }
    Job* next(uint32_t now) {
        expire(now);
        for(auto& job:jobs_) if(!job.payload.empty() && !job.sent && !job.complete) return &job;
        return nullptr;
    }
    bool finish(Kind kind,uint32_t request,std::string payload,uint32_t now) {
        expire(now);
        if(!enabled_ || kind==Kind::Invalid || payload.empty() || payload.size()>resultLimit(kind)) return false;
        auto& job=jobs_[size_t(kind)];
        if(job.payload.empty() || !job.sent || job.complete || job.request!=request) return false;
        job.payload=std::move(payload);job.complete=true;return true;
    }
    bool take(Kind kind,std::string& payload) {
        if(kind==Kind::Invalid) return false;
        auto& job=jobs_[size_t(kind)];
        if(!job.complete) return false;
        payload=std::move(job.payload);job={};return true;
    }
private:
    void expire(uint32_t now) {
        for(auto& job:jobs_) if(!job.payload.empty() && uint32_t(now-job.startedAt)>=timeoutMs) job={};
    }
    bool enabled_=false;
    std::array<Job,3> jobs_{};
};
}
