#include "ui_sounds.h"
#include "ui_sound_catalog.h"
#include "ui_sound_data.h"
#include "user_archive.h"
#include <LittleFS.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <zlib.h>

int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream input(argv[1],std::ios::binary);
    const std::vector<uint8_t> audio{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    assert(audio.size()==sfx::catalog::archiveBytes);
    const auto protectUsers=[] {
        LittleFS.files["/notes/note-0123456789abcdef.wav"]={1,2,3,4,5};
        LittleFS.files["/badge.bin"]={6,7,8,9};
    };
    const auto usersUnchanged=[] {
        assert((LittleFS.files.at("/notes/note-0123456789abcdef.wav")==std::vector<uint8_t>{1,2,3,4,5}));
        assert((LittleFS.files.at("/badge.bin")==std::vector<uint8_t>{6,7,8,9}));
    };
    protectUsers();
#ifdef SFX_SEED_ARCHIVE
    assert(sfx::begin(1));assert(LittleFS.files.at("/ui-sounds.bin")==audio);
    assert(LittleFS.writtenBytes==audio.size() && !LittleFS.exists("/ui-sounds.tmp"));usersUnchanged();
    LittleFS.writtenBytes=0;
    assert(sfx::begin(1) && LittleFS.writtenBytes==0); // A completed migration is not rewritten.
#else
    assert(!sfx::begin(1) && !sfx::ready());usersUnchanged();
    assert(sfx::play(1,sfx::Cue::Select,200,true) && M5.Speaker.tones==1 && M5.Speaker.volume==255);
    sfx::stop();
    LittleFS.files["/ui-sounds.bin"]=audio;
    assert(sfx::begin(1));
#endif
    assert(sfx::ready());
    for(uint8_t pack=1;pack<sfx::packIds.size();++pack) {
        assert(sfx::prepare(pack));LittleFS.readBytes=0;
        for(unsigned cue=0;cue<unsigned(sfx::Cue::Count);++cue) {
            assert(sfx::play(pack,sfx::Cue(cue),200,true));
            const auto& original=sfx::data::packs[pack-1][cue];
            std::vector<uint8_t> pcm(original.decodedBytes);uLongf bytes=pcm.size();
            assert(uncompress(pcm.data(),&bytes,original.bytes,original.size)==Z_OK);
            assert(M5.Speaker.sampleRate==16000 && M5.Speaker.volume==255);
            assert(bytes==M5.Speaker.pcm.size()*2 && std::memcmp(pcm.data(),M5.Speaker.pcm.data(),bytes)==0);
        }
        assert(LittleFS.readBytes==0); // Every cue in the cached pack plays without filesystem reads.
    }
    sfx::stop();
    M5.Speaker.playing=true;
    assert(!sfx::play(1,sfx::Cue::Select,200,true));M5.Speaker.playing=false;
    assert(!sfx::play(0,sfx::Cue::Select,200,true) && !sfx::play(13,sfx::Cue::Select,200,true));
    assert(!sfx::play(1,sfx::Cue::Count,200,true) && !sfx::play(1,sfx::Cue::Select,0,true));
    fakeMillis+=100;assert(sfx::play(1,sfx::Cue::Select,200,false));
    ++fakeMillis;assert(!sfx::play(1,sfx::Cue::Select,200,false));
    fakeMillis+=90;assert(sfx::play(1,sfx::Cue::Select,200,false));

    LittleFS.files["/ui-sounds.bin"][100]^=0xff;
#ifdef SFX_SEED_ARCHIVE
    LittleFS.renameFailure=true;LittleFS.writtenBytes=0;
    assert(!sfx::begin(1));assert(!LittleFS.exists("/ui-sounds.tmp"));usersUnchanged();
    assert(LittleFS.files["/ui-sounds.bin"][100]!=audio[100]);
    LittleFS.renameFailure=false;LittleFS.writeLimit=LittleFS.writtenBytes+5000;
    assert(!sfx::begin(1) && !LittleFS.exists("/ui-sounds.tmp"));usersUnchanged();
    LittleFS.files.erase("/ui-sounds.bin");LittleFS.capacity=1000000;LittleFS.writeLimit=SIZE_MAX;LittleFS.writtenBytes=0;
    assert(!sfx::begin(1) && LittleFS.writtenBytes==0);usersUnchanged();
    LittleFS.capacity=8*1024*1024;
    assert(sfx::begin(1) && LittleFS.files["/ui-sounds.bin"]==audio);usersUnchanged();
#else
    assert(!sfx::begin(1) && !sfx::ready());usersUnchanged();
    LittleFS.files["/ui-sounds.bin"].pop_back();assert(!sfx::begin(1));
    LittleFS.files["/ui-sounds.bin"]=audio;
#endif
    failAudioAllocation=true;assert(!sfx::begin(1));failAudioAllocation=false;
    assert(sfx::begin(1));
    LittleFS.files["/notes/note-fedcba9876543210.wav.part"]={10,11};
    LittleFS.files["/notes/keep.txt"]={12};LittleFS.files["/badge.upload"]={13};
    LittleFS.removeFailure="/notes/note-0123456789abcdef.wav";
    assert(!archive::clearUserFiles(LittleFS) && LittleFS.files["/ui-sounds.bin"]==audio);
    LittleFS.removeFailure="";
    assert(archive::clearUserFiles(LittleFS));
    assert(LittleFS.files["/ui-sounds.bin"]==audio && LittleFS.files["/notes/keep.txt"]==std::vector<uint8_t>{12});
    assert(!LittleFS.exists("/badge.bin") && !LittleFS.exists("/badge.upload"));
    assert(!LittleFS.exists("/notes/note-0123456789abcdef.wav") && !LittleFS.exists("/notes/note-fedcba9876543210.wav.part"));
    sfx::prepare(0);sfx::stop();
    std::cout<<"OK: 96 PCM identici, cache senza I/O, SHA-256, fallback, migrazione atomica e conservazione dei file utente\n";
}
