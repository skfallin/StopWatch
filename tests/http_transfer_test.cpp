#include "http_transfer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main() {
    uint8_t buffer[3];
    uint32_t now=0;
    const auto clock=[&] { return now; };
    const auto retry=[](int code) { return code==-3; };
    const auto pause=[&] { now+=1000; };
    const std::string input="hello";
    std::string output;
    std::vector<int> chunks{2,-3,3};size_t next=0,source=0;
    auto read=[&](uint8_t* bytes,size_t capacity) {
        const int count=chunks.at(next++);
        if(count>0) { assert(size_t(count)<=capacity);std::memcpy(bytes,input.data()+source,count);source+=count; }
        return count;
    };
    const auto write=[&](size_t offset,const uint8_t* bytes,size_t count) {
        assert(offset==output.size());output.append(reinterpret_cast<const char*>(bytes),count);return true;
    };
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,read,write,clock,retry,pause)==transfer::Result::Complete);
    assert(output==input && next==3);
    chunks={2,0};next=source=0;output.clear();
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,read,write,clock,retry,pause)==transfer::Result::Closed);
    assert(output=="he");
    chunks={2,-3,3};
    now=0;size_t calls=0;
    const auto stalled=[&](uint8_t*,size_t) { ++calls;return -3; };
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,stalled,write,clock,retry,pause)==transfer::Result::Timeout);
    assert(now==15000 && calls==15);
    for(const int code:{0,-1}) {
        now=0;
        const auto stopped=[&](uint8_t*,size_t) { return code; };
        assert(transfer::receiveBody(buffer,sizeof(buffer),5,stopped,write,clock,retry,pause)==(code==0?transfer::Result::Closed:transfer::Result::ReadError));
    }
    const auto one=[](uint8_t* bytes,size_t) { *bytes=7;return 1; };
    const auto failed=[](size_t,const uint8_t*,size_t) { return false; };
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,one,failed,clock,retry,pause)==transfer::Result::WriteError);
    const auto oversized=[](uint8_t*,size_t capacity) { return int(capacity+1); };
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,oversized,write,clock,retry,pause)==transfer::Result::ReadError);
    now=0;size_t written=0;
    const auto slow=[&](uint8_t* bytes,size_t) { now+=4000;*bytes=9;return 1; };
    const auto counted=[&](size_t offset,const uint8_t*,size_t count) { assert(offset==written);written+=count;return true; };
    assert(transfer::receiveBody(buffer,sizeof(buffer),100,slow,counted,clock,retry,pause)==transfer::Result::Timeout);
    assert(now==120000 && written==29);
    now=std::numeric_limits<uint32_t>::max()-500;next=source=0;output.clear();
    assert(transfer::receiveBody(buffer,sizeof(buffer),5,read,write,clock,retry,pause)==transfer::Result::Complete);
    assert(output==input && now==499);
    std::cout<<"OK: letture parziali, pausa recuperata, timeout limitati, chiusura, errore archivio e rollover\n";
}
