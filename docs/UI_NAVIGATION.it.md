> Riferimento tecnico italiano: le sezioni che citano versioni precedenti descrivono il relativo comportamento storico. Per lo stato della beta e le nuove impostazioni di lingua/fuso, vedi [STATUS.it.md](STATUS.it.md).

[English](UI_NAVIGATION.md)

# Scene: navigazione approvata

Firmware sorgente **0.17.1-lab**; slider luminosità lineare scelto il 27 ottobre 2026. La versione scelta conserva le schermate Focus e riunisce le dieci applicazioni in tre gruppi, senza nome del gruppo o descrizione sopra i cerchi.

| Gruppo | App |
|---|---|
| Casa | Casa, Spotify |
| Tasca | Note, Badge, Orologio, Focus |
| Strumenti | Sistema, Ponte, Chiave, Slide |

La prima schermata mostra Tasca. Il gruppo di cerchi è centrato sia con due sia con tre app. Il cerchio principale mantiene il colore della palette; le app secondarie hanno superfici scure. La selezione ruota le app nel cerchio principale. Le sfumature sono disegnate con primitive, senza immagini aggiuntive o animazioni continue; la transizione dura 180 ms.

Dal firmware 0.15.1, cerchi e alone usano dithering spaziale RGB565 prima del disegno sul canvas a 16 bit. Questo distribuisce i livelli di colore tra pixel vicini invece di creare fasce uniformi; la matrice è fissa per evitare sfarfallio. Viene preparata una sola riga per volta, senza aumentare la profondità colore del canvas o aggiungere immagini in flash. Anche il renderer host legge i pixel RGB565 effettivi.

## Vibrazione e tono udibile

La prova dell'utente con Vibrazione a zero ha isolato il beep aggiuntivo al motorino. Il default M5Unified 0.2.27 usa una portante PWM da 2 kHz su IOE1/G9. La 0.15.1 imposta 24 kHz all'avvio, a motore spento, mantenendo intensità, durata degli impulsi e Non disturbare. Il cambio della portante non altera il segnale dello speaker; i nuovi campioni Microclick sono documentati separatamente. La configurazione usa l'API M5Unified; un errore I2C viene registrato nel log. L'eventuale rumore meccanico e la forza percepita richiedono una prova fisica.

Riferimenti hardware: [controllo periferiche StopWatch](https://docs.m5stack.com/en/arduino/stopwatch/m5pm1_m5ioe1) e [scheda tecnica M5IOE1, PWM_FREQ a 16 bit in Hz](https://github.com/m5stack/M5IOE1/blob/main/docs/IO_Expander_Datasheet_EN.pdf). La frequenza IOE1 è condivisa tra i suoi PWM; su StopWatch il firmware usa quel PWM solo per il motorino, mentre display e audio mantengono i propri controlli.

## Home: touch e pulsanti

| Interazione | Risultato |
|---|---|
| Tocco su qualsiasi app | Apre direttamente quell’app |
| Frecce laterali o scorrimento orizzontale | Gruppo precedente/successivo |
| Scorrimento verticale dal centro | App precedente/successiva nel cerchio grande, con transizione di 180 ms |
| A breve, scegliendo i gruppi | Gruppo successivo |
| B breve, scegliendo i gruppi | Passa alla selezione delle app, senza cambiare schermata |
| A breve, scegliendo le app | Porta l’app successiva nel cerchio principale |
| B breve, scegliendo le app | Apre l’app principale |
| A tenuto nella home | Torna alla scelta dei gruppi |
| A+B tenuti | Torna alla home e alla scelta dei gruppi |

B tenuto conserva la registrazione globale delle note. Chiave · LAB mantiene la conferma solo con B fisico; il touch non autorizza credenziali. Tornando da un’app, la selezione resta nel gruppo corrente. Ponte e Chiave hanno nomi brevi anche nelle intestazioni.

## Sistema e impostazioni

Sistema ha due pagine con righe da **298×91 px**: Impostazioni/Diagnostica e iPhone/Browser Wi-Fi. Frecce e scorrimento cambiano pagina, A seleziona la riga successiva e passa alla pagina seguente al termine, B apre. Il ritorno conserva la pagina.

Impostazioni mostra tre categorie, con righe da **298×77 px**:

- **Display**: Luminosità, Colori.
- **Audio**: Interazioni, Volume note; nella seconda pagina Vibrazione, Pacchetto suoni.
- **Standby**: apre direttamente Spegni display.

Gli editor restano quelli esistenti. Le frecce hanno lo stesso stile della home e diametro **72 px**. Gli slider mantengono disegno, posizione, aree di trascinamento, minimi/massimi, arrotondamento, anteprima e singolo salvataggio al rilascio; B mantiene gli incrementi precedenti. Le frecce nell’editor e A scorrono tutti e sette gli editor, anche attraversando le categorie; Indietro torna alla categoria dell’editor visualizzato.

## Stato, ricarica e avvisi

Ora, rete e percentuale batteria formano una riga centrata, presente nella home e nelle pagine Sistema/Impostazioni. Durante la ricarica effettiva, percentuale e icona con fulmine usano il colore della palette. L’alimentazione USB da sola non basta: la lettura combina VBUS e stato CHG_STAT del caricatore M5PM1_G2, attraverso M5Unified 0.2.27.

Due campioni coerenti distanti un secondo stabilizzano il rilevamento. L’avviso **In carica / percentuale · USB collegata** dura 3,2 secondi e si mostra una sola volta per collegamento USB; i cicli di mantenimento non lo ripetono. Il fulmine segue invece lo stato effettivo. A USB scollegata la scheda di ricarica scompare; al prossimo collegamento l’avviso è riabilitato.

L’avviso attende se una nota è in registrazione, i controlli rapidi sono aperti, una richiesta della Chiave attende conferma o un altro avviso è ancora attivo. Non aggiunge suoni o vibrazioni. Se il display è spento lo riaccende per mostrare la scheda, poi rimane valido il timeout impostato.

Tutti gli avvisi ordinari condividono la scheda arrotondata della ricarica: **298×84 px**, centrata a **(233,149)**, icona e massimo due righe. Restano temporanei e si chiudono toccando la scheda; il gesto non passa ai controlli sottostanti. Le conferme di azioni, formattazione e abbinamento restano schermate dedicate con le protezioni precedenti.

## Verifica

```sh
.venv/bin/python tests/scene_test.py
.venv/bin/python tests/menu_labels_test.py
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/gradient_test.cpp -o /tmp/stopwatch-gradient-test
/tmp/stopwatch-gradient-test
.venv/bin/python tools/render_clock_preview.py /tmp/stopwatch-scene-preview --scene
```

I test compilano i rami reali della navigazione e del rilevamento di carica con backend controllati. Verificano accesso a tutte le app e impostazioni, pagine e ritorno, stabilizzazione, assenza di avvisi ripetuti, ritardo durante note/controlli/Chiave e rollover del timer. Il core controlla geometria centrata, touch coerente con i cerchi, frecce senza sovrapposizioni e area completa degli slider. Il controllo font usa Inter e le metriche VLW effettive per tutti gli otto nomi.

Il renderer genera 30 schermate dal codice di disegno reale: tre gruppi e tutte le selezioni, Sistema, categorie, sette slider, avvisi, sei palette e una vista Strumenti Giallo/Blu per controllare la Chiave segnalata nella foto. Nessuna etichetta troncata. Il test dei gradienti controlla estremi, colori primari, conservazione del colore medio e distribuzione spaziale dei livelli RGB565. La comparazione con la build precedente conferma invariati mapping, limiti, disegno, gestione drag/release e incrementi B degli slider.

**Stato:** la 0.15.0 è stata caricata via USB il 26 ottobre 2026, con verifica degli hash del flasher e comparsa della porta USB runtime dopo il riavvio. La 0.15.2 con dithering, frequenza del motorino aggiornata e Microclick è stata caricata il 26 ottobre; archivio audio verificato e Companion aggiornato alla 0.7.3 / 19. Gradiente, rumore/forza della vibrazione, velocità di rendering, touch, ricarica fisica e regressione del ponte con iPhone bloccato richiedono collaudo sullo StopWatch.

### Avvisi coordinati al tema (0.15.3, preparata)

Fondo e base di profondità derivano dalla palette selezionata: Arancio usa toni bruni, Giallo/Blu usa il suo blu e così per gli altri temi. La base posteriore è scurita, mantenendo dimensioni e posizione approvate. Il controllo luminosità Casa e gli slider delle impostazioni mantengono le geometrie documentate.

### Casa: luminosità lineare (0.16.1)

L’ultima richiesta sostituisce la ghiera con lo slider delle impostazioni: stessa linea (x109…357, y339, altezza 8), stesso cursore di raggio 15, area touch 298×66. «Applica» è separato dall’area di trascinamento da 8 pixel, così le zone non si sovrappongono. Gli slider delle impostazioni restano invariati.

Accesso da Casa tramite pressione lunga (650 ms, con avanzamento visibile e annullamento se il dito si sposta), tocco sul sole con percentuale oppure A lungo. Il vecchio pulsante «Regola» nella schermata Casa è rimosso; lo swipe verso l’alto resta compatibile; con i pulsanti B apre la conferma del dispositivo, A seleziona «Regola» e B apre l’editor. Nell’editor A breve diminuisce e B breve aumenta di 5 punti, con limiti 0…100. A tenuto, se la bozza cambia, apre «Applico il nuovo livello?»; B conferma e A annulla. Senza cambi torna a Casa. Touch «Applica» invia direttamente la scelta esplicita; il tasto indietro la scarta. B tenuto conserva la registrazione e A+B conserva Home, senza applicare bozze. Un cambio dei preferiti invalida l’editor.

Solo le luci con supported_color_modes compatibile espongono il comando; zero spegne, un valore positivo usa light.turn_on con brightness_pct. Il firmware aspetta on/off e livello effettivo con tolleranza di 2 punti prima di confermare. Letture, errori, scadenze e comando incerto non causano reinvio. Il blueprint deve essere aggiornato anche sulle installazioni MQTT e via iPhone; il percorso HTTP diretto legge le stesse capacità.


## Home personalizzabile, Focus, Slide e pannello rapido — 0.17.0

Companion → Impostazioni → Home e applicazioni permette di riordinare e nascondere le app. L'ordine vale dentro ciascun gruppo; la prima app del gruppo è quella principale. I gruppi vuoti sono saltati e Sistema non può essere nascosto. Si vedono fino a tre icone, con un indicatore aggiuntivo quando il gruppo ne contiene altre. Scorrere verticalmente nella zona centrale ruota le app; toccarle le apre. Lo scorrimento dal bordo superiore resta riservato ai controlli rapidi. I comandi dei tasti della Home rimangono invariati.

Il pannello rapido ha tre schede (luna, sole, altoparlante), selezionabili per tocco o swipe orizzontale. A chiude, B cambia il valore della scheda corrente. Sole e altoparlante usano la barra e il cursore degli editor esistenti: l'anteprima della luminosità è immediata, il rilascio salva una sola volta; annullamenti/multitouch ripristinano il valore salvato. Il salvataggio NVS resta differito durante registrazioni e richieste Casa. Non disturbare conserva il silenzio completo.

Focus: B avvia/mette in pausa/riprende; A resetta solo a timer fermo. Touch Inizia/Pausa/Riprendi/Continua e Azzera sessione. Ciclo 25/5 minuti, pausa lunga di 15 minuti ogni quattro sessioni; nessuna fase parte automaticamente. La conclusione rispetta Non disturbare e attende se audio o conferma FIDO sono occupati. Il conteggio non sopravvive al riavvio. Timer e lavoro in corso continuano uscendo dalla schermata.

Slide: aprire con USB dati collegata al computer. A precedente, B successivo; i due cerchi touch fanno lo stesso. Oscura / riprendi invia la lettera B; è una scorciatoia, non una lettura dello stato del programma. A lungo/Home esce e rilascia i tasti. B lungo conserva la registrazione delle note e sospende l'USB. Il tempo mostrato parte all'ingresso nella schermata. Con USB collegata il display rimane visibile e si attenua dopo 20 secondi, così il primo comando non viene perso per riaccenderlo. Companion → Impostazioni → Presentazioni sceglie frecce o Pagina su/giù. Non serve tenere il telefono collegato in questa app; il controllo remoto del mouse rimane separato in Ponte.

Casa/Spotify mostrano l'icona della connessione effettiva (Wi-Fi diretto, ponte iPhone o collegamento barrato). Un dato precedente rimane leggibile durante una lettura o un errore; l'etichetta Ultimo stato indica che non è confermato. Le protezioni su età dello stato, correlazione e mancato reinvio dei comandi restano attive.


## Correzione dello scorrimento Home — 0.17.1

Le icone mantengono le posizioni approvate durante lo scorrimento, con la sola variazione del raggio (massimo 3 pixel, 180 ms); sono state rimosse le traiettorie che facevano incrociare i cerchi. Il disegno della Home registra se serve ancora un fotogramma finale: anche se un rendering supera la scadenza, viene mostrata una schermata assestata prima di interrompere l'animazione. Nessuno stato di rotazione viene persistito o trasferito al Companion.

La regressione si verifica con `tests/home_animation_test.py`: disegni di produzione a 0/90/179/180/220 ms nei tre gruppi, entrambe le direzioni, controllo di posizioni, separazione dei cerchi e presenza dei testi. `tests/scene_test.py` controlla anche completamento della sequenza quando il rendering supera 180 ms e rollover del contatore.
