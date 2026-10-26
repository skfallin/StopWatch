# Credenziali e dati personali

La repository privata deve contenere codice, test e risorse del progetto. Non deve diventare una copia dei dati del dispositivo o del portachiavi del telefono.

## Da conservare fuori da Git

- Password Wi-Fi/MQTT, token Home Assistant, segreti OAuth Spotify e file `.env` personali.
- Chiavi private, certificati di firma Apple, profili di provisioning e dati del portachiavi.
- Dump della flash/NVS, backup completi, esportazioni della configurazione e log con dati sensibili.
- Registrazioni vocali, trascrizioni, immagini Badge personali ed esportazioni dell’app.

`.gitignore` esclude i formati e le cartelle locali più comuni; non può riconoscere una credenziale incollata in un sorgente. Prima di ogni pubblicazione controllare i file e il diff in stage. Le chiavi e i valori negli esempi di test devono rimanere sintetici.

## Dove restano i dati

La configurazione del firmware si inserisce dal dispositivo, dal Companion o dal portale locale. Le normali letture di configurazione omettono password e token. L’importazione del token per il ponte richiede il collegamento BLE autenticato e il Companion lo conserva nel portachiavi iOS. Non copiare un token nella documentazione per semplificare il setup.

I trasferimenti HTTPS locali verificano certificato e ticket forniti dal dispositivo autenticato. Non disabilitare la verifica TLS per aggirare un problema di connessione. Per problemi del ponte controllare URL remoto, connettività, credenziali e stato della richiesta senza pubblicare segreti nei log.

## Se viene pubblicato un segreto

Revocare o ruotare la credenziale interessata presso il relativo servizio. Eliminare la riga dall’ultimo commit non rimuove il dato dalla cronologia Git. Prima di riscrivere la cronologia o forzare un push concordare l’operazione con chi usa la repository.

Non rendere pubblica questa repository o aggiungere accessi esterni senza una richiesta esplicita del proprietario.
