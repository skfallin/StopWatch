# Sicurezza

[English](SECURITY.md)

Non pubblicare credenziali, dump della flash, registrazioni private o dettagli di vulnerabilità nelle issue. Usa la segnalazione privata GitHub quando abilitata. Se manca, apri soltanto una richiesta di attivazione del canale privato e attendilo prima di condividere dettagli. Questa funzione va abilitata nelle impostazioni GitHub prima della release pubblica.

Le correzioni sono destinate all’ultima beta; non esistono SLA, audit indipendente o certificazione FIDO.

Credenziali Wi-Fi/MQTT e token Home Assistant si inseriscono tramite dispositivo, Companion abbinato o portale temporaneo. Restano nella NVS locale, **senza cifratura della flash**: accesso fisico o firmware sostituito possono esporle. Il ponte iPhone usa il portachiavi iOS e richiede attualmente un token amministratore Home Assistant; attivalo solo se accetti tale ambito.

Audio, trascrizioni e Badge restano su dispositivo/telefono. La trascrizione richiede il modello locale, senza fallback cloud. Non disabilitare la verifica TLS: ticket e certificato esatto del dispositivo arrivano dal BLE autenticato. HTTP e MQTT senza TLS sono adatti soltanto a reti locali fidate.

La build normale non registra né avvia l’autenticatore FIDO. La build `stopwatch-lab` è esplicitamente sperimentale, solo per account sacrificabili: nessuna resistenza alla lettura della flash o isolamento hardware. Non usarla come unico accesso a un account o vault reale.

Prima della pubblicazione esegui `python tools/check_public_repo.py`, poi rivedi file, storico, asset precedenti, screenshot ed eventuali log/artifact GitHub. La scansione cerca solo pattern ad alta confidenza e non certifica l’assenza di tutti i segreti. `.gitignore` non protegge credenziali incollate nei sorgenti.

Se un segreto è esposto, revocalo/ruotalo. Cancellarlo nell’ultimo commit non lo elimina dallo storico; qualsiasi riscrittura va coordinata separatamente.
