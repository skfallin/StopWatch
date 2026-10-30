# Development and verification

[Italiano](DEVELOPMENT.it.md) · [Setup](SETUP.md)

Install `requirements-dev.txt` into a virtual environment. The checked-in Xcode project is usable directly; `ios/project.yml` is its XcodeGen specification. Do not commit local signing-team changes. Firmware libraries/toolchain are pinned in `platformio.ini`.

## One verification command

```sh
.venv/bin/python tools/verify.py --firmware
```

This builds the normal firmware, checks generated translations, scans working files and reachable Git blobs for high-confidence secret patterns, and runs the C++/Python checks. It never flashes a device or contacts your Home Assistant. After the first build, omit `--firmware` to reuse the installed ArduinoJson headers.

On macOS with Xcode selected, also run:

```sh
.venv/bin/python tools/verify.py --swift --lab
xcodebuild -project ios/StopWatch.xcodeproj -scheme StopWatch \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath /tmp/stopwatch-derived CODE_SIGNING_ALLOWED=NO build
```

`--swift` tests Companion models/protocols and the Swift-to-C++ Badge fixture. `--lab` compiles the opt-in experimental FIDO firmware; it does not enroll any credentials. The audio archive tests use CommonCrypto and run on macOS. Linux runs the portable host checks. The GitHub Actions workflow covers both runners, Swift tests on macOS and an unsigned simulator build. Remote CI results are only available after pushing.

When Xcode is installed but not selected, set `DEVELOPER_DIR` to your own Xcode application's `Contents/Developer`. Test temporary files use the OS temporary directory. No personal absolute paths are required.

## Languages and configuration

`locales/en.json` maps existing Italian message keys to reviewed English translations. Firmware uses an explicit lookup before composing messages, so user-provided names and protocol IDs are never translated. `language` and `time_zone` are optional configuration fields; old clients preserve them. Existing saved configurations lacking these fields migrate to Italian/Rome; new devices start in English/UTC.

The firmware publishes supported `time_zones`; Companion and portal use that list. Rules live in `include/regional_settings.h` and are intentionally versioned, not fetched from a network. Add tests when changing DST rules. Do not silently use a fixed UTC offset for a region with DST.

The app uses native Foundation localization and en/it resources, with English as the development language. App language follows iOS. Speech recognition is on-device only and has a separate en-US/en-GB/it-IT selection. Existing paired installations retain their previous Italian speech/keyboard defaults.

```sh
.venv/bin/python tools/generate_localizations.py
.venv/bin/python tools/generate_localizations.py --check
```

Edit `web/portal.html`, not its generated `src/portal_page.h`. Do not edit generated `include/i18n.h` or `Localizable.strings` directly. If adding Swift interpolation, update the generator's format mapping and check compiler-extracted `.stringsdata`; formatting arguments must match in both languages. Native iOS permission strings live in each `InfoPlist.strings`.

## Assets and visual checks

Fonts, sounds and app icons are included. Regeneration is optional and may require ffmpeg/Xcode:

```sh
.venv/bin/python tools/generate_ui_fonts.py
.venv/bin/python tools/generate_ui_sounds.py
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-scene --scene
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-clock
```

The host renderer runs production drawing code. Label checks cover English and Italian launcher names. It does not verify screen hardware, radio behavior, haptics or battery life.

## Release procedure

1. Review intended tracked and untracked files; a successful local build may still depend on files absent from Git.
2. Run checks in a clean source snapshot/checkout with only intended source files. Include generated assets and vendored LAB sources/licenses.
3. Build and verify the sound installer as well as the normal firmware. Preserve `partitions.csv`.
4. Complete [hardware checks](STATUS.md), document exact versions and remaining limits, and review third-party notices, historical assets and personal data.
5. Create release artifacts/checksums only from that verified version. Visibility changes, pushes, tags, GitHub releases and app distribution are separate explicit operations.

The secret-pattern guard is deliberately limited. It does not prove absence of all passwords, Home Assistant tokens, personal data or licensing issues. Review history, GitHub logs/artifacts and screenshots before making the repository public.
