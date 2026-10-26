#pragma once
#include <cstddef>
#include <cstdint>
#include <zlib.h>
using mz_uint8=uint8_t;
struct tinfl_decompressor {};
enum tinfl_status { TINFL_STATUS_FAILED=-1,TINFL_STATUS_DONE=0 };
constexpr int TINFL_FLAG_PARSE_ZLIB_HEADER=1,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF=2;
inline void tinfl_init(tinfl_decompressor*) {}
inline tinfl_status tinfl_decompress(tinfl_decompressor*,const uint8_t* input,size_t* inputBytes,uint8_t*,uint8_t* output,size_t* outputBytes,int) {
    uLongf out=*outputBytes;uLong in=*inputBytes;
    const int result=uncompress2(output,&out,input,&in);
    *inputBytes=in;*outputBytes=out;return result==Z_OK?TINFL_STATUS_DONE:TINFL_STATUS_FAILED;
}
