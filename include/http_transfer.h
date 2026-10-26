#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace transfer {
enum class Result { Complete, Closed, ReadError, WriteError, Timeout };

template<class Read, class Write, class Clock, class Retryable, class Pause>
Result receiveBody(uint8_t* buffer, size_t capacity, size_t total,
                   Read read, Write write, Clock clock, Retryable retryable, Pause pause) {
    const uint32_t started=clock();
    uint32_t lastData=started;
    size_t offset=0;
    while(offset<total) {
        const uint32_t now=clock();
        if(uint32_t(now-started)>=120000 || uint32_t(now-lastData)>=15000) return Result::Timeout;
        const size_t wanted=std::min(capacity,total-offset);
        const int count=read(buffer,wanted);
        if(uint32_t(clock()-started)>=120000) return Result::Timeout;
        if(count<=0) {
            if(retryable(count)) { pause(); continue; }
            return count==0?Result::Closed:Result::ReadError;
        }
        if(size_t(count)>wanted) return Result::ReadError;
        if(!write(offset,buffer,size_t(count))) return Result::WriteError;
        offset+=count;
        lastData=clock();
    }
    return Result::Complete;
}
}
