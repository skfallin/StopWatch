# Architecture

[Italiano — detailed module reference](ARCHITECTURE.it.md)

The firmware is Arduino ESP32 on ESP-IDF/FreeRTOS, with M5Unified/M5GFX for C152 peripherals. The iPhone Companion uses native SwiftUI/CoreBluetooth. No shared cloud backend is required.

- `src/main.cpp`: device lifecycle, navigation, input routing and coordination.
- `src/settings.*`: validated, atomic configuration updates persisted in NVS. Passwords/tokens are excluded from ordinary reads. Optional fields preserve older-client behavior.
- `include/regional_settings.h`, `include/i18n.h`: explicit timezone rules and generated English/Italian text lookup. New defaults are English/UTC; legacy stored settings migrate to Italian/Rome.
- `src/*_view.cpp`: screen drawing; `include/`: host-testable models, policies and protocol logic.
- `src/voice_notes.*`, `badge.*`, `ui_sounds.*`: LittleFS content with validation, bounded memory and preservation of user files. Partition layout stays unchanged.
- `src/companion.*`: authenticated BLE protocol 1. `note_transfer.*` negotiates per-transfer HTTPS tickets/certificate pinning through BLE.
- `src/home_assistant.*`, `mqtt_bridge.*`: direct HTTP(S)/MQTT integration. `ios/StopWatch/HomeRelay.swift` provides the optional phone bridge.
- `src/usb_control.*`: HID keyboard/mouse and standalone presentation remote. `security_key.cpp` supplies inert stubs in the normal build; `STOPWATCH_FIDO_LAB` enables the test authenticator.
- `web/portal.html`: local portal source; generated into a firmware header with reviewed translations.

Device settings, user names, entity IDs and protocol keys remain distinct from translated messages. iPhone models preserve optional configuration fields; saving one editor rereads current configuration and modifies only that section. The firmware publishes the supported time-zone IDs rather than duplicating that list in clients.

Notes and Badge transfers validate size/content before activating replacements. Lost acknowledgements do not justify blindly retrying a home-control command. Background iPhone activity is opportunistic and bounded by iOS. Host tests and simulators do not prove radio reliability or battery life.
