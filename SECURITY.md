# Security

[Italiano](SECURITY.it.md)

## Reporting a vulnerability

Do not post credentials, exploit details exposing users, flash dumps or private recordings in a public issue. Use GitHub's private vulnerability reporting on this repository when enabled. If it is unavailable, open an issue containing only “Please enable private vulnerability reporting” and wait for a private channel before sharing details. Maintainers should enable the private reporting feature before public release; it is not configured by files in this repository.

Only the latest beta is intended to receive fixes. There is no security SLA, independent audit or FIDO certification.

## Credentials and data

- Enter Wi-Fi/MQTT credentials and Home Assistant tokens through the device, paired Companion or temporary local portal. Never commit them or embed them in example files.
- Device configuration and BLE bonds are stored locally in NVS. **Flash is not encrypted** by this project. Physical access or replacement firmware may expose stored secrets.
- The iPhone bridge stores its Home Assistant token in the iOS keychain. The remote bridge currently requires an administrator token for its Home Assistant subscription flow; enable it only if you accept that scope.
- Note audio, transcripts and Badge content remain on the device/phone. Speech requests require on-device recognition; no cloud fallback is enabled.
- HTTPS transfer tickets and exact device certificates arrive over authenticated BLE. Do not disable certificate verification. HTTP and MQTT without TLS are only suitable for a trusted local network.
- Pairing, credential reset and destructive storage initialization require the documented physical interactions. Export important notes before firmware replacement.

## Experimental FIDO build

The standard `stopwatch` build does not start or register the FIDO authenticator. `stopwatch-lab` explicitly enables it for disposable test accounts. It is a software prototype on a multipurpose device, without hardware isolation, flash-extraction resistance or secure-boot provisioning. Never make it the only way to access a real account or vault. See [LAB documentation](docs/SECURITY_KEY.md).

## Before publication

`.gitignore` excludes common credentials, exports and signing material, but cannot detect secrets pasted into code. Run `python tools/check_public_repo.py` and review all intended files, reachable Git history, old assets, screenshots and any GitHub logs/artifacts. The scanner reports paths only and detects a narrow set of high-confidence patterns; it is not a complete security audit.

If a secret is exposed, revoke/rotate it at its service. Deleting the latest file does not remove Git history. Coordinate any history rewrite with collaborators; never force-push as an incidental cleanup.
