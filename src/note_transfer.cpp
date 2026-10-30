#include "i18n.h"
#include "note_transfer.h"
#include "policy.h"
#include "http_transfer.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_tls.h>
#include <mbedtls/base64.h>
#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

namespace {
struct TransferActivity {
    std::atomic<bool>& active;
    ~TransferActivity() { active=false; }
};

int randomBytes(void*, unsigned char* bytes, size_t count) {
    // Offers are created only with Wi-Fi active, which supplies hardware entropy.
    esp_fill_random(bytes, count);
    return 0;
}
}

bool NoteTransfer::createIdentity() {
    mbedtls_pk_context key;
    mbedtls_x509write_cert certificate;
    mbedtls_pk_init(&key);
    mbedtls_x509write_crt_init(&certificate);
    const bool result = [&] {
        if (mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
            mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), randomBytes, nullptr) != 0) return false;
        mbedtls_x509write_crt_set_version(&certificate, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_x509write_crt_set_md_alg(&certificate, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_subject_key(&certificate, &key);
        mbedtls_x509write_crt_set_issuer_key(&certificate, &key);
        unsigned char serial[16];
        esp_fill_random(serial, sizeof(serial));
        serial[0] = (serial[0] & 0x7f) | 1;
        if (mbedtls_x509write_crt_set_serial_raw(&certificate, serial, sizeof(serial)) != 0 ||
            mbedtls_x509write_crt_set_subject_name(&certificate, "CN=StopWatch") != 0 ||
            mbedtls_x509write_crt_set_issuer_name(&certificate, "CN=StopWatch") != 0 ||
            mbedtls_x509write_crt_set_validity(&certificate, "20240101000000", "20400101000000") != 0 ||
            mbedtls_x509write_crt_set_basic_constraints(&certificate, 0, -1) != 0 ||
            mbedtls_x509write_crt_set_key_usage(&certificate, MBEDTLS_X509_KU_DIGITAL_SIGNATURE) != 0) return false;
        const int certificateSize = mbedtls_x509write_crt_der(&certificate, certificate_.data(), certificate_.size(), randomBytes, nullptr);
        const int keySize = mbedtls_pk_write_key_der(&key, key_.data(), key_.size());
        if (certificateSize <= 0 || keySize <= 0) return false;
        certificateBytes_ = certificateSize;
        keyBytes_ = keySize;
        memmove(certificate_.data(), certificate_.data() + certificate_.size() - certificateBytes_, certificateBytes_);
        memmove(key_.data(), key_.data() + key_.size() - keyBytes_, keyBytes_);
        unsigned char encoded[1025];
        size_t size = 0;
        if (mbedtls_base64_encode(encoded, sizeof(encoded), &size, certificate_.data(), certificateBytes_) != 0) return false;
        encoded[size] = 0;
        encodedCertificate_ = reinterpret_cast<const char*>(encoded);
        return true;
    }();
    mbedtls_x509write_crt_free(&certificate);
    mbedtls_pk_free(&key);
    if (!result) { certificateBytes_ = keyBytes_ = 0; encodedCertificate_ = ""; }
    return result;
}

bool NoteTransfer::start() {
    if (server_) return true;
    if (!certificateBytes_ && !createIdentity()) return false;
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.port_secure = 8443;
    config.servercert = certificate_.data();
    config.servercert_len = certificateBytes_;
    config.prvtkey_pem = key_.data();
    config.prvtkey_len = keyBytes_;
    config.httpd.max_open_sockets = 1;
    config.httpd.max_uri_handlers = 2;
    config.httpd.recv_wait_timeout = 5;
    config.httpd.send_wait_timeout = 8;
    config.tls_handshake_timeout_ms = 8000;
    if (httpd_ssl_start(&server_, &config) != ESP_OK) { server_ = nullptr; return false; }
    httpd_uri_t route{};
    route.uri = "/note";
    route.method = HTTP_GET;
    route.handler = sendNote;
    route.user_ctx = this;
    if (httpd_register_uri_handler(server_, &route) != ESP_OK) {
        httpd_ssl_stop(server_); server_ = nullptr; return false;
    }
    route.uri="/badge"; route.method=HTTP_POST; route.handler=receiveBadge;
    if(httpd_register_uri_handler(server_,&route)!=ESP_OK) { httpd_ssl_stop(server_); server_=nullptr; return false; }
    return true;
}

bool NoteTransfer::offer(const String& name, JsonObject out) {
    out["available"] = false;
    if (WiFi.status() != WL_CONNECTED || !policy::noteFilename(name.c_str()) || !start()) return false;
    File file = LittleFS.open(String("/notes/") + name, "r");
    if (!file || file.size() < 44 || file.size() > policy::maxSamples * 2 + 44) return false;
    return makeOffer(name,false,file.size(),out);
}

bool NoteTransfer::offerBadge(const String& upload, JsonObject out) {
    out["available"]=false;
    const size_t bytes=badge_.uploadSize(upload);
    if(!bytes || WiFi.status()!=WL_CONNECTED || !start()) return false;
    return makeOffer(upload,true,bytes,out);
}

bool NoteTransfer::makeOffer(const String& reference, bool upload, size_t bytes, JsonObject out) {
    unsigned char random[32];
    char ticket[65];
    esp_fill_random(random, sizeof(random));
    for (size_t i = 0; i < sizeof(random); ++i) snprintf(ticket + i * 2, 3, "%02x", random[i]);
    portENTER_CRITICAL(&lock_);
    snprintf(name_,sizeof(name_),"%s",reference.c_str());
    memcpy(ticket_, ticket, sizeof(ticket_));
    expires_ = millis() + 90000;
    upload_=upload; bytes_=bytes;
    portEXIT_CRITICAL(&lock_);
    out["available"] = true;
    out["host"] = WiFi.localIP().toString();
    out["port"] = 8443;
    out["ticket"] = ticket;
    out["certificate"] = encodedCertificate_;
    return true;
}

bool NoteTransfer::busy() {
    portENTER_CRITICAL(&lock_);
    const bool offered=expires_ && int32_t(expires_-millis())>0;
    const bool active=transferring_;
    portEXIT_CRITICAL(&lock_);
    return offered || active;
}

bool NoteTransfer::authorize(httpd_req_t* request, bool upload, char* reference, uint32_t& bytes) {
    char authorization[72]{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", authorization, sizeof(authorization)) != ESP_OK ||
        strncmp(authorization, "Bearer ", 7) != 0) return false;
    bool valid;
    portENTER_CRITICAL(&lock_);
    valid = expires_ && int32_t(expires_ - millis())>0 && upload_==upload && strcmp(authorization+7,ticket_)==0;
    if(valid) { memcpy(reference,name_,sizeof(name_)); bytes=bytes_; transferring_=true; expires_=0; }
    portEXIT_CRITICAL(&lock_);
    return valid;
}

esp_err_t NoteTransfer::sendNote(httpd_req_t* request) {
    auto& self=*static_cast<NoteTransfer*>(request->user_ctx);
    char name[26]{}; uint32_t expectedBytes=0;
    if(!self.authorize(request,false,name,expectedBytes)) return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Expired or invalid ticket");
    const TransferActivity activity{self.transferring_};
    File file = LittleFS.open(String("/notes/") + name, "r");
    if (!file || file.size()!=expectedBytes) return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Note not found");
    httpd_resp_set_type(request, "audio/wav");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    char bytes[2048];
    while (file.available()) {
        const size_t count = file.read(reinterpret_cast<uint8_t*>(bytes), sizeof(bytes));
        if (!count || httpd_resp_send_chunk(request, bytes, count) != ESP_OK) return ESP_FAIL;
    }
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t NoteTransfer::receiveBadge(httpd_req_t* request) {
    auto& self=*static_cast<NoteTransfer*>(request->user_ctx);
    char upload[26]{}; uint32_t total=0;
    if(!self.authorize(request,true,upload,total)) return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Expired or invalid ticket");
    const TransferActivity activity{self.transferring_};
    if(request->content_len!=total || total>badge::maxBytes) return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid size");
    uint8_t bytes[2048];
    int socketError=0;
    // Retry temporary socket/TLS pauses without accepting a truncated file.
    const auto result=transfer::receiveBody(bytes,sizeof(bytes),total,
        [&](uint8_t* buffer,size_t wanted) {
            socketError=httpd_req_recv(request,reinterpret_cast<char*>(buffer),wanted);
            return socketError;
        },
        [&](size_t offset,const uint8_t* buffer,size_t count) { return self.badge_.append(upload,offset,buffer,count); },
        [] { return millis(); },
        [](int count) { return count==HTTPD_SOCK_ERR_TIMEOUT || count==ESP_TLS_ERR_SSL_WANT_READ || count==ESP_TLS_ERR_SSL_WANT_WRITE; },
        [] { delay(1); });
    if(result!=transfer::Result::Complete) {
        const char* error=result==transfer::Result::Timeout?i18n::text("Pausa troppo lunga nell'invio Wi-Fi"):
            result==transfer::Result::Closed?i18n::text("L'iPhone ha chiuso la connessione Wi-Fi"):
            result==transfer::Result::WriteError?i18n::text("Scrittura dell'archivio Badge interrotta"):i18n::text("Errore di lettura della connessione Wi-Fi");
        self.badge_.transferFailed(upload,error);
        Serial.printf("[badge-wifi] ricezione interrotta: risultato=%u socket=%d\n",unsigned(result),socketError);
        httpd_resp_send_err(request,result==transfer::Result::Timeout?HTTPD_408_REQ_TIMEOUT:HTTPD_500_INTERNAL_SERVER_ERROR,error);
        return ESP_FAIL;
    }
    httpd_resp_set_type(request,"application/json"); httpd_resp_set_hdr(request,"Cache-Control","no-store");
    httpd_resp_set_hdr(request,"Connection","close");
    const esp_err_t sent=httpd_resp_sendstr(request,"{\"ok\":true}");
    Serial.printf("[badge-wifi] ricevuti=%lu risposta=%d\n",(unsigned long)total,sent);
    if(sent==ESP_OK) httpd_sess_trigger_close(self.server_,httpd_req_to_sockfd(request));
    return sent;
}
