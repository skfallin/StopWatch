#include "i18n.h"
#pragma once
#include <USB.h>
#include <USBHID.h>
#include "usb_input.h"

class UsbControl : private USBHIDDevice {
public:
    UsbControl();
    void begin();
    bool update(bool active,bool phone,uint32_t now,bool local=false);
    void status(JsonObject out) const;
    const char* input(JsonObjectConst value) { return local_?i18n::text("Presentazioni attive sul dispositivo"):queue_.accept(value,millis()); }
    void motion(const uint8_t* bytes,size_t size) {
        usb_input::Motion motion;
        if(!local_ && usb_input::decodeMotion(bytes,size,motion)) queue_.motion(motion,millis());
    }
    bool presentationKey(usb_input::Stroke key) { return local_ && queue_.press(key,millis()); }
    bool busy() const { return queue_.busy(); }
    bool connected() const { return queue_.connected(); }
    bool active() const { return queue_.active() && !local_; }
    bool fault() const { return queue_.fault(); }
private:
    uint16_t _onGetDescriptor(uint8_t* buffer) override;
    USBHID hid_;
    usb_input::Queue queue_;
    bool registered_=false,local_=false;
};
