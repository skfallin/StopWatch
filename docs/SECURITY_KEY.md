# USB security key — experimental LAB

[Italiano — protocol and test details](SECURITY_KEY.it.md)

**Disabled in the normal beta. Test accounts only.** The `stopwatch` build neither registers nor starts the FIDO authenticator. Existing `/security-key.lfs` data is preserved; ordinary note cleanup does not erase it.

Explicit opt-in build:

```sh
.venv/bin/python -m platformio run -e stopwatch-lab
```

Flashing is a separate, deliberate action. Never use this prototype as the only access to a real account or password vault. It is not FIDO-certified or independently audited and has no flash encryption, secure-boot provisioning or physical-extraction resistance. USB keyboard/mouse support remains independent of this experiment.

The LAB build integrates pinned CanoKey sources for resident credentials, ES256, PIN protocols and WebAuthn PRF. Confirmation requires a fresh physical B-button interaction with the relying-party prompt visible. USB framing, cancellation, RP validation and physical-presence gates have host tests; full WebAuthn behavior requires actual hardware.

See `tools/key_lab/` and the Italian technical reference for disposable local tests. That test server is loopback-only and does not store PIN/PRF secrets. Do not enroll production accounts as part of beta verification. [Vendored sources and modifications](../lib/CanokeyFido/README.md).
