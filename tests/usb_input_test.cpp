#include "usb_input.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace usb_input;
JsonDocument command(Queue& q,const char* body) {
    JsonDocument doc;assert(!deserializeJson(doc,body));doc["session"]=q.session();return doc;
}
Report take(Queue& q,uint32_t now=10) {
    Report report;assert(q.next(report,now));q.sent(now);return report;
}
void release(Queue& q,uint32_t now=10) {
    auto keyboard=take(q,now),mouse=take(q,now);
    assert((keyboard.id==1 && keyboard.bytes==std::array<uint8_t,8>{}));
    assert((mouse.id==2 && mouse.bytes==std::array<uint8_t,8>{}));
}
int main() {
    for(uint32_t c=32;c<=126;++c) assert(ascii(c,false).key);
    assert(ascii('@',false).key==0x1f && ascii('@',false).modifiers==2);
    assert(ascii('@',true).key==0x33 && ascii('@',true).modifiers==64);
    assert(ascii('<',true).key==0x64 && ascii('>',true).modifiers==2);
    assert(ascii(0xe8,true).key==0x2f && !ascii(0xe8,false).key);
    assert(ascii(0x20ac,true).modifiers==64);
    std::array<Stroke,maxText> strokes{};size_t count=0;
    assert(text("Caffè è già qui: 10€",true,strokes,count));
    for(auto invalid:{"hello😀","x\r","\xc3","\xc0\xaf","x\xe2\x82","~","`"}) assert(!text(invalid,true,strokes,count));
    assert(!text(std::string_view("a\0b",3),false,strokes,count));
    assert(!text("",false,strokes,count));
    assert(text(std::string(96,'x'),false,strokes,count) && count==96);
    assert(!text(std::string(97,'x'),false,strokes,count));

    Queue q;
    Motion motion{};
    uint8_t bytes[]={1,0x78,0x56,0x34,0x12,12,252,1};
    assert(decodeMotion(bytes,sizeof(bytes),motion));
    assert(motion.session==0x12345678 && motion.x==12 && motion.y==-4 && motion.wheel==1);
    assert(!decodeMotion(bytes,7,motion));bytes[0]=2;assert(!decodeMotion(bytes,8,motion));bytes[0]=1;
    bytes[5]=128;assert(!decodeMotion(bytes,8,motion));
    assert(!q.motion({q.session(),2,3,0},0));
    auto input=command(q,R"({"kind":"click","button":"left"})");
    assert(q.accept(input.as<JsonObjectConst>(),0));
    q.state(true,true,true,0);release(q);
    assert(q.available());
    assert(!q.motion({q.session()-1,2,3,0},10));
    assert(q.motion({q.session(),2,-3,1},10));
    auto fast=take(q);assert(fast.id==2 && fast.bytes[1]==2 && int8_t(fast.bytes[2])==-3);
    input=command(q,R"({"kind":"text","text":"Ab","layout":"us"})");
    assert(!q.accept(input.as<JsonObjectConst>(),10));assert(q.count()==4);
    auto report=take(q);assert(report.id==1 && report.bytes[0]==2 && report.bytes[2]==4);
    report=take(q);assert((report.bytes==std::array<uint8_t,8>{}));
    report=take(q);assert(report.bytes[0]==0 && report.bytes[2]==5);
    take(q);assert(!q.busy());
    input=command(q,R"({"kind":"click","button":"right"})");
    assert(!q.accept(input.as<JsonObjectConst>(),10));
    assert(take(q).bytes[0]==2);assert(take(q).bytes[0]==0);
    input=command(q,R"({"kind":"move","x":-127,"y":127,"wheel":-3})");
    assert(!q.accept(input.as<JsonObjectConst>(),10));report=take(q);
    assert(report.id==2 && int8_t(report.bytes[1])==-127 && int8_t(report.bytes[2])==127 && int8_t(report.bytes[3])==-3);
    for(auto invalid:{
        R"({"kind":"move","x":128,"y":0,"wheel":0})",
        R"({"kind":"move","x":0.5,"y":0,"wheel":0})",
        R"({"kind":"move","x":0,"y":0})",
        R"({"kind":"click","button":"middle"})",
        R"({"kind":"text","text":"a😀","layout":"us"})",
        R"({"kind":"text","text":12,"layout":"us"})",
        R"({"kind":"text","text":"abc","layout":"bad"})",
        R"({"kind":"key","key":"anything"})",
        R"({"kind":"exec","text":"unsafe"})"}) {
        input=command(q,invalid);assert(q.accept(input.as<JsonObjectConst>(),10));assert(!q.count());
    }
    for(auto name:{"enter","escape","backspace","tab","delete","right","left","down","up"}) {
        input=command(q,R"({"kind":"key"})");input["key"]=name;
        assert(!q.accept(input.as<JsonObjectConst>(),10));assert(take(q).bytes[2]==key(name).key);take(q);
    }
    input=command(q,R"({"kind":"text","layout":"us"})");input["text"]=std::string(96,'x');
    assert(!q.accept(input.as<JsonObjectConst>(),10));assert(q.count()==192);
    assert(q.accept(input.as<JsonObjectConst>(),10));assert(q.count()==192);
    const auto oldSession=q.session();
    q.state(false,true,true,20);assert(!q.count() && !q.available());release(q,20);
    q.state(true,true,true,30);release(q,30);
    input["session"]=oldSession;assert(q.accept(input.as<JsonObjectConst>(),30));
    input["session"]=q.session();assert(!q.accept(input.as<JsonObjectConst>(),30));
    q.state(true,true,false,40);assert(!q.count());release(q,40);assert(!q.available());
    q.state(true,true,true,50);release(q,50);assert(q.available());
    input["session"]=q.session();assert(!q.accept(input.as<JsonObjectConst>(),50));
    // A stalled endpoint cannot replay old input when the host resumes.
    report={};assert(q.next(report,1550));assert(q.fault() && !q.count() && !q.available());
    release(q,1550);
    assert(!q.available());q.state(false,true,true,1600);release(q,1600);
    q.state(true,true,true,1700);release(q,1700);assert(q.available());
    input["session"]=q.session();assert(!q.accept(input.as<JsonObjectConst>(),1700));
    q.state(true,false,true,1800);assert(!q.busy() && !q.available());
    q.state(true,true,true,std::numeric_limits<uint32_t>::max()-20);release(q,std::numeric_limits<uint32_t>::max()-20);
    input["session"]=q.session();assert(!q.accept(input.as<JsonObjectConst>(),std::numeric_limits<uint32_t>::max()-20));
    assert(q.next(report,10) && !q.fault());q.failed(11);release(q,12);assert(q.fault() && !q.busy());
    std::cout<<"USB input: layout, validation, ordering, backpressure, session gates and release passed\n";
}
