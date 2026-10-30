# Sviluppo e verifiche

[English](DEVELOPMENT.md)

Installa `requirements-dev.txt` nel virtualenv e usa:

```sh
.venv/bin/python tools/verify.py --firmware
```

Il comando compila il firmware, controlla cataloghi generati, cerca pattern di segreti nei file e nello storico Git ed esegue test C++/Python. Non carica firmware né contatta Home Assistant. Dopo il primo build puoi omettere `--firmware`.

Su macOS con Xcode:

```sh
.venv/bin/python tools/verify.py --swift --lab
xcodebuild -project ios/StopWatch.xcodeproj -scheme StopWatch \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath /tmp/stopwatch-derived CODE_SIGNING_ALLOWED=NO build
```

`--swift` aggiunge test Companion e interoperabilità Badge Swift/C++; `--lab` compila la chiave sperimentale senza registrare credenziali. I test audio CommonCrypto richiedono macOS. Il workflow GitHub Actions copre Linux e macOS; il risultato remoto sarà disponibile soltanto dopo un push.

Traduzioni: modifica `locales/en.json` e `web/portal.html`, poi esegui `python tools/generate_localizations.py`. `--check` controlla che gli output siano aggiornati. Non tradurre identificatori del protocollo o contenuti degli utenti. L’app usa le risorse native en/it e i permessi localizzati; la lingua della trascrizione è separata.

`language` e `time_zone` sono opzionali: vecchi client li conservano. Le configurazioni precedenti migrano a italiano/Roma; i nuovi dispositivi partono in inglese/UTC. Il firmware espone l’elenco dei fusi supportati e applica regole DST versionate.

Prima di una release verifica una copia pulita contenente soltanto i sorgenti destinati a Git, includendo asset generati e licenze vendorizzate. Compila anche l’installer audio, conserva le partizioni e completa la [checklist hardware](STATUS.it.md). La scansione automatica dei segreti è limitata: non certifica l’assenza di password, token o dati personali. Pubblicazione, push, tag e distribuzione dell’app sono operazioni separate.
