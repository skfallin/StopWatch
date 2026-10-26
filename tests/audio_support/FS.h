#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs { class FS; }
class File {
public:
    struct State {
        fs::FS* owner;std::string path,name;bool directory=false;size_t position=0,index=0;
        std::vector<std::string> children;
    };
    File()=default;
    File(fs::FS* owner,const std::string& path,bool directory=false);
    explicit operator bool() const { return bool(state); }
    size_t size() const;
    size_t read(uint8_t* bytes,size_t count);
    size_t write(const uint8_t* bytes,size_t count);
    bool seek(size_t offset) { if(!state || offset>size()) return false;state->position=offset;return true; }
    bool isDirectory() const { return state && state->directory; }
    const char* name() const { return state?state->name.c_str():""; }
    File openNextFile();
    void flush() {}
    void close() { state.reset(); }
private:
    std::shared_ptr<State> state;
};
namespace fs {
class FS {
public:
    std::map<std::string,std::vector<uint8_t>> files;
    std::set<std::string> directories={"/","/notes"};
    size_t capacity=8*1024*1024,readBytes=0,writtenBytes=0,writeLimit=std::numeric_limits<size_t>::max();
    bool renameFailure=false;
    std::string removeFailure;
    bool exists(const char* path) const { return files.count(path) || directories.count(path); }
    File open(const char* path,const char* mode="r") {
        if(directories.count(path)) return File(this,path,true);
        if(mode[0]=='w') files[path]={};
        return files.count(path)?File(this,path):File();
    }
    bool remove(const char* path) { return removeFailure!=path && files.erase(path)!=0; }
    bool rename(const char* source,const char* target) {
        if(renameFailure || !files.count(source)) return false;
        files[target]=std::move(files[source]);files.erase(source);return true;
    }
    size_t totalBytes() const { return capacity; }
    size_t usedBytes() const { size_t size=0;for(const auto& item:files) size+=item.second.size();return size; }
};
}
inline File::File(fs::FS* owner,const std::string& path,bool directory) {
    state=std::make_shared<State>();state->owner=owner;state->path=path;state->name=path.substr(path.find_last_of('/')+1);state->directory=directory;
    if(directory) {
        const auto add=[&](const std::string& candidate) {
            const std::string prefix=path+"/";
            if(candidate.find(prefix)==0 && candidate.substr(prefix.size()).find('/')==std::string::npos) state->children.push_back(candidate);
        };
        for(const auto& item:owner->files) add(item.first);
        for(const auto& item:owner->directories) add(item);
    }
}
inline size_t File::size() const { return !state || state->directory?0:state->owner->files.at(state->path).size(); }
inline size_t File::read(uint8_t* bytes,size_t count) {
    if(!state || state->directory) return 0;
    auto& data=state->owner->files.at(state->path);
    count=std::min(count,data.size()-state->position);
    std::memcpy(bytes,data.data()+state->position,count);state->position+=count;state->owner->readBytes+=count;return count;
}
inline size_t File::write(const uint8_t* bytes,size_t count) {
    if(!state || state->directory) return 0;
    auto& fs=*state->owner;
    count=std::min(count,fs.writeLimit-fs.writtenBytes);
    auto& data=fs.files.at(state->path);data.resize(std::max(data.size(),state->position+count));
    std::memcpy(data.data()+state->position,bytes,count);state->position+=count;fs.writtenBytes+=count;return count;
}
inline File File::openNextFile() {
    if(!state || !state->directory || state->index==state->children.size()) return {};
    const auto& path=state->children[state->index++];return File(state->owner,path,state->owner->directories.count(path));
}
