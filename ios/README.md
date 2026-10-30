# StopWatch Companion

[Italiano](README.it.md) · [Project](../README.md)

Native iPhone app, **0.9.0 / build 22**, iOS 17+. SwiftUI, CoreBluetooth and Apple Speech; no third-party runtime dependencies. English is primary, Italian is supported through native iOS localization.

## Install

Open `StopWatch.xcodeproj` in Xcode, select the StopWatch target and your Team in Signing & Capabilities. Connect/authorize your iPhone, enable Developer Mode if requested, and Run. The repository contains no signing certificates or team identity. If `dev.stopwatch.companion` is unavailable to your team, choose your own bundle ID for the first installation; preserve it and the team on subsequent updates.

App Store/TestFlight distribution is not configured. The simulator can build/preview the UI but cannot test device Bluetooth pairing. XcodeGen can regenerate the project using `xcodegen generate --spec ios/project.yml` from the repository root.

## Pair and configure

Open System → iPhone on StopWatch, press B and enter its six-digit code in the native iOS pairing request after selecting the nearby device. The window lasts 90 seconds. Reconnects use system-managed bonds. If bonds were removed on one side, forget StopWatch in iOS Bluetooth settings and pair again.

Settings → Language and time zone configures the **device**. The app follows iOS app-language preferences, with English fallback. Settings → Transcription language independently selects English US/UK or Italian. Existing paired installations preserve their previous Italian transcription/keyboard defaults. The keyboard layout in USB Bridge must match the computer.

Notes are downloaded over pinned HTTPS when reachable, otherwise BLE. Originals are not removed automatically. Transcription requires Apple Speech authorization and a supported on-device model; missing availability leaves audio saved with Retry. Background reception is bounded by iOS scheduling. Unlock once after restarting, keep Bluetooth enabled and avoid force-quitting the app. Transcription starts with the app open.

Badge prepares images/GIFs with a circular crop and actual RGB565 colors. Inputs: up to 25 MB; output: up to 3 MB, 120 frames and 15 seconds, 5/10 fps. Upload verification preserves the previous Badge until the replacement is valid.

Home Assistant's remote bridge is optional and requires your HTTPS endpoint and administrator token, stored in the iOS keychain. See [integration setup](../home_assistant/README.md). Device Wi-Fi/MQTT passwords are not exported.

## Development

See [checks](../docs/DEVELOPMENT.md). The `ios/Package.swift` package tests models/protocols on macOS, including localized resources. Launch the app with `--preview` for an offline UI preview; this does not initialize Bluetooth or invent connected-device data.

BLE service UUID remains `75f3a100-9e9a-4b9c-9d73-c85f44e6a001`; JSON protocol remains version 1. Regional fields are optional, and settings edits reread current device configuration before applying only the selected section.
