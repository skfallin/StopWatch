# Third-party notices

[Italiano](THIRD_PARTY.it.md)

Original project code is licensed under [MIT](../LICENSE). The project license does not replace component licenses or grant trademark rights. Keep the following notices with redistributed sources/assets and review toolchain notices when distributing binaries.

| Component | Pinned source | License / notice |
|---|---|---|
| M5Unified | 0.2.27, downloaded by PlatformIO | MIT; preserve upstream LICENSE |
| M5GFX | 0.2.34, downloaded by PlatformIO | MIT plus notices in individual components |
| ArduinoJson | 7.4.2 | MIT |
| PIOArduino / Arduino ESP32 / ESP-IDF | `platformio.ini` | Respective bundled notices; build caches are not vendored |
| Inter | `assets/fonts/InterVariable.ttf` | [SIL OFL](../assets/fonts/OFL.txt), including generated subsets |
| Kenney Interface Sounds | 1.0, Microclick selection | [CC0 notice](../assets/sounds/kenney-interface-1.0/License.txt), [source manifest](../assets/sounds/README.md) |
| Spotify icon | `assets/spotify/` | Spotify trademark/asset; [source and usage guidelines](../assets/spotify/README.md) |
| CanoKey core 3.0.3 and crypto | `lib/CanokeyFido/vendor/` | Apache-2.0; preserved [license](../lib/CanokeyFido/vendor/LICENSE) and [port provenance](../lib/CanokeyFido/README.md) |
| littlefs / TinyCBOR | Subsets in the same vendor tree | Included component licenses and source notices |
| Apple frameworks / SF Symbols | Installed SDK | Platform APIs; their SDKs are not copied into this repository |

Generated firmware fonts, sound headers/archive and localization resources are checked in. Retain source attribution when regenerating. The normal firmware disables the FIDO implementation; the vendored source remains available for the explicit LAB build.

Older sound packs may remain in Git history even though the current tree uses Kenney sounds. Publicizing an existing history exposes those older assets too: review their historical licenses before changing visibility. No history rewrite is performed by the preparation scripts.
