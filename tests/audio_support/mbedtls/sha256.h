#pragma once
#include <CommonCrypto/CommonDigest.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
using mbedtls_sha256_context=CC_SHA256_CTX;
inline void mbedtls_sha256_init(mbedtls_sha256_context* c) { std::memset(c,0,sizeof(*c)); }
inline void mbedtls_sha256_free(mbedtls_sha256_context*) {}
inline int mbedtls_sha256_starts(mbedtls_sha256_context* c,int) { return CC_SHA256_Init(c)?0:-1; }
inline int mbedtls_sha256_update(mbedtls_sha256_context* c,const uint8_t* data,size_t count) { return CC_SHA256_Update(c,data,CC_LONG(count))?0:-1; }
inline int mbedtls_sha256_finish(mbedtls_sha256_context* c,uint8_t* digest) { return CC_SHA256_Final(digest,c)?0:-1; }
