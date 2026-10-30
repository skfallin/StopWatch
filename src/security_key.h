#pragma once
#include <Arduino.h>

namespace security_key {
void begin();
bool update(bool active,bool a,bool b);
uint32_t promptId();
void presented(uint32_t id);
bool busy();
bool connected();
bool awaiting();
String relyingParty();
String message();
}
