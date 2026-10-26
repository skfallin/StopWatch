# Preferiti StopWatch in Home Assistant

Il blueprint `blueprints/stopwatch_favorites.yaml` crea una configurazione visuale con il selettore del dispositivo e otto selettori di entita. I nomi vengono letti da Home Assistant: non occorre copiare ID o inserire nomi a mano. Il firmware richiesto e 0.3.0 o successivo.

## Installazione

Salvare il file in `/homeassistant/blueprints/automation/stopwatch/favorites.yaml` (nelle installazioni che usano `/config`, il percorso equivalente e `/config/blueprints/automation/stopwatch/favorites.yaml`). Aprire **Impostazioni → Automazioni e scene → Blueprint → StopWatch — Preferiti**.

Selezionare il dispositivo StopWatch e fino a otto elementi, poi salvare l'automazione con nome **StopWatch — Preferiti**. Si possono lasciare vuote le posizioni inutilizzate. L'ordine dei campi determina l'ordine sul dispositivo; selezioni duplicate vengono unite. Per modificare la lista, riaprire l'automazione da Home Assistant e salvare.

L'automazione deve rimanere abilitata: sincronizza la configurazione dopo il salvataggio, all'avvio di Home Assistant, alla riconnessione dello StopWatch e dopo modifiche ai nomi delle entita. Risponde anche alle richieste del dispositivo. Non e necessario riavviare Home Assistant o installare un componente Python.

## Comandi

Sullo StopWatch aprire **Home Assistant**. A breve sceglie il preferito successivo; B breve o un tocco apre **Annulla / Esegui** per tutti i comandi, comprese luci e ventilatori. Le azioni ammesse sono le stesse del firmware REST: luci, interruttori, ventilatori e input_boolean cambiano stato; scene e script vengono attivati; button e input_button ricevono una pressione. Gli altri domini sono in sola lettura e il tocco aggiorna lo stato.

Dal firmware 0.6.0 lo stato visibile viene letto ogni 2 secondi e scade dopo 10 secondi; disconnessioni e cambi di configurazione invalidano la cache. Sul ponte iPhone della 0.11.0 le letture ordinarie attendono 5 secondi dalla risposta e si fermano durante la conferma. Dopo un toggle il blueprint aggiornato aspetta il cambio effettivo di stato, fino a 3 secondi, rispondendo subito se è già avvenuto. Se occorre ancora verificare, il dispositivo mostra **Verifica stato…** ed esegue soltanto letture ogni 500 ms per un massimo di 12 secondi, senza mai ripetere il comando. Per aggiornare il blueprint esistente, sostituire il file e salvare nuovamente l'automazione mantenendo i selettori.

Il percorso usa il broker MQTT gia configurato e non richiede URL o token REST sul dispositivo. La precedente configurazione manuale tramite companion resta compatibile. Se si modifica manualmente la lista dal companion viene usato il percorso REST; una successiva sincronizzazione del blueprint ripristina la lista di Home Assistant e il percorso MQTT.

## Protocollo

- `stopwatch/<identificatore>/favorites/config`: configurazione JSON retained, QoS 1, massimo 2048 byte. Contiene `favorites: [{entity, label}]` con massimo otto elementi. Il firmware convalida tutto prima di salvare, tronca i nomi senza spezzare UTF-8 e non riscrive la flash se la lista e identica. Durante la registrazione la configurazione viene rinviata.
- `stopwatch/<identificatore>/favorite/request`: richiesta con `request`, `entity`, `action`, `sent_at`. QoS 0, non retained. Una sola richiesta pendente e nessun tentativo automatico dopo un timeout.
- `stopwatch/<identificatore>/favorite/result`: risposta correlata con identificatore della richiesta, entita, esito, stato e unita. QoS 0, non retained.

Home Assistant accetta richieste solo per le entita selezionate, per il dispositivo scelto e per i servizi ammessi. Un comando deve arrivare entro cinque secondi dalla generazione: lo StopWatch richiede un orologio sincronizzato per le azioni. Le letture non richiedono questo controllo temporale. Dopo otto secondi senza risposta un comando e indicato come esito incerto; nessun toggle viene ripetuto automaticamente.

La configurazione sopravvive al riavvio sia nella NVS dello StopWatch sia nel messaggio retained del broker. Rimuovere l'automazione non cancella la lista gia salvata. Per svuotarla, salvare tutti i campi preferiti vuoti mentre l'automazione e ancora presente.

### Conferme Govee

Le H6008 `mobile` e `scrivania` passavano soltanto dalla Platform API in Govee2MQTT **2026.03.25-ab9deb66**. In questa versione il bridge attende intenzionalmente 5 secondi dopo un comando cloud prima di rileggerne lo stato; il gruppo rimane acceso finché anche l'ultima lampadina non riporta `off`. Aumentare il polling dello StopWatch non elimina questa attesa.

Per abilitare gli aggiornamenti IoT già supportati dal bridge, nell'add-on **Govee to MQTT Bridge → Configuration** mostrare le opzioni facoltative e inserire personalmente email e password dell'account Govee Home; salvare e riavviare il solo add-on. Le credenziali restano nell'add-on e non vengono copiate nel firmware o nel Companion. Conservare l'API key esistente per le funzioni che la richiedono. Verificare la connessione IoT e uno stato recente proveniente da `AWS IoT API` nei diagnostici della lampada, quindi provare il primo spegnimento dopo una pausa. Non vengono inviate richieste cloud periodiche aggiuntive né pubblicati stati ottimistici dal firmware. Riferimenti: [configurazione del bridge](https://github.com/wez/govee2mqtt/blob/ab9deb66/docs/CONFIG.md), [attesa dopo comando](https://github.com/wez/govee2mqtt/blob/ab9deb66/src/service/state.rs#L528).

Collaudo del 25 ottobre 2026: account inserito dall'utente, add-on riavviato e connessione IoT confermata senza errori di login. Il sensore `scrivania Status` ha ricevuto uno stato `AWS IoT API` alle 01:07:19 locali; l'utente ha confermato un primo spegnimento più rapido dopo una pausa. La Platform API resta disponibile e può ancora comparire come sorgente complessiva se arriva una lettura successiva. Non è stato misurato il tempo completo comando/conferma né verificato un funzionamento prolungato. Firmware **0.11.1** compilato, caricato con verifica hash e avviato con archivio montato; l'utente ha confermato pausa e ripresa toccando la copertina. Test musica/touch, core, Non disturbare e interazione Casa passati.

## Verifica

`tests/blueprint_test.py` verifica selettori, ordine, deduplicazione, nomi Unicode, limite del payload, scadenza dei comandi, filtro per dispositivo e servizi consentiti. Richiede PyYAML e Jinja2 solo nell'ambiente di test; nessuna dipendenza viene aggiunta a Home Assistant.

Il collaudo finale richiede una selezione reale in Home Assistant, la lettura dello stato e una pressione sul dispositivo.

## Spotify sullo StopWatch · firmware 0.9.0

Serve Spotify Premium e l'[integrazione ufficiale Spotify](https://www.home-assistant.io/integrations/spotify/) in Home Assistant. L'entità rappresenta l'account e segue il suo lettore attivo anche passando tra telefono, computer e altri dispositivi Spotify Connect. I lettori devono essere visibili a Spotify; per far comparire un dispositivo inattivo può essere necessario aprire Spotify su quel dispositivo. Non viene riprodotta musica sull'altoparlante dello StopWatch.

1. Nel portale Spotify Developer, creare un'app Web API per uso personale, con redirect esatto `https://my.home-assistant.io/redirect/oauth`. Se esiste già un'app, riutilizzarla: Spotify limita a una le nuove app in Development Mode. Il consenso e le credenziali si inseriscono solo nei siti ufficiali e in Home Assistant.
2. In Home Assistant → Impostazioni → Dispositivi e servizi → Aggiungi integrazione → Spotify, completare il collegamento dell'account. La creazione dell'app e l'autorizzazione richiedono l'accesso dell'utente; il firmware non contiene credenziali Spotify.
3. Aggiungere `blueprints/stopwatch_spotify.yaml` nella cartella `/config/blueprints/automation/stopwatch/` di Home Assistant e ricaricare le automazioni. Da Impostazioni → Automazioni e scene → Blueprint → **StopWatch — Spotify**, creare una sola automazione per il dispositivo.
4. Scegliere StopWatch e l'account Spotify nei selettori. Per le copertine, indicare l'URL raggiungibile dallo StopWatch, ad esempio `http://192.168.0.150:8123`; vuoto usa l'URL Home Assistant già salvato sul dispositivo. Su HTTPS viene verificato il certificato; la CA privata salvata viene usata solo per lo stesso URL già configurato.
5. Salvare, avviare un brano su Spotify e aprire **Spotify** sullo StopWatch.

**B breve** alterna play/pausa, **A breve** passa al brano successivo; il touch offre anche il precedente. Dal firmware **0.11.1**, un tocco nella copertina (anche sul segnaposto senza immagine) alterna play/pausa come B e il pulsante touch in basso. Restano gli stessi blocchi per richieste pendenti, stato scaduto e capacità del lettore. Il precedente mantiene il comportamento di Spotify (può ricominciare il brano corrente). Toccare l'intestazione Spotify apre **Playlist / Dispositivi**: A scorre, B apre, oppure toccare una voce. Nei dispositivi A scorre e B seleziona. A tenuto torna indietro, B tenuto continua a registrare note, A+B torna alla home.

I comandi e le risposte usano `stopwatch/<id>/music/request` e `music/result`, QoS 0, senza retained né tentativi automatici. Le risposte sono correlate alla richiesta corrente; Home Assistant ammette solo comandi Spotify espliciti e scarta comandi più vecchi di cinque secondi, destinazioni non selezionate, URI non Spotify e sorgenti non presenti nell'account. Una sola operazione è in corso per dispositivo. Il firmware disabilita i comandi se lo stato ricevuto supera 15 secondi e verifica le capacità del lettore. Un timeout non conferma che il comando sia fallito e non provoca la ripetizione di uno skip o dell'avvio di una playlist.

Lo stato viene richiesto ogni cinque secondi solo mentre l'app è visibile; dopo un errore i tentativi automatici rallentano a 30 secondi. Il blueprint forza l'aggiornamento dell'entità nelle letture; dopo i comandi usa quello già completato dall'integrazione Spotify, senza richiederlo una seconda volta. Nell'integrazione ufficiale di Home Assistant 2026.10.3 i comandi attendono comunque un secondo prima di rileggere Spotify. Il ridisegno è limitato a una volta al secondo. La copertina viene scaricata solo quando cambia, decodificata a 188×188 e tenuta in PSRAM; nessuna scrittura nell'archivio. Si accettano JPEG baseline/PNG fino a 2048×2048 e 384 KiB, serviti dai CDN Spotify ammessi oppure dal solo endpoint immagini media player di Home Assistant. Download e TLS avvengono in un task separato, senza redirect e senza inviare token REST. Le URL firmate delle immagini non vengono stampate nei log. File non supportati o non raggiungibili lasciano l'icona musicale.

Il blueprint invia fino a 12 sorgenti con nomi fino a 128 byte UTF-8; un nome più lungo non viene alterato né proposto come destinazione. Copertina e sfondo sono separati: l'immagine resta intera e i suoi colori scuriscono lo sfondo, senza sovrapporre comandi all'immagine, seguendo le [linee guida Spotify](https://developer.spotify.com/documentation/design).

Dal firmware **0.9.1**, **Playlist** legge il catalogo dell'account già autorizzato usando `media_player.browse_media`, categoria `current_user_playlists`. Non servono nuove credenziali. Si carica una pagina di otto nomi/URI aprendo la sezione o cambiando pagina: il messaggio rimane entro 4096 byte anche con nomi Unicode. A e gli swipe orizzontali scorrono la selezione; dopo l'ultima voce A carica la pagina seguente e dall'ultima pagina ritorna alla prima. Le frecce touch in basso cambiano pagina. B o un tocco sulla riga avviano la playlist sul lettore attivo con `media_player.play_media`, poi tornano alla copertina. Sono accettate solo URI `spotify:playlist:` con ID valido e presenti nella pagina ricevuta. Il catalogo resta in RAM e non modifica note, Badge o impostazioni. Il menu conserva anche la scelta del dispositivo di riproduzione.

Collaudo della **0.9.0**: firmware caricato con verifica hash, avvio normale, integrità heap e archivio confermate via USB; l'utente ha confermato copertina, titolo, dispositivo e comandi A/B funzionanti. L’utente ha confermato anche il funzionamento del catalogo playlist della 0.9.1.

Verifiche locali: `tests/music_test.cpp` usa il parser JSON reale, esercita freschezza, avanzamento, limiti, URL delle immagini, dimensioni, dati troncati e bersagli touch. `tests/music_blueprint_test.py` controlla template, filtro per dispositivo, scadenze, sorgenti, avanzamento e dimensione dei messaggi Unicode. `tools/render_clock_preview.py /tmp/stopwatch-spotify-preview --music` esegue il renderer C++ reale per sei anteprime. Il collaudo finale richiede l'account Spotify autorizzato, il blueprint attivo e il dispositivo fisico.

Verifica del 25 ottobre 2026: blueprint aggiornato e automazione Spotify salvata mantenendo account e URL; 16 test dei blueprint passati. L'utente ha provato pausa, ripresa e cambio brano, riportando un miglioramento modesto. Una traccia di comando registra 1,38 s sul server, senza aggiornamento duplicato; non è una misura dell'intero viaggio Bluetooth/rete. Per Casa, il primo spegnimento lento osservato alle 00:57:14 ha restituito ancora `on` dopo l'attesa di 3 s: il gruppo `light.camera_da_letto` è passato a `off` soltanto alle 00:57:21, quando `light.scrivania` ha aggiornato il proprio stato. Lo StopWatch conserva la verifica reale del gruppo.

## Uso fuori casa con Companion

Firmware 0.11.0 e Companion 0.5.0 riutilizzano questi blueprint dal telefono: HTTPS/WebSocket verso Home Assistant, sottoscrizione `subscribe_trigger` del topic di risposta e singolo `mqtt.publish` del topic di richiesta. Serve un token amministratore e l’accesso HTTPS remoto. Non occorre pubblicare Mosquitto su Internet né cambiare i selettori di preferiti, account Spotify, lettori o playlist. Il Wi-Fi/MQTT diretto mantiene la precedenza; le regole di scadenza e le conferme sul dispositivo valgono anche sul ponte.
