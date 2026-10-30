> Riferimento tecnico italiano: le sezioni che citano versioni precedenti descrivono il relativo comportamento storico. Per lo stato della beta e le nuove impostazioni di lingua/fuso, vedi [STATUS.it.md](STATUS.it.md).

[English](USB_CONTROL.md)

# Ponte USB

Firmware **0.13.2**, Companion **0.7.2 / build 18**.

## Utilizzo

1. Collegare StopWatch a una porta USB host del computer con un **cavo dati**.
2. Aprire **Ponte USB** dalla home dello StopWatch, con l’icona USB.
3. Nel Companion abbinato, aprire **Controllo USB** e attendere «Mouse e tastiera pronti».
4. Un dito muove il puntatore; due dita scorrono. Un tocco fa clic sinistro; un tocco con due dita fa clic destro. Sono presenti anche Sinistro/Destro separati.
5. **Apri tastiera** mostra la tastiera iPhone: il testo va direttamente nel campo attivo sul computer. Cancella e Invio sono sulla tastiera; Escape, Tab e frecce nella scheda.

Sul dispositivo, **B breve** o il pulsante a schermo sospendono/riprendono il controllo. **A lungo**, Indietro e Home escono dalla funzione. Il controllo continua se si spegne soltanto il display dello StopWatch. Registrazione e pannello Non disturbare interrompono la sessione. Non servono Wi-Fi né server desktop.

### Layout e limiti

Scegliere il layout **attivo sul computer**: Italiano (PC), con `à è é ì ò ù £ €`, oppure Inglese (US / ABC), con ASCII stampabili. Su macOS usare la sorgente **Italiano — PC** oppure **ABC**. Italiano Apple standard non coincide con Italiano — PC per tutti i simboli. Completare o chiudere l’eventuale Assistente configurazione tastiera prima della prova.

Emoji, altri alfabeti e caratteri assenti nel layout sono rifiutati prima di inviare l’intera inserzione; massimo 96 caratteri per inserzione. Non viene emulato un incolla Unicode del sistema operativo. Correzione automatica e virgolette intelligenti sono disattivate. Caps Lock e il campo selezionato restano gestiti dal computer. Il testo digitato non viene salvato.

La prima versione non include trascinamento con un pulsante mantenuto, combinazioni con modificatori o tasti funzione. La porta collegata deve supportare periferiche USB HID; una porta di sola ricarica non basta.

## Trasporto e sicurezza

`platformio.ini` abilita TinyUSB (`ARDUINO_USB_MODE=0`) mantenendo CDC all’avvio. HID e console seriale condividono il cavo. I descrittori sono registrati prima dell’avvio USB del core; il loop invia i report con un’attesa massima di 10 ms ciascuno.

- Il canale BLE richiede cifratura, autenticazione e abbinamento. Non ci sono comandi web/MQTT per attivare il controllo.
- Serve aprire fisicamente Ponte USB. Cambio schermata, pausa, USB/BLE disconnessi e pannello invalidano la sessione e svuotano la coda.
- Code limitate: 32 inserzioni sul telefono, 8 pacchetti di movimento in ingresso, 256 report HID. Tasti e clic sono coppie pressione/rilascio.
- All’interruzione si inviano report neutri. Dopo 1,5 secondi senza progresso USB si annullano gli input residui; occorre riprendere la funzione sul dispositivo.
- Gli errori non causano reinvii automatici del testo. La conferma BLE indica **accodamento**, non la comparsa del testo nell’applicazione del computer.
- Il polling rapido esiste soltanto con la scheda visibile e iPhone attivo. Casa/Spotify mantengono il servizio precedente in background.

### Puntatore rapido

Dal firmware 0.13.1 il movimento usa la caratteristica `75f3a103-9e9a-4b9c-9d73-c85f44e6a001`, cifrata e autenticata, con scritture senza risposta GATT. Il telefono invia direttamente dai gesti, rispetta `canSendWriteWithoutResponse` e riprende tramite `peripheralIsReady`. Non attende polling, JSON o risposte dei normali comandi. Clic e tastiera mantengono la conferma. Il percorso precedente rimane disponibile per compatibilità.

Formato di 8 byte: versione `1`, sessione `uint32` little endian, `dx`, `dy`, rotella come `int8` fra −127 e 127. Input disabilitati, sessioni vecchie e pacchetti malformati sono scartati. Il movimento è best effort: una coda piena può perdere spostamenti senza scollegare il telefono o ripetere clic.

RX/TX vengono creati prima della nuova caratteristica rapida per ridurre lo spostamento degli handle. Il firmware segnala inoltre Service Changed a ogni avvio; il Companion gestisce `didModifyServices` riscoprendo il servizio e ignorando callback degli oggetti invalidati. Non vengono cancellati i bond.

### Operazioni JSON BLE

`usb.status` restituisce `supported`, `active`, `connected`, `ready`, `busy`, `session`, `fault`, `queued`. Lo stesso oggetto facoltativo compare in `status.usb`; il protocollo resta 1.

`usb.input` accetta:

```json
{"session":42,"kind":"move","x":12,"y":-4,"wheel":0}
{"session":42,"kind":"click","button":"left"}
{"session":42,"kind":"key","key":"enter"}
{"session":42,"kind":"text","layout":"it","text":"Caffè"}
```

Movimento JSON: compatibilità, interi da −127 a 127. Clic: `left`, `right`. Tasti: `enter`, `escape`, `backspace`, `tab`, `delete`, `left`, `right`, `up`, `down`. `queued` e `fault` descrivono progresso ed errori USB.

## Verifiche e aggiornamenti

Sorgenti: `include/usb_input.h`, `src/usb_control.*`, `src/companion.*`; nel Companion `USBControl.swift`, `USBControlView.swift`, `WatchConnection.swift`.

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Iinclude -I.pio/libdeps/stopwatch/ArduinoJson/src \
  tests/usb_input_test.cpp -o /tmp/stopwatch-usb-input-test
/tmp/stopwatch-usb-input-test
xcrun swift test --package-path ios --scratch-path /tmp/stopwatch-protocol-tests
```

Test: layout, Unicode rifiutato senza invii parziali, pacchetto rapido, ordine, coda piena, sessioni, disconnessioni, rilascio e timeout con rollover. Il collaudo fisico deve verificare puntatore, clic, scorrimento, lettere/simboli, sospensione e recupero dell’upload USB.

Partizioni e dati non cambiano. La porta seriale può cambiare nome in modalità composita: rilevare nuovamente `/dev/cu.usbmodem*`. Il core supporta anche l’ingresso software in download aprendo CDC a 1200 baud, verificato sul dispositivo; se il comando di upload non lo attiva, usare alimentazione/reset. La modalità download ROM resta disponibile; non eseguire `erase` o `uploadfs` per aggiornare.


## Slide — telecomando locale (0.17.0)

Slide usa il descrittore tastiera già presente, senza nuove interfacce USB o software sul computer. A/cerchio sinistro inviano sinistra (oppure Pagina su), B/cerchio destro inviano destra (oppure Pagina giù); Oscura / riprendi invia B. Ogni comando accoda pressione e rilascio, senza ripetizioni automatiche. Occorre avere il programma giusto in primo piano: il firmware non legge né conferma lo stato del computer.

La sessione locale funziona senza iPhone. Il passaggio Ponte ↔ Slide invalida la sessione precedente e scarica la coda; durante Slide vengono rifiutati mouse e tastiera dal Companion. USB assente/sospesa, uscita, pannello rapido e registrazione disabilitano gli invii e gestiscono i rilasci. La conferma FIDO rimane vincolata alla sua schermata e al pulsante fisico.
