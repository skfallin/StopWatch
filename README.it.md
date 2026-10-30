# StopWatch

[English — documentazione principale](README.md)

Firmware della community per **M5Stack StopWatch C152**, con Companion iPhone e integrazione Home Assistant facoltativa. Note vocali, foto e GIF Badge, orologio, timer, cronometro, Focus, telecomando presentazioni e controllo mouse/tastiera USB.

**Candidata beta pubblica:** firmware `0.18.0-beta.1`, Companion `0.9.0` / build 22, protocollo BLE `1`. La preparazione della repo non implica che siano già pubblicati binari o un’app firmata. Non è un prodotto ufficiale M5Stack.

Le interfacce sono disponibili in inglese e italiano. I nuovi dispositivi partono in inglese e UTC; le configurazioni precedenti conservano italiano e fuso di Roma. Lingua e fuso del dispositivo si scelgono dal Companion o dal portale Wi-Fi. L’app segue la lingua di iOS; la lingua della trascrizione si sceglie separatamente.

Home Assistant, MQTT e Spotify sono facoltativi e usano esclusivamente configurazione e credenziali dell’utente. Nessun account del progetto è necessario. Il Companion richiede iOS 17+, macOS/Xcode e firma con il proprio Team Apple. Android, TestFlight/App Store e OTA dal Companion non sono inclusi nella beta.

- [Installazione e aggiornamenti](docs/SETUP.it.md)
- [Companion iPhone](ios/README.it.md)
- [Home Assistant e Spotify](home_assistant/README.it.md)
- [Sviluppo e verifiche](docs/DEVELOPMENT.it.md)
- [Stato e checklist della beta](docs/STATUS.it.md)
- [Sicurezza](SECURITY.it.md) · [Contribuire](CONTRIBUTING.it.md)

Prima di sostituire firmware, esporta note e contenuti personali. Non usare erase o uploadfs per aggiornamenti ordinari: il layout delle partizioni resta invariato. L’installazione su un firmware con partizioni differenti richiede una valutazione separata.

La chiave FIDO2 è disabilitata nella build normale; la build `stopwatch-lab` è una scelta esplicita, destinata esclusivamente ad account di prova. Non protegge dalla lettura della flash. [Limiti del prototipo](docs/SECURITY_KEY.md).

Codice originale sotto licenza [MIT](LICENSE); dipendenze, asset e marchi conservano i propri termini. [Avvisi di terze parti](docs/THIRD_PARTY.md).
