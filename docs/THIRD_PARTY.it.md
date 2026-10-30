[English](THIRD_PARTY.md)

# Risorse e dipendenze di terze parti

Il codice originale è distribuito sotto licenza MIT. Le risorse esterne mantengono i propri avvisi, licenze e condizioni; i marchi non sono concessi dalla licenza del codice.

| Risorsa | Versione/provenienza | Avvisi |
|---|---|---|
| M5Unified | 0.2.27, scaricato da PlatformIO | [MIT upstream](https://github.com/m5stack/M5Unified/blob/0.2.27/LICENSE) |
| M5GFX | 0.2.34, scaricato da PlatformIO | [MIT upstream](https://github.com/m5stack/M5GFX/blob/0.2.34/LICENSE); conservare anche gli avvisi dei singoli componenti |
| ArduinoJson | 7.4.2, scaricato da PlatformIO | [MIT upstream](https://github.com/bblanchon/ArduinoJson/blob/v7.4.2/LICENSE.txt) |
| PIOArduino / Arduino ESP32 / ESP-IDF | Versioni in `platformio.ini` | Distribuiti dalla toolchain con i rispettivi avvisi; non inclusi come cache nel repository |
| Inter | Sorgente TTF in `assets/fonts/` | [SIL Open Font License](../assets/fonts/OFL.txt), anche per i sottoinsiemi generati |
| Kenney Interface Sounds | 1.0, selezione Microclick | [CC0-1.0](../assets/sounds/kenney-interface-1.0/License.txt); [fonte ufficiale](https://kenney.nl/assets/interface-sounds), integrità e generazione in [assets/sounds/README.md](../assets/sounds/README.md) |
| Icona Spotify | Risorsa ufficiale in `assets/spotify/` | Marchio e risorsa di Spotify; [provenienza e linee guida](../assets/spotify/README.md) |
| SwiftUI / CoreBluetooth / Apple Speech / SF Symbols | SDK Apple installato con Xcode | Utilizzati attraverso le API di sistema; non vengono copiati nel repository |

I file generati `src/ui_fonts.h`, `src/ui_sound_data.h`, `src/ui_sound_catalog.h` e `assets/sounds/ui-sounds.bin` permettono la compilazione e l’installer senza riscaricare gli asset originali. Conservare la provenienza quando vengono rigenerati o redistribuiti.

## Core FIDO del prototipo

CanoKey core 3.0.3 e sottoinsiemi di canokey-crypto, littlefs e TinyCBOR sono inclusi con le relative licenze in `lib/CanokeyFido/vendor/`. Commit e modifiche locali: [provenienza del port](../lib/CanokeyFido/README.md). Non vengono compilati avvio hardware, provisioning o eFuse degli upstream.
