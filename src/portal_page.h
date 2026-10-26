#pragma once

const char portalPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>StopWatch · Companion</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#101416;color:#edf3f5}*{box-sizing:border-box}
body{max-width:720px;margin:auto;padding:24px 18px 60px}h1{font-size:2rem;margin-bottom:4px}h2{font-size:1.2rem}
p,small{color:#b8c6cb;line-height:1.5}section{margin:24px 0;padding:20px;background:#1a2327;border-radius:18px}
label{display:block;margin:16px 0 6px}input,textarea,button{font:inherit;border-radius:9px;padding:12px}
input,textarea{width:100%;background:#10191c;color:inherit;border:1px solid #60767f}textarea{min-height:90px}
button{border:0;background:#b8efb8;color:#102314;cursor:pointer;min-height:44px;margin:8px 8px 0 0}
button.secondary{background:#34474f;color:#fff}button.danger{background:#5a2930;color:#fff}
button:disabled{opacity:.5;cursor:wait}button:focus-visible,input:focus-visible,textarea:focus-visible{outline:3px solid #80cfff;outline-offset:3px}
.favorite{border-top:1px solid #43545b;padding:8px 0}.note{border-top:1px solid #43545b;padding:12px 0;overflow-wrap:anywhere}
#message{position:sticky;top:8px;padding:12px;border-radius:8px;background:#253a43;z-index:1;white-space:pre-wrap}
#message:empty{display:none}.grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}@media(max-width:480px){.grid{grid-template-columns:1fr}}
</style></head><body>
<h1>StopWatch</h1><p>Companion locale · Configurazione e note vocali</p>
<div id="message" role="status" aria-live="polite"></div>
<p id="status">Connessione al dispositivo…</p>
<form id="settings"><fieldset id="fields" style="padding:0;border:0" disabled>
<section><h2>Display, suono e vibrazione</h2><div class="grid">
<div><label for="brightness">Luminosità (16–255)</label><input id="brightness" type="number" min="16" max="255" required></div>
<div><label for="volume">Volume interazioni (0–200)</label><input id="volume" type="number" min="0" max="200" required></div>
<div><label for="note_volume">Volume ascolto note (0–200)</label><input id="note_volume" type="number" min="0" max="200" required></div>
<div><label for="vibration">Vibrazione (0–255)</label><input id="vibration" type="number" min="0" max="255" required></div>
<div><label for="screen_timeout">Spegnimento display (secondi)</label><input id="screen_timeout" type="number" min="15" max="300" required></div>
</div></section>
<section><h2>Wi-Fi</h2><label for="ssid">Nome della rete 2,4 GHz</label><input id="ssid" maxlength="32" autocomplete="off">
<label for="wifi_password">Password</label><input id="wifi_password" type="password" maxlength="63" autocomplete="new-password">
<small>Se mantieni la rete e lasci vuoto, conserviamo la password. Per una rete diversa, vuoto indica una rete aperta.</small></section>
<section><h2>Home Assistant</h2><label for="ha_url">Indirizzo del server</label><input id="ha_url" type="url" placeholder="http://192.168.1.20:8123" maxlength="256">
<label for="ha_token">Token di accesso a lunga durata</label><input id="ha_token" type="password" maxlength="2048" autocomplete="new-password">
<small>Crealo nel tuo profilo Home Assistant → Sicurezza. Vuoto conserva il token già salvato. Svuotare l'indirizzo disattiva l'integrazione.</small>
<details><summary style="margin-top:16px">CA privata per HTTPS · facoltativa</summary><label for="ha_ca">CA del server, formato PEM</label><textarea id="ha_ca" maxlength="4096" spellcheck="false"></textarea>
<small>Lascia vuoto con un certificato pubblico valido. Per HTTP usa esclusivamente una rete locale fidata.</small></details>
<h2>Preferiti</h2><p>Fino a 8 entità. Luci, interruttori, input_boolean e ventole: toggle; scene e script: attiva; pulsanti: premi. Le altre entità sono in sola lettura.</p>
<div id="favorites"></div><button type="button" id="add" class="secondary">Aggiungi preferito</button></section>
<button type="submit">Salva sul dispositivo</button>
</fieldset></form>
<section><h2>Note vocali</h2><p>Tieni premuto B sullo StopWatch, parla quando compare REC e rilascia per salvare. Le note sono audio WAV locali; la trascrizione sarà aggiunta dopo la scelta del servizio.</p>
<button id="refresh" type="button" class="secondary">Aggiorna elenco</button><div id="notes"></div></section>
<script>
'use strict';
const $ = id => document.getElementById(id);
let downloading = false, favoriteId = 0;
function message(text) { $('message').textContent = text; }
async function api(path, options = {}) {
  const response = await fetch(path, {cache:'no-store', ...options,
    headers:{'X-StopWatch':'1', ...(options.body ? {'Content-Type':'application/json'} : {}), ...options.headers}});
  if (!response.ok) {
    let text = `Richiesta fallita (${response.status})`;
    try { text = (await response.json()).error || text; } catch (_) {}
    throw new Error(text);
  }
  return response;
}
function addFavorite(value = {label:'',entity:''}) {
  if ($('favorites').children.length >= 8) return;
  const row = document.createElement('div'); row.className = 'favorite';
  for (const [key, label, limit] of [['label','Nome visualizzato',32],['entity','Entity ID',96]]) {
    const id = `favorite-${++favoriteId}-${key}`;
    const caption = document.createElement('label'); caption.htmlFor = id; caption.textContent = label;
    const input = document.createElement('input'); input.id = id; input.dataset.key = key; input.value = value[key]; input.maxLength = limit; input.required = true;
    row.append(caption,input);
  }
  const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'danger'; remove.textContent = 'Rimuovi preferito';
  remove.onclick = () => row.remove(); row.append(remove); $('favorites').append(row);
}
$('add').onclick = () => addFavorite();
$('settings').onsubmit = async event => {
  event.preventDefault(); const config = {};
  for (const key of ['brightness','volume','note_volume','vibration','screen_timeout']) config[key] = Number($(key).value);
  for (const key of ['ssid','wifi_password','ha_url','ha_token','ha_ca']) config[key] = $(key).value;
  config.favorites = [...$('favorites').children].map(row => Object.fromEntries([...row.querySelectorAll('input')].map(input => [input.dataset.key,input.value])));
  $('fields').disabled = true;
  try { await api('/api/config',{method:'PUT',body:JSON.stringify(config)}); $('wifi_password').value = ''; $('ha_token').value = ''; message('Configurazione salvata. Se hai cambiato rete, attendi la connessione sullo StopWatch.'); }
  catch (error) { message(error.message); }
  finally { $('fields').disabled = false; }
};
async function loadNotes() {
  const data = await (await api('/api/notes')).json(); $('notes').replaceChildren();
  if (!data.notes.length) $('notes').textContent = 'Nessuna nota salvata.';
  for (const note of data.notes) {
    const row = document.createElement('div'); row.className = 'note';
    const label = document.createElement('div'); label.textContent = `${note.name} · ${Math.round(note.seconds)} s`;
    const download = document.createElement('button'); download.type = 'button'; download.textContent = 'Scarica WAV';
    download.onclick = async () => {
      if (downloading) return; downloading = true; download.disabled = true;
      try {
        const chunks = []; let offset = 0;
        while (offset < note.bytes) {
          const response = await api(`/api/note?name=${encodeURIComponent(note.name)}&offset=${offset}`);
          const chunk = await response.arrayBuffer(); if (!chunk.byteLength) throw new Error('Download interrotto');
          chunks.push(chunk); offset += chunk.byteLength;
        }
        if (offset !== note.bytes) throw new Error('Dimensione del file inattesa');
        const url = URL.createObjectURL(new Blob(chunks,{type:'audio/wav'}));
        const link = document.createElement('a'); link.href = url; link.download = note.name; link.textContent = 'Apri / salva audio';
        row.append(link); link.click(); setTimeout(() => { URL.revokeObjectURL(url); link.remove(); }, 300000);
        message('Audio scaricato. La copia sullo StopWatch è ancora presente.');
      } catch (error) { message(error.message); } finally { downloading = false; download.disabled = false; }
    };
    const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'danger'; remove.textContent = 'Elimina';
    remove.onclick = async () => {
      if (downloading || !confirm(`Eliminare ${note.name} dal dispositivo? Scarica prima una copia.`)) return;
      try { await api('/api/notes',{method:'DELETE',body:JSON.stringify({name:note.name})}); await loadNotes(); }
      catch (error) { message(error.message); }
    };
    row.append(label,download,remove); $('notes').append(row);
  }
}
$('refresh').onclick = () => loadNotes().catch(error => message(error.message));
async function start() {
  try {
    const config = await (await api('/api/config')).json();
    for (const key of ['brightness','volume','note_volume','vibration','screen_timeout','ssid','ha_url','ha_ca']) $(key).value = config[key];
    $('wifi_password').placeholder = config.wifi_password_saved ? 'Password salvata' : 'Nessuna password salvata';
    $('ha_token').placeholder = config.ha_token_saved ? 'Token salvato' : 'Inserisci il token';
    for (const favorite of config.favorites) addFavorite(favorite);
    $('fields').disabled = false;
    await api('/api/time',{method:'PUT',body:JSON.stringify({epoch:Math.floor(Date.now()/1000)})});
    const status = await (await api('/api/status')).json();
    $('status').textContent = `Wi-Fi: ${status.wifi ? 'connesso' : 'non connesso'} · Archivio: ${status.storage_ready ? Math.round(status.used/1024)+' / '+Math.round(status.total/1024)+' KB' : 'da inizializzare in Diagnostica → Archivio'}`;
    await loadNotes();
  } catch (error) { message(error.message); }
}
start();
</script></body></html>)HTML";
