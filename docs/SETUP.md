# Installazione da un clone pulito

## Prerequisiti

- Git e Python 3 con `venv` e `pip`.
- Accesso a Internet per scaricare PlatformIO, toolchain e librerie.
- Per i test nativi: compilatore C++17; su macOS gli strumenti a riga di comando di Xcode.
- Per il Companion: macOS, Xcode con SDK iOS e un account Apple Developer configurato in Xcode.
- Per installare firmware o app: StopWatch C152 e iPhone autorizzati sul proprio computer.

Non servono account Home Assistant o Spotify per compilare. Servono per configurare e collaudare le rispettive integrazioni.

## Clone e firmware

```sh
git clone https://github.com/skfallin/StopWatch.git
cd StopWatch
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/pio run
```

Su Windows usare gli eseguibili equivalenti in `.venv\Scripts\`. I comandi delle guide sono scritti per una shell macOS/Linux; i test audio basati su CommonCrypto richiedono macOS.

`platformio.ini` fissa PIOArduino 55.03.37, Arduino ESP32 3.3.7, M5Unified 0.2.27, M5GFX 0.2.34 e ArduinoJson 7.4.2. Le dipendenze vengono scaricate: `.platformio/`, `.pio/` e `.venv/` non fanno parte del repository. `NETWORK_EVENTS_MUTEX` resta necessario per la gestione degli eventi di rete sui due core.

I caratteri, il catalogo audio e l’icona dell’app sono già generati. Non occorre rigenerarli per una compilazione normale. Il firmware applicazione viene prodotto in `.pio/build/stopwatch/firmware.bin`; da solo non contiene bootloader e tabella delle partizioni.

## Caricamento sullo StopWatch

1. Collegare con un cavo USB dati e leggere la porta con `.venv/bin/pio device list`.
2. Se necessario, tenere premuto alimentazione/reset per circa due secondi, fino al LED verde, poi rilasciare.
3. Caricare usando la porta effettiva:

```sh
.venv/bin/pio run --target upload --upload-port /dev/cu.PORTA_DEL_DISPOSITIVO
```

Se resta in modalità download, premere una volta brevemente alimentazione/reset. Non usare `erase` o `uploadfs` per un aggiornamento ordinario: note, Badge, suoni e configurazione devono essere conservati. La tabella in `partitions.csv` deve rimanere invariata per preservare i dati tra gli aggiornamenti di questo progetto. Il primo passaggio da un firmware con un layout differente richiede una valutazione separata dei dati da conservare.

## Archivio e suoni al primo avvio

Su un dispositivo non ancora inizializzato, aprire **Diagnostica → Archivio / RAM** e seguire la conferma fisica. L’inizializzazione è distruttiva per eventuali note e Badge: non usarla per risolvere un normale aggiornamento firmware.

Per installare i 12 pacchetti sonori su un archivio che non li contiene, seguire [la procedura dell’installer](../assets/sounds/README.md#filesystem-archive-and-migration). L’installer temporaneo verifica e copia l’archivio audio senza formattare; dopo va caricato il firmware normale. Il file sorgente dei suoni è `assets/sounds/ui-sounds.bin`.

## Companion, rete e integrazioni

1. Compilare e installare il [Companion](../ios/README.md), selezionando in Xcode il proprio Team. Non occorre cambiare l’identificatore del bundle per un normale aggiornamento dell’app esistente.
2. Sullo StopWatch aprire **iPhone Bluetooth**, generare il codice e completare l’abbinamento dall’app.
3. Configurare Wi-Fi a 2,4 GHz e, se desiderato, MQTT dal Companion.
4. Importare i [blueprint Home Assistant](../home_assistant/README.md) per preferiti e Spotify. L’integrazione Spotify di Home Assistant deve essere già autorizzata con l’account corretto.
5. Per i comandi fuori casa, configurare **Internet tramite iPhone** con URL HTTPS remoto e token Home Assistant sul telefono. L’accesso remoto a Home Assistant deve essere già funzionante.

Username, password, token e indirizzi personali non sono inclusi nel repository. I valori nei test e nei blueprint sono esempi: vanno sostituiti con la configurazione della propria installazione.

## Prima verifica

Provare touch e pulsanti, audio e vibrazione, una nota registrata e riascoltata, il trasferimento Badge e un comando Casa con conferma. Per il ponte Internet spegnere il Wi-Fi solo quando si è certi del percorso di prova e verificare separatamente l’esecuzione con iPhone bloccato. Le istruzioni di sviluppo e i test automatici sono in [DEVELOPMENT.md](DEVELOPMENT.md).
