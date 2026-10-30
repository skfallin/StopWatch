// Platform adapter for CanoKey's primitive API; cryptography is provided by ESP-IDF mbedTLS.
#include <stdbool.h>
#include <ecc.h>
#include <aes.h>
#include <sha.h>
#include <rand.h>
#include <memzero.h>
#include <string.h>
#include <esp_random.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/sha1.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/bignum.h>

static int rng(void *unused,unsigned char *out,size_t n) { (void)unused;esp_fill_random(out,n);return 0; }
void random_buffer(uint8_t *out,size_t n) { esp_fill_random(out,n); }
uint32_t random32(void) { return esp_random(); }

static mbedtls_sha1_context sha1_ctx;
void sha1_init(void) { mbedtls_sha1_init(&sha1_ctx);mbedtls_sha1_starts(&sha1_ctx); }
void sha1_update(const uint8_t *data,uint16_t len) { mbedtls_sha1_update(&sha1_ctx,data,len); }
void sha1_final(uint8_t out[20]) { mbedtls_sha1_finish(&sha1_ctx,out);mbedtls_sha1_free(&sha1_ctx); }
void sha1_raw(const uint8_t *data,size_t len,uint8_t out[20]) { mbedtls_sha1(data,len,out); }

static mbedtls_sha256_context sha256_ctx;
void sha256_init(void) { mbedtls_sha256_init(&sha256_ctx);mbedtls_sha256_starts(&sha256_ctx,0); }
void sha256_update(const uint8_t *data,uint16_t len) { mbedtls_sha256_update(&sha256_ctx,data,len); }
void sha256_final(uint8_t out[32]) { mbedtls_sha256_finish(&sha256_ctx,out);mbedtls_sha256_free(&sha256_ctx); }
void sha256_raw(const uint8_t *data,size_t len,uint8_t out[32]) { mbedtls_sha256(data,len,out,0); }

static int aes(const uint8_t *in,uint8_t *out,const uint8_t *key,unsigned bits,bool encrypt) {
    mbedtls_aes_context ctx;mbedtls_aes_init(&ctx);
    int err=encrypt?mbedtls_aes_setkey_enc(&ctx,key,bits):mbedtls_aes_setkey_dec(&ctx,key,bits);
    if(!err) err=mbedtls_aes_crypt_ecb(&ctx,encrypt?MBEDTLS_AES_ENCRYPT:MBEDTLS_AES_DECRYPT,in,out);
    mbedtls_aes_free(&ctx);return err;
}
int aes128_enc(const uint8_t *in,uint8_t *out,const uint8_t *key) { return aes(in,out,key,128,true); }
int aes128_dec(const uint8_t *in,uint8_t *out,const uint8_t *key) { return aes(in,out,key,128,false); }
int aes256_enc(const uint8_t *in,uint8_t *out,const uint8_t *key) { return aes(in,out,key,256,true); }
int aes256_dec(const uint8_t *in,uint8_t *out,const uint8_t *key) { return aes(in,out,key,256,false); }

int ecc_verify_private_key(key_type_t type,ecc_key_t *key) {
    if(type!=SECP256R1) return 0;
    mbedtls_ecp_group grp;mbedtls_mpi d;mbedtls_ecp_group_init(&grp);mbedtls_mpi_init(&d);
    int err=mbedtls_ecp_group_load(&grp,MBEDTLS_ECP_DP_SECP256R1);
    if(!err) err=mbedtls_mpi_read_binary(&d,key->pri,32);
    if(!err) err=mbedtls_ecp_check_privkey(&grp,&d);
    mbedtls_mpi_free(&d);mbedtls_ecp_group_free(&grp);return err==0;
}
int ecc_complete_key(key_type_t type,ecc_key_t *key) {
    if(type!=SECP256R1) return -1;
    mbedtls_ecp_group grp;mbedtls_mpi d;mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);mbedtls_mpi_init(&d);mbedtls_ecp_point_init(&q);
    int err=mbedtls_ecp_group_load(&grp,MBEDTLS_ECP_DP_SECP256R1);
    if(!err) err=mbedtls_mpi_read_binary(&d,key->pri,32);
    if(!err) err=mbedtls_ecp_check_privkey(&grp,&d);
    if(!err) err=mbedtls_ecp_mul(&grp,&q,&d,&grp.G,rng,NULL);
    uint8_t encoded[65];size_t len=0;
    if(!err) err=mbedtls_ecp_point_write_binary(&grp,&q,MBEDTLS_ECP_PF_UNCOMPRESSED,&len,encoded,sizeof(encoded));
    if(!err && len==65) memcpy(key->pub,encoded+1,64);else err=-1;
    memzero(encoded,sizeof(encoded));mbedtls_ecp_point_free(&q);mbedtls_mpi_free(&d);mbedtls_ecp_group_free(&grp);return err;
}
int ecc_generate(key_type_t type,ecc_key_t *key) {
    if(type!=SECP256R1) return -1;
    mbedtls_ecp_keypair pair;mbedtls_ecp_keypair_init(&pair);
    int err=mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,&pair,rng,NULL);
    if(!err) err=mbedtls_mpi_write_binary(&pair.MBEDTLS_PRIVATE(d),key->pri,32);
    mbedtls_ecp_keypair_free(&pair);return err?err:ecc_complete_key(type,key);
}
int ecc_sign(key_type_t type,const ecc_key_t *key,const uint8_t *hash,size_t n,uint8_t *sig) {
    if(type!=SECP256R1 || n!=32) return -1;
    mbedtls_ecp_group grp;mbedtls_mpi d,r,s;
    mbedtls_ecp_group_init(&grp);mbedtls_mpi_init(&d);mbedtls_mpi_init(&r);mbedtls_mpi_init(&s);
    int err=mbedtls_ecp_group_load(&grp,MBEDTLS_ECP_DP_SECP256R1);
    if(!err) err=mbedtls_mpi_read_binary(&d,key->pri,32);
    if(!err) err=mbedtls_ecdsa_sign(&grp,&r,&s,&d,hash,n,rng,NULL);
    if(!err) err=mbedtls_mpi_write_binary(&r,sig,32);
    if(!err) err=mbedtls_mpi_write_binary(&s,sig+32,32);
    mbedtls_mpi_free(&s);mbedtls_mpi_free(&r);mbedtls_mpi_free(&d);mbedtls_ecp_group_free(&grp);return err;
}
int ecdh(key_type_t type,const uint8_t *private_key,const uint8_t *peer,uint8_t *out) {
    if(type!=SECP256R1) return -1;
    mbedtls_ecp_group grp;mbedtls_mpi d;mbedtls_ecp_point p,q;
    mbedtls_ecp_group_init(&grp);mbedtls_mpi_init(&d);mbedtls_ecp_point_init(&p);mbedtls_ecp_point_init(&q);
    uint8_t encoded[65];encoded[0]=4;memcpy(encoded+1,peer,64);
    int err=mbedtls_ecp_group_load(&grp,MBEDTLS_ECP_DP_SECP256R1);
    if(!err) err=mbedtls_mpi_read_binary(&d,private_key,32);
    if(!err) err=mbedtls_ecp_point_read_binary(&grp,&p,encoded,sizeof(encoded));
    if(!err) err=mbedtls_ecp_check_pubkey(&grp,&p);
    if(!err) err=mbedtls_ecp_mul(&grp,&q,&d,&p,rng,NULL);
    size_t len=0;
    if(!err) err=mbedtls_ecp_point_write_binary(&grp,&q,MBEDTLS_ECP_PF_UNCOMPRESSED,&len,encoded,sizeof(encoded));
    if(!err && len==65) memcpy(out,encoded+1,32);else err=-1;
    memzero(encoded,sizeof(encoded));mbedtls_ecp_point_free(&q);mbedtls_ecp_point_free(&p);mbedtls_mpi_free(&d);mbedtls_ecp_group_free(&grp);return err;
}
size_t ecdsa_sig2ansi(uint8_t n,const uint8_t *input,uint8_t *output) {
    if(n!=32) return 0;
    uint8_t encoded[72];size_t off=2;
    for(unsigned part=0;part<2;++part) {
        const uint8_t *v=input+part*32;size_t len=32;
        while(len>1 && *v==0) { ++v;--len; }
        encoded[off++]=2;const bool prefix=(*v&0x80)!=0;encoded[off++]=len+prefix;
        if(prefix) encoded[off++]=0;
        memcpy(encoded+off,v,len);off+=len;
    }
    encoded[0]=0x30;encoded[1]=off-2;memcpy(output,encoded,off);memzero(encoded,sizeof(encoded));return off;
}
// This port advertises ES256 only; the upstream's other algorithms are unavailable.
const uint8_t SM2_ID_DEFAULT[17]={0};
int sm2_z(const uint8_t *id,const ecc_key_t *key,uint8_t *z) { (void)id;(void)key;(void)z;return -1; }
