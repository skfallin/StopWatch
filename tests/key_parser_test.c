#include "ctap-parser.h"
#include "ctap-errors.h"
#include "key_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

CTAP_sm2_attr ctap_sm2_attr;
static char shown[81],hashed[255];
void key_display_rp(const char* value) { strcpy(shown,value); }
// Records parser input; cryptographic signatures are verified separately over USB.
void sha256_raw(const uint8_t* bytes,size_t len,uint8_t out[32]) {
    assert(len<sizeof(hashed));memcpy(hashed,bytes,len);hashed[len]=0;memset(out,0,32);
}
static uint8_t parse(const char* rp,const char* second,int outerDuplicate,int missing) {
    uint8_t bytes[1300],hash[32]={0};CborEncoder root,map,nested,item;
    cbor_encoder_init(&root,bytes,sizeof(bytes),0);
    assert(cbor_encoder_create_map(&root,&map,outerDuplicate?5:4)==0);
    assert(cbor_encode_int(&map,1)==0);assert(cbor_encode_byte_string(&map,hash,32)==0);
    assert(cbor_encode_int(&map,2)==0);
    assert(cbor_encoder_create_map(&map,&nested,missing?0:second&&!outerDuplicate?2:1)==0);
    if(!missing) { assert(cbor_encode_text_stringz(&nested,"id")==0);assert(cbor_encode_text_stringz(&nested,rp)==0); }
    if(second&&!outerDuplicate) { assert(cbor_encode_text_stringz(&nested,"id")==0);assert(cbor_encode_text_stringz(&nested,second)==0); }
    assert(cbor_encoder_close_container(&map,&nested)==0);
    assert(cbor_encode_int(&map,3)==0);assert(cbor_encoder_create_map(&map,&nested,1)==0);
    assert(cbor_encode_text_stringz(&nested,"id")==0);assert(cbor_encode_byte_string(&nested,(uint8_t*)"test",4)==0);
    assert(cbor_encoder_close_container(&map,&nested)==0);
    assert(cbor_encode_int(&map,4)==0);assert(cbor_encoder_create_array(&map,&nested,1)==0);
    assert(cbor_encoder_create_map(&nested,&item,2)==0);
    assert(cbor_encode_text_stringz(&item,"alg")==0);assert(cbor_encode_int(&item,-7)==0);
    assert(cbor_encode_text_stringz(&item,"type")==0);assert(cbor_encode_text_stringz(&item,"public-key")==0);
    assert(cbor_encoder_close_container(&nested,&item)==0);assert(cbor_encoder_close_container(&map,&nested)==0);
    if(outerDuplicate) {
        assert(cbor_encode_int(&map,2)==0);assert(cbor_encoder_create_map(&map,&nested,1)==0);
        assert(cbor_encode_text_stringz(&nested,"id")==0);assert(cbor_encode_text_stringz(&nested,second)==0);
        assert(cbor_encoder_close_container(&map,&nested)==0);
    }
    assert(cbor_encoder_close_container(&root,&map)==0);
    CborParser parser;CTAP_make_credential mc;
    shown[0]=hashed[0]=0;
    return parse_make_credential(&parser,&mc,bytes,cbor_encoder_get_buffer_size(&root,bytes));
}
int main(void) {
    assert(parse("localhost",NULL,0,0)==0 && !strcmp(shown,"localhost") && !strcmp(shown,hashed));
    assert(parse("first.example","last.example",0,0)==0 && !strcmp(shown,"last.example") && !strcmp(shown,hashed));
    assert(parse("first.example","last.example",1,0)==0 && !strcmp(shown,"last.example") && !strcmp(shown,hashed));
    assert(parse("",NULL,0,1)==CTAP2_ERR_MISSING_PARAMETER && !*shown && !*hashed);
    char longName[82];memset(longName,'x',81);longName[81]=0;
    assert(parse(longName,NULL,0,0)==CTAP1_ERR_INVALID_PARAMETER && !*shown && !*hashed);
    assert(parse("bad domain",NULL,0,0)==CTAP1_ERR_INVALID_PARAMETER && !*shown && !*hashed);
    puts("OK: il parser visualizza lo stesso RP usato per la credenziale, anche con chiavi duplicate; rifiuta RP assente/non leggibile");
}
