# StopWatch Companion

Versione corrente: **0.6.0 / build 15**, con [ponte Internet Bluetooth per Casa e Spotify](../README.md#internet-tramite-iphone).

In **Impostazioni → Orologio** si sceglie tra Digitale, Analogico, Halo, Doppio, Orbita e Segmenti e si può lasciare il display acceso nell’orologio (firmware 0.12.0). Dopo 20 secondi senza interazioni la luminosità si riduce; Timer e Cronometro seguono il normale timeout. Il salvataggio aggiorna soltanto i due campi dell’orologio, dopo aver riletto la configurazione attuale. Con firmware precedenti, i campi mancanti usano i valori predefiniti.

La versione 0.5.1 aggiorna soltanto l’icona e l’illustrazione della home: stessa cassa circolare, pulsante giallo in alto a sinistra e blu in alto a destra, proporzioni condivise in `WatchDrawing.swift`. L’icona si genera da quel componente con `tools/render_app_icon.swift`.

Dalla versione 0.4.3 / build 12: prima di inviare Badge o ricevere note l'app rilegge via BLE lo stato Wi-Fi e l'IP attuale, senza decidere dal dato memorizzato alla connessione. Badge invia una lunghezza esplicita e consente 20 secondi di inattività (120 secondi totali). Solo una perdita di connessione, un timeout del client o un host temporaneamente irraggiungibile consentono un secondo tentativo Wi-Fi: nuovo stato, archivio provvisorio e ticket. Errori di certificato, autorizzazione e HTTP non vengono ritentati. Il successivo fallback Bluetooth riparte da una nuova transazione. I vecchi callback di progresso non aggiornano il nuovo tentativo. La barra non arretra nello stesso tentativo; il secondo tentativo, l’attesa della conferma Wi-Fi e la verifica finale del Badge hanno messaggi distinti.

Badge mostra metodo e tempo dell'ultimo invio confermato; se passa a Bluetooth conserva il motivo del mancato trasferimento Wi-Fi e, dal firmware 0.7.1, la causa rilevata sul dispositivo. I log riportano solo codici e conteggi dei byte, senza credenziali o ticket. Le risposte diverse da 200 e i redirect sono rifiutati. Restano obbligatori IP previsto, certificato esatto ricevuto sul BLE autenticato e ticket monouso.

In **Impostazioni → Display e feedback** si regolano separatamente **Volume interazioni** e **Volume note vocali** sullo StopWatch (firmware 0.7.0). Il secondo controllo cambia solo l'ascolto sul dispositivo: microfono, file registrati e volume dell'iPhone restano indipendenti. Con configurazioni precedenti il volume delle note eredita il vecchio valore comune.

La scheda Badge permette foto e GIF, selezione da Foto o File, ritaglio circolare con zoom e trascinamento, anteprima dei fotogrammi e invio Wi-Fi cifrato con alternativa Bluetooth. La preparazione avviene fuori dal thread dell'interfaccia ed è annullata quando cambia il ritaglio. Limiti: 25 MB per la sorgente, GIF fino a 15 secondi, 120 fotogrammi e 3 MB sul dispositivo; 5 o 10 fps. La copia inviata resta nell'archivio privato dell'app per l'anteprima successiva.

Le impostazioni includono ora i 12 pacchetti sonori, Silenzioso, le 6 palette e i controlli Badge (rotazione, animazione e display sempre acceso). Il salvataggio continua a rileggere la configurazione attuale e applicare solo la sezione modificata, conservando credenziali, preferiti e altre impostazioni. Il bundle e l'identità di firma restano gli stessi per mantenere abbinamento e note salvate.

App nativa SwiftUI per iPhone, iOS 17 o successivo. Nessuna dipendenza esterna. Interfaccia: fondo scuro, schede chiare arrotondate, simboli SF Symbols e accento arancione. Le letture sono reali: l'app non inventa dati quando il dispositivo e scollegato.

## Installazione

Aprire `StopWatch.xcodeproj` in Xcode, selezionare il target StopWatch e il proprio Apple Developer Team in **Signing & Capabilities**. Collegare e autorizzare l'iPhone, attivare Developer Mode se richiesto e scegliere **Run**. Xcode gestisce certificato di sviluppo e provisioning. Il progetto non include identita, chiavi private o credenziali Apple.

L'identificatore predefinito e `dev.stopwatch.companion`; cambiarlo se risulta gia registrato da un altro team. La firma di sviluppo e adatta all'installazione personale; la distribuzione tramite TestFlight richiede un passaggio separato su App Store Connect.

Il progetto e riproducibile con XcodeGen (gia presente su questo Mac):

```sh
xcodegen generate --spec ios/project.yml
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcodebuild \
  -project ios/StopWatch.xcodeproj -scheme StopWatch \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath /tmp/stopwatch-derived CODE_SIGNING_ALLOWED=NO build
```

## Primo abbinamento

1. Sullo StopWatch: **iPhone Bluetooth → B**. Il codice a sei cifre rimane visibile per una finestra di 90 secondi.
2. Nell'app: **Abbina StopWatch → Cerca StopWatch**, quindi selezionare il nome corrispondente.
3. La richiesta di abbinamento viene mostrata direttamente da iOS. Inserire il codice del display.
4. Dopo l'abbinamento l'app memorizza l'identificatore CoreBluetooth; le chiavi crittografiche sono gestite dal sistema. Le riconnessioni non richiedono username o password.

Se vengono cancellate le chiavi da uno dei dispositivi, dissociare StopWatch anche nelle impostazioni Bluetooth dell'iPhone e rifare l'abbinamento. L'app non puo cancellare direttamente un bond di iOS tramite API pubbliche.

## Funzioni

- **Dispositivo**: batteria, numero di note, vibrazione, suono e LED.
- **Impostazioni**: display e feedback, Wi-Fi 2,4 GHz, endpoint/token/preferiti Home Assistant e broker MQTT.
- **Note**: ricezione e trascrizione automatica in italiano con app aperta, testo selezionabile e condivisibile, download con progresso e annullamento, ascolto locale, condivisione WAV e cancellazione esplicita dal dispositivo.
- **Casa**: stato dell'integrazione MQTT e preferiti configurati sullo StopWatch.
- **Diagnostica**: disponibilita di microfono, touch, IMU e RTC, pulsanti, rete e memoria. Il rilevamento non sostituisce la prova fisica dei componenti.

Le normali API di configurazione non restituiscono password o token. Il solo ponte Internet può importare il token Home Assistant su richiesta autenticata `relay.credentials`, conservandolo nel portachiavi di questo iPhone; password Wi-Fi e MQTT rimangono sul dispositivo. Dopo il salvataggio, i campi segreti restano vuoti nell'app e un indicatore conferma che esiste un valore sullo StopWatch. I draft dei form rimangono in memoria fino alla chiusura della schermata; non sono salvati in UserDefaults.

Le note scaricate sono scritte atomicamente nel contenitore privato dell'app, separate per dispositivo. Prima di salvare vengono verificati nome, lunghezza, offset di ogni blocco e intestazione WAV. L'originale non viene cancellato dopo il download. Una cancellazione sullo StopWatch conserva la copia gia scaricata. I file da 60 secondi occupano circa 1,92 MB: il trasferimento BLE puo richiedere tempo; il browser Wi-Fi rimane disponibile come alternativa.

## Trascrizione automatica

StopWatch invia l'evento BLE `notes.changed` dopo il salvataggio: l'app aggiorna l'elenco e riceve l'audio senza aspettare il controllo periodico. Prova prima HTTPS sulla rete Wi-Fi locale e usa BLE quando non raggiungibile. Con l'app aperta avvia anche il riconoscimento Apple Speech in italiano, una nota alla volta. Il controllo ogni cinque secondi rimane come recupero per firmware precedenti o eventi persi. Al primo utilizzo iOS chiede il consenso per il riconoscimento vocale. Non viene usato il microfono dell'iPhone.

La richiesta impone il riconoscimento sul dispositivo e verifica che sia supportato: nessun audio viene inviato a servizi cloud e non servono chiavi API. Se il modello italiano non è disponibile o il consenso è negato, l'audio rimane salvato e la scheda mostra l'errore con **Riprova trascrizione**. Il testo finale è salvato atomicamente accanto al WAV, resta leggibile offline e non viene rigenerato a ogni apertura. Eliminare l'originale dallo StopWatch conserva audio e testo sul telefono. Le copie locali vengono mostrate anche dopo il riavvio dell'app, senza connessione Bluetooth.

La ricezione audio è attivata anche dagli eventi CoreBluetooth con schermo bloccato. L'app dichiara `bluetooth-central`, ripristina la connessione e richiede tempo di esecuzione limitato per completare il salvataggio; se iOS fa scadere il tempo, annulla in sicurezza. L'elaborazione non viene cancellata dal semplice cambio di `scenePhase`. La trascrizione delle note ricevute mentre l'app non è attiva viene avviata all'apertura, evitando richieste di consenso in background.

Serve un iPhone acceso, sbloccato almeno una volta dopo il riavvio, Bluetooth attivo e abbinamento già effettuato. La chiusura forzata dell'app, la perdita del collegamento o i limiti imposti da iOS possono rimandare la ricezione all'apertura successiva. Un trasferimento incompleto viene riscaricato, senza eliminare l'originale. L'annullamento esplicito richiede **Riprova trascrizione**. Il limite delle registrazioni resta 60 secondi. L'accuratezza e la disponibilità del modello vanno verificate sull'iPhone reale.

Il firmware 0.3.2 offre il download via HTTPS sulla porta 8443. Il certificato ECDSA P-256 è generato sul dispositivo a ogni avvio, la chiave privata rimane in RAM e il certificato pubblico arriva all'app attraverso il collegamento BLE autenticato. Un ticket casuale a 256 bit autorizza una sola richiesta per la nota scelta e scade dopo 90 secondi. Il server non espone elenco, cancellazione o impostazioni. Il client verifica certificato esatto, indirizzo e porta, rifiuta redirect e usa una sessione effimera senza cache o credenziali condivise. Non serve installare certificati di sistema. iOS chiede il consenso **Rete locale**: consentirlo e tenere iPhone e StopWatch sulla stessa rete per usare il percorso veloce. Se il primo tentativo avviene prima del consenso, può ripiegare su Bluetooth; la nota successiva ritenta Wi-Fi.

Il percorso BLE negozia MTU fino a 517 byte, invia fino a quattro notifiche per ciclo senza pause artificiali e ritenta lo stesso pacchetto se la coda NimBLE è piena. `notes.read` accetta `length` da 1 a 4096 byte (default 768 per app precedenti). L'app mostra metodo e durata reale di ogni trasferimento completato; il valore include la negoziazione del download e un eventuale tentativo Wi-Fi fallito, non include registrazione o trascrizione e non viene conservato dopo il riavvio dell'app.

Collaudo del 24 ottobre 2026 su StopWatch e iPhone 17 Pro: circa 105 KB in 10 secondi via BLE, 216 KB in 2,7 secondi via Wi-Fi (circa otto volte il throughput). L'utente ha confermato la prova a schermo bloccato; il controllo del contenitore dell'app ha rilevato nuovi WAV già salvati durante la prova. Il caso fuori rete con ricezione BLE in background non è stato collaudato. I tempi dipendono da rete e condizioni radio.

Bluetooth central e ripristino di stato sono abilitati. La presenza radio e l'esecuzione in background restano soggette alle regole di iOS. Non sono ancora implementate notifiche push o servizi cloud.

## Protocollo BLE 1

- Servizio: `75f3a100-9e9a-4b9c-9d73-c85f44e6a001`.
- RX scrittura: `75f3a101-9e9a-4b9c-9d73-c85f44e6a001`.
- TX lettura/notifiche: `75f3a102-9e9a-4b9c-9d73-c85f44e6a001`.
- Ogni messaggio e JSON UTF-8 seguito da newline, massimo 12.288 byte prima del delimitatore.
- Richiesta: `{"id":1,"op":"status","data":{}}`.
- Risposta: `{"id":1,"ok":true,"data":{...}}`, oppure `ok:false` ed `error`.
- Una richiesta alla volta; scritture con conferma GATT, frammentazione al limite di CoreBluetooth, timeout di 12 secondi. I comandi non vengono ritentati dopo un esito incerto.
- Operazioni: `status`, `config.get`, `config.set`, `time.set`, `output`, `notes.list`, `notes.transfer`, `notes.read`, `notes.delete`.
- `notes.transfer` richiede `name` e restituisce `available` e, se disponibile, `host`, `port`, `certificate` (DER Base64) e `ticket`. La richiesta HTTPS usa `Authorization: Bearer <ticket>` e non inserisce segreti nell'URL.
- I blocchi audio contengono offset, lunghezza totale e fino a 4096 byte codificati Base64. L'app accetta anche risposte legacy da 768 byte.
- L'evento asincrono `{"id":0,"ok":true,"event":"notes.changed"}` usa lo stesso flusso JSON e viene inviato fra risposte complete.

I callback firmware consegnano i dati a una coda limitata; UI, file e impostazioni vengono elaborati dal loop principale. Non esistono comandi remoti per formattare l'archivio o rimuovere i bond.

## Test

I test del protocollo riusano gli stessi modelli dell'app e possono girare su macOS senza simulatore:

```sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcrun swift test \
  --package-path ios --scratch-path /tmp/stopwatch-protocol-tests
```

Sono disponibili anche nel target Xcode `StopWatchTests`. Il lancio con `--preview` mostra l'interfaccia scollegata senza inizializzare Bluetooth e senza inserire dati dimostrativi. Il simulatore non verifica l'abbinamento radio; primo pairing, reconnect dopo riavvio, trasferimento audio e MQTT vanno collaudati su hardware.

## Protocollo del ponte

- `status` aggiunge i campi opzionali `relay_supported`, `relay_enabled`, `connection_route` (`wifi`, `iphone`, `offline`).
- `relay.credentials` legge soltanto URL HTTPS e token Home Assistant sul collegamento BLE autenticato; non viene usato dalle API web.
- `relay.configure` abilita/disabilita la capacità per la sessione Bluetooth corrente.
- Il firmware invia eventi `relay.request` con `kind` (`favorite`, `music`, `artwork`), `payload` JSON come stringa e `age_ms`.
- L’app restituisce `relay.reply` con lo stesso tipo e una risposta JSON come stringa, correlata con `request`. Le code vengono svuotate alla disconnessione, senza reinvio.
- Il token resta nel portachiavi. `relayStatus` e `relayLastActivity[.<kind>]` in UserDefaults contengono solo esiti, tipo, tempo e indicazione foreground/background, per il collaudo senza debugger.
- I frame JSON non fanno escaping superfluo delle barre: anche un JPEG Base64 al limite resta nei 12 KiB del protocollo esistente.

Prima del comando il client verifica l’ascolto con un echo non retained sul topic di risposta. Home Assistant raggruppa le sottoscrizioni MQTT: il solo ACK WebSocket non garantisce ancora la ricezione. Si possono ripetere solo questi messaggi di readiness, senza effetti sui dispositivi; il comando effettivo viene pubblicato una sola volta dopo l’echo. I marker non vengono confusi con risposte Casa/Spotify.
