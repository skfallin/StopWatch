# Installazione e aggiornamenti

[English](SETUP.md) · [Home](../README.it.md)

Servono StopWatch **C152**, cavo USB dati, Git e Python 3.11+ con venv/pip. Per il Companion: macOS, Xcode, un Team Apple e iPhone con iOS 17+. Home Assistant e Spotify non servono per compilare o usare le funzioni autonome.

```sh
git clone https://github.com/skfallin/StopWatch.git
cd StopWatch
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python -m platformio run -e stopwatch
.venv/bin/python -m platformio device list
.venv/bin/python -m platformio run -e stopwatch --target upload --upload-port PORTA_REALE
```

Su Windows usa `.venv\Scripts\python.exe`. La build `stopwatch` non attiva la chiave FIDO; `stopwatch-lab` è riservata a prove esplicite. Se necessario, tieni alimentazione/reset per circa due secondi fino al LED verde, rilascia e ripeti. Un breve reset esce dalla modalità download dopo il caricamento.

**Prima installazione:** esporta i dati del firmware precedente. Un layout di partizioni differente può richiedere operazioni distruttive. Se l’archivio è da inizializzare, apri Sistema → Diagnostica → Archivio / RAM e conferma fisicamente: note e Badge possono essere cancellati.

**Aggiornamento:** conserva `partitions.csv`, non usare erase, uploadfs o inizializzazione dell’archivio. PlatformIO carica i componenti corretti; il solo `firmware.bin` non include bootloader e partizioni.

Per installare Microclick senza cancellare note:

```sh
.venv/bin/python tools/build_sound_installer.py /tmp/stopwatch-sound-installer
```

Segui il comando di upload stampato, attendi `[audio-store] pronto=1 cache=1` e ricarica il firmware normale. L’installer sostituisce soltanto l’archivio audio dopo verifica; se manca spazio non elimina dati. In assenza di Microclick restano i toni di ripiego.

Abbina il [Companion](../ios/README.it.md) da Sistema → iPhone e scegli lingua e fuso in Impostazioni → Lingua e fuso orario. I nuovi dispositivi usano inglese/UTC; le configurazioni precedenti italiano/Roma. I fusi elencati applicano l’ora legale, ma non costituiscono l’intero database IANA.

Configura Wi-Fi 2,4 GHz dal telefono o da Sistema → Browser Wi-Fi: il QR collega alla rete temporanea; il portale usa utente `stopwatch` e password casuale sul display. Il portale permette anche di scegliere lingua e fuso.

La lingua dell’app segue iOS. La trascrizione ha una scelta separata tra inglese US/UK e italiano: richiede modello locale e permesso Apple Speech; in caso di errore l’audio resta salvato. Per Ponte USB scegli il layout realmente attivo sul computer.

[Home Assistant e Spotify](../home_assistant/README.it.md) sono facoltativi. Negli aggiornamenti dell’app conserva bundle ID e Team per mantenere i dati. Prima della beta prova su hardware pulsanti, note, abbinamento, lingua dopo riavvio e integrazioni: [checklist](STATUS.it.md).
