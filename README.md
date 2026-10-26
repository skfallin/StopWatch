# StopWatch — firmware personale

Firmware personale per **M5Stack StopWatch C152**, con interfaccia circolare Orbita + Focus, Companion iPhone e integrazioni Home Assistant. Arduino ESP32 usa ESP-IDF/FreeRTOS; M5Unified e M5GFX gestiscono le periferiche della scheda.

Versioni nel repository: **firmware 0.12.0**, **Companion 0.6.0 / build 15**, protocollo BLE **1**. Il progetto è mantenuto in una repository privata.

## Documentazione

| Guida | Contenuto |
|---|---|
| [Installazione e compilazione](docs/SETUP.md) | Clone, prerequisiti, firmware, primo avvio e aggiornamenti |
| [Architettura](docs/ARCHITECTURE.md) | Moduli, trasporti, archivio e gestione energetica |
| [Sviluppo e test](docs/DEVELOPMENT.md) | Verifiche ripetibili, generazione asset e convenzioni |
| [Stato e limiti](docs/STATUS.md) | Funzioni disponibili, verifiche eseguite e attività ancora aperte |
| [Companion iPhone](ios/README.md) | Firma, installazione, abbinamento, Badge, note e ponte Internet |
| [Home Assistant e Spotify](home_assistant/README.md) | MQTT, blueprint, preferiti, playlist e dispositivi |
| [Sicurezza dei dati](SECURITY.md) | Credenziali, dati personali e materiale escluso da Git |
| [Risorse di terze parti](docs/THIRD_PARTY.md) | Provenienza, versioni e licenze degli asset |

Il codice, i test e gli asset necessari sono inclusi. Cache delle dipendenze, compilati, certificati di firma, credenziali e dati esportati dal dispositivo restano locali.

## Cosa contiene

- Interfaccia **Orbita + Focus (0.12.0)**: home con cerchio principale centrato, Casa, Note, Badge, Orologio, Spotify e Sistema; azioni grandi e stato a pieno schermo. Sistema raccoglie impostazioni, diagnostica, iPhone Bluetooth e browser Wi-Fi. Gli avvisi appaiono in una scheda interna al display circolare, su due righe al massimo; un tocco sulla scheda la chiude senza attivare i comandi sottostanti. Gli aggiornamenti automatici riusciti di Home Assistant aggiornano lo stato senza avviso.
- Avvio senza notifica di sistema pronto: dopo che display e caratteri sono disponibili, **Caricamento… / v0.12.0** appare soltanto se l'inizializzazione è ancora in corso dopo 400 ms. Scompare appena termina, senza durata minima, attese della rete o percentuali simulate. Se i servizi sono pronti prima, si passa direttamente alla schermata iniziale. Gli avvisi di errore restano visibili. Versione del caricamento e discovery MQTT condividono `include/firmware.h`.
- **Badge**: foto e GIF preparate dall'app iPhone, ritaglio e anteprima circolare con i colori RGB565 effettivamente inviati. GIF fino a 15 secondi, 120 fotogrammi distinti, 3 MB; scelta tra 5 e 10 fps. Trasferimento HTTPS con certificato ricevuto sul BLE autenticato, oppure Bluetooth. Il file precedente resta attivo fino alla verifica SHA-256, controllo dei limiti e decodifica del primo fotogramma; la sostituzione avviene con rename atomico. Non occorre riflashare per cambiare immagine. Viene conservato spazio per una nota da 60 secondi; nessuna nota viene eliminata per fare posto al Badge.
- Impostazioni persistenti: luminosità, volume interazioni e volume note vocali separati, intensità della vibrazione, timeout del display, pacchetto suoni e colori. I due volumi si regolano sia sullo StopWatch sia nel companion e tramite MQTT; il volume delle note riguarda solo l’ascolto, senza alterare microfono o WAV. Alla migrazione eredita il vecchio volume comune; i client precedenti che omettono `note_volume` conservano il valore indipendente già salvato.
- Il volume logico 0–200, usato anche dal companion e da MQTT, viene convertito nell'intera scala 0–255 dell'altoparlante: il 100% raggiunge il massimo del driver. I 96 effetti vengono normalizzati e compressi offline con rapporto 2:1 e limitatore a 94% del fondo scala. Il livello medio digitale passa a -12,5…-9,0 dBFS, con differenze massime inferiori a 4 dB; nessun costo DSP durante la riproduzione. Il generatore verifica durata, picchi, RMS, decompressione e budget flash. La distorsione acustica a volume massimo richiede una prova sul dispositivo.
- **Colori**: Lime, Giallo/Blu dei pulsanti, Arancio, Viola, Ciano e Rosa. In Sistema → Impostazioni → Colori, trascina per l'anteprima e rilascia per salvare, oppure premi B per cambiare. Giallo/Blu usa il giallo per la navigazione e il blu per le azioni principali; i pannelli Focus seguono la palette. La riga di stato della home viene centrata misurando data e percentuale della batteria, così resta centrata al variare dei testi.
- **Non disturbare**: scorri verso il basso dalla parte alta centrale di qualsiasi schermata e tocca la scheda con la luna. Disattiva tutti i suoni dello StopWatch, inclusi timer, riascolto e test diagnostici, tutte le vibrazioni e il LED di feedback; ferma subito anche audio e vibrazione già in corso. I volumi restano invariati e tornano disponibili disattivando la modalità. La scelta resta salvata al riavvio; durante una registrazione il silenzio è immediato e il salvataggio attende che la nota sia terminata. I client precedenti conservano la modalità quando modificano altre impostazioni.
- **Suoni UI SFX**: tutti i 12 stili, con 8 effetti per stile (96 suoni). In Sistema → Impostazioni → Pacchetto suoni, trascina lo slider o premi B per cambiare e ascoltare un esempio; scegli Silenzioso per disattivarli. Il volume è **Interazioni**; **Volume note** regola separatamente le registrazioni. I suoni non interrompono l'ascolto delle note e vengono fermati prima di registrare; nessun effetto viene riprodotto durante la cattura. Dal firmware 0.8.1 i campioni sono nel file di sistema `/ui-sounds.bin` nell’archivio LittleFS; il firmware contiene solo indice e SHA-256. I 12 pacchetti restano disponibili, con il solo pacchetto selezionato in cache PSRAM. I client companion precedenti conservano la scelta anche quando salvano altre impostazioni.
- Home Assistant: fino a 8 preferiti configurabili, lettura dello stato e azioni REST. Le richieste girano in un task separato dall'interfaccia.
- Note vocali offline: audio mono PCM a 16 kHz / 16 bit, fino a 60 secondi per nota, riascolto e download WAV. Nessun audio viene inviato a servizi esterni.
- [Companion iOS nativo](ios/README.md): SwiftUI, abbinamento BLE con codice a sei cifre, impostazioni, Wi-Fi, preferiti, diagnostica e download/ascolto/condivisione delle note.
- Integrazione Home Assistant tramite discovery MQTT: un dispositivo con sensori, comandi, impostazioni ed eventi dei pulsanti fisici. Usa il client ESP-MQTT gia incluso nella piattaforma, senza dipendenze aggiuntive.
- [Preferiti da Home Assistant](home_assistant/README.md): otto selettori visuali, nomi automatici e controllo via MQTT dal dispositivo, senza token REST.
- **Spotify**: segue il lettore attivo dell'account Premium tramite l'integrazione ufficiale Home Assistant e il broker già configurato. Play/pausa con B, brano successivo con A, precedente/successivo anche al touch; tocca l'intestazione Spotify per scegliere **Playlist** o **Dispositivi**. Le playlist si leggono dalla libreria dell'account, a pagine di otto, senza URL da inserire; A scorre e B avvia sul lettore attivo. Copertina intera, titolo, artista, avanzamento e sfondo scuro ricavato dai colori dell'immagine. Si configura con [il blueprint dedicato](home_assistant/blueprints/stopwatch_spotify.yaml), senza password Spotify nel firmware né dipendenza dal Companion aperto.
- Browser Wi-Fi locale come alternativa: QR per collegarsi al Wi-Fi temporaneo del dispositivo e aprire la pagina di configurazione.

Le note vengono ricevute automaticamente dal companion tramite eventi Bluetooth: l'audio viaggia via HTTPS Wi-Fi quando raggiungibile, altrimenti via BLE. La ricezione può continuare a schermo bloccato entro i limiti di esecuzione di iOS; la trascrizione italiana parte con l'app attiva. Apple Speech lavora sull'iPhone e conserva testo e audio offline. Non servono chiavi API; è necessario che il riconoscimento italiano sul dispositivo sia disponibile.

**Non sono ancora implementati:** tastiera remota generica per altre app del firmware, notifiche iOS e integrazione Grok.

## Comandi

| Gesto | Risultato |
|---|---|
| A breve | Voce / preferito / test successivo |
| B breve | Apri, modifica o esegui l'azione indicata sul display |
| A tenuto, 650 ms | Torna alla schermata precedente; nelle conferme annulla |
| B tenuto, 650 ms | Prepara e avvia una nota vocale da qualsiasi schermata |
| Rilascio B | Salva la nota |
| A+B tenuti, 800 ms | Menu principale; un'eventuale nota in corso viene prima salvata |
| Tocco su un'icona | Apre direttamente la funzione indicata |
| Scorrimento in basso dalla zona alta centrale | Apre Controlli rapidi, anche durante una registrazione |
| Controlli rapidi → luna / B breve | Attiva o disattiva Non disturbare |
| Controlli rapidi → A / × / scorrimento in alto | Chiude il pannello senza azionare la schermata sottostante |
| Scorrimento orizzontale | Cambia funzione, preferito o impostazione |
| Freccia in alto a sinistra | Torna alla schermata precedente |
| Note → microfono | Avvia una registrazione senza tenere premuto B |
| Quadrato durante la registrazione | Ferma e salva; funziona anche B breve per una nota avviata dal touch |
| Trascinamento dello slider | Anteprima del valore; salva una sola volta al rilascio |

Un tocco breve a display spento lo riaccende senza eseguire un comando. Tenere B premuto consente invece di arrivare direttamente alla registrazione. **Parlare quando compare REC**: l'ES8311 richiede circa un secondo di stabilizzazione quando il microfono viene riacceso. Un breve impulso precede la cattura; un impulso più lungo conferma il salvataggio. La vibrazione non viene attivata durante l'audio registrato. Con Non disturbare gli impulsi di inizio e salvataggio sono disattivati; la cattura e la sincronizzazione delle note continuano.

## Risparmio energetico (0.12.0)

A display spento, nell’orologio visibile o con un Badge fermo, dopo 2 secondi senza attività la CPU può scendere da 240 a 80 MHz. I ridisegni periodici usano brevemente 240 MHz senza prolungare il periodo di attività: tra i secondi dell’orologio torna subito a 80 MHz. GIF in riproduzione, movimento con rotazione automatica, tocchi, pulsanti, registrazioni, ascolto, pairing, comandi Casa/Spotify, ponte iPhone, browser e trasferimenti HTTPS/BLE richiedono la frequenza piena. Se il cambio di frequenza fallisce, il firmware resta a 240 MHz. Wi-Fi e Bluetooth restano connessi: non viene usata la sospensione della CPU e i parametri BLE restano invariati.

Nel Companion → Impostazioni → Orologio si scelgono i sei quadranti già disponibili sul dispositivo e si può attivare il display sempre acceso. Quando Orologio è aperto, il quadrante resta visibile; dopo 20 secondi senza input la luminosità scende a un massimo di 48/255. Un’interazione ripristina la luminosità e la velocità piena. Il normale timeout continua a valere in Timer e Cronometro; il Badge conserva la propria impostazione indipendente. Client precedenti che omettono `clock_keep_awake` conservano il valore già salvato.

La home viene ridisegnata al cambio di minuto, batteria, connessione o interazione; menu e impostazioni statiche si aggiornano quando cambiano. Diagnostica, animazioni, orologio, timer, cronometro e intervalli dei comandi Casa/Spotify conservano gli aggiornamenti precedenti. Le letture della batteria passano a 5 secondi con display acceso e 30 secondi a display spento; Diagnostica → Batteria resta a 500 ms. Le letture IMU periodiche dell’interfaccia vengono limitate alla diagnostica; Badge conserva il proprio campionamento adattivo. Questo non spegne fisicamente l’IMU.

Le copertine Spotify restano in cache anche uscendo dall’app o spegnendo il display e vengono sostituite solo quando cambia l’immagine. La telemetria MQTT periodica passa da 15 a 60 secondi con display spento; impostazioni ed eventi continuano a essere inviati subito e il keepalive resta a 30 secondi. Il Companion e il protocollo del ponte in background restano invariati.

Il risparmio effettivo e l’autonomia richiedono una misura a batteria; frequenze e minore attività non equivalgono a una percentuale di risparmio già misurata.

Verifica del firmware 0.11.2 sul dispositivo del 25 ottobre 2026: CPU a 80 MHz a display spento, ritorno a 240 MHz alle richieste e successivo rientro a 80 MHz. Con una build separata identica salvo il Wi-Fi temporaneamente spento, Casa e Spotify sono stati provati con iPhone bloccato: il Companion ha registrato esiti riusciti in background (ultimo comando luce circa 0,48 s, Spotify circa 1,80 s). Sono singoli riscontri funzionali, non una misura statistica della latenza. La versione normale conserva il Wi-Fi automatico.

## Pacchetti audio nell’archivio

Dal firmware **0.8.1** i 96 campioni normalizzati occupano **1.440.941 byte** in `/ui-sounds.bin`, condividendo l’archivio con note e Badge. La partizione del firmware rimane da 4 MiB e l’archivio da 7,94 MiB: nessuna modifica alle partizioni. La compilazione normale scende dal 90,7% a circa **56,4%**, recuperando circa 1,37 MiB per il codice di nuove applicazioni.

All’avvio vengono verificati dimensione e SHA-256 dell’archivio rispetto al catalogo generato. Viene poi tenuto in PSRAM soltanto il pacchetto selezionato (circa 65–195 KiB), oltre al buffer del campione corrente. I tocchi successivi dello stesso pacchetto non leggono il filesystem. Audio, livelli, compressione e campioni PCM sono identici alla versione incorporata; le due regolazioni del volume restano separate.

La prima migrazione usa un installatore temporaneo con `SFX_SEED_ARCHIVE`. Scrive solo `/ui-sounds.tmp`, verifica il file completo e lo rinomina su `/ui-sounds.bin`; conserva spazio sufficiente per una nota da 60 s. Una copia interrotta viene ripetuta e una copia già valida non viene riscritta. Prima di caricare il firmware normale occorre verificare nel log **`[audio-store] pronto=1 cache=1`**. Non usare `uploadfs` e non formattare per installare o aggiornare i suoni.

```sh
python3 tools/build_sound_installer.py /tmp/stopwatch-sound-installer
.venv/bin/pio run -d . -c /tmp/stopwatch-sound-installer/platformio.ini -t upload --upload-port /dev/cu.usbmodem2101
# Avviare il dispositivo e attendere la copia verificata, quindi:
.venv/bin/pio run -t upload --upload-port /dev/cu.usbmodem2101
```

Il builder riusa l’ambiente `stopwatch` e le librerie hardware locali già corrette, con una cartella di compilazione distinta; non modifica `platformio.ini`. Lo stesso installatore serve per ripristinare pacchetti mancanti o aggiornare un catalogo audio cambiato. Nei normali aggiornamenti con catalogo invariato il file resta al suo posto.

**Inizializza archivio**, su un volume sano, elimina soltanto note riconosciute, note parziali e Badge, conservando suoni e altri file di sistema. Un volume non montabile richiede ancora la conferma fisica prima della formattazione; dopo una formattazione occorre reinstallare i pacchetti. Se i pacchetti mancano o sono corrotti, il firmware lo segnala e conserva brevi toni di emergenza; Silenzioso e i volumi configurati restano rispettati. Il filesystem non viene mai formattato automaticamente.

I test host compilano `src/ui_sounds.cpp` in entrambe le varianti: confrontano i 96 PCM con gli originali, verificano cache senza letture, corruzione/troncamento, copia interrotta, rinomina fallita, spazio insufficiente e conservazione dei file utente. I test simulano questi errori senza cancellare dati sul dispositivo; non è stata effettuata una prova con interruzione elettrica reale.

## Orologio

Dal firmware **0.8.0**, la nuova app della home, prima di Sistema raccoglie **Ora, Timer e Cronometro**. Font, colori e feedback seguono Orbita; i quadranti vengono disegnati con primitive vettoriali, senza nuove immagini o download.

| Funzione | A breve | B breve | Touch |
|---|---|---|---|
| Menu Orologio | Voce successiva | Apri | Tocca una delle tre voci |
| Ora | Quadrante successivo, salvato | Torna al menu Orologio | Un tocco mostra per 4 s freccia cambio quadrante e le tre sezioni |
| Timer pronto | Durata rapida successiva: 1, 3, 5, 10, 15, 25 min | Avvia | +/− separati per ore, minuti e secondi; Avvia |
| Timer in corso | Menu, continuando a contare | Pausa | Menu o Pausa; azzeramento disponibile solo in pausa |
| Timer in pausa | Azzera | Riprendi | Azzera o Riprendi |
| Timer scaduto | Ripeti | OK | Ripeti o OK |
| Cronometro in corso | Giro | Pausa | Giro o Pausa |
| Cronometro in pausa | Azzera | Riprendi | Azzera o Riprendi |

A tenuto torna al menu Orologio; dal menu torna alla home. B tenuto registra sempre una nota vocale. Swipe orizzontali e le tre schede in basso passano tra Ora, Timer e Cronometro. Un tocco su display spento lo riaccende senza azionare i comandi.

Sei quadranti originali: **Digitale** (ore/minuti e secondi separati), **Analogico**, **Halo** (lancette e alone), **Doppio** (cifre su due righe), **Orbita** (anelli di ore, minuti e secondi), **Segmenti** (digitale a sette segmenti). Halo riprende l’atmosfera delle immagini fornite dall’utente; Doppio e Orbita usano come riferimenti visivi [Numerals Duo e i quadranti Apple](https://support.apple.com/en-lamr/guide/watch/apde9218b440/watchos) e [Concentric di Google](https://store.google.com/au/magazine/make-a-statement-with-google-pixel-watch-bands?hl=en-GB), con geometrie proprie per 466×466. Non vengono incorporate immagini o font proprietari di questi prodotti.

L’ora usa il sistema già sincronizzato da RTC, iPhone o NTP, nel fuso di Roma con cambio automatico dell’ora legale. Senza un’ora valida compare una richiesta di sincronizzazione. I quadranti aggiornano al secondo e fermano il microfono inattivo; il normale timeout del display resta valido.

Timer da **1 s a 23:59:59**, con ultima durata salvata quando viene avviato. La scadenza riaccende il display e mostra Tempo scaduto, con quattro impulsi di suono e vibrazione, ai livelli già configurati. Con Non disturbare compare soltanto l’avviso visivo, senza suoni o vibrazioni. Se è in corso una registrazione o il riascolto di una nota, l’avviso attende la fine senza interferire con l’audio. Cronometro al centesimo, fino a **99:59:59.99**, con ultimi tre giri e azzeramento solo da fermo; al limite si ferma e permette Azzera o Menu.

Timer e cronometro usano tempo monotono e continuano anche fuori dall’app e a display spento: correzioni NTP, fuso e ora legale non cambiano i conteggi. Al riavvio i conteggi in corso si azzerano; quadrante scelto e ultima durata restano salvati. I campi opzionali `clock_face` e `timer_seconds` conservano questi valori anche quando un Companion precedente salva altre impostazioni.

`tests/clock_test.cpp` verifica pausa/ripresa, limiti, scadenza, avviso differito e limitato, giri, rollover e bersagli touch. `tools/render_clock_preview.py`, con clang++ e Pillow, esegue lo stesso renderer C++ e le primitive delle icone su un registratore host: produce tavole di anteprima, usa Inter con gli stessi avanzamenti e segnala testi troncati. L’antialiasing e la resa finale AMOLED richiedono il dispositivo.

## Badge

Nell'app iPhone aprire la quarta scheda, scegliere una foto/GIF da Foto o File, regolare riempimento, zoom e posizione e premere **Invia allo StopWatch**. L'anteprima animata usa i fotogrammi già preparati per il dispositivo. Suoni e colori sono modificabili anche nelle impostazioni dell'app aggiornata.

Sullo StopWatch aprire Badge dalla home. Un tocco mostra per 4 secondi i comandi: rotazione automatica e pausa/ripresa GIF; A torna alla home. Con rotazione automatica attiva, B breve mantiene i comandi precedenti. Con rotazione automatica disattivata, dal firmware 0.7.3 ogni B breve (pulsante blu) ruota foto o GIF di 45°: 0°, 45°, 90° … 315°, poi 0°. I comandi sul touch restano disponibili e B tenuto continua a registrare note e interrompe un eventuale upload non ancora confermato, conservando il Badge precedente. L’angolo manuale rimane in RAM quando si esce dal Badge, si registra una nota o si passa tra modalità automatica e fissa; si azzera al riavvio e non modifica il file dell’immagine.

La rotazione continua usa la gravità come riferimento e il giroscopio tra le misure. La lettura passa da 40 Hz in movimento a 10 Hz da fermo; il ridisegno è limitato a 25 fps e richiede un nuovo fotogramma o uno spostamento di almeno 1,5 gradi. Le GIF vengono tenute compresse in PSRAM, con lettura da flash come alternativa se la cache non entra; memoria e immagini vengono rilasciate uscendo o iniziando una registrazione. Il microfono non viene mantenuto caldo durante il Badge.

La versione 0.6.1 allinea il riferimento al montaggio dell'IMU sullo StopWatch: dispositivo verticale con pulsanti in alto, gravità lungo -X. Corregge il quarto di giro osservato con la rotazione automatica; il file inviato dall'iPhone e il disegno a rotazione fissa restano identici.

Per impostazione predefinita resta valido il normale timeout del display. **Impostazioni → Badge** nell'app permette di tenerlo acceso: dopo 20 secondi senza input o movimento la luminosità scende a un massimo di 48/255. La durata effettiva della batteria e il verso della rotazione richiedono una prova fisica; non è stata stimata un'autonomia in ore.

Dal firmware 0.7.1 la ricezione HTTPS conserva l'offset durante pause temporanee del socket o TLS, con limiti di 15 secondi senza dati e 120 secondi totali. Una chiusura o un errore di scrittura non possono risultare un invio completo. La risposta POST chiude la connessione per liberare l'unico slot HTTPS; la verifica SHA-256 e il commit via BLE rimangono obbligatori. Durante un upload Badge il microfono inattivo viene fermato anche fuori dalla schermata Badge, liberando memoria interna per TLS; B tenuto continua a interrompere l'upload prima di registrare. `tests/http_transfer_test.cpp` esercita il ciclo di ricezione reale con pause, letture parziali, disconnessioni, errori di archivio e rollover.

La versione 0.7.2 corregge la scadenza dell’upload: il thread HTTPS può aggiornare l’ultima scrittura dopo il campionamento dell’orologio del loop UI. Il confronto tratta questo piccolo scarto come un’attività recente, evitando di cancellare il provvisorio durante un trasferimento attivo. `tests/badge_test.cpp` copre lo scarto tra i thread, la vera scadenza dopo 120 secondi e il rollover di `millis()`. I log distinguono scadenza, blocchi rifiutati, scrittura fallita e ricezione completa senza stampare ticket o contenuto del Badge.

Collaudo hardware della 0.7.2: il log USB conferma due ricezioni HTTPS complete, da 1.153.312 e 225.239 byte, entrambe con risposta inviata correttamente (`ESP_OK`). L’utente ha confermato «Wi-Fi, invio riuscito» nel Companion. Il tempo di questi invii non è stato misurato.

Il formato interno SWB1 contiene header little-endian di 16 byte (magic, larghezza, altezza, numero fotogrammi, riservato, byte totali), tabella di righe da 12 byte (offset, byte compressi, durata in ms), fotogrammi RGB565 little-endian 466×466 compressi zlib e SHA-256 finale dei byte precedenti. Il decoder ROM lavora in PSRAM, fuori dallo stack del loop. I parziali vengono scartati al riavvio. L'inizializzazione dell'archivio in Diagnostica cancella sia note sia Badge e richiede conferma fisica con B.

Il pulsante di alimentazione mantiene le funzioni gestite dall'hardware. Lo spegnimento del display è un timeout di luminosità, **non deep sleep**; l'autonomia deve ancora essere misurata sul dispositivo.

La home usa nero AMOLED, icone vettoriali e Inter antialias incorporato nel firmware. I caratteri non dipendono dall'archivio LittleFS. I sorgenti e la licenza SIL Open Font License sono in `assets/fonts/`; `tools/generate_ui_fonts.py` rigenera i sottoinsiemi VLW con Pillow/FreeType. Il firmware comprende già i caratteri generati, senza bisogno di installare Pillow per compilare.

Il verde a pieno schermo indica uno stato `on` confermato e recente oppure la cattura audio attiva. La preparazione del microfono resta distinguibile da REC. Il grafico della registrazione usa i picchi reali del microfono. Non vengono mostrate conferme di consegna all'iPhone che il dispositivo non ha ricevuto.

Tutti i preferiti che eseguono un'azione, comprese luci e ventilatori, aprono la stessa conferma con **Annulla / Esegui** prima di inviare il comando. I primi 350 ms ignorano un secondo tocco/pressione accidentale. I sensori in sola lettura possono essere aggiornati senza conferma. Cancellazione dell'archivio e rimozione degli abbinamenti richiedono ancora il pulsante fisico B: il touch può annullarle, non confermarle. Un gesto iniziato prima del risveglio o di un cambio schermata non viene riutilizzato come comando.

## Compilare

Prerequisiti: Python 3 e Git. Da questa cartella:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install platformio==6.2.0
.venv/bin/pio run
.venv/bin/python tests/quiet_test.py
```

Per installare anche gli strumenti dei test Python e delle anteprime usare `.venv/bin/python -m pip install -r requirements-dev.txt`. La prima compilazione deve precedere i test che utilizzano ArduinoJson dalla cartella `.pio/libdeps/`.

Il progetto fissa PIOArduino 55.03.37 / Arduino ESP32 3.3.7, M5Unified 0.2.27, M5GFX 0.2.34 e ArduinoJson 7.4.2. Il profilo `esp32s3box` riprende il riferimento M5Stack per flash 16 MB e PSRAM OPI; i driver rilevano lo StopWatch e il firmware verifica il modello all'avvio. Toolchain e cache rimangono nella cartella ignorata `.platformio/`.

`NETWORK_EVENTS_MUTEX` abilita la sincronizzazione gia prevista dal core Arduino tra registrazione e distribuzione degli eventi Wi-Fi sui due core. Senza questa protezione il primo avvio sullo StopWatch ha provocato un crash in `NetworkEvents::_checkForEvent()`. Il log USB include le fasi di avvio e il controllo d'integrita dell'heap, senza password o token.

Il risultato è `.pio/build/stopwatch/firmware.bin`. Il file applicazione da solo non contiene bootloader e tabella delle partizioni.

## Primo caricamento e avvio

Il caricamento sostituisce il firmware presente e installa una **nuova tabella delle partizioni**: prima conservare una copia del firmware e dei dati originali che interessa mantenere. Non eseguire `erase` o `uploadfs` per un normale aggiornamento.

Per caricare, collegare lo StopWatch con un cavo USB dati e usare la porta effettiva riportata da `pio device list`. Se necessario, tenere premuto il pulsante di alimentazione/reset per circa due secondi, finché si accende il LED verde, per entrare nella modalità download.

```sh
.venv/bin/pio device list
.venv/bin/pio run --target upload --upload-port /dev/cu.PORTA_DEL_DISPOSITIVO
.venv/bin/pio device monitor --port /dev/cu.PORTA_DEL_DISPOSITIVO
```

Questi comandi di caricamento **non vengono eseguiti dalla sola compilazione**.

1. Installare l'app da [ios/StopWatch.xcodeproj](ios/StopWatch.xcodeproj), seguendo [le istruzioni iOS](ios/README.md).
2. Aprire **iPhone Bluetooth** sullo StopWatch e premere B. Nell'app scegliere **Abbina StopWatch → Cerca StopWatch**, selezionare il dispositivo e inserire il codice nella richiesta di abbinamento di iOS. La finestra dura 90 secondi; le riconnessioni usano le chiavi memorizzate.
3. Nell'app, Impostazioni: inserire Wi-Fi 2,4 GHz e, facoltativamente, Home Assistant e preferiti. Lasciare vuoti indirizzo e preferiti se si vuole iniziare offline.
4. Uscire con A tenuto. Aprire **Sistema → Diagnostica → Archivio / RAM**. Se l'archivio non è inizializzato, B apre la conferma e un secondo B lo inizializza. Questa operazione cancella tutte le note dell'archivio. Il touch non può confermarla.
5. Tenere B, attendere REC, parlare e rilasciare. In Note vocali, B breve riascolta l'ultima nota. Dal companion si scaricano i WAV.

Il server companion rimane disponibile solo durante una sessione aperta dal dispositivo, per un massimo di 10 minuti; uscire dalla schermata chiude la sessione. Durante la cattura l'elaborazione HTTP è sospesa: un download si può riprendere/ripetere dopo il salvataggio. L'iPhone potrebbe segnalare che la rete dello StopWatch non ha accesso a Internet: è previsto.

Per usare il browser di riserva, aprire **Browser Wi-Fi**: scansionare il primo QR per la rete `StopWatch-…`, B passa al QR della pagina `http://192.168.4.1/`. Utente `stopwatch`, password sul display. Questa sessione mantiene una password temporanea; il companion nativo non la usa.

## Abbinamento iPhone

Il firmware richiede **LE Secure Connections, autenticazione con passkey e bonding**, con chiavi a 128 bit. RX richiede scritture cifrate e autenticate; TX richiede letture cifrate e autenticate e invia dati solo dopo autenticazione. La prima connessione di un dispositivo sconosciuto e consentita solo nella finestra aperta fisicamente con B. Il codice non viene esposto dal protocollo, dalle API web o dai log applicativi.

Le chiavi BLE vengono mantenute dallo stack NimBLE nella NVS, e da iOS nel suo archivio di sistema. Nell'app viene salvato solo l'identificatore CoreBluetooth dello StopWatch. Non servono certificati creati dall'utente per le connessioni BLE. Il firmware gestisce una connessione alla volta; la piattaforma conserva fino a tre bond.

Nella schermata iPhone Bluetooth, A cambia opzione. **Rimuovi** apre una conferma fisica con B che revoca tutti gli abbinamenti; il touch non puo confermare. Dopo la revoca, dissociare anche lo StopWatch nelle impostazioni Bluetooth di iPhone e ripetere l'abbinamento. La cancellazione del solo identificatore nell'app non revoca le chiavi di sistema.

Il collegamento riconosce lo stesso dispositivo dopo riavvio, ma non garantisce presenza radio continua o esecuzione permanente dell'app in background. Il simulatore non verifica la radio fisica. Il collaudo di abbinamento, riconnessione dopo riavvio e revoca richiede iPhone e StopWatch reali.

## StopWatch dentro Home Assistant

Con Mosquitto gia collegato a Home Assistant, aprire **Impostazioni → Integrazione Home Assistant** nell'app e salvare host, porta e credenziali del broker. Non occorre creare un componente Python o modificare `configuration.yaml`: il firmware pubblica la discovery MQTT su `homeassistant/…/config` e Home Assistant raggruppa le entita sotto **StopWatch**.

Entita disponibili:

- Sensori: batteria, segnale Wi-Fi, numero di note; stati di carica e registrazione.
- Impostazioni numeriche: luminosita, volume interazioni (`volume`), volume note (`note_volume`), vibrazione e timeout del display.
- Pulsanti: impulso di vibrazione e tono.
- Eventi Pulsante A/B: `press`, `hold`, `release` (rilascio B dopo registrazione). I gesti restano utilizzabili dal menu del firmware.

I topic usano `stopwatch/stopwatch_<MAC>/…`, con identificatore stabile. Lo stato viene aggiornato ogni 15 secondi e dopo modifiche; availability usa online/offline e last will. Discovery e stato sono retained; gli eventi non lo sono. I comandi retained vengono ignorati e il dispositivo non ripete automaticamente azioni. Durante la registrazione i comandi vengono rifiutati per proteggere la cattura.

MQTT TCP e adatto a una LAN fidata; TLS verifica nome del broker e CA (bundle pubblico integrato, oppure PEM privato). L'orologio viene sincronizzato via BLE/NTP prima di usare TLS. Un broker vuoto disattiva il collegamento. Per rimuovere definitivamente le entita gia pubblicate, rimuovere anche i loro topic di discovery retained dal broker.

## Home Assistant

Dal profilo Home Assistant, sezione Sicurezza, creare un **token di accesso a lunga durata** e inserirlo nel companion. Usare un utente dedicato con i permessi necessari. Il token non viene restituito dalle API di configurazione né scritto nei log.

Esempi di preferiti:

| Nome | Entity ID | B breve |
|---|---|---|
| Luce studio | `light.studio` | Toggle |
| Presa scrivania | `switch.scrivania` | Toggle |
| Cinema | `scene.cinema` | Attiva |
| Routine sera | `script.sera` | Attiva |
| Temperatura | `sensor.temperatura_studio` | Aggiorna stato |

Azioni ammesse: `light`, `switch`, `input_boolean`, `fan` → `toggle`; `scene`, `script` → `turn_on`; `button`, `input_button` → `press`. Le altre entità, incluse serrature e allarmi, vengono visualizzate in sola lettura.

Lo stato del preferito visibile viene riletto ogni 2 secondi sul percorso diretto e dopo 5 secondi dalla risposta sul ponte iPhone; durante la conferma e a display spento il polling ordinario si ferma. Gli stati scadono dopo 10 secondi e vengono invalidati dopo una disconnessione o modifica della configurazione. Solo `on` e `off` possono apparire come ON/OFF: `unknown`, `unavailable` e risposte di un'altra entità/configurazione non riutilizzano il vecchio valore.

Dopo un toggle compare **Verifica stato…**: il comando viene inviato una sola volta, poi si effettuano solo letture ogni 500 ms finché Home Assistant riporta lo stato opposto a quello iniziale, per un massimo di 12 secondi. Durante la verifica non si possono inviare altri comandi. Se lo stato non viene confermato compare un avviso, senza ripetere il toggle. Una risposta REST positiva conferma l'accettazione del comando, non garantisce il completamento fisico di una scena o di uno script.

HTTP è adatto solo a una LAN fidata. HTTPS usa il bundle pubblico integrato; per un certificato privato si puo aggiungere la CA PEM corretta. Il firmware verifica nome del server, certificato e orologio, senza `setInsecure()`. L'ora viene sincronizzata dall'iPhone quando si collega il companion o tramite NTP. Token, password Wi-Fi/MQTT e chiavi BLE sono nella NVS locale, **senza cifratura della flash** in questa versione.

## Diagnostica

| Test | Cosa verifica / cosa osservare |
|---|---|
| Pulsanti | Stato dei due pulsanti e contatori delle pressioni |
| Touch | Punto toccato e coordinate |
| Display | Cinque colori selezionabili, verifica visiva di pixel e contrasto |
| Vibrazione | Impulso manuale di 250 ms |
| Altoparlante | Tono breve al volume impostato |
| Microfono | Nota di prova con B tenuto e riascolto con B breve |
| IMU | Accelerometro e giroscopio dal BMI270 |
| RTC | Lettura dell'ora UTC e indicazione di perdita dell'ora |
| Batteria / USB | Tensione batteria, stima percentuale, tensione USB, stato di carica |
| Wi-Fi | Collegamento, IP e RSSI; B apre la configurazione |
| BLE | Stato del companion, nome del dispositivo, numero di bond e accesso all'abbinamento iPhone |
| Archivio / RAM | Scrittura/lettura di un pattern di 64 KB in PSRAM, spazio flash e inizializzazione manuale |
| Grove / GPIO | Scansione I2C senza abilitare l'uscita 5 V; GPIO posteriori richiedono un fixture/accessorio |
| LED | Accensione e spegnimento del LED verde |

La diagnostica distingue rilevamento/configurazione da verifica manuale: vedere un driver attivo non certifica che il componente funzioni fisicamente. Non modifica correnti di carica, pin BAT o segnali del bus posteriore. Le revisioni v1.0 e v1.0.1 differiscono nella funzione BAT/5V IN: verificare l'hardware prima di collegare accessori al bus.

## Internet tramite iPhone

Dal firmware **0.11.0** e Companion **0.5.0**, Casa e Spotify possono usare il telefono come ponte quando il collegamento diretto a Mosquitto non è disponibile. Il Wi-Fi diretto resta la prima scelta. Il ponte usa gli stessi blueprint già configurati: l’iPhone si autentica sull’API WebSocket HTTPS di Home Assistant, sottoscrive la risposta MQTT del solo StopWatch, verifica l’ascolto effettivo con un echo e pubblica una sola richiesta non retained con QoS 0. Mosquitto rimane nella rete di casa; non serve esporne la porta su Internet. Non è un modem IP generico per tutte le applicazioni.

Nell’app: **Impostazioni → Internet tramite iPhone**. Servono l’URL HTTPS di Home Assistant raggiungibile dalla rete cellulare e un token di un utente amministratore (necessario per `subscribe_trigger`). Se la configurazione HTTPS e il token sono già sullo StopWatch, vengono importati sul BLE abbinato. Il token è conservato nel portachiavi dell’iPhone con `AfterFirstUnlockThisDeviceOnly`, non in UserDefaults o nei log. Dopo un riavvio del telefono occorre sbloccarlo almeno una volta. Credenziali MQTT e Wi-Fi non vengono esportate.

Un evento `relay.request` risveglia l’app via Core Bluetooth. L’app apre una sessione di rete limitata, inoltra una sola richiesta e restituisce `relay.reply`; non mantiene una connessione WebSocket permanente per impedire la sospensione. Ripristino Bluetooth e brevi attività in background gestiscono riconnessione e completamento a schermo bloccato. L’iPhone deve essere vicino, avere Bluetooth e accesso a Internet; iOS può limitare l’esecuzione e la chiusura forzata dell’app può interrompere il servizio.

Stati, conferme Casa, play/pausa, precedente/successivo, lettori e playlist restano gli stessi. L’accesso remoto usa la configurazione dei preferiti gestita dal blueprint MQTT; i preferiti REST manuali mantengono il precedente percorso diretto. Le copertine passano dal telefono solo sul percorso del ponte: download da CDN Spotify consentiti o proxy immagini Home Assistant, thumbnail JPEG con lato massimo 188 px e limite di 6000 byte, senza modificare il Badge o l’archivio. I token HA non vengono inviati ai CDN.

Le richieste sono correlate per tipo e ID, vincolate alla sessione BLE, limitate a 12 s e cancellate al cambio di connessione. I comandi con effetti conservano la scadenza di 5 s anche su Home Assistant: non vengono ritentati automaticamente né trasferiti sull’altro percorso dopo un invio incerto. Casa invalida lo stato quando cambia collegamento. Le letture sul ponte attendono 5 s dalla risposta completata, senza occupare continuamente il canale; lo stato ancora valido rimane visibile durante un refresh e le letture automatiche si fermano nella conferma. La conferma si chiude solo se la richiesta è stata accettata. Una mancata conferma richiede verifica prima di ripetere un comando.

Verifiche: `tests/relay_test.cpp` esercita coda, limiti, rollover, scadenza e riconnessione; `tests/relay_dispatch_test.py` compila la scelta di trasporto reale e controlla che un errore di invio MQTT non generi un secondo comando sul telefono. `tests/home_interaction_test.py` esercita le funzioni reali di comando, conferma e polling con backend controllati, inclusi lettura occupata, stato vecchio, mancato invio e pausa tra aggiornamenti. I test Swift `HomeRelayTests` verificano URL HTTPS, topic, limiti, ID, scadenza, proxy immagini e frammentazione BLE. La prova con rete dati e iPhone bloccato richiede i dispositivi fisici; l’esito non si deduce dalla sola compilazione.

Collaudo del 25 ottobre 2026 con Wi-Fi dello StopWatch disabilitato soltanto nella build di prova e Wi-Fi dell’iPhone spento: Casa e Spotify hanno completato richieste con l’iPhone bloccato. Dopo la verifica di ascolto MQTT e la correzione della conferma degli stati, l’utente ha confermato un netto miglioramento in entrambi i versi del toggle. Gli ultimi diagnostici registrano circa 1,14 s per Casa e 1,01 s per Spotify in background; sono campioni, non un tempo massimo garantito. A fine prova è stato ripristinato il firmware normale 0.11.0 con verifica hash, avvio USB riuscito, archivio montato e versione/stati pubblicati nuovamente su MQTT. Non è stato eseguito un collaudo di durata del ponte.

## Conservazione delle note

L'archivio LittleFS occupa circa 8 MB. Sono riservati due slot applicazione da 4 MB; l'aggiornamento OTA non è ancora implementato. Ogni nota richiede fino a circa 1,92 MB; il numero effettivo dipende dalla durata e dallo spazio libero, con limite di 64 file. Quando non rimane spazio per una nota completa viene richiesto di scaricare e rimuovere delle note: nessuna nota viene sovrascritta automaticamente.

Durante la registrazione i blocchi vengono scritti e sincronizzati in un file `.wav.part`. Al salvataggio viene completata l'intestazione WAV e il file viene rinominato. All'avvio si tenta il recupero dei file interrotti con campioni audio; un mount fallito **non causa formattazione automatica**. Un'interruzione dell'alimentazione può perdere i campioni non ancora persistiti e il recupero va collaudato su hardware. I file che non possono essere recuperati restano conservati.

## Verifiche locali

Test senza dispositivo, con un compilatore C++17:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/core_test.cpp -o /tmp/stopwatch-core-test
/tmp/stopwatch-core-test
clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/clock_test.cpp -o /tmp/stopwatch-clock-test
/tmp/stopwatch-clock-test
.venv/bin/pio run
```

I test coprono il pannello rapido, il blocco delle uscite e il ripristino dei volumi, la configurazione opzionale per client precedenti, tap, pressioni prolungate, combinazioni, risveglio, rollover di `millis()`, convalida di URL/broker/entita/nomi dei file, framing BLE e intestazione WAV. I test Swift in `ios/StopWatchTests` verificano frammentazione UTF-8, limiti dei messaggi, configurazione e integrita dei blocchi audio. Le prove audio, tattili, display, radio, autonomia e recupero dopo perdita di alimentazione richiedono il dispositivo. REST e MQTT richiedono un test sul proprio Home Assistant dopo la configurazione.

## Riferimenti

- [Hardware e schema StopWatch](https://docs.m5stack.com/en/core/StopWatch)
- [Esempi Arduino StopWatch](https://docs.m5stack.com/en/arduino/stopwatch/program)
- [Microfono: alternanza con l'altoparlante](https://docs.m5stack.com/en/arduino/stopwatch/mic)
- [M5Unified](https://github.com/m5stack/M5Unified)
- [PIOArduino 55.03.37](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.37)
- [API REST Home Assistant](https://developers.home-assistant.io/docs/api/rest/)
- [Discovery MQTT Home Assistant](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
- [ESP-MQTT 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/mqtt.html)
