# StopWatch Companion

[English](README.md)

App nativa iPhone **0.9.0 / build 22**, iOS 17+, SwiftUI/CoreBluetooth/Apple Speech. Inglese principale e italiano tramite localizzazione iOS.

Apri `StopWatch.xcodeproj`, seleziona il target e il tuo Team in Signing & Capabilities, collega/autorizza iPhone e avvia Run. Se il bundle `dev.stopwatch.companion` non è disponibile per il tuo Team, scegline uno tuo al primo uso e conservalo negli aggiornamenti. Certificati e identità di firma non sono inclusi; distribuzione TestFlight/App Store non configurata.

Sul dispositivo apri Sistema → iPhone, premi B e inserisci il codice a sei cifre nella richiesta iOS dopo aver selezionato StopWatch nell’app. La finestra dura 90 secondi. Per rifare un abbinamento revocato, dimentica StopWatch anche nelle impostazioni Bluetooth iOS.

Lingua e fuso del dispositivo si scelgono nelle impostazioni del Companion. La lingua dell’app segue iOS. La trascrizione ha scelta separata inglese US/UK o italiano e richiede un modello Apple Speech locale. Le installazioni già abbinate mantengono i precedenti valori italiani per trascrizione e tastiera. Il layout Ponte USB deve corrispondere al computer.

Le note arrivano via HTTPS verificato, con alternativa BLE; gli originali non vengono cancellati. Trascrizione con app aperta; audio conservato se modello o permesso non sono disponibili. Ricezione in background soggetta a iOS: sblocca una volta dopo il riavvio ed evita la chiusura forzata.

Badge: sorgente fino a 25 MB; risultato fino a 3 MB, 120 frame, 15 secondi e 5/10 fps. La sostituzione verifica il contenuto prima di attivarlo. Il ponte Home Assistant è facoltativo e conserva il token nel portachiavi: [configurazione](../home_assistant/README.it.md).

[Test e compilazione](../docs/DEVELOPMENT.it.md). `--preview` mostra l’interfaccia senza Bluetooth né dati inventati. Il simulatore non verifica l’abbinamento reale. Protocollo BLE sempre 1; lingua e fuso sono campi opzionali compatibili con i client precedenti.
