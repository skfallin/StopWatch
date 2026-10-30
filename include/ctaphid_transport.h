#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace keylab {
constexpr size_t maxMessage=1300;
inline uint32_t channel(const uint8_t* bytes) { return uint32_t(bytes[0])<<24|uint32_t(bytes[1])<<16|uint32_t(bytes[2])<<8|bytes[3]; }
inline void channel(uint8_t* bytes,uint32_t cid) { for(unsigned i=0;i<4;++i) bytes[i]=cid>>(24-8*i); }
class Transport {
public:
    enum class Kind { None, Message, Init, Ping, Cancel, Wink, Error };
    struct Event { Kind kind=Kind::None;uint32_t cid=0;uint8_t command=0,error=0;size_t size=0; };
    Event expire(uint32_t now) {
        if(!current_ || uint32_t(now-last_)<750) return {};
        const auto cid=current_;clear();return error(cid,5);
    }
    Event receive(const uint8_t* bytes,size_t size,uint32_t now) {
        if(size!=64) return {};
        const uint32_t cid=channel(bytes);const uint8_t command=bytes[4];
        if(command==0x86) {
            const size_t length=(size_t(bytes[5])<<8)|bytes[6];
            if(!cid) return error(cid,0x0b);
            if(length!=8) return error(cid,3);
            if(cid!=0xffffffff && !known(cid)) return error(cid,0x0b);
            // INIT re-synchronizes only its own channel.
            if(cid==current_) clear();
            memcpy(nonce_.data(),bytes+7,8);return {Kind::Init,cid,command,0,8};
        }
        if(!known(cid) || cid==0xffffffff) return error(cid,0x0b);
        if(command==0x91) {
            if(bytes[5] || bytes[6]) return error(cid,3);
            if(cid==current_) clear();
            return {Kind::Cancel,cid,command};
        }
        if(current_ && uint32_t(now-last_)>=750) { const auto expired=current_;clear();if(cid==expired) return error(cid,5); }
        if(current_ && current_!=cid) return error(cid,6);
        if(command&0x80) {
            if(current_) return error(cid,6);
            const size_t length=(size_t(bytes[5])<<8)|bytes[6];
            if(length>data_.size()) return error(cid,3);
            if(command!=0x81 && command!=0x90 && command!=0x88) return error(cid,1);
            if(command==0x88 && length) return error(cid,3);
            current_=cid;command_=command;expected_=length;size_=std::min(length,size_t(57));sequence_=0;
            memcpy(data_.data(),bytes+7,size_);
        } else {
            if(!current_ || command!=sequence_ || sequence_>=128) { clear();return error(cid,4); }
            const auto count=std::min(size_t(59),expected_-size_);memcpy(data_.data()+size_,bytes+5,count);size_+=count;++sequence_;
        }
        last_=now;
        if(size_<expected_) return {};
        const Event result{command_==0x81?Kind::Ping:command_==0x88?Kind::Wink:Kind::Message,cid,command_,0,size_};
        clear();return result;
    }
    bool addChannel(uint32_t cid) { if(!cid || cid==0xffffffff || known(cid)) return false;channels_[next_++%channels_.size()]=cid;return true; }
    bool known(uint32_t cid) const { return cid && std::find(channels_.begin(),channels_.end(),cid)!=channels_.end(); }
    const uint8_t* nonce() const { return nonce_.data(); }
    uint8_t* data() { return data_.data(); }
    void reset() { clear();channels_.fill(0);data_.fill(0);nonce_.fill(0);next_=0; }
private:
    static Event error(uint32_t cid,uint8_t value) { return {Kind::Error,cid,0xbf,value}; }
    void clear() { current_=0;expected_=size_=0;command_=sequence_=0; }
    std::array<uint8_t,maxMessage> data_{};
    std::array<uint32_t,8> channels_{};
    std::array<uint8_t,8> nonce_{};
    uint32_t current_=0,last_=0,next_=0;
    size_t expected_=0,size_=0;
    uint8_t command_=0,sequence_=0;
};
}
