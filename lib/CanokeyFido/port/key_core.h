#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool key_storage_open(void);
bool key_core_init(void);
size_t key_core_execute(uint8_t *request,size_t bytes,uint8_t *response,size_t capacity);
void key_core_lock(void);
// Implemented by the device port, never by the Companion or the generic touch handler.
uint8_t key_user_presence(void);
void key_processing_keepalive(void);
bool key_request_cancelled(void);
uint32_t key_tick(void);
void key_display_rp(const char *value);
bool key_show_rp(const uint8_t *value,size_t bytes);
#ifdef __cplusplus
}
#endif
