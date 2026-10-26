#include "phone_relay.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using namespace phone;
    Relay relay;
    assert(!relay.pending());
    assert(kind("favorite")==Kind::Favorite && kind("music")==Kind::Music && kind("artwork")==Kind::Artwork);
    assert(kind("music/request")==Kind::Invalid && kind("../set/beep")==Kind::Invalid);
    assert(!relay.enqueue(Kind::Favorite,1,"request",0));
    relay.enable(true);
    assert(!relay.enqueue(Kind::Invalid,1,"request",0));
    assert(!relay.enqueue(Kind::Favorite,1,std::string(2049,'x'),0));
    assert(relay.enqueue(Kind::Favorite,1,"favorite",100));
    assert(relay.pending());
    assert(relay.enqueue(Kind::Music,2,"music",100));
    assert(relay.enqueue(Kind::Artwork,3,"artwork",100));
    assert(!relay.enqueue(Kind::Favorite,4,"overwrite",101));
    assert(!relay.finish(Kind::Music,2,"early",101));
    auto* first=relay.next(101);assert(first && first->kind==Kind::Favorite);first->sent=true;
    auto* second=relay.next(102);assert(second && second->kind==Kind::Music);second->sent=true;
    auto* third=relay.next(103);assert(third && third->kind==Kind::Artwork);third->sent=true;
    assert(!relay.next(104)); // Sent commands never reappear for retries.
    assert(!relay.finish(Kind::Favorite,99,"wrong id",105));
    assert(!relay.finish(Kind::Favorite,1,std::string(2049,'x'),105));
    assert(!relay.finish(Kind::Music,2,std::string(4097,'x'),105));
    assert(!relay.finish(Kind::Artwork,3,std::string(10241,'x'),105));
    assert(relay.finish(Kind::Favorite,1,"reply",106));
    assert(!relay.finish(Kind::Favorite,1,"duplicate",107));
    std::string result;
    assert(relay.take(Kind::Favorite,result) && result=="reply");
    assert(!relay.take(Kind::Favorite,result));
    assert(relay.enqueue(Kind::Favorite,4,"new",108));
    relay.enable(false);relay.enable(true);
    assert(!relay.pending());
    assert(!relay.next(109));
    assert(!relay.finish(Kind::Music,2,"previous connection",110));
    assert(relay.enqueue(Kind::Favorite,0,"zero is valid",1000));
    relay.next(1000)->sent=true;
    assert(!relay.finish(Kind::Favorite,0,"expired",13000));
    assert(!relay.next(13001));
    assert(!relay.pending());
    constexpr uint32_t boundary=std::numeric_limits<uint32_t>::max();
    assert(relay.enqueue(Kind::Music,boundary,"rollover",boundary-1000));
    relay.next(500)->sent=true;
    assert(relay.finish(Kind::Music,boundary,"reply",1000));
    assert(relay.take(Kind::Music,result) && result=="reply");
    assert(!relay.pending());
    std::cout<<"OK: ponte BLE, isolamento richieste, limiti, scadenza, riconnessione e nessuna ripetizione\n";
}
