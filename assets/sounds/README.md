# Microclick

Il pacchetto quotidiano scelto dall’utente sostituisce i dodici stili UI SFX precedenti. Le sole scelte attive sono **Microclick** (`microclick`) e **Silenzioso** (`off`). I vecchi identificatori sono convertiti in Microclick sia dal firmware sia dal Companion; Silenzioso viene conservato. Gli altri valori della configurazione non cambiano.

Sorgenti: [Kenney Interface Sounds 1.0](https://kenney.nl/assets/interface-sounds), licenza **CC0-1.0**. I sette Ogg originali, l’avviso originale `License.txt` e un manifest locale con SHA-256 e assegnazione agli eventi sono in `kenney-interface-1.0/`. Il manifest descrive questa selezione, non è un catalogo upstream. I vecchi sorgenti rimangono nello storico Git.

| Evento | Sorgente | Durata finale |
|---|---|---:|
| Tocco / selezione | `click_003.ogg` | 7,1 ms |
| Apertura | `open_001.ogg` | 147,8 ms |
| Indietro | `back_001.ogg` | 61,0 ms |
| Conferma breve | `select_001.ogg` | 40,4 ms |
| Errore / avviso | `error_001.ogg` | 161,7 ms |
| Completamento | `confirmation_001.ogg` | 289,9 ms |
| Anteprima volume | `click_001.ogg` | 51,3 ms |

## Generazione

Eseguire `.venv/bin/python tools/generate_ui_sounds.py` con ffmpeg disponibile. Il generatore controlla gli hash originali, decodifica mono PCM16 a 16 kHz, rimuove solo i bordi quasi silenziosi (soglia max di 16/32768 e 0,3% del picco, margine di 10 ms) e applica il guadagno dell’ascolto approvato: target **−22 dBFS RMS**, limite di picco **−8 dBFS**. I transitori più marcati rimangono sotto il target RMS per rispettare il picco: intervallo finale −29,9…−22,0 dBFS. Nessun compressore, riverbero o modifica di intonazione.

Il risultato è compresso con zlib senza perdita. Sono verificati durata ≤ 500 ms, campioni non silenziosi, headroom, decompressione identica e budget di 64 KB. Output:

- `assets/sounds/ui-sounds.bin`: **25.530 byte**, otto flussi nell’ordine dell’enum `Cue`.
- `src/ui_sound_catalog.h`: offset, dimensioni, frequenza e SHA-256 completo, incluso nel firmware normale.
- `src/ui_sound_data.h`: stessi flussi incorporati solo nell’installer temporaneo e nei test host.

Il runtime mantiene 25.530 byte compressi in cache PSRAM e un buffer PCM da massimo 9.276 byte più il decoder. I tocchi non leggono il filesystem e non eseguono normalizzazione/compressione audio. Il volume Interazioni conserva la sua scala, indipendente da Volume note. Non disturbare e Silenzioso continuano a escludere i suoni. I campioni non interrompono la riproduzione delle note e vengono fermati prima della registrazione.

## Filesystem archive and migration

Il file di sistema è `/ui-sounds.bin` su LittleFS. L’archivio precedente occupava **1.440.941 byte**: la sostituzione recupera **1.415.411 byte**, prima dell’overhead del filesystem, per note e Badge. Le partizioni rimangono invariate.

1. Compilare il firmware normale con `.venv/bin/pio run`.
2. Compilare l’installer con `.venv/bin/python tools/build_sound_installer.py /tmp/stopwatch-microclick-installer`.
3. Caricare l’installer seguendo il comando stampato dallo script; attendere `[audio-store] pronto=1 cache=1` nel log. Se la scelta salvata è Silenzioso, il log indica comunque `pronto=1` e il file è verificato.
4. Caricare il firmware normale e verificare nuovamente il log audio.

L’installer scrive `/ui-sounds.tmp`, verifica SHA-256 e rinomina solo dopo il successo. Non formatta LittleFS né rimuove note, Badge, file Chiave o configurazione. Un errore di copia/rinomina conserva il vecchio file; ripetere la stessa installazione non riscrive un archivio già valido. Occorre spazio per il file temporaneo e la riserva di registrazione; se non basta, l’installer fallisce senza rimuovere dati. **Non usare `uploadfs` o erase per questa migrazione.**

All’avvio il firmware normale verifica il file completo una volta e poi usa la cache; un archivio mancante o corrotto attiva brevi toni di ripiego, sempre subordinati a Silenzioso, Non disturbare e al volume Interazioni. La pulizia ordinaria dell’archivio conserva il file audio su un filesystem sano. Una formattazione esplicita di un volume non montabile richiede invece di reinstallarlo.

I test host compilano il runtime reale nelle varianti normale e installer: PCM identico, cache senza letture, integrità, copia parziale, rinomina fallita, spazio insufficiente e conservazione dei file utente/Chiave, anche sostituendo il file dei vecchi pacchetti. Ascolto sullo speaker e comportamento del motorino restano da verificare sullo StopWatch; non è stata provocata un’interruzione elettrica reale.
