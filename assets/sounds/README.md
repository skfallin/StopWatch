# UI SFX

Source: https://github.com/romainsimon/uisfx, npm `uisfx@0.4.0`.
Audio license: CC0-1.0; original notice in `uisfx-0.4.0/LICENSE-AUDIO`.

The original Ogg files for select, open, back, success, error, warning, complete and volume-change are included for all twelve packs. The manifest is the unmodified upstream catalog; unused audio is omitted.

Source archive: https://registry.npmjs.org/uisfx/-/uisfx-0.4.0.tgz
Verified npm integrity: `sha512-W891N75C09cvpaNeChP+7BiP0Yp5VSbrIcRgZvzpFUT3o/2DxZqROmUcX50oxeyMnY4Hqxu4jEfjndVUpmheEQ==`.

Run `python3 tools/generate_ui_sounds.py` with ffmpeg installed to regenerate `src/ui_sound_data.h`. It decodes mono 16 kHz float PCM, normalizes to -14 dBFS RMS, applies soft-knee 2:1 peak-detected compression (threshold 0.2, attack 1 ms, release 25 ms), then makeup toward -9 dBFS RMS and a 94% peak lookahead limiter (attack 2 ms, release 5 ms, automatic level disabled, latency compensated). The upstream `defaultVolume` is no longer applied: all cues use the same target. The limiter can reduce the average level of transient-heavy cues; the generator requires a final range within 4 dB.

The result is mono 16-bit PCM. Assertions check the original sample count, headroom, non-silence, RMS bounds, lossless zlib decompression and flash budget. Current output: 96 cues, 1,440,941 compressed bytes, RMS -12.5 to -9.0 dBFS, peaks no higher than 94%. The ESP32-S3 ROM decoder expands one cue into a reusable PSRAM buffer; dynamics processing consumes no runtime CPU on the watch. Original files, licenses and the generator are not flashed; partition sizes remain unchanged. Since firmware 0.8.1 the 1,440,941 compressed bytes live in `/ui-sounds.bin` on LittleFS, sharing storage with notes and Badge; only the generated catalog and SHA-256 remain in the normal firmware. These are digital checks; speaker distortion and perceived loudness still require listening on the physical device.


## Filesystem archive and migration

`generate_ui_sounds.py` also writes `ui-sounds.bin` and `src/ui_sound_catalog.h`. The archive concatenates the existing zlib streams, in the unchanged pack/cue order. The catalog supplies offsets, compressed/decoded sizes and the full-file SHA-256. The legacy `src/ui_sound_data.h` is retained only for the one-time installer and host equivalence tests; normal firmware does not include it.

`tools/build_sound_installer.py` builds the temporary `SFX_SEED_ARCHIVE` firmware using the same pinned project dependencies as the normal build. The installer writes a temporary archive, validates it and atomically renames it. It never formats LittleFS or removes user files. Keep that firmware until `[audio-store] pronto=1 cache=1`, then upload the normal firmware. Repeating the installer with the same valid archive does not rewrite the file.

At runtime the whole file is checked once per boot, then only the selected compressed pack is cached in PSRAM (largest: 199,492 bytes), plus the existing PCM/decoder buffer. Switching packs replaces this cache. Playing cues within it requires no filesystem reads. Missing/corrupt archives use short fallback tones, honoring Silence, the configured interaction volume and ongoing note playback.

Archive initialization preserves the sound file on a mounted, healthy volume by deleting only recognized user notes, partial notes and Badge. Formatting an unmountable volume still requires physical confirmation and requires reinstalling sounds afterward.

Hardware check, firmware 0.8.1: full archive SHA-256 matched after both installer and normal boot; LittleFS used 2,854,912 of 8,323,072 bytes in both cases. Cached packs measured 21 ms (Zen), 47 ms (Organic), 58 ms (Dreamy), and 35 ms (Sci-fi). These are pack-change loads; subsequent cues use the cache. Normal firmware uses 2,366,267 / 4,194,304 program bytes (56.4%); no embedded cue arrays remain in its ELF.
