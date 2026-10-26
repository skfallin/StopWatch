# Sviluppo e verifiche

## Preparazione

Seguire [SETUP.md](SETUP.md) e completare almeno una compilazione firmware: alcuni test riusano ArduinoJson da `.pio/libdeps/stopwatch/ArduinoJson/src`. I test Python richiedono il compilatore `clang++` nel `PATH`; i test blueprint richiedono Jinja2 e PyYAML, inclusi in `requirements-dev.txt`.

## Test del firmware senza hardware

Dalla radice, con un compilatore C++17:

```sh
for name in core clock relay http_transfer; do
  clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Iinclude "tests/${name}_test.cpp" -o "/tmp/stopwatch-${name}-test"
  "/tmp/stopwatch-${name}-test"
done

clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Iinclude -I.pio/libdeps/stopwatch/ArduinoJson/src \
  tests/music_test.cpp -o /tmp/stopwatch-music-test
/tmp/stopwatch-music-test

.venv/bin/python tests/quiet_test.py
.venv/bin/python tests/power_test.py
.venv/bin/python tests/home_interaction_test.py
.venv/bin/python tests/relay_dispatch_test.py
.venv/bin/python tests/artwork_cache_test.py
.venv/bin/python -m unittest discover -s tests -p '*blueprint_test.py'
```

I test Python che estraggono le funzioni del firmware compilano il codice reale con backend controllati. Controllano, tra l’altro, conferme delle azioni, silenzio globale, frequenza CPU, luminosità, compatibilità della configurazione, cache e scelta del trasporto. Non sostituiscono un collaudo radio o una misura dell’autonomia.

Per l’archivio audio, su macOS:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -fsanitize=address,undefined -Itests/audio_support -Iinclude -Isrc \
  tests/audio_archive_test.cpp src/ui_sounds.cpp -lz -o /tmp/stopwatch-audio-test
/tmp/stopwatch-audio-test assets/sounds/ui-sounds.bin
```

Il supporto SHA-256 del test usa CommonCrypto. Un porting di questo test a Linux richiede un backend equivalente; il firmware usa invece mbedTLS/ESP-IDF.

Per verificare anche l’installer e i suoi errori di scrittura/rename, ripetere la compilazione del test audio aggiungendo `-DSFX_SEED_ARCHIVE`, poi eseguire lo stesso comando con `assets/sounds/ui-sounds.bin`.

## Companion iOS

I test dei modelli e del protocollo girano su macOS senza simulatore:

```sh
STOPWATCH_BADGE_FIXTURE=/tmp/stopwatch-badge-fixture.swb \
  xcrun swift test --package-path ios --scratch-path /tmp/stopwatch-protocol-tests
```

La variabile facoltativa esporta un Badge sintetico a quattro quadranti tramite l’encoder Swift reale. Serve al test di interoperabilità C++:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Iinclude tests/badge_test.cpp -lz -o /tmp/stopwatch-badge-test
/tmp/stopwatch-badge-test /tmp/stopwatch-badge-fixture.swb
```

Il test richiede il percorso del file esportato e zlib (`-lz`), fornita dal sistema su macOS. Non usare un’immagine personale come fixture.

La compilazione per iPhone richiede Xcode e un proprio Team configurato. Le istruzioni per firma, simulatore e dispositivo reale sono in [ios/README.md](../ios/README.md). Conservare bundle ID e identità di firma durante gli aggiornamenti per mantenere l’app esistente e i suoi dati.

Il progetto Xcode è incluso. Se si usa XcodeGen, `ios/project.yml` è la specifica: mantenerla allineata a versioni, file e impostazioni del progetto generato. Non pubblicare profili, certificati o chiavi Apple.

## Asset e anteprime

I file generati sono già versionati. Rigenerarli solo quando si modifica la risorsa sorgente:

```sh
.venv/bin/python tools/generate_ui_fonts.py
.venv/bin/python tools/generate_ui_sounds.py
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-clock-preview
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-music-preview --music
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-quick-preview --quick
```

La generazione dei suoni richiede `ffmpeg` nel `PATH`. Font, suoni, catalogo e archivio devono restare coerenti: eseguire i relativi controlli prima di caricare sul dispositivo. L’icona iOS e il disegno nella home condividono `WatchDrawing.swift`:

```sh
xcrun swiftc ios/StopWatch/WatchDrawing.swift tools/render_app_icon.swift -o /tmp/render-stopwatch-icon
/tmp/render-stopwatch-icon
```

## Modifiche e consegna

- Tenere separati codice del firmware, configurazione personale e dati degli utenti.
- Per una modifica al protocollo, aggiornare entrambi i client interessati e i test; non rimuovere la compatibilità senza una migrazione esplicita.
- Aggiornare `include/firmware.h` per il firmware; per l’app aggiornare versione e build in `ios/project.yml` e nel progetto Xcode.
- Documentare cosa è stato verificato automaticamente e cosa richiede ancora hardware.
- Prima di fare push controllare `git diff --cached`, i file aggiunti e [SECURITY.md](../SECURITY.md).
- Non eseguire flash, cancellazioni dell’archivio, aggiornamenti Home Assistant o pubblicazioni dell’app come effetto collaterale di un test locale.

Per il flusso Git usare modifiche piccole e rami `codex/<descrizione>` quando serve un ramo di lavoro. La creazione di una repository privata non autorizza a cambiarne successivamente la visibilità o ad aggiungere collaboratori.
