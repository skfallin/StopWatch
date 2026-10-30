#include "security_key.h"
#include "i18n.h"
#ifndef STOPWATCH_FIDO_LAB
namespace security_key {
void begin() {}
bool update(bool,bool,bool) { return false; }
uint32_t promptId() { return 0; }
void presented(uint32_t) {}
bool busy() { return false; }
bool connected() { return false; }
bool awaiting() { return false; }
String relyingParty() { return {}; }
String message() { return i18n::text("Richiede la build LAB separata"); }
}
#else
#include "security_key.h"
#include "ctaphid_transport.h"
#include "key_presence.h"
#include <key_core.h>
#include <esp32-hal-tinyusb.h>
#include <esp_random.h>
#include <tusb.h>
#include <device/usbd_pvt.h>
#include <atomic>
#include <cstring>

namespace {
constexpr uint8_t reportDescriptor[]={0x06,0xd0,0xf1,0x09,1,0xa1,1,0x09,0x20,0x15,0,0x26,0xff,0,0x75,8,0x95,64,0x81,2,0x09,0x21,0x95,64,0x91,2,0xc0};
uint8_t interfaceNumber=0xff,epIn=0,epOut=0;
uint8_t hidDescriptor[9];
CFG_TUSB_MEM_ALIGN uint8_t receiveBuffer[64],sendBuffer[64],controlBuffer[64];
QueueHandle_t incoming=nullptr,busyPackets=nullptr;
SemaphoreHandle_t sent=nullptr;
std::atomic<bool> configured{false},active{false},executing{false},cancelled{false},initialized{false},failed{false};
std::atomic<bool> lockNeeded{false};
std::atomic<uint32_t> generation{0},requestChannel{0},prompt{0};
std::atomic<int> decision{0};
std::atomic<uint32_t> visiblePrompt{0};
uint32_t promptCounter=0;
portMUX_TYPE stateLock=portMUX_INITIALIZER_UNLOCKED;
char domain[81]="Chiave USB";
keylab::Transport transport;
uint32_t requestGeneration=0,lastKeepalive=0;

void enqueue(const uint8_t* bytes,size_t size) {
    if(size!=64 || !incoming) return;
    if(executing && keylab::channel(bytes)==requestChannel && bytes[4]==0x91 && !bytes[5] && !bytes[6]) {
        cancelled=true;return;
    }
    if(executing) {
        if(bytes[4]&0x80 && (!busyPackets || xQueueSend(busyPackets,bytes,0)!=pdTRUE)) cancelled=true;
        return;
    }
    if(xQueueSend(incoming,bytes,0)!=pdTRUE) { cancelled=true;generation.fetch_add(1); }
}
void driverInit() {
    if(!incoming) incoming=xQueueCreate(16,64);
    if(!sent) sent=xSemaphoreCreateBinary();
    if(!busyPackets) busyPackets=xQueueCreate(8,64);
}
void driverReset(uint8_t) {
    configured=false;cancelled=true;generation.fetch_add(1);
    if(incoming) xQueueReset(incoming);
    if(busyPackets) xQueueReset(busyPackets);
}
bool driverDeinit() { driverReset(0);return true; }
uint16_t descriptor(uint8_t* dst,uint8_t* itf) {
    interfaceNumber=(*itf)++;
    epIn=tinyusb_get_free_in_endpoint()|0x80;epOut=tinyusb_get_free_out_endpoint();
    if(epIn==0x80 || !epOut) return 0;
    const uint8_t bytes[]={TUD_HID_INOUT_DESCRIPTOR(interfaceNumber,tinyusb_add_string_descriptor("StopWatch Chiave USB (LAB)"),HID_ITF_PROTOCOL_NONE,sizeof(reportDescriptor),epOut,epIn,64,1)};
    memcpy(dst,bytes,sizeof(bytes));memcpy(hidDescriptor,bytes+9,9);return sizeof(bytes);
}
struct Registration {
    Registration() { tinyusb_enable_interface(USB_INTERFACE_CUSTOM,TUD_HID_INOUT_DESC_LEN,descriptor); }
} registration;
uint16_t driverOpen(uint8_t port,const tusb_desc_interface_t* itf,uint16_t available) {
    if(itf->bInterfaceNumber!=interfaceNumber || itf->bInterfaceClass!=TUSB_CLASS_HID || available<TUD_HID_INOUT_DESC_LEN) return 0;
    const auto* bytes=reinterpret_cast<const uint8_t*>(itf);
    if(!incoming || !sent || !usbd_edpt_open(port,reinterpret_cast<const tusb_desc_endpoint_t*>(bytes+18)) ||
       !usbd_edpt_open(port,reinterpret_cast<const tusb_desc_endpoint_t*>(bytes+25))) return 0;
    configured=usbd_edpt_xfer(port,epOut,receiveBuffer,64,false);
    return configured?TUD_HID_INOUT_DESC_LEN:0;
}
bool driverTransfer(uint8_t port,uint8_t endpoint,xfer_result_t result,uint32_t bytes) {
    if(endpoint==epIn) { if(sent) xSemaphoreGive(sent);return true; }
    if(endpoint!=epOut) return false;
    if(result==XFER_RESULT_SUCCESS) enqueue(receiveBuffer,bytes);
    return usbd_edpt_xfer(port,epOut,receiveBuffer,64,false);
}
bool driverControl(uint8_t port,uint8_t stage,const tusb_control_request_t* req) {
    if(req->wIndex!=interfaceNumber) return false;
    if(stage==CONTROL_STAGE_ACK) {
        if(req->bmRequestType_bit.type==TUSB_REQ_TYPE_CLASS && req->bRequest==HID_REQ_CONTROL_SET_REPORT)
            enqueue(controlBuffer,req->wLength);
        return true;
    }
    if(stage!=CONTROL_STAGE_SETUP) return true;
    if(req->bmRequestType_bit.type==TUSB_REQ_TYPE_STANDARD && req->bRequest==TUSB_REQ_GET_DESCRIPTOR) {
        if((req->wValue>>8)==HID_DESC_TYPE_REPORT) return tud_control_xfer(port,req,const_cast<uint8_t*>(reportDescriptor),sizeof(reportDescriptor));
        if((req->wValue>>8)==HID_DESC_TYPE_HID) return tud_control_xfer(port,req,hidDescriptor,sizeof(hidDescriptor));
    }
    if(req->bmRequestType_bit.type!=TUSB_REQ_TYPE_CLASS) return false;
    switch(req->bRequest) {
    case HID_REQ_CONTROL_SET_REPORT:
        if(req->wLength!=64 || req->wValue!=0x0200) return false;
        return tud_control_xfer(port,req,controlBuffer,64);
    case HID_REQ_CONTROL_SET_IDLE: return tud_control_status(port,req);
    case HID_REQ_CONTROL_GET_IDLE: controlBuffer[0]=0;return tud_control_xfer(port,req,controlBuffer,1);
    default: return false;
    }
}
const usbd_class_driver_t driver={"StopWatch FIDO LAB",driverInit,driverDeinit,driverReset,driverOpen,driverControl,driverTransfer,nullptr,nullptr};

bool sendPacket(const uint8_t* bytes) {
    if(!configured || requestGeneration!=generation || !tud_mounted() || tud_suspended()) return false;
    // Completion owns sendBuffer until the transfer ends; never replace an in-flight packet.
    const uint32_t start=millis();
    while(usbd_edpt_busy(0,epIn)) {
        if(!configured || requestGeneration!=generation || uint32_t(millis()-start)>250) return false;
        vTaskDelay(1);
    }
    if(!usbd_edpt_claim(0,epIn)) return false;
    xSemaphoreTake(sent,0);memcpy(sendBuffer,bytes,64);
    if(!usbd_edpt_xfer(0,epIn,sendBuffer,64,false)) { usbd_edpt_release(0,epIn);return false; }
    return xSemaphoreTake(sent,pdMS_TO_TICKS(250))==pdTRUE && configured && requestGeneration==generation;
}
bool sendMessage(uint32_t cid,uint8_t command,const uint8_t* data,size_t size) {
    if(size>keylab::maxMessage) return false;
    uint8_t packet[64]={};keylab::channel(packet,cid);packet[4]=command;packet[5]=size>>8;packet[6]=size;
    size_t offset=std::min(size,size_t(57));if(offset) memcpy(packet+7,data,offset);
    if(!sendPacket(packet)) return false;
    uint8_t sequence=0;
    while(offset<size) {
        memset(packet+4,0,60);packet[4]=sequence++;const size_t count=std::min(size-offset,size_t(59));memcpy(packet+5,data+offset,count);offset+=count;
        if(!sendPacket(packet)) return false;
    }
    return true;
}
void rejectBusyPackets() {
    uint8_t packet[64];const uint8_t busy=6;
    while(busyPackets && xQueueReceive(busyPackets,packet,0)==pdTRUE) {
        if(packet[4]==0x86 && keylab::channel(packet)==requestChannel && !packet[5] && packet[6]==8) {
            cancelled=true;
            if(xQueueSend(incoming,packet,0)!=pdTRUE) generation.fetch_add(1);
        } else sendMessage(keylab::channel(packet),0xbf,&busy,1);
    }
}
void keepalive(uint8_t state) {
    rejectBusyPackets();
    const uint32_t now=millis();
    if(uint32_t(now-lastKeepalive)<150 || !requestChannel) return;
    lastKeepalive=now;
    if(!sendMessage(requestChannel,0xbb,&state,1)) cancelled=true;
}
void worker(void*) {
    uint8_t packet[64];static uint8_t response[keylab::maxMessage];
    uint32_t seenGeneration=0;bool wasActive=false;
    for(;;) {
        if(!configured) {
            if(wasActive && initialized) key_core_lock();
            wasActive=false;transport.reset();seenGeneration=generation;
            vTaskDelay(pdMS_TO_TICKS(20));continue;
        }
        const bool enabled=active.load();
        const bool relock=lockNeeded.exchange(false);
        if(relock || (wasActive && !enabled) || seenGeneration!=generation) {
            if(initialized) key_core_lock();
        }
        if(seenGeneration!=generation) { transport.reset();seenGeneration=generation; }
        wasActive=enabled;
        if(enabled && !initialized && !failed) { initialized=key_core_init();failed=!initialized; }
        const auto expired=transport.expire(millis());
        if(expired.kind==keylab::Transport::Kind::Error) {
            requestGeneration=seenGeneration;sendMessage(expired.cid,0xbf,&expired.error,1);
        }
        if(xQueueReceive(incoming,packet,pdMS_TO_TICKS(20))!=pdTRUE) continue;
        requestGeneration=seenGeneration;
        const auto event=transport.receive(packet,64,millis());
        switch(event.kind) {
        case keylab::Transport::Kind::Init: {
            uint32_t cid=event.cid;
            if(cid==0xffffffff) do { cid=esp_random(); } while(!transport.addChannel(cid));
            uint8_t reply[17];memcpy(reply,transport.nonce(),8);keylab::channel(reply+8,cid);
            reply[12]=2;reply[13]=0;reply[14]=14;reply[15]=0;reply[16]=0x0c; // CBOR; no CTAP1/U2F.
            sendMessage(event.cid,0x86,reply,sizeof(reply));break;
        }
        case keylab::Transport::Kind::Wink: {
            const uint8_t unsupported=1;sendMessage(event.cid,0xbf,&unsupported,1);break;
        }
        case keylab::Transport::Kind::Error: sendMessage(event.cid,0xbf,&event.error,1);break;
        case keylab::Transport::Kind::Ping: sendMessage(event.cid,0x81,transport.data(),event.size);break;
        case keylab::Transport::Kind::Message: {
            if(!active) { const uint8_t denied=0x27;sendMessage(event.cid,0x90,&denied,1);break; }
            cancelled=false;requestChannel=event.cid;executing=true;
            size_t size=0;
            if(initialized) size=key_core_execute(transport.data(),event.size,response,sizeof(response));
            rejectBusyPackets();
            if(key_request_cancelled()) { response[0]=0x2d;size=1; }
            if(!size) { response[0]=0x7f;size=1; }
            if(requestGeneration==generation) sendMessage(event.cid,0x90,response,size);
            memset(response,0,sizeof(response));memset(transport.data(),0,keylab::maxMessage);
            executing=false;requestChannel=0;break;
        }
        default: break;
        }
    }
}
}
extern "C" const usbd_class_driver_t* usbd_app_driver_get_cb(uint8_t* count) { *count=1;return &driver; }
extern "C" uint32_t key_tick() { return millis(); }
extern "C" bool key_request_cancelled() { return cancelled || !active || !configured || requestGeneration!=generation; }
extern "C" void key_processing_keepalive() { keepalive(1);vTaskDelay(1); }
extern "C" void key_display_rp(const char* value) {
    portENTER_CRITICAL(&stateLock);strlcpy(domain,value,sizeof(domain));portEXIT_CRITICAL(&stateLock);
}
extern "C" uint8_t key_user_presence() {
    decision=0;visiblePrompt=0;prompt=++promptCounter;const uint32_t started=millis();
    while(!decision && !key_request_cancelled() && uint32_t(millis()-started)<25000) { keepalive(2);vTaskDelay(pdMS_TO_TICKS(5)); }
    const auto result=decision.load();prompt=0;
    return key_request_cancelled() || result<0?1:result==1?0:2;
}
namespace security_key {
void begin() { if(xTaskCreate(worker,"fido-lab",24576,nullptr,1,nullptr)!=pdPASS) failed=true; }
bool update(bool enabled,bool a,bool b) {
    static uint32_t seenPrompt=0;static keylab::Presence presence;static unsigned previous=0;static uint32_t previousPrompt=0;
    if(active.exchange(enabled) && !enabled) { cancelled=true;lockNeeded=true; }
    const auto id=prompt.load();
    if(id && visiblePrompt==id && id!=seenPrompt) { seenPrompt=id;presence.begin(id,millis()); }
    if(id && visiblePrompt==id && !decision) {
        const auto result=presence.update(millis(),a,b,enabled && configured && !cancelled);
        if(result==keylab::Presence::Result::Confirm) decision=1;
        else if(result!=keylab::Presence::Result::Waiting) decision=-1;
    }
    if(!id) seenPrompt=0;
    const unsigned state=(initialized?1:0)|(failed?2:0)|(configured?4:0)|(id?8:0)|(executing?16:0);
    const bool changed=state!=previous || id!=previousPrompt;previous=state;previousPrompt=id;return changed;
}
uint32_t promptId() { return prompt.load(); }
void presented(uint32_t id) { visiblePrompt=id; }
bool busy() { return executing || (active && !initialized && !failed); }
bool connected() { return configured && tud_mounted() && !tud_suspended(); }
bool awaiting() { return prompt!=0; }
String relyingParty() { char copy[81];portENTER_CRITICAL(&stateLock);memcpy(copy,domain,sizeof(copy));portEXIT_CRITICAL(&stateLock);return String(copy); }
String message() { return failed?i18n::text("Archivio chiave non disponibile"):!connected()?i18n::text("Collega il cavo USB-C"):!initialized?i18n::text("Preparazione archivio…"):awaiting()?(relyingParty()=="Reset chiave"?i18n::text("Elimina tutte le passkey e il PIN"):i18n::text("Conferma solo se riconosci il sito")):i18n::text("Pronta per le credenziali di prova"); }
}

#endif
