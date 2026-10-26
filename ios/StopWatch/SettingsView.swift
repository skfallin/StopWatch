import SwiftUI

struct PairingView: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Il tuo StopWatch, collegato.").font(.title2.weight(.medium))
                            .fixedSize(horizontal: false, vertical: true)
                        Text("Inserisci il codice al primo abbinamento. Poi l'app ritrova il tuo dispositivo.")
                            .font(.body).foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }.padding(.vertical, 6)
                }
                Section("Dispositivi vicini") {
                    if connection.devices.isEmpty {
                        HStack(spacing: 12) {
                            if connection.phase == .scanning { ProgressView() }
                            Text(connection.preview ? "L'anteprima non usa la radio Bluetooth." : connection.phase.title)
                                .foregroundStyle(.secondary)
                        }.padding(.vertical, 8)
                    }
                    ForEach(connection.devices) { device in
                        Button { connection.connect(device.id) } label: {
                            HStack { Label(device.name, systemImage: "stopwatch"); Spacer(); Image(systemName: "arrow.up.right") }
                                .frame(minHeight: 44)
                        }
                        .disabled(connection.phase == .connecting || connection.phase == .authorizing)
                    }
                    if connection.hasRememberedDevice {
                        Button("Ricollega il dispositivo memorizzato") { connection.reconnect() }.frame(minHeight: 44)
                    }
                    Button("Cerca StopWatch") { connection.scan() }.frame(minHeight: 44)
                        .disabled(connection.preview || connection.phase == .authorizing || connection.phase == .connecting)
                }
                Section("La prima volta") {
                    Label("Sul dispositivo apri iPhone Bluetooth.", systemImage: "1.circle")
                    Label("Premi B per mostrare il codice a sei cifre.", systemImage: "2.circle")
                    Label("Scegli StopWatch qui sopra. Inserisci il codice nella richiesta di iOS.", systemImage: "3.circle")
                }
                if let error = connection.error { Section { Text(error).foregroundStyle(.red) } }
                Section {
                    Text("La riconnessione avviene quando Bluetooth è attivo e il dispositivo è vicino. iOS gestisce le attività in background: l'app non può garantire una connessione continua.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Collega StopWatch").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Fine") { dismiss() } } }
            .onDisappear { connection.stopScanning() }
            .task { if connection.phase == .idle { connection.scan() } }
            .onChange(of: connection.ready) { _, connected in if connected { dismiss() } }
        }
        .preferredColorScheme(.light).tint(.primary)
    }
}

struct SettingsSheet: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.dismiss) private var dismiss
    @State private var forgetting = false

    var body: some View {
        NavigationStack {
            List {
                Section {
                    LabeledContent("StopWatch", value: connection.phase.title)
                    NavigationLink { PairingView() } label: { Label("Abbinamento iPhone", systemImage: "link") }
                }
                Section("Configurazione") {
                    NavigationLink { RelaySettingsView() } label: {
                        Label("Internet tramite iPhone", systemImage: "iphone.radiowaves.left.and.right")
                    }
                    if let configuration = connection.configuration {
                        ForEach(SettingsPart.allCases) { part in
                            NavigationLink { ConfigurationEditor(part: part, configuration: configuration) } label: {
                                Label(part.title, systemImage: part.symbol).frame(minHeight: 34)
                            }
                            .disabled(!connection.ready || connection.working || (part == .badge && connection.status?.badge == nil))
                        }
                    } else {
                        Text("Collega il dispositivo per leggere le impostazioni salvate.").foregroundStyle(.secondary)
                    }
                    NavigationLink { DiagnosticsView() } label: { Label("Diagnostica", systemImage: "heart.text.square") }
                }
                Section {
                    Text("Le credenziali di Wi-Fi e MQTT rimangono sullo StopWatch. Il ponte Internet conserva il token Home Assistant nel portachiavi protetto di questo iPhone.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                if connection.hasRememberedDevice {
                    Section {
                        Button("Dimentica su questa app", role: .destructive) { forgetting = true }
                    } footer: {
                        Text("Per revocare le chiavi anche sullo StopWatch apri iPhone Bluetooth → Rimuovi sul dispositivo. Poi dissocia StopWatch nelle impostazioni Bluetooth di iPhone.")
                    }
                }
            }
            .navigationTitle("Impostazioni").navigationBarTitleDisplayMode(.large)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Fine") { dismiss() } } }
            .confirmationDialog("Dimenticare questo StopWatch nell'app?", isPresented: $forgetting) {
                Button("Dimentica", role: .destructive) { connection.forget() }
            } message: { Text("Dovrai selezionarlo di nuovo. Le chiavi Bluetooth di sistema e le note sul dispositivo vengono conservate.") }
        }
        .preferredColorScheme(.light).tint(.primary)
    }
}

struct RelaySettingsView: View {
    @EnvironmentObject private var connection: WatchConnection
    @State private var url = ""
    @State private var token = ""
    @State private var enabled = true
    @State private var saving = false
    @State private var result: String?

    var body: some View {
        Form {
            Section {
                Toggle("Usa i dati dell’iPhone", isOn: $enabled)
                Text("Lo StopWatch preferisce il Wi-Fi diretto. Quando non raggiunge Mosquitto, Casa e Spotify passano dal Companion via Bluetooth.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            Section("Accesso remoto a Home Assistant") {
                TextField("https://casa.example.it", text: $url)
                    .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                SecureField(connection.relayConfigured ? "Token già nel portachiavi" : "Token Home Assistant", text: $token)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                Text("Serve un indirizzo raggiungibile anche in 4G/5G e un token amministratore. La configurazione HTTPS già presente sullo StopWatch viene importata sul collegamento Bluetooth abbinato; il token resta nel portachiavi di questo iPhone.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            Section {
                Button {
                    saving = true; result = nil
                    Task {
                        do {
                            try await connection.saveRelay(url: url, token: token, enabled: enabled)
                            token = ""; result = enabled ? "Ponte salvato e verificato." : "Ponte disattivato."
                        } catch { result = error.localizedDescription }
                        saving = false
                    }
                } label: { HStack { Text("Salva e verifica"); Spacer(); if saving { ProgressView() } } }
                    .disabled(saving || !connection.ready || connection.status?.relaySupported != true)
                Text(connection.relayMessage).font(.footnote).foregroundStyle(.secondary)
                if let result { Text(result).font(.subheadline) }
                if let activity = connection.relayLastActivity {
                    LabeledContent("Ultimo inoltro", value: activity.success ? "Riuscito" : "Non riuscito")
                    LabeledContent("Esecuzione", value: activity.background ? "In background" : "App aperta")
                    LabeledContent("Tempo", value: String(format: "%.1f s", activity.seconds))
                    Text(activity.date, style: .time).font(.caption).foregroundStyle(.secondary)
                }
            }
            Section {
                Text("Puoi bloccare l’iPhone lasciando l’app abbinata. Dopo un riavvio sbloccalo almeno una volta. iOS gestisce il tempo in background: evita di chiudere forzatamente il Companion. Le note rimangono sul dispositivo anche senza rete.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
        }
        .navigationTitle("Internet tramite iPhone").navigationBarTitleDisplayMode(.inline)
        .toolbar(.visible, for: .navigationBar)
        .task { url = connection.relayURL; enabled = connection.relayConfigured ? connection.relayEnabled : true }
        .onChange(of: connection.relayURL) { _, value in if url.isEmpty { url = value } }
        .interactiveDismissDisabled(saving)
    }
}

struct ConfigurationEditor: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.dismiss) private var dismiss
    let part: SettingsPart
    @State private var draft: DeviceConfiguration
    @State private var saving = false
    @State private var error: String?

    init(part: SettingsPart, configuration: DeviceConfiguration) {
        self.part = part
        _draft = State(initialValue: configuration)
    }

    var body: some View {
        Form {
            if part == .device { deviceFields }
            if part == .wifi { wifiFields }
            if part == .home { homeFields }
            if part == .mqtt { mqttFields }
            if part == .badge { badgeFields }
            if part == .clock { clockFields }
            if let error { Section { Label(error, systemImage: "exclamationmark.circle").foregroundStyle(.red) } }
        }
        .navigationTitle(part == .mqtt ? "MQTT" : part.title).navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    saving = true; error = nil
                    Task {
                        do { try await connection.save(draft, part: part); dismiss() }
                        catch { self.error = error.localizedDescription }
                        saving = false
                    }
                } label: { if saving { ProgressView() } else { Text("Salva").fontWeight(.semibold) } }
                    .disabled(saving || !connection.ready || connection.working)
            }
        }
        .interactiveDismissDisabled(saving)
    }

    private func numberBinding(_ path: WritableKeyPath<DeviceConfiguration, Int>) -> Binding<Double> {
        Binding(get: { Double(draft[keyPath: path]) }, set: { draft[keyPath: path] = Int($0) })
    }

    private var deviceFields: some View {
        Group {
            Section("Display") {
                Picker("Colori", selection: $draft.colorTheme) {
                    ForEach(Array(zip(["lime","buttons","orange","violet","cyan","rose"], ["Lime","Giallo / Blu","Arancio","Viola","Ciano","Rosa"])), id: \.0) { id, name in Text(name).tag(id) }
                }
                LabeledContent("Luminosità", value: "\(draft.brightness * 100 / 255)%")
                Slider(value: numberBinding(\.brightness), in: 16...255, step: 1).accessibilityLabel("Luminosità")
                Stepper("Si spegne dopo \(draft.screenTimeout) secondi", value: $draft.screenTimeout, in: 15...300, step: 15)
            }
            Section("Feedback") {
                Picker("Pacchetto suoni", selection: $draft.soundPack) {
                    ForEach(Array(zip(["off","minimal","soft","glass","arcade","mechanical","organic","dreamy","scifi","rubber","cinematic","studio","zen"],
                                      ["Silenzioso","Minimal","Soft","Glass","Arcade","Mechanical","Organic","Dreamy","Sci-fi","Rubber","Cinematic","Studio","Zen"])), id: \.0) { id, name in Text(name).tag(id) }
                }
                LabeledContent("Volume interazioni", value: "\(draft.volume * 100 / 200)%")
                Slider(value: numberBinding(\.volume), in: 0...200, step: 1).accessibilityLabel("Volume interazioni")
                LabeledContent("Vibrazione", value: "\(draft.vibration * 100 / 255)%")
                Slider(value: numberBinding(\.vibration), in: 0...255, step: 1).accessibilityLabel("Intensità vibrazione")
            }
            Section {
                LabeledContent("Volume note vocali", value: "\(draft.noteVolume * 100 / 200)%")
                Slider(value: numberBinding(\.noteVolume), in: 0...200, step: 1).accessibilityLabel("Volume ascolto note sullo StopWatch")
            } header: { Text("Ascolto sullo StopWatch") } footer: {
                Text("Regola l'ascolto delle note sul dispositivo. Non modifica il microfono né l'audio registrato. Sull'iPhone si usa il volume del telefono.")
            }
        }
    }

    private var wifiFields: some View {
        Section {
            TextField("Nome rete (SSID)", text: $draft.ssid).textContentType(.none)
            SecureField(draft.wifiPasswordSaved ? "Password già salvata" : "Password Wi-Fi", text: $draft.wifiPassword).textContentType(.password)
        } header: { Text("Rete a 2,4 GHz") } footer: {
            Text("Con lo stesso SSID, la password vuota conserva quella salvata. Per una nuova rete aperta lascia la password vuota. Svuota il nome rete per disconnettere il Wi-Fi.")
        }
        .textInputAutocapitalization(.never).autocorrectionDisabled()
    }

    private var badgeFields: some View {
        Group {
            Section("Immagine e movimento") {
                Toggle("Rotazione automatica", isOn: $draft.badge.autoRotate)
                Toggle("Riproduci GIF", isOn: $draft.badge.animate)
            }
            Section {
                Toggle("Display sempre acceso nel Badge", isOn: $draft.badge.keepAwake)
            } footer: {
                Text("Disattivato: segue il timeout del dispositivo. Attivato: resta visibile e abbassa la luminosità dopo 20 secondi di inattività. Foto ferme non vengono ridisegnate; i sensori rallentano quando il dispositivo è fermo.")
            }
        }
    }

    private var clockFields: some View {
        Group {
            Section("Quadrante") {
                Picker("Stile", selection: $draft.clockFace) {
                    ForEach(Array(zip(["digital", "analog", "halo", "duo", "orbit", "segments"],
                                      ["Digitale", "Analogico", "Halo", "Doppio", "Orbita", "Segmenti"])), id: \.0) { id, name in
                        Text(name).tag(id)
                    }
                }
            }
            Section {
                Toggle("Display sempre acceso nell’orologio", isOn: $draft.clockKeepAwake)
            } footer: {
                Text("Quando apri Orologio, il quadrante resta visibile e la luminosità si abbassa dopo 20 secondi senza interazioni. Timer e cronometro seguono il normale timeout.")
            }
        }
    }

    private var homeFields: some View {
        Group {
            Section {
                TextField("https://casa.example.it", text: $draft.haUrl).keyboardType(.URL)
                SecureField(draft.haTokenSaved ? "Token già salvato" : "Token di accesso a lunga durata", text: $draft.haToken)
            } header: { Text("Controlla casa dallo StopWatch") } footer: {
                Text("Un dominio con certificato pubblico valido funziona senza aggiungere una CA. L'indirizzo vuoto disattiva i comandi REST e rimuove il token salvato.")
            }
            .textInputAutocapitalization(.never).autocorrectionDisabled()
            Section("Preferiti · \(draft.favorites.count)/8") {
                ForEach($draft.favorites) { $favorite in
                    VStack(alignment: .leading, spacing: 8) {
                        TextField("Nome", text: $favorite.label)
                        TextField("light.studio", text: $favorite.entity).font(.subheadline.monospaced())
                            .textInputAutocapitalization(.never).autocorrectionDisabled()
                    }.padding(.vertical, 5)
                }
                .onDelete { draft.favorites.remove(atOffsets: $0) }
                Button { draft.favorites.append(Favorite(label: "", entity: "")) } label: {
                    Label("Aggiungi preferito", systemImage: "plus")
                }.disabled(draft.favorites.count >= 8)
            }
            Section {
                DisclosureGroup("CA privata · facoltativa") {
                    TextEditor(text: $draft.haCa).font(.caption.monospaced()).frame(minHeight: 140).accessibilityLabel("Certificato CA Home Assistant in formato PEM")
                }
            }
        }
    }

    private var mqttFields: some View {
        Group {
            Section {
                TextField("Indirizzo broker, es. 192.168.1.10", text: $draft.mqttHost)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                LabeledContent("Porta") { TextField("1883", value: $draft.mqttPort, format: .number.grouping(.never)).keyboardType(.numberPad).multilineTextAlignment(.trailing) }
                Toggle("Connessione TLS", isOn: $draft.mqttTls)
                    .onChange(of: draft.mqttTls) { old, new in
                        if new && draft.mqttPort == 1883 { draft.mqttPort = 8883 }
                        if old && !new && draft.mqttPort == 8883 { draft.mqttPort = 1883 }
                    }
                TextField("Utente Mosquitto", text: $draft.mqttUser).textInputAutocapitalization(.never).autocorrectionDisabled()
                SecureField(draft.mqttPasswordSaved ? "Password già salvata" : "Password Mosquitto", text: $draft.mqttPassword)
            } header: { Text("Collega StopWatch a Home Assistant") } footer: {
                Text("Usa il broker già collegato a Home Assistant. La discovery MQTT crea automaticamente dispositivo, sensori e comandi. Svuota il broker per disattivare il collegamento. TCP senza TLS è adatto alla tua rete locale fidata.")
            }
            if draft.mqttTls {
                Section {
                    DisclosureGroup("CA privata · facoltativa") {
                        TextEditor(text: $draft.mqttCa).font(.caption.monospaced()).frame(minHeight: 140).accessibilityLabel("Certificato CA MQTT in formato PEM")
                    }
                } footer: { Text("Lascia vuota la CA con un certificato pubblico valido. Nome del broker e certificato devono corrispondere.") }
            }
        }
    }
}

struct DiagnosticsView: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                if let status = connection.status {
                    Section("Collegamenti") {
                        LabeledContent("Bluetooth", value: connection.phase.title)
                        LabeledContent("Wi-Fi", value: status.wifi ? status.ip : "Non connesso")
                        if let rssi = status.rssi { LabeledContent("Segnale", value: "\(rssi) dBm") }
                        LabeledContent("Home Assistant MQTT", value: status.mqttConnected ? "Connesso" : "Non connesso")
                        LabeledContent("iPhone abbinati", value: "\(status.bondedPhones)")
                    }
                    Section("Hardware rilevato") {
                        LabeledContent("Microfono", value: status.microphone ? "Configurato" : "Non disponibile")
                        LabeledContent("Movimento / IMU", value: status.imu ? "Rilevato" : "Non disponibile")
                        if status.imu, status.acceleration.count == 3 {
                            LabeledContent("Accelerazione", value: status.acceleration.map { String(format: "%.2f", $0) }.joined(separator: " · "))
                        }
                        LabeledContent("Orologio RTC", value: status.rtc ? "Rilevato" : "Non disponibile")
                        LabeledContent("Touch", value: status.touch ? "Configurato" : "Non disponibile")
                        LabeledContent("Pulsante A", value: status.buttonA ? "Premuto" : "Rilasciato")
                        LabeledContent("Pulsante B", value: status.buttonB ? "Premuto" : "Rilasciato")
                    }
                    Section("Archivio") {
                        LabeledContent("Note", value: "\(status.notes)")
                        LabeledContent("LittleFS", value: status.storageReady ? ByteCountFormatter.string(fromByteCount: Int64(status.storageUsed), countStyle: .file) + " usati" : "Da inizializzare sul dispositivo")
                        LabeledContent("PSRAM libera", value: ByteCountFormatter.string(fromByteCount: Int64(status.psramFree), countStyle: .memory))
                        LabeledContent("Registrazione", value: status.recording ? "In corso" : "In attesa")
                    }
                    Section("Prova sul dispositivo") {
                        Button("Vibrazione") { connection.output("vibrate") }
                        Button("Tono altoparlante") { connection.output("beep") }
                        Button("Accendi / spegni LED") { connection.output("led") }
                    }.disabled(connection.working || status.recording)
                } else {
                    ContentUnavailableView("Collega StopWatch", systemImage: "link", description: Text("La diagnostica mostra le letture reali del dispositivo connesso."))
                }
                Section {
                    Text("Rilevare un componente non ne conferma il funzionamento fisico. Per touch, display, pulsanti e audio usa anche Diagnostica sullo StopWatch.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Diagnostica").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Fine") { dismiss() } } }
            .refreshable { await connection.refresh() }
        }
        .preferredColorScheme(.light).tint(.primary)
    }
}
