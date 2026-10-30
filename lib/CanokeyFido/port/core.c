#include "key_core.h"
#include <ctap.h>
#include <ctap-internal.h>
#include <device.h>
#include <string.h>

uint32_t device_get_tick(void) { return key_tick(); }
uint8_t is_nfc(void) { return 0; }
uint8_t wait_for_user_presence(uint8_t entry) { (void)entry;return key_user_presence(); }
int send_keepalive_during_processing(uint8_t entry) { (void)entry;key_processing_keepalive();return key_request_cancelled()?1:0; }
void start_blinking_interval(uint8_t seconds,uint32_t interval) { (void)seconds;(void)interval; }
void stop_blinking(void) {}
int strong_user_presence_test(void) { return key_user_presence()!=USER_PRESENCE_OK; }

bool key_core_init(void) {
    if(!key_storage_open()) return false;
    return ctap_install(0)==0;
}
void key_core_lock(void) { ctap_lock(); }

bool key_show_rp(const uint8_t *value,size_t bytes) {
    if(!bytes || bytes>80) return false;
    for(size_t i=0;i<bytes;++i) if(value[i]<33 || value[i]>126) return false;
    char domain[81];memcpy(domain,value,bytes);domain[bytes]=0;key_display_rp(domain);return true;
}
size_t key_core_execute(uint8_t *request,size_t bytes,uint8_t *response,size_t capacity) {
    if(!bytes || !capacity) return 0;
    key_display_rp(request[0]==7?"Reset chiave":"Chiave USB");
    size_t len=capacity;
    if(ctap_process_cbor_with_src(request,bytes,response,&len,CTAP_SRC_HID)<0 || len>capacity) return 0;
    return len;
}
