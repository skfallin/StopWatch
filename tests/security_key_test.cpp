#include "ctaphid_transport.h"
#include "key_presence.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace keylab;
using Result=Presence::Result;
void presence() {
    Presence gate;gate.begin(1,100);
    assert(gate.update(101,false,true,true)==Result::Waiting);
    assert(gate.update(102,false,false,true)==Result::Waiting); // B inherited from opening the app cannot confirm.
    assert(gate.update(103,false,false,true)==Result::Waiting);
    assert(gate.update(104,false,true,true)==Result::Waiting);
    assert(gate.update(105,false,false,true)==Result::Confirm);
    gate.begin(2,200);assert(gate.update(201,true,false,true)==Result::Cancel);
    gate.begin(3,300);assert(gate.update(301,false,false,false)==Result::Cancel);
    gate.begin(4,400);assert(gate.update(25400,false,false,true)==Result::Timeout);
    gate.begin(5,std::numeric_limits<uint32_t>::max()-100);
    assert(gate.update(24900,false,false,true)==Result::Timeout);
}
void transportTest() {
    Transport t;uint8_t bytes[64]={};channel(bytes,0xffffffff);bytes[4]=0x86;bytes[6]=8;
    for(unsigned i=0;i<8;++i) bytes[7+i]=i+1;
    auto event=t.receive(bytes,64,0);assert(event.kind==Transport::Kind::Init);
    for(unsigned i=0;i<8;++i) assert(t.nonce()[i]==i+1);
    assert(!t.addChannel(0) && !t.addChannel(0xffffffff));assert(t.addChannel(42) && !t.addChannel(42));assert(t.addChannel(43));
    channel(bytes,44);assert(t.receive(bytes,64,0).error==0x0b);
    channel(bytes,42);bytes[4]=0x90;bytes[5]=0;bytes[6]=120;memset(bytes+7,0x12,57);
    assert(t.receive(bytes,63,1).kind==Transport::Kind::None);
    assert(t.receive(bytes,64,1).kind==Transport::Kind::None);
    channel(bytes,43);assert(t.receive(bytes,64,2).error==6);
    channel(bytes,42);bytes[4]=0;memset(bytes+5,0x34,59);
    assert(t.receive(bytes,64,3).kind==Transport::Kind::None);
    bytes[4]=1;memset(bytes+5,0x56,59);event=t.receive(bytes,64,4);
    assert(event.kind==Transport::Kind::Message && event.size==120 && event.cid==42);
    for(unsigned i=0;i<120;++i) assert(t.data()[i]==(i<57?0x12:i<116?0x34:0x56));
    bytes[4]=0x90;bytes[5]=0xff;bytes[6]=0xff;assert(t.receive(bytes,64,5).error==3);
    bytes[5]=0;bytes[6]=60;assert(t.receive(bytes,64,6).kind==Transport::Kind::None);
    bytes[4]=1;assert(t.receive(bytes,64,7).error==4); // Wrong sequence clears only the assembly.
    bytes[4]=0x90;bytes[6]=60;assert(t.receive(bytes,64,8).kind==Transport::Kind::None);
    bytes[4]=0;assert(t.receive(bytes,64,758).error==5);
    bytes[4]=0x90;bytes[6]=60;assert(t.receive(bytes,64,759).kind==Transport::Kind::None);
    assert(t.expire(1508).kind==Transport::Kind::None);assert(t.expire(1509).error==5);
    bytes[4]=0x91;bytes[5]=bytes[6]=0;assert(t.receive(bytes,64,759).kind==Transport::Kind::Cancel);
    bytes[4]=0x81;bytes[6]=0;event=t.receive(bytes,64,760);assert(event.kind==Transport::Kind::Ping && event.size==0);
    bytes[4]=0x88;assert(t.receive(bytes,64,761).kind==Transport::Kind::Wink);
    bytes[6]=1;assert(t.receive(bytes,64,762).error==3);
    bytes[4]=0x83;assert(t.receive(bytes,64,763).error==1); // CTAP1 is not exposed.
    t.reset();assert(!t.known(42));
}
int main() { presence();transportTest();std::cout<<"OK: fresh physical presence, denial, timeout, framing, channels, cancellation and resynchronization\n"; }
