# Home Assistant e Spotify

[English](README.md)

Integrazioni facoltative: servono Home Assistant, broker MQTT e account Spotify propri. Non inserire credenziali nei sorgenti. I blueprint dichiarano Home Assistant 2024.10 come base sintattica; verifica il comportamento sulla tua versione.

1. Configura MQTT in Home Assistant e inserisci host, porta e credenziali dal Companion. Fuori da una LAN fidata usa TLS. La discovery crea dispositivo, sensori e comandi.
2. Copia `blueprints/stopwatch_favorites.it.yaml` in `config/blueprints/automation/stopwatch/` di Home Assistant, ricarica i blueprint e crea un’automazione. Seleziona StopWatch e fino a otto entità; lascia vuote le posizioni inutilizzate. La variante senza `.it` è inglese: installa una sola variante per evitare duplicati.
3. Salva e attendi la sincronizzazione. Luci, interruttori e ventole cambiano stato; scene/script vengono eseguiti, pulsanti premuti, altre entità lette. La luminosità delle luci regolabili richiede conferma e verifica dello stato reale.
4. Per Spotify autorizza l’integrazione ufficiale Home Assistant, poi importa `stopwatch_spotify.it.yaml` e crea una sola automazione per StopWatch. Seleziona l’account e, se necessario, l’URL Home Assistant raggiungibile dal dispositivo per le copertine. L’audio resta sul lettore Spotify attivo.

Usa i blueprint della stessa versione del firmware: i selettori accettano sia il modello discovery attuale sia quello precedente, mantenendo ID, topic e selezioni.

Il ponte Internet iPhone è facoltativo: richiede URL HTTPS raggiungibile in rete cellulare e token amministratore, conservato nel portachiavi. Riusa i blueprint attraverso Home Assistant; non esporre MQTT su Internet. Il Wi-Fi diretto ha priorità. iOS limita il background: sblocca dopo il riavvio, evita la chiusura forzata e verifica un comando reale con telefono bloccato.

In caso di errore controlla broker, entità, selettori, certificato e orologio. Se l’esito è incerto, verifica il dispositivo prima di ripetere scene/script. Le integrazioni cloud possono ritardare lo stato. I preferiti REST manuali restano disponibili, ma una sincronizzazione successiva del blueprint può sostituirli.
