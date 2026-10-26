#include "voice_notes.h"
#include "user_archive.h"
#include <LittleFS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <vector>

bool VoiceNotes::begin() {
    Preferences preferences;
    if (preferences.begin("stopwatch", false)) {
        const String name = preferences.getString("last_note", "");
        if (policy::noteFilename(name.c_str())) lastSaved_ = String("/notes/") + name;
        preferences.end();
    }
    mounted_ = LittleFS.begin(false);
    if (mounted_) {
        if (!LittleFS.exists("/notes") && !LittleFS.mkdir("/notes")) {
            mounted_ = false;
        } else {
            recover();
        }
    }
    return mounted_;
}

bool VoiceNotes::initializeStorage() {
    if (busy()) return false;
    if(mounted_) {
        if(!archive::clearUserFiles(LittleFS)) return false;
    } else {
        LittleFS.end();
        if (!LittleFS.format()) return false;
    }
    Preferences preferences;
    if(preferences.begin("stopwatch",false)) { preferences.remove("last_note");preferences.end(); }
    lastSaved_="";
    return begin();
}

bool VoiceNotes::finishFile(const String& partial) {
    File file = LittleFS.open(partial, "r+");
    if (!file || file.size() < 46) return false;
    const auto bytes = uint32_t((file.size() - 44) & ~size_t(1));
    const auto header = policy::wavHeader(bytes);
    const bool written = file.seek(0) && file.write(header.data(), header.size()) == header.size();
    file.flush();
    file.close();
    if (!written) return false;
    String destination = partial.substring(0, partial.length() - 5);
    if (LittleFS.exists(destination) || !LittleFS.rename(partial, destination)) return false;
    lastSaved_ = destination;
    Preferences preferences;
    if (preferences.begin("stopwatch", false)) {
        preferences.putString("last_note", destination.substring(7));
        preferences.end();
    }
    return true;
}

void VoiceNotes::recover() {
    std::vector<String> partials;
    File root = LittleFS.open("/notes");
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        const String name = file.name();
        if (name.endsWith(".wav.part") && policy::noteFilename(name.substring(0, name.length() - 5).c_str())) {
            partials.push_back(String("/notes/") + name);
        }
    }
    root.close();
    for (const auto& partial : partials) {
        if (!finishFile(partial)) message_ = "Nota interrotta: recupero non riuscito";
    }
}

bool VoiceNotes::warmMicrophone() {
    if (M5.Speaker.isPlaying()) return false;
    if (M5.Mic.isRunning()) return true;
    M5.Speaker.end();
    if (!M5.Mic.begin()) return false;
    micStarted_ = millis();
    return true;
}

void VoiceNotes::fail(const String& error) {
    message_ = error;
    succeeded_ = false;
    finishedEvent_ = true;
}

bool VoiceNotes::start() {
    if (busy()) return false;
    finishedEvent_ = succeeded_ = false;
    if (!mounted_) { fail("Inizializza l'archivio in Diagnostica"); return false; }
    if (count() >= policy::maxNotes) { fail("Archivio pieno: scarica e rimuovi note"); return false; }
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < policy::maxSamples * 2 + 65536) {
        fail("Spazio insufficiente: scarica e rimuovi note"); return false;
    }
    if (!warmMicrophone()) { fail("Microfono non disponibile"); return false; }
    samples_ = static_cast<int16_t*>(heap_caps_malloc(policy::maxSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!samples_) { fail("PSRAM insufficiente per registrare"); return false; }
    char name[48];
    do {
        snprintf(name, sizeof(name), "/notes/note-%08lx%08lx.wav.part", (unsigned long)esp_random(), (unsigned long)esp_random());
        partial_ = name;
    } while (LittleFS.exists(partial_) || LittleFS.exists(partial_.substring(0, partial_.length() - 5)));
    file_ = LittleFS.open(partial_, "w");
    const auto header = policy::wavHeader(0);
    if (!file_ || file_.write(header.data(), header.size()) != header.size()) {
        release(); fail("Impossibile creare la nota"); return false;
    }
    file_.flush();
    written_ = 0;
    completed_.store(0);
    peak_.store(0);
    captureFinished_.store(false);
    captureError_.store(false);
    requested_.store(true);
    preparedAt_ = millis();
    state_ = State::Preparing;
    message_ = "Preparo il microfono: attendi REC";
    return true;
}

void VoiceNotes::onBuffer(void* context, void* data, size_t length) {
    auto& self = *static_cast<VoiceNotes*>(context);
    auto* samples = static_cast<int16_t*>(data);
    uint16_t peak = 0;
    for (size_t i = 0; i < length; ++i) {
        const int32_t value = samples[i];
        peak = std::max(peak, uint16_t(value < 0 ? -value : value));
    }
    self.peak_.store(peak);
    const size_t completed = self.completed_.load(std::memory_order_relaxed) + length;
    self.completed_.store(completed, std::memory_order_release);
    if (self.requested_.load() && completed + chunkSamples <= policy::maxSamples) {
        if (M5.Mic.record(self.samples_ + completed, chunkSamples, policy::sampleRate)) return;
        self.captureError_.store(true);
    }
    self.captureFinished_.store(true, std::memory_order_release);
}

void VoiceNotes::stop() {
    requested_.store(false);
    if (state_ == State::Preparing) {
        release();
        LittleFS.remove(partial_);
        state_ = State::Idle;
        message_ = "Registrazione annullata prima di REC";
    }
}

void VoiceNotes::release() {
    M5.Mic.end();
    M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
    file_.close();
    free(samples_);
    samples_ = nullptr;
}

void VoiceNotes::update() {
    if (playback_ && !M5.Speaker.isPlaying()) {
        free(playback_);
        playback_ = nullptr;
        M5.Speaker.end();
    }
    if (state_ == State::Preparing && uint32_t(millis() - micStarted_) >= 1100 &&
        uint32_t(millis() - preparedAt_) >= 100) {
        M5.Mic.setBufferReleaseCallback(this, onBuffer);
        if (!M5.Mic.record(samples_, chunkSamples, policy::sampleRate)) {
            release(); state_ = State::Idle; fail("Avvio registrazione fallito"); return;
        }
        state_ = State::Recording;
        message_ = "REC - parla, rilascia B per salvare";
    }
    if (state_ != State::Recording && state_ != State::Saving) return;
    const size_t completed = completed_.load(std::memory_order_acquire);
    if (completed > written_) {
        const size_t samples = std::min(completed - written_, size_t(2048));
        const size_t bytes = file_.write(reinterpret_cast<uint8_t*>(samples_ + written_), samples * 2);
        written_ += bytes / 2;
        file_.flush();
        if (bytes != samples * 2) {
            requested_.store(false);
            M5.Mic.end();
            file_.close();
            finishFile(partial_);
            release(); state_ = State::Idle;
            fail("Scrittura interrotta: audio recuperabile conservato"); return;
        }
    }
    if (captureFinished_.load(std::memory_order_acquire)) {
        state_ = State::Saving;
        if (written_ != completed_.load()) return;
        release();
        state_ = State::Idle;
        if (!finishFile(partial_)) { fail("Audio conservato, finalizzazione fallita"); return; }
        succeeded_ = !captureError_.load();
        message_ = succeeded_ ? "Nota salvata sul dispositivo" : "Nota parziale salvata: errore microfono";
        finishedEvent_ = true;
        warmMicrophone();
    }
}

bool VoiceNotes::takeFinished() {
    const bool result = finishedEvent_;
    finishedEvent_ = false;
    return result;
}

size_t VoiceNotes::count() const {
    if (!mounted_) return 0;
    size_t count = 0;
    File root = LittleFS.open("/notes");
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        if (policy::noteFilename(file.name())) ++count;
    }
    return count;
}

String VoiceNotes::latest() const {
    if (!mounted_) return "";
    if (!lastSaved_.isEmpty() && LittleFS.exists(lastSaved_)) return lastSaved_;
    String path;
    time_t newest = 0;
    File root = LittleFS.open("/notes");
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        if (policy::noteFilename(file.name()) && (path.isEmpty() || file.getLastWrite() >= newest)) {
            newest = file.getLastWrite();
            path = String("/notes/") + file.name();
        }
    }
    return path;
}

bool VoiceNotes::playLatest(uint8_t volume) {
    if (busy() || playback_ || M5.Speaker.isPlaying()) return false;
    const String path = latest();
    File file = LittleFS.open(path, "r");
    if (!file || file.size() < 46 || file.size() > policy::maxSamples * 2 + 44) return false;
    const size_t size = file.size();
    playback_ = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!playback_) return false;
    if (file.read(playback_, size) != size) { free(playback_); playback_ = nullptr; return false; }
    M5.Mic.end();
    if (!M5.Speaker.begin()) { free(playback_); playback_ = nullptr; return false; }
    M5.Speaker.setVolume(policy::speakerVolume(volume));
    if (!M5.Speaker.playWav(playback_, size)) { free(playback_); playback_ = nullptr; return false; }
    return true;
}
