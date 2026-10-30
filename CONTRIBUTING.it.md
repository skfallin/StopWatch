# Contribuire

[English](CONTRIBUTING.md)

Segui [installazione](docs/SETUP.it.md) e [verifiche](docs/DEVELOPMENT.it.md). Mantieni modifiche mirate, compatibilità BLE, configurazioni e file utente. Aggiungi test per il comportamento modificato e distingui verifiche software da prove hardware.

La documentazione principale è inglese; conserva l’italiano. Le chiavi italiane già presenti nei cataloghi sono identificatori stabili. Modifica `locales/en.json` e rigenera con `python tools/generate_localizations.py`. Non tradurre campi JSON, topic MQTT, entity ID, nomi file, etichette degli utenti o identificatori dei siti FIDO.

Nelle segnalazioni indica versioni, hardware, passaggi, esito atteso/reale, lingua e collegamento usato. Rimuovi credenziali, indirizzi personali, audio e immagini dagli allegati. Per vulnerabilità segui [SECURITY](SECURITY.it.md).

Prima di una PR esegui le verifiche pertinenti e controlla il diff; dichiara le prove hardware ancora necessarie. I contributi al codice originale seguono la licenza MIT, mantenendo gli avvisi delle dipendenze.
