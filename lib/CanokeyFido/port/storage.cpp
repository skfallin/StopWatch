#include <Arduino.h>
#include <LittleFS.h>
extern "C" {
#include "key_core.h"
#include <key_fs.h>
}

namespace {
constexpr const char* imagePath="/security-key.lfs";
constexpr size_t blockSize=4096,blockCount=32,imageSize=blockSize*blockCount;
File image;
bool mounted=false;
int readBlock(const lfs_config*,lfs_block_t block,lfs_off_t offset,void* data,lfs_size_t size) {
    if(block>=blockCount || offset>blockSize || size>blockSize-offset || !image.seek(block*blockSize+offset)) return LFS_ERR_IO;
    return image.read(static_cast<uint8_t*>(data),size)==size?0:LFS_ERR_IO;
}
int programBlock(const lfs_config*,lfs_block_t block,lfs_off_t offset,const void* data,lfs_size_t size) {
    if(block>=blockCount || offset>blockSize || size>blockSize-offset || !image.seek(block*blockSize+offset)) return LFS_ERR_IO;
    return image.write(static_cast<const uint8_t*>(data),size)==size?0:LFS_ERR_IO;
}
int eraseBlock(const lfs_config* cfg,lfs_block_t block) {
    uint8_t erased[256];memset(erased,0xff,sizeof(erased));
    for(unsigned offset=0;offset<blockSize;offset+=sizeof(erased))
        if(programBlock(cfg,block,offset,erased,sizeof(erased))) return LFS_ERR_IO;
    return 0;
}
int syncBlock(const lfs_config*) { image.flush();return image.getWriteError()?LFS_ERR_IO:0; }
}
extern "C" bool key_storage_open() {
    if(mounted) return true;
    const bool exists=LittleFS.exists(imagePath);
    if(!exists) {
        if(LittleFS.totalBytes()-LittleFS.usedBytes()<imageSize+65536) return false;
        image=LittleFS.open("/security-key.part","w");
        if(!image) return false;
        uint8_t erased[256];memset(erased,0xff,sizeof(erased));bool ok=true;
        for(unsigned i=0;i<imageSize/sizeof(erased) && ok;++i) ok=image.write(erased,sizeof(erased))==sizeof(erased);
        image.flush();ok=ok && !image.getWriteError();image.close();
        if(!ok || !LittleFS.rename("/security-key.part",imagePath)) return false;
    }
    image=LittleFS.open(imagePath,"r+");
    if(!image || image.size()!=imageSize) return false;
    static uint8_t readCache[512],writeCache[512],lookahead[16];
    static lfs_config cfg{};
    cfg.read=readBlock;cfg.prog=programBlock;cfg.erase=eraseBlock;cfg.sync=syncBlock;
    cfg.read_size=16;cfg.prog_size=16;cfg.block_size=blockSize;cfg.block_count=blockCount;
    cfg.block_cycles=100;cfg.cache_size=512;cfg.lookahead_size=sizeof(lookahead);
    cfg.read_buffer=readCache;cfg.prog_buffer=writeCache;cfg.lookahead_buffer=lookahead;
    // Existing images are never reformatted automatically after a mount error.
    if(!exists && fs_format(&cfg)) return false;
    mounted=fs_mount(&cfg)==0;
    return mounted;
}
