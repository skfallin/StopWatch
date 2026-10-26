# Stato del progetto

## Versioni sorgenti

| Componente | Versione |
|---|---|
| Firmware | 0.12.0 |
| Companion iOS | 0.6.0, build 15 |
| Protocollo BLE | 1 |

## Disponibile

- Orbita + Focus, touch e pulsanti, impostazioni, diagnostica e Non disturbare.
- Preferiti Home Assistant, conferme dei comandi, MQTT discovery e ponte Bluetooth tramite iPhone.
- Spotify: lettore attivo, copertina, riproduzione, brani, playlist e dispositivi.
- Note offline, ricezione nell’app e trascrizione locale italiana quando disponibile.
- Badge con foto/GIF, trasferimento HTTPS/BLE e orientamento automatico o manuale.
- Sei quadranti, timer, cronometro e impostazioni Orologio nel Companion.
- CPU adattiva e display dell’orologio sempre acceso con attenuazione della luminosità.

## Evidenza e limiti

Durante la preparazione della repository, il 26 ottobre 2026, sono stati verificati: compilazione firmware; suite host C++/Python; 16 test dei blueprint Home Assistant; 33 test Swift; interoperabilità del Badge esportato dall’encoder Swift e letto dal parser C++; archivio audio sia nel percorso normale sia nell’installer. Tutti questi controlli sono passati. I comandi ripetibili sono in [DEVELOPMENT.md](DEVELOPMENT.md). Non è stato eseguito un caricamento sul dispositivo durante la pubblicazione del repository.

Prima della preparazione del repository, il firmware 0.12.0 e il Companion 0.6.0 compilavano e i 33 test Swift passavano. I test host coprivano la politica CPU, la compatibilità della configurazione, il dimming, il protocollo, i comandi Casa e la scelta del trasporto. L’utente ha confermato sul dispositivo la selezione del quadrante e il display dell’orologio sempre acceso con attenuazione e risposta al tocco.

Il ponte Casa/Spotify con iPhone bloccato era stato verificato fisicamente con firmware 0.11.2. Dopo le modifiche 0.12.0 i relativi controlli software passavano, ma la nuova prova fisica completa e la misura della CPU a schermo acceso non erano state concluse. Non è stata misurata l’autonomia in ore né una percentuale di risparmio energetico.

**Ultimo stato noto del dispositivo nella sessione precedente:** era installata una build temporanea di prova con Wi-Fi spento. Il ripristino del firmware normale era rimasto in attesa del collegamento USB. Il sorgente in questo repository contiene invece il Wi-Fi automatico normale. Non dedurre dal contenuto del repository quale build sia attualmente sul dispositivo: verificarlo prima di un nuovo collaudo.

Non sono implementati una tastiera remota generica, notifiche iOS generiche, integrazione Grok o aggiornamento OTA dal Companion. La presenza degli slot OTA nella tabella delle partizioni non costituisce un updater.
