#include "i18n.h"
#pragma once

#include <ArduinoJson.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace usb_input {
struct Stroke { uint8_t key=0, modifiers=0; };
struct Report {
    uint8_t id=0, size=0;
    std::array<uint8_t,8> bytes{};
};
struct Motion { uint32_t session; int x,y,wheel; };
inline bool decodeMotion(const uint8_t* bytes,size_t size,Motion& out) {
    if(size!=8 || bytes[0]!=1) return false;
    for(size_t i=5;i<8;++i) if(bytes[i]==128) return false;
    out={uint32_t(bytes[1])|(uint32_t(bytes[2])<<8)|(uint32_t(bytes[3])<<16)|(uint32_t(bytes[4])<<24),
        int8_t(bytes[5]),int8_t(bytes[6]),int8_t(bytes[7])};
    return true;
}
constexpr size_t maxText=96, maxReports=256;

inline Stroke ascii(uint32_t c,bool italian) {
    if(c>='a' && c<='z') return {uint8_t(4+c-'a'),0};
    if(c>='A' && c<='Z') return {uint8_t(4+c-'A'),2};
    if(c>='1' && c<='9') return {uint8_t(0x1e + c-'1'),0};
    if(c=='0') return {0x27,0};
    if(c==' ') return {0x2c,0};
    if(c=='\n') return {0x28,0};
    if(c=='\t') return {0x2b,0};
    struct Symbol { char value; uint8_t key, modifiers; };
    constexpr Symbol us[]={
        {'!',0x1e,2},{'"',0x34,2},{'#',0x20,2},{'$',0x21,2},{'%',0x22,2},{'&',0x24,2},
        {'\'',0x34,0},{'(',0x26,2},{')',0x27,2},{'*',0x25,2},{'+',0x2e,2},{',',0x36,0},
        {'-',0x2d,0},{'.',0x37,0},{'/',0x38,0},{':',0x33,2},{';',0x33,0},{'<',0x36,2},
        {'=',0x2e,0},{'>',0x37,2},{'?',0x38,2},{'@',0x1f,2},{'[',0x2f,0},{'\\',0x31,0},
        {']',0x30,0},{'^',0x23,2},{'_',0x2d,2},{'`',0x35,0},{'{',0x2f,2},{'|',0x31,2},
        {'}',0x30,2},{'~',0x35,2}};
    constexpr Symbol it[]={
        {'!',0x1e,2},{'"',0x1f,2},{'#',0x34,64},{'$',0x21,2},{'%',0x22,2},{'&',0x23,2},
        {'\'',0x2d,0},{'(',0x25,2},{')',0x26,2},{'*',0x30,2},{'+',0x30,0},{',',0x36,0},
        {'-',0x38,0},{'.',0x37,0},{'/',0x24,2},{':',0x37,2},{';',0x36,2},{'<',0x64,0},
        {'=',0x27,2},{'>',0x64,2},{'?',0x2d,2},{'@',0x33,64},{'[',0x2f,64},{'\\',0x35,0},
        {']',0x30,64},{'^',0x2e,2},{'_',0x38,2},{'{',0x2f,66},{'|',0x35,2},{'}',0x30,66}};
    if(italian) { for(auto s:it) if(c==uint8_t(s.value)) return {s.key,s.modifiers}; }
    else { for(auto s:us) if(c==uint8_t(s.value)) return {s.key,s.modifiers}; }
    if(italian) switch(c) {
        case 0xe0:return {0x34,0}; case 0xe8:return {0x2f,0}; case 0xe9:return {0x2f,2};
        case 0xec:return {0x2e,0}; case 0xf2:return {0x33,0}; case 0xf9:return {0x31,0};
        case 0xa3:return {0x20,2}; case 0x20ac:return {0x08,64};
    }
    return {};
}

// Reject the whole insertion before producing reports, including unsupported Unicode.
inline bool text(std::string_view value,bool italian,std::array<Stroke,maxText>& strokes,size_t& count) {
    count=0;
    for(size_t offset=0;offset<value.size();) {
        if(count==maxText) return false;
        const uint8_t a=value[offset++]; uint32_t c=a;
        if(a>=0x80) {
            if(a==0xc2 || a==0xc3) {
                if(offset==value.size()) return false;
                const uint8_t b=value[offset++]; if((b&0xc0)!=0x80) return false;
                c=((a&31)<<6)|(b&63);
            } else if(a==0xe2 && value.size()-offset>=2 && uint8_t(value[offset])==0x82 && uint8_t(value[offset+1])==0xac) {
                c=0x20ac;offset+=2;
            } else return false;
        }
        const auto stroke=ascii(c,italian);
        if(!stroke.key) return false;
        strokes[count++]=stroke;
    }
    return count>0;
}

inline Stroke key(std::string_view name) {
    if(name=="enter") return {0x28,0}; if(name=="escape") return {0x29,0};
    if(name=="backspace") return {0x2a,0}; if(name=="tab") return {0x2b,0};
    if(name=="delete") return {0x4c,0}; if(name=="right") return {0x4f,0};
    if(name=="left") return {0x50,0}; if(name=="down") return {0x51,0};
    if(name=="up") return {0x52,0}; return {};
}

class Queue {
public:
    void state(bool active,bool usb,bool phone,uint32_t now) {
        if(active==active_ && usb==usb_ && phone==phone_) return;
        active_=active;usb_=usb;phone_=phone;
        ++session_;fault_=false;clear();release_=usb?2:0;lastProgress_=now;
    }
    bool active() const { return active_; }
    bool connected() const { return usb_; }
    bool available() const { return active_ && usb_ && phone_ && !fault_ && !release_; }
    bool busy() const { return count_ || release_; }
    bool fault() const { return fault_; }
    uint32_t session() const { return session_; }
    size_t count() const { return count_; }
    bool motion(const Motion& motion,uint32_t now) {
        if(!available() || motion.session!=session_ || count_==maxReports) return false;
        push(mouse(0,motion.x,motion.y,motion.wheel));
        if(!started_) { started_=true;lastProgress_=now; }
        return true;
    }
    const char* accept(JsonObjectConst value,uint32_t now) {
        if(!available()) return i18n::text("Apri Ponte USB sullo StopWatch e collega il cavo dati");
        if(!value["session"].is<uint32_t>() || value["session"].as<uint32_t>()!=session_) return i18n::text("Sessione USB cambiata: riprova il comando");
        const std::string_view kind=value["kind"] | "";
        if(kind=="move") {
            for(const auto field:{"x","y","wheel"})
                if(!value[field].is<int>() || value[field].as<int>() < -127 || value[field].as<int>()>127) return i18n::text("Movimento non valido");
            if(count_==maxReports) return i18n::text("Coda USB piena");
            push(mouse(0,value["x"].as<int>(),value["y"].as<int>(),value["wheel"].as<int>()));
        } else if(kind=="click") {
            const std::string_view button=value["button"] | "";
            if(button!="left" && button!="right") return i18n::text("Clic non valido");
            if(count_>maxReports-2) return i18n::text("Coda USB piena");
            push(mouse(button=="left"?1:2));push(mouse(0));
        } else if(kind=="key" || kind=="text") {
            std::array<Stroke,maxText> strokes{};size_t count=1;
            if(kind=="key") { strokes[0]=key(value["key"] | "");if(!strokes[0].key) return i18n::text("Tasto non valido"); }
            else {
                const std::string_view layout=value["layout"] | "";
                if(layout!="it" && layout!="us") return i18n::text("Layout tastiera non valido");
                if(!value["text"].is<const char*>()) return i18n::text("Testo non valido");
                const auto input=value["text"].as<JsonString>();
                if(!text({input.c_str(),input.size()},layout=="it",strokes,count))
                    return i18n::text("Carattere non disponibile nel layout scelto o testo oltre 96 caratteri");
            }
            if(count*2>maxReports-count_) return i18n::text("Coda USB piena");
            for(size_t i=0;i<count;++i) { push(keyboard(strokes[i]));push(keyboard({})); }
        } else return i18n::text("Comando USB non valido");
        if(!started_) { started_=true;lastProgress_=now; }
        return nullptr;
    }
    bool press(Stroke stroke,uint32_t now) {
        if(!available() || !stroke.key || count_>maxReports-2) return false;
        push(keyboard(stroke));push(keyboard({}));
        if(!started_) { started_=true;lastProgress_=now; }
        return true;
    }
    bool next(Report& out,uint32_t now) {
        if(count_ && uint32_t(now-lastProgress_)>=1500) failed(now);
        if(release_) { out=release_==2?keyboard({}):mouse(0);return true; }
        if(!count_) return false;
        out=reports_[head_];return true;
    }
    void sent(uint32_t now) {
        if(release_) --release_;
        else if(count_) { head_=(head_+1)%maxReports;--count_; }
        lastProgress_=now;if(!count_) started_=false;
    }
    void failed(uint32_t now) { clear();fault_=true;++session_;release_=usb_?2:0;lastProgress_=now; }
private:
    static Report keyboard(Stroke stroke) { Report r{1,8,{}};r.bytes[0]=stroke.modifiers;r.bytes[2]=stroke.key;return r; }
    static Report mouse(uint8_t button,int x=0,int y=0,int wheel=0) {
        return {2,5,{button,uint8_t(x),uint8_t(y),uint8_t(wheel),0}};
    }
    void clear() { head_=count_=0;started_=false; }
    void push(Report report) { reports_[(head_+count_)%maxReports]=report;++count_; }
    std::array<Report,maxReports> reports_{};
    size_t head_=0,count_=0;
    uint32_t session_=1,lastProgress_=0;
    uint8_t release_=0;
    bool active_=false,usb_=false,phone_=false,fault_=false,started_=false;
};
}
