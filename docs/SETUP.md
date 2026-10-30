# Installation and updates

[Italiano](SETUP.it.md) · [Home](../README.md)

## Requirements

- M5Stack StopWatch **C152**, a USB data cable and Git.
- Python 3.11 or newer with venv/pip; Internet access for pinned dependencies.
- Firmware: PlatformIO, installed below. Host checks: clang/clang++ with C++17.
- Companion: macOS, Xcode, an Apple signing team and an iPhone with iOS 17+.

Home Assistant and Spotify accounts are not required to build or use standalone features.

## Build the normal beta

```sh
git clone https://github.com/skfallin/StopWatch.git
cd StopWatch
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python -m platformio run -e stopwatch
```

On Windows, use `.venv\Scripts\python.exe` in place of `.venv/bin/python`. The normal environment has no working FIDO authenticator. Do not select `stopwatch-lab` unless you intend to run the experimental test-only authenticator.

The project pins PIOArduino 55.03.37 / Arduino ESP32 3.3.7, M5Unified 0.2.27, M5GFX 0.2.34 and ArduinoJson 7.4.2. Keep `NETWORK_EVENTS_MUTEX`. Build caches and user credentials are not part of Git.

## Flash and initialize

List ports, then replace `YOUR_PORT` with the actual USB device:

```sh
.venv/bin/python -m platformio device list
.venv/bin/python -m platformio run -e stopwatch --target upload --upload-port YOUR_PORT
```

If necessary, hold power/reset for about two seconds until the green LED appears, release, and retry. If the device remains in download mode afterwards, briefly press power/reset.

**First installation:** export data from the previous firmware first. Compatibility with another firmware's partition layout is not guaranteed. If storage is uninitialized, use System → Diagnostics → Storage / RAM and confirm physically. Initializing storage is destructive to notes and Badge.

**Existing StopWatch installation:** keep `partitions.csv` unchanged. Do not erase flash, upload a filesystem image, or initialize storage as part of an ordinary update. A standalone `firmware.bin` does not contain the bootloader and partition table; PlatformIO upload supplies the correct components.

## Install the sound pack

The normal firmware uses fallback tones if Microclick is absent. To install/update it without erasing recordings:

```sh
.venv/bin/python tools/build_sound_installer.py /tmp/stopwatch-sound-installer
```

Follow the upload command printed by the script. Wait for `[audio-store] pronto=1 cache=1`, then upload the normal firmware again. The installer verifies the bundled archive and atomically replaces only `/ui-sounds.bin`. Insufficient space fails without deleting user files. Never substitute `uploadfs` or erase. See [sound sources and migration](../assets/sounds/README.md).

## Configure your own device

1. Install and pair [Companion](../ios/README.md). On StopWatch open System → iPhone, then press B for the pairing code. The window lasts 90 seconds.
2. In Companion Settings → Language and time zone, select English or Italian and one of the time zones reported by the firmware. New devices use UTC; existing configurations migrate to Europe/Rome. Listed DST rules apply automatically; the list is not a full IANA time-zone database.
3. Configure 2.4 GHz Wi-Fi. Alternatively open System → Wi-Fi browser on StopWatch, join the temporary network using its QR code and open the local page. Use username `stopwatch` and the random password shown on the display. The portal also exposes language and time zone.
4. Choose transcription language in Companion Settings. English (US/UK) and Italian require a supported on-device Apple Speech model. Missing permission/model leaves the recording intact.
5. Optionally set up [Home Assistant](../home_assistant/README.md). No addresses, credentials or entities need to be edited in source code.

Device language, app language, transcription language and computer keyboard layout are separate choices. App language follows iOS preferences with English as fallback. Choose the same USB keyboard layout as on your computer.

## Update and recover

Download or build the intended version, export important notes, upload the normal firmware and reopen Companion. Preserve the app bundle ID and signing team when updating an existing installation, so its local data and pairing identity remain available. Do not publish flash/NVS backups.

After an update, check touch/buttons, recording/playback, pairing, language after reboot and one real command per integration you use. Verify background iPhone behavior on actual hardware; simulator builds cannot establish Bluetooth compatibility. See the [beta checklist](STATUS.md).
