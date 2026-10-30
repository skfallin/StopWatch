# Contributing

[Italiano](CONTRIBUTING.it.md)

Start with [setup](docs/SETUP.md) and [development checks](docs/DEVELOPMENT.md). Keep changes focused and preserve BLE protocol compatibility, saved configuration and user files. Add tests for changed behavior; distinguish host checks from actual hardware evidence.

Use English for shared documentation and new translation keys only when adding genuinely new text. Existing Italian localization keys are stable identifiers; edit translations in `locales/en.json` and regenerate with `python tools/generate_localizations.py`. Keep Italian available. Do not translate JSON fields, MQTT topics, entity IDs, filenames, user labels or security relying-party IDs.

Bug reports should include firmware/app versions, hardware, steps, expected/actual behavior, language and connection path. Remove Wi-Fi names, private addresses, tokens, personal photos/audio and device exports from attachments. Use the security reporting path for vulnerabilities.

Before a pull request, run the applicable checks, review your diff and state anything requiring hardware verification. Do not commit build caches, signing files or personal data. Contributions to original project code use the repository's MIT license; preserve third-party notices.
