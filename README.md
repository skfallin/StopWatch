# StopWatch

[Italiano](README.it.md)

Community firmware for **M5Stack StopWatch C152**, with an iPhone companion and optional Home Assistant integration. Record voice notes, display photos and GIFs, run timers and focus sessions, and control your home or a USB-connected computer.

**Public beta candidate:** firmware `0.18.0-beta.1`, Companion `0.9.0` (build 22), BLE protocol `1`. This repository prepares a beta; it does not imply that a signed app or downloadable release has been published. Not an official M5Stack product.

## What works without an account

- Clock faces, timer, stopwatch, Pomodoro and voice recording on the device.
- Badge photos/GIFs, note downloads and on-device transcription with the iPhone app.
- USB presentation remote; mouse and keyboard control from the paired iPhone.
- English and Italian interfaces. New devices start in English and UTC. Existing saved configurations retain Italian and Rome time.

Home Assistant, MQTT and Spotify are optional. They use your own installation and credentials. There is no project cloud service or shared account.

## Start here

1. Read [installation and updates](docs/SETUP.md). Supported hardware is C152 only.
2. Install the [iPhone Companion](ios/README.md), or configure the device through its local Wi-Fi portal.
3. Choose the device language and time zone. The iPhone app follows iOS language preferences; transcription language is selected separately.
4. Optionally configure [Home Assistant and Spotify](home_assistant/README.md).

Firmware builds use PlatformIO. Companion requires macOS, Xcode and an iPhone running **iOS 17+**, signed with your own Apple team. Android, App Store/TestFlight distribution and Companion OTA updates are not part of this beta. Firmware compilation can run on macOS/Linux/Windows; the complete test suite requires macOS.

## Before installing a beta

Back up notes and personal content before replacing firmware. Ordinary updates preserve the existing partition layout, configuration, notes and Badge. **Do not use erase or uploadfs for an ordinary update.** First installation on another firmware's partition layout is a separate, potentially destructive operation.

The USB FIDO2 security-key prototype is disabled in the normal build. The explicitly selected `stopwatch-lab` build is for test accounts only; it has no flash-extraction protection or independent security audit. See [LAB limitations](docs/SECURITY_KEY.md).

## Documentation

| Guide | Purpose |
|---|---|
| [Setup](docs/SETUP.md) | Build, install, configure and update |
| [Development](docs/DEVELOPMENT.md) | Repeatable local and CI checks |
| [Beta status](docs/STATUS.md) | Scope, limits and release checklist |
| [Architecture](docs/ARCHITECTURE.md) | Modules, storage and transports |
| [USB control](docs/USB_CONTROL.md) | Pointer, keyboard and presentation remote |
| [Navigation](docs/UI_NAVIGATION.md) | Touch and physical controls |
| [Contributing](CONTRIBUTING.md) | Changes and bug reports |
| [Security](SECURITY.md) | Credentials and vulnerability reporting |
| [Third-party notices](docs/THIRD_PARTY.md) | Dependency and asset licenses |

Original project code is [MIT licensed](LICENSE). Third-party components and trademarks retain their own terms. Generated fonts, sound data and localization catalogs are included; ordinary builds do not require regenerating them.
