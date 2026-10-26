#pragma once
#include <FS.h>
#include <vector>
#include <string>
#include "policy.h"

namespace archive {
inline bool clearUserFiles(fs::FS& filesystem) {
    std::vector<std::string> paths;
    if(filesystem.exists("/notes")) {
        File directory=filesystem.open("/notes");
        if(!directory || !directory.isDirectory()) return false;
        for(File file=directory.openNextFile();file;file=directory.openNextFile()) {
            if(file.isDirectory()) continue;
            const std::string name=file.name();
            const bool partial=name.size()>5 && name.compare(name.size()-5,5,".part")==0 &&
                policy::noteFilename(std::string_view(name).substr(0,name.size()-5));
            if(policy::noteFilename(name) || partial) paths.push_back("/notes/"+name);
        }
    }
    paths.push_back("/badge.bin");paths.push_back("/badge.upload");
    for(const auto& path:paths) if(filesystem.exists(path.c_str()) && !filesystem.remove(path.c_str())) return false;
    return true;
}
}
