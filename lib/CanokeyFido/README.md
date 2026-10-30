# CanoKey FIDO port — StopWatch LAB

Only the FIDO core is built. No upstream board startup, USB stack, provisioning tool, CCID, OTP, OpenPGP, PIV or eFuse code is included in the build.

Pinned sources (Apache-2.0 unless the included component license states otherwise):

| Component | Source | Commit |
|---|---|---|
| CanoKey core 3.0.3 | https://github.com/canokeys/canokey-core | `7644370f0c16d5b5e0d2f503e6c47bd39aaa0e2f` |
| canokey-crypto | https://github.com/canokeys/canokey-crypto | `c65b9015d44115182ac5106f58a228efb569c93f` |
| littlefs | upstream submodule | `c733d9ec5776dfc949ec6dc71fa9ce3ff71de6e5` |
| TinyCBOR | upstream submodule | `d393c16f3eb30d0c47e6f9d92db62272f0ec4dc7` |

The `vendor/` tree is a source subset. Original licenses and source notices remain in it. The library manifest lists the compiled files explicitly.

## Local changes

- `port/crypto.c`: ESP-IDF mbedTLS 3 implements SHA-1/SHA-256, AES and P-256 ECDH/ECDSA; ESP-IDF supplies hardware randomness. Only ES256 is available. No custom ECC or AES implementation.
- `port/storage.cpp`: a 128 KiB inner littlefs image in `/security-key.lfs`, backed by the existing archive. Existing unreadable images are never automatically formatted. No partition changes.
- `port/core.c`: clock, sanitized RP display, cancellation/keepalive and physical presence hooks. Only the firmware's physical presence gate may return confirmation.
- `vendor/include/key_fs.h`: renamed from `fs.h` to avoid Arduino's case-insensitive FS header; a C++ reserved parameter name was changed.
- `ctap.c`: zero AAGUID, no attestation (`fmt=none`, empty statement), ES256 only, USB transport only, no CTAP1 advertising, physical confirmation before reset, no implicit reinitialization of a partial credential store, transient state lock hook.
- `ctap-parser.c`: the same RP bytes used to compute the credential RP hash are validated and displayed by the port; missing RP IDs are rejected.
- `secret.c`: ES256-only signing path.
- `key_cbor.h` avoids the framework's same-named header; only the TinyCBOR parser and encoder are compiled. Its string iterator avoids arithmetic on a null destination during length-only queries.
- Private build flags namespace CBOR, littlefs and conflicting hash/HMAC symbols to avoid collisions with ESP-IDF's archive and Wi-Fi libraries. TinyCBOR recursion is bounded to eight levels; diagnostic dumps are disabled.

USB CTAPHID and UI live in `src/security_key.cpp`, `include/ctaphid_transport.h` and `include/key_presence.h`. They use a custom TinyUSB class driver so FIDO has its own unnumbered 64-byte HID interface alongside the existing numbered keyboard/mouse HID and CDC interfaces.

This is an experimental software authenticator in a multipurpose device. It is not certified, isolated or resistant to flash extraction / replacement firmware. See `docs/SECURITY_KEY.md` before enrolling credentials.
