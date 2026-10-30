# Stato della beta pubblica

[English](STATUS.md)

Candidata: firmware **0.18.0-beta.1**, Companion **0.9.0 / build 22**, protocollo BLE **1**. Queste versioni descrivono i sorgenti, non una release già pubblicata o un’app firmata.

Perimetro: C152, firmware/portale/Companion inglese e italiano, fuso configurabile, trascrizione locale con lingua separata, funzioni autonome e integrazioni facoltative Home Assistant/Spotify. Nuove configurazioni inglese/UTC; precedenti italiano/Roma. La chiave FIDO è disattivata nella build normale, esplicitamente sperimentale in `stopwatch-lab`.

## Verifiche locali — 30 ottobre 2026

- Build firmware normale e LAB riuscite.
- Build installer audio riuscita; test runtime/installer passati, inclusi errori e conservazione dei file utente.
- Suite C++/Python passata: navigazione, energia, protocolli, blueprint, migrazione delle impostazioni regionali e ora legale.
- **45 test Swift passati**, comprese risorse bilingui e compatibilità della configurazione.
- Build iOS per simulatore senza firma riuscita; controllate le chiavi di localizzazione estratte dal compilatore.
- **76 viste Scene per lingua senza etichette troncate**; renderizzati anche orologio e Focus. Il rendering host non equivale a una prova sul display reale o a una verifica visiva iOS.
- Build e suite completa ripetute con successo su una copia dei soli sorgenti destinati a Git, riutilizzando esclusivamente le cache esterne di toolchain/librerie.
- Portale verificato nel browser con dati sintetici: inglese/italiano, salvataggio lingua/fuso e persistenza dopo ricaricamento.
- Nessun pattern di segreto ad alta confidenza trovato nei file di lavoro e nello storico locale raggiungibile. Non è un audit completo di tutti i dati personali o segreti possibili.

Nessun caricamento sul dispositivo, registrazione di credenziali, distribuzione dell’app o cambio di visibilità della repo. Nessun simulatore iOS era avviato per la prova visiva interattiva. Gli esiti CI di ogni revisione inviata sono disponibili in [GitHub Actions](https://github.com/skfallin/StopWatch/actions); i risultati locali sopra riportati non certificano la CI remota.

## Prima della pubblicazione

- [ ] Rivedere e versionare i file previsti, inclusi nuovi sorgenti vendorizzati, licenze e risorse generate; ricontrollare la revisione esatta.
- [ ] Provare su C152/iPhone avvio nuovo, aggiornamento senza perdita dati, scelta lingua/fuso e persistenza dopo riavvio.
- [ ] Controllare entrambe le lingue nelle schermate, nel portale e nel Companion, anche con testo iOS grande.
- [ ] Verificare note HTTPS/BLE, trascrizione inglese/italiana e gestione di permessi/modelli mancanti.
- [ ] Verificare Badge, interruzioni di trasferimento, Casa/Spotify e ponte con iPhone bloccato.
- [ ] Verificare tastiere USB US/italiana, puntatore, disconnessione e Slide; FIDO soltanto nella build LAB e con account sacrificabili.
- [ ] Abilitare la segnalazione privata delle vulnerabilità GitHub, rivedere metadati, storico, vecchi asset ed eventuali log/artifact remoti.
- [ ] Eseguire la revisione committata in CI e generare binari/checksum da quella revisione; pubblicare solo come operazione esplicita.

Limiti: elenco fusi esplicito e regole DST versionate, background soggetto a iOS, flash non cifrata, ponte Home Assistant con token amministratore. Autonomia, qualità acustica e affidabilità radio non sono misurate dai test host. Android, OTA dal Companion e distribuzione TestFlight/App Store non sono inclusi.
