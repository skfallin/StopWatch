#include "usb_control.h"
#include <cstring>
#include <tusb.h>

namespace {
const uint8_t descriptor[]={
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2))
};
}

UsbControl::UsbControl() { registered_=USBHID::addDevice(this,sizeof(descriptor)); }
void UsbControl::begin() { hid_.begin(); }
uint16_t UsbControl::_onGetDescriptor(uint8_t* buffer) { std::memcpy(buffer,descriptor,sizeof(descriptor));return sizeof(descriptor); }

bool UsbControl::update(bool active,bool phone,uint32_t now,bool local) {
    const bool usb=registered_ && bool(USB) && !tud_suspended();
    const auto previous=queue_.session();
    if(local_!=local) { queue_.state(false,usb,false,now);local_=local; }
    queue_.state(active,usb,phone || local,now);
    usb_input::Report report;
    if(queue_.next(report,now) && usb && hid_.ready()) {
        // A short bounded write leaves BLE, audio and the UI running between reports.
        if(hid_.SendReport(report.id,report.bytes.data(),report.size,10)) queue_.sent(now);
        else queue_.failed(now);
    }
    return queue_.session()!=previous;
}
void UsbControl::status(JsonObject out) const {
    out["supported"]=registered_;out["active"]=queue_.active() && !local_;out["connected"]=queue_.connected();
    out["ready"]=queue_.available() && !local_;out["busy"]=queue_.busy();out["session"]=queue_.session();
    out["fault"]=queue_.fault();out["queued"]=queue_.count();
}
