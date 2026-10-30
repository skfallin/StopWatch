# Public beta status

[Italiano](STATUS.it.md)

Candidate: firmware **0.18.0-beta.1**, Companion **0.9.0 / build 22**, BLE protocol **1**. This describes the source candidate, not a published binary or signed app.

## Beta scope

C152 only. English/Italian firmware, local portal and Companion; configurable device time zone; separate on-device transcription language; clock/timer/stopwatch/Focus, notes, Badge, USB Bridge/Slide and optional Home Assistant/Spotify. New configurations start English/UTC; existing stored configurations preserve Italian/Rome. Previous paired iPhone installations keep their Italian speech/keyboard defaults.

The normal build disables FIDO. `stopwatch-lab` is an explicit test-only variant. No Android client, App Store/TestFlight distribution, Companion OTA or generic iOS notifications are promised.

## Local verification — 30 October 2026

- Normal and LAB firmware builds passed, using pinned dependencies.
- Sound installer build passed; runtime and installer host tests passed, including atomic replacement, corruption/failure paths and preservation of user files.
- C++/Python suite passed, including navigation, power policy, commands, protocol validation, blueprints and regional defaults/migration/DST checks.
- **45 Swift tests passed**, including English/Italian resource formatting and regional settings compatibility.
- Unsigned iOS simulator build passed. Compiler-extracted localization keys were checked against generated resources.
- Firmware host render: **76 Scene views in each language, no truncated labels**; clock and Focus views were also rendered. This is not a physical screen or iOS visual test.
- Build and full checks also passed in a source-only snapshot of intended Git files, reusing only external toolchain/library caches.
- Portal browser checks with synthetic data passed: English/Italian switching, regional-setting save and persistence after reload.
- Working files and reachable local Git history passed the narrow high-confidence secret-pattern check. This is not an exhaustive audit of every possible secret or personal datum.

No firmware was flashed, no account credentials were enrolled, no app was distributed and no repository visibility was changed as part of this preparation. No iOS simulator was running for an interactive visual check. Remote CI results for each pushed revision are available in [GitHub Actions](https://github.com/skfallin/StopWatch/actions); the local results above do not certify remote CI.

## Before tagging/publishing

- [ ] Review and commit the intended source files, including currently untracked vendored code, notices and generated resources. Recheck the exact committed revision.
- [ ] Complete physical C152 + iPhone checks below and record the tested versions.
- [ ] Enable GitHub private vulnerability reporting and review repository metadata, old assets and any remote logs/artifacts.
- [ ] Run the committed revision through remote CI; build release artifacts/checksums from that revision.
- [ ] Publish explicitly when approved. Do not describe a build-only check as hardware validation.

## Hardware checklist

Use synthetic content and test accounts. Export important notes first.

- [ ] New device: English/UTC; choose Italian and another time zone through both Companion and portal, then reboot and confirm persistence.
- [ ] Upgrade: previous Italian/Rome configuration, notes, Badge, audio pack and BLE bonds preserved; no erase/uploadfs.
- [ ] Both languages: launcher, settings, recording/errors, pairing, portal and Companion at normal/large text sizes; no clipped critical controls.
- [ ] Record/play a note, download via HTTPS and BLE, transcribe with supported English and Italian models; denied permission/model unavailable preserves audio.
- [ ] Badge upload, cancellation, interrupted transfer and ordinary firmware update preserve the previous content.
- [ ] Home Assistant actual-state confirmation and one Spotify action over direct Wi-Fi and the iPhone bridge, including locked iPhone.
- [ ] USB Bridge US/Italian keyboard layouts, pointer, disconnect/suspend and Slide in a focused presentation application.
- [ ] Normal build has no functioning FIDO authenticator; LAB is tested only with disposable credentials and a fresh physical confirmation.

## Known limits

Time zones are an explicit list, not the full IANA database; future DST rule changes require firmware updates. App, device, speech and keyboard languages/layouts are independent. iOS controls background scheduling. Device flash is not encrypted. The remote Home Assistant bridge requires administrator scope. Battery life, acoustic quality and radio reliability are not established by host tests.
