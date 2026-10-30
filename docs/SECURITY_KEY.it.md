> Nella beta pubblica la build normale disabilita la chiave. Per attivarla esplicitamente compila `python -m platformio run -e stopwatch-lab`; solo account di prova. Nessuna migrazione o cancellazione delle credenziali esistenti viene eseguita dalla build normale.

[English](SECURITY_KEY.md)

# Chiave USB · LAB

Il firmware **0.14.0-lab** aggiunge un prototipo reversibile FIDO2/WebAuthn. La scelta approvata è collaudarlo con credenziali di prova. Non abilita Secure Boot, cifratura flash o blocchi permanenti e non programma eFuse. Il Companion non cambia.

## Uso

1. Collegare lo StopWatch al computer con un cavo USB dati e aprire **Chiave USB · LAB** sul dispositivo.
2. Nel browser Chromium scegliere la **chiave di sicurezza USB**. Il PIN della chiave si imposta e si inserisce nella finestra del browser, mai nel Companion o in chat.
3. Controllare il sito indicato sullo StopWatch. Rilasciare eventuali pulsanti già premuti, poi premere e rilasciare **B blu** per confermare. **A** annulla, A lungo esce. Non confermare richieste inattese.
4. La richiesta scade dopo 25 secondi. Uscire dall'app o scollegare USB annulla la richiesta e invalida lo stato transitorio di verifica PIN. Il touch e il ponte iPhone non possono confermare una credenziale; B nell'app Chiave non avvia note vocali.

La chiave risponde alle richieste solo mentre la sua applicazione è aperta e il display è attivo. In questa schermata, con USB collegata, il display rimane visibile e si attenua dopo inattività. Mouse/tastiera e CDC restano descrittori USB separati; il mouse resta utilizzabile aprendo Ponte USB. Home Assistant e il ponte Bluetooth continuano a girare nei rispettivi task.

## Funzioni del prototipo

- CTAPHID su interfaccia HID dedicata, report da 64 byte, messaggi fino a 1300 byte. Il prototipo accetta RP ID di massimo 80 caratteri ASCII stampabili, mostrati per intero su più righe; rifiuta quelli che non può visualizzare.
- FIDO2.0/2.1, ES256/P-256, credenziali discoverable/residenti, ClientPIN protocolli 1 e 2.
- `hmac-secret`, usato dal browser per WebAuthn PRF; credProtect, credBlob, gestione delle credenziali e largeBlob del core CanoKey.
- Attestation **none** e AAGUID nullo: non dichiara l'identità o la certificazione di una chiave commerciale. Niente U2F, NFC, biometria o FIDO tramite Bluetooth.
- Il core gira in un task dedicato. Il firmware mantiene CPU attiva durante le operazioni della chiave; l'interfaccia e la conferma fisica restano nel task principale.

## Archivio e reversibilità

Le credenziali, il materiale di derivazione e il verificatore PIN sono nel filesystem interno dell'immagine **`/security-key.lfs`**, 128 KiB nell'archivio LittleFS esistente. Nessuna partizione viene cambiata. L'immagine non viene esportata via Companion, portale o MQTT.

L'inizializzazione ordinaria dell'archivio note, quando il filesystem è leggibile, conserva questo file insieme ai pacchetti audio. Un filesystem esterno corrotto che richieda formattazione completa può perdere anche la chiave: il prototipo non è un metodo di recupero del vault. Un'immagine della chiave illeggibile o parziale produce un errore, senza cancellare automaticamente le credenziali. Un'interruzione durante la prima creazione può richiedere un intervento manuale sul solo archivio della chiave.

Si può tornare al firmware precedente con il normale caricamento USB. Ciò non revoca credenziali già registrate e non cancella automaticamente l'immagine delle chiavi. La revoca si esegue sul servizio interessato; un reset CTAP cancella tutte le credenziali della chiave e il suo PIN, richiede conferma fisica ed è ammesso dal core solo nei primi 10 secondi dall'avvio. Non cancellare file personali o l'intero archivio per rimuovere il prototipo.

## Limiti di sicurezza

È una **chiave software sperimentale su un dispositivo multifunzione**, senza isolamento hardware dal resto del firmware. Chi legge la flash o sostituisce il firmware può ottenere il materiale necessario a clonare le credenziali e a riprodurre i risultati PRF. Il PIN limita l'uso del protocollo standard; non sostituisce la cifratura della memoria contro quel tipo di attacco. Una compromissione delle altre funzioni del firmware può esporre lo stesso processo e archivio.

Non è una chiave FIDO certificata e non ha ricevuto un audit indipendente. Usare credenziali e account di prova; mantenere sempre un accesso indipendente. Questa implementazione non registra automaticamente una passkey Bitwarden reale e non conserva né digita la master password.

## Bitwarden

Per aprire e decifrare il vault tramite passkey serve **WebAuthn PRF**, oltre a una normale credenziale FIDO: una registrazione come secondo fattore da sola non sblocca il vault. La prova locale richiede credenziale residente, verifica PIN, presenza fisica, firma valida e risultati PRF stabili.

Il percorso scelto è l'estensione di un browser Chromium sul Mac. Disponibilità e comportamento di accesso/blocco vanno verificati nel flusso Bitwarden effettivo; questo prototipo non promette che ogni semplice sblocco di un'estensione già autenticata venga sostituito da un tocco sulla chiave. La registrazione del vault reale rimane separata dalla prova locale. [Bitwarden: accesso con passkey](https://bitwarden.com/help/login-with-passkeys/).

## Prova locale ripetibile

```sh
.venv/bin/python -m pip install -r tools/key_lab/requirements.txt
.venv/bin/python tools/key_lab/server.py
```

Aprire **http://localhost:8765** nel browser Chromium. Il server ascolta solo su loopback, verifica Host/Origin e un token della pagina; non accetta richieste da siti esterni. Il PIN resta nel flusso nativo del browser. Creare una passkey, verificare due volte e ripetere dopo riavvio del dispositivo. La pagina confronta due input PRF diversi e controlla la stabilità dello stesso input, senza inviare il valore PRF al server o salvarlo. Il server verifica challenge, origine, UV/UP e firme; `.venv/key-lab-public.json` conserva solo la credenziale pubblica e il contatore.

Per i controlli USB negativi, lasciare Chiave aperta e **non premere B**:

```sh
.venv/bin/python tools/key_lab/check_usb.py
```

Controlla capacità dichiarate, RP estraneo rifiutato, cancellazione e timeout senza presenza. Non imposta/cambia il PIN, non prova PIN sbagliati e non cancella credenziali.

Per presenza fisica e framing, senza dispositivo:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Iinclude tests/security_key_test.cpp -o /tmp/stopwatch-key-test
/tmp/stopwatch-key-test
.venv/bin/python tests/key_parser_test.py
```

Provenienza, commit e modifiche del core sono in [lib/CanokeyFido/README.md](../lib/CanokeyFido/README.md). Gli esiti hardware e i controlli ancora mancanti sono in [STATUS.md](STATUS.md).
