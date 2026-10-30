import SwiftUI

struct PairingView: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 12) {
                        Text(String(localized: "Il tuo StopWatch, collegato.", bundle: AppLocalization.bundle)).font(.title2.weight(.medium))
                            .fixedSize(horizontal: false, vertical: true)
                        Text(String(localized: "Inserisci il codice al primo abbinamento. Poi l'app ritrova il tuo dispositivo.", bundle: AppLocalization.bundle))
                            .font(.body).foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }.padding(.vertical, 6)
                }
                Section(String(localized: "Dispositivi vicini", bundle: AppLocalization.bundle)) {
                    if connection.devices.isEmpty {
                        HStack(spacing: 12) {
                            if connection.phase == .scanning { ProgressView() }
                            Text(connection.preview ? String(localized: "L'anteprima non usa la radio Bluetooth.", bundle: AppLocalization.bundle) : connection.phase.title)
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
                        Button(String(localized: "Ricollega il dispositivo memorizzato", bundle: AppLocalization.bundle)) { connection.reconnect() }.frame(minHeight: 44)
                    }
                    Button(String(localized: "Cerca StopWatch", bundle: AppLocalization.bundle)) { connection.scan() }.frame(minHeight: 44)
                        .disabled(connection.preview || connection.phase == .authorizing || connection.phase == .connecting)
                }
                Section(String(localized: "La prima volta", bundle: AppLocalization.bundle)) {
                    Label(String(localized: "Sul dispositivo apri iPhone Bluetooth.", bundle: AppLocalization.bundle), systemImage: "1.circle")
                    Label(String(localized: "Premi B per mostrare il codice a sei cifre.", bundle: AppLocalization.bundle), systemImage: "2.circle")
                    Label(String(localized: "Scegli StopWatch qui sopra. Inserisci il codice nella richiesta di iOS.", bundle: AppLocalization.bundle), systemImage: "3.circle")
                }
                if let error = connection.error { Section { Text(error).foregroundStyle(.red) } }
                Section {
                    Text(String(localized: "La riconnessione avviene quando Bluetooth è attivo e il dispositivo è vicino. iOS gestisce le attività in background: l'app non può garantire una connessione continua.", bundle: AppLocalization.bundle))
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle(String(localized: "Collega StopWatch", bundle: AppLocalization.bundle)).navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button(String(localized: "Fine", bundle: AppLocalization.bundle)) { dismiss() } } }
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
    @AppStorage("transcriptionLanguage") private var transcriptionLanguage = "en-US"

    var body: some View {
        NavigationStack {
            List {
                Section {
                    LabeledContent("StopWatch", value: connection.phase.title)
                    NavigationLink { PairingView() } label: { Label(String(localized: "Abbinamento iPhone", bundle: AppLocalization.bundle), systemImage: "link") }
                }
                Section(String(localized: "Lingua", bundle: AppLocalization.bundle)) {
                    Text(String(localized: "La lingua dell’app segue le impostazioni iOS. La lingua dello StopWatch si sceglie in Lingua e fuso orario.", bundle: AppLocalization.bundle))
                    Picker(String(localized: "Lingua della trascrizione", bundle: AppLocalization.bundle), selection: $transcriptionLanguage) {
                        Text("English (US)").tag("en-US")
                        Text("English (UK)").tag("en-GB")
                        Text("Italiano").tag("it-IT")
                    }
                    Text(String(localized: "Il modello deve supportare la trascrizione sul dispositivo. L’audio non viene inviato al cloud.", bundle: AppLocalization.bundle))
                }
                Section(String(localized: "Configurazione", bundle: AppLocalization.bundle)) {
                    NavigationLink { RelaySettingsView() } label: {
                        Label(String(localized: "Internet tramite iPhone", bundle: AppLocalization.bundle), systemImage: "iphone.radiowaves.left.and.right")
                    }
                    if let configuration = connection.configuration {
                        ForEach(SettingsPart.allCases) { part in
                            NavigationLink { ConfigurationEditor(part: part, configuration: configuration) } label: {
                                Label(part.title, systemImage: part.symbol).frame(minHeight: 34)
                            }
                            .disabled(!connection.ready || connection.working || (part == .badge && connection.status?.badge == nil))
                        }
                    } else {
                        Text(String(localized: "Collega il dispositivo per leggere le impostazioni salvate.", bundle: AppLocalization.bundle)).foregroundStyle(.secondary)
                    }
                    NavigationLink { DiagnosticsView() } label: { Label(String(localized: "Diagnostica", bundle: AppLocalization.bundle), systemImage: "heart.text.square") }
                }
                Section {
                    Text(String(localized: "Le credenziali di Wi-Fi e MQTT rimangono sullo StopWatch. Il ponte Internet conserva il token Home Assistant nel portachiavi protetto di questo iPhone.", bundle: AppLocalization.bundle))
                        .font(.footnote).foregroundStyle(.secondary)
                }
                if connection.hasRememberedDevice {
                    Section {
                        Button(String(localized: "Dimentica su questa app", bundle: AppLocalization.bundle), role: .destructive) { forgetting = true }
                    } footer: {
                        Text(String(localized: "Per revocare le chiavi anche sullo StopWatch apri iPhone Bluetooth → Rimuovi sul dispositivo. Poi dissocia StopWatch nelle impostazioni Bluetooth di iPhone.", bundle: AppLocalization.bundle))
                    }
                }
            }
            .navigationTitle(String(localized: "Impostazioni", bundle: AppLocalization.bundle)).navigationBarTitleDisplayMode(.large)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button(String(localized: "Fine", bundle: AppLocalization.bundle)) { dismiss() } } }
            .confirmationDialog(String(localized: "Dimenticare questo StopWatch nell'app?", bundle: AppLocalization.bundle), isPresented: $forgetting) {
                Button(String(localized: "Dimentica", bundle: AppLocalization.bundle), role: .destructive) { connection.forget() }
            } message: { Text(String(localized: "Dovrai selezionarlo di nuovo. Le chiavi Bluetooth di sistema e le note sul dispositivo vengono conservate.", bundle: AppLocalization.bundle)) }
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
                Toggle(String(localized: "Usa i dati dell’iPhone", bundle: AppLocalization.bundle), isOn: $enabled)
                Text(String(localized: "Lo StopWatch preferisce il Wi-Fi diretto. Quando non raggiunge Mosquitto, Casa e Spotify passano dal Companion via Bluetooth.", bundle: AppLocalization.bundle))
                    .font(.footnote).foregroundStyle(.secondary)
            }
            Section(String(localized: "Accesso remoto a Home Assistant", bundle: AppLocalization.bundle)) {
                TextField("https://home.example.com", text: $url)
                    .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                SecureField(connection.relayConfigured ? String(localized: "Token già nel portachiavi", bundle: AppLocalization.bundle) : String(localized: "Token Home Assistant", bundle: AppLocalization.bundle), text: $token)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                Text(String(localized: "Serve un indirizzo raggiungibile anche in 4G/5G e un token amministratore. La configurazione HTTPS già presente sullo StopWatch viene importata sul collegamento Bluetooth abbinato; il token resta nel portachiavi di questo iPhone.", bundle: AppLocalization.bundle))
                    .font(.footnote).foregroundStyle(.secondary)
            }
            Section {
                Button {
                    saving = true; result = nil
                    Task {
                        do {
                            try await connection.saveRelay(url: url, token: token, enabled: enabled)
                            token = ""; result = enabled ? String(localized: "Ponte salvato e verificato.", bundle: AppLocalization.bundle) : String(localized: "Ponte disattivato.", bundle: AppLocalization.bundle)
                        } catch { result = error.localizedDescription }
                        saving = false
                    }
                } label: { HStack { Text(String(localized: "Salva e verifica", bundle: AppLocalization.bundle)); Spacer(); if saving { ProgressView() } } }
                    .disabled(saving || !connection.ready || connection.status?.relaySupported != true)
                Text(connection.relayMessage).font(.footnote).foregroundStyle(.secondary)
                if let result { Text(result).font(.subheadline) }
                if let activity = connection.relayLastActivity {
                    LabeledContent(String(localized: "Ultimo inoltro", bundle: AppLocalization.bundle), value: activity.success ? String(localized: "Riuscito", bundle: AppLocalization.bundle) : String(localized: "Non riuscito", bundle: AppLocalization.bundle))
                    LabeledContent(String(localized: "Esecuzione", bundle: AppLocalization.bundle), value: activity.background ? String(localized: "In background", bundle: AppLocalization.bundle) : String(localized: "App aperta", bundle: AppLocalization.bundle))
                    LabeledContent(String(localized: "Tempo", bundle: AppLocalization.bundle), value: String(format: "%.1f s", activity.seconds))
                    Text(activity.date, style: .time).font(.caption).foregroundStyle(.secondary)
                }
            }
            Section {
                Text(String(localized: "Puoi bloccare l’iPhone lasciando l’app abbinata. Dopo un riavvio sbloccalo almeno una volta. iOS gestisce il tempo in background: evita di chiudere forzatamente il Companion. Le note rimangono sul dispositivo anche senza rete.", bundle: AppLocalization.bundle))
                    .font(.footnote).foregroundStyle(.secondary)
            }
        }
        .navigationTitle(String(localized: "Internet tramite iPhone", bundle: AppLocalization.bundle)).navigationBarTitleDisplayMode(.inline)
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
            if part == .regional { regionalFields }
            if part == .device { deviceFields }
            if part == .wifi { wifiFields }
            if part == .home { homeFields }
            if part == .mqtt { mqttFields }
            if part == .badge { badgeFields }
            if part == .clock { clockFields }
            if part == .launcher { launcherFields }
            if part == .presentation { presentationFields }
            if let error { Section { Label(error, systemImage: "exclamationmark.circle").foregroundStyle(.red) } }
        }
        .navigationTitle(part == .mqtt ? "MQTT" : part.title).navigationBarTitleDisplayMode(.inline)
        .toolbar {
            if part == .launcher { ToolbarItem(placement: .topBarLeading) { EditButton() } }
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    saving = true; error = nil
                    Task {
                        do { try await connection.save(draft, part: part); dismiss() }
                        catch { self.error = error.localizedDescription }
                        saving = false
                    }
                } label: { if saving { ProgressView() } else { Text(String(localized: "Salva", bundle: AppLocalization.bundle)).fontWeight(.semibold) } }
                    .disabled(saving || !connection.ready || connection.working)
            }
        }
        .interactiveDismissDisabled(saving)
    }

    private func numberBinding(_ path: WritableKeyPath<DeviceConfiguration, Int>) -> Binding<Double> {
        Binding(get: { Double(draft[keyPath: path]) }, set: { draft[keyPath: path] = Int($0) })
    }

    private var regionalFields: some View {
        Section("StopWatch") {
            if draft.timeZones.isEmpty {
                Text(String(localized: "Aggiorna il firmware per scegliere lingua e fuso orario.", bundle: AppLocalization.bundle))
            } else {
                Picker(String(localized: "Lingua", bundle: AppLocalization.bundle), selection: $draft.language) {
                    Text("English").tag("en")
                    Text("Italiano").tag("it")
                }
                Picker(String(localized: "Fuso orario", bundle: AppLocalization.bundle), selection: $draft.timeZone) {
                    ForEach(draft.timeZones, id: \.self) { Text($0).tag($0) }
                }
                Text(String(localized: "L’ora legale viene applicata automaticamente nei fusi che la prevedono. I dati salvati non cambiano.", bundle: AppLocalization.bundle))
            }
        }
    }

    private var deviceFields: some View {
        Group {
            Section("Display") {
                Picker(String(localized: "Colori", bundle: AppLocalization.bundle), selection: $draft.colorTheme) {
                    ForEach(Array(zip(["lime","buttons","orange","violet","cyan","rose"], ["Lime",String(localized: "Giallo / Blu", bundle: AppLocalization.bundle),String(localized: "Arancio", bundle: AppLocalization.bundle),String(localized: "Viola", bundle: AppLocalization.bundle),String(localized: "Ciano", bundle: AppLocalization.bundle),String(localized: "Rosa", bundle: AppLocalization.bundle)])), id: \.0) { id, name in Text(name).tag(id) }
                }
                LabeledContent(String(localized: "Luminosità", bundle: AppLocalization.bundle), value: "\(draft.brightness * 100 / 255)%")
                Slider(value: numberBinding(\.brightness), in: 16...255, step: 1).accessibilityLabel(String(localized: "Luminosità", bundle: AppLocalization.bundle))
                Stepper(String(localized: "Si spegne dopo \(draft.screenTimeout) secondi", bundle: AppLocalization.bundle), value: $draft.screenTimeout, in: 15...300, step: 15)
            }
            Section("Feedback") {
                Picker(String(localized: "Pacchetto suoni", bundle: AppLocalization.bundle), selection: $draft.soundPack) {
                    Text(String(localized: "Silenzioso", bundle: AppLocalization.bundle)).tag("off")
                    Text("Microclick").tag("microclick")
                }
                LabeledContent(String(localized: "Volume interazioni", bundle: AppLocalization.bundle), value: "\(draft.volume * 100 / 200)%")
                Slider(value: numberBinding(\.volume), in: 0...200, step: 1).accessibilityLabel(String(localized: "Volume interazioni", bundle: AppLocalization.bundle))
                LabeledContent(String(localized: "Vibrazione", bundle: AppLocalization.bundle), value: "\(draft.vibration * 100 / 255)%")
                Slider(value: numberBinding(\.vibration), in: 0...255, step: 1).accessibilityLabel(String(localized: "Intensità vibrazione", bundle: AppLocalization.bundle))
            }
            Section {
                LabeledContent(String(localized: "Volume note vocali", bundle: AppLocalization.bundle), value: "\(draft.noteVolume * 100 / 200)%")
                Slider(value: numberBinding(\.noteVolume), in: 0...200, step: 1).accessibilityLabel(String(localized: "Volume ascolto note sullo StopWatch", bundle: AppLocalization.bundle))
            } header: { Text(String(localized: "Ascolto sullo StopWatch", bundle: AppLocalization.bundle)) } footer: {
                Text(String(localized: "Regola l'ascolto delle note sul dispositivo. Non modifica il microfono né l'audio registrato. Sull'iPhone si usa il volume del telefono.", bundle: AppLocalization.bundle))
            }
        }
    }

    private var wifiFields: some View {
        Section {
            TextField(String(localized: "Nome rete (SSID)", bundle: AppLocalization.bundle), text: $draft.ssid).textContentType(.none)
            SecureField(draft.wifiPasswordSaved ? String(localized: "Password già salvata", bundle: AppLocalization.bundle) : String(localized: "Password Wi-Fi", bundle: AppLocalization.bundle), text: $draft.wifiPassword).textContentType(.password)
        } header: { Text(String(localized: "Rete a 2,4 GHz", bundle: AppLocalization.bundle)) } footer: {
            Text(String(localized: "Con lo stesso SSID, la password vuota conserva quella salvata. Per una nuova rete aperta lascia la password vuota. Svuota il nome rete per disconnettere il Wi-Fi.", bundle: AppLocalization.bundle))
        }
        .textInputAutocapitalization(.never).autocorrectionDisabled()
    }

    private var badgeFields: some View {
        Group {
            Section(String(localized: "Immagine e movimento", bundle: AppLocalization.bundle)) {
                Toggle(String(localized: "Rotazione automatica", bundle: AppLocalization.bundle), isOn: $draft.badge.autoRotate)
                Toggle(String(localized: "Riproduci GIF", bundle: AppLocalization.bundle), isOn: $draft.badge.animate)
            }
            Section {
                Toggle(String(localized: "Display sempre acceso nel Badge", bundle: AppLocalization.bundle), isOn: $draft.badge.keepAwake)
            } footer: {
                Text(String(localized: "Disattivato: segue il timeout del dispositivo. Attivato: resta visibile e abbassa la luminosità dopo 20 secondi di inattività. Foto ferme non vengono ridisegnate; i sensori rallentano quando il dispositivo è fermo.", bundle: AppLocalization.bundle))
            }
        }
    }

    private var launcherFields: some View {
        Group {
            Section {
                ForEach(draft.homeApps, id: \.self) { id in
                    if let app = HomeApplication(rawValue: id) {
                        HStack {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(app.title)
                                Text(app.group).font(.caption).foregroundStyle(.secondary)
                            }
                            Spacer()
                            if app == .system { Image(systemName: "lock").foregroundStyle(.secondary) }
                            else { Button { draft.homeApps.removeAll { $0 == id } } label: { Image(systemName: "eye.slash").frame(width: 44, height: 44) }.buttonStyle(.borderless).accessibilityLabel(String(localized: "Nascondi ", bundle: AppLocalization.bundle) + app.title) }
                        }
                    }
                }.onMove { source, destination in draft.homeApps.move(fromOffsets: source, toOffset: destination) }
            } header: { Text(String(localized: "Applicazioni visibili", bundle: AppLocalization.bundle)) } footer: {
                Text(String(localized: "Tocca Modifica per riordinare. L'ordine si applica all'interno di ogni gruppo: la prima app diventa quella grande. I gruppi vuoti vengono saltati; Sistema resta sempre accessibile.", bundle: AppLocalization.bundle))
            }
            Section(String(localized: "Applicazioni nascoste", bundle: AppLocalization.bundle)) {
                ForEach(HomeApplication.allCases.filter { !draft.homeApps.contains($0.rawValue) }) { app in
                    Button { draft.homeApps.append(app.rawValue) } label: { Label(app.title, systemImage: "plus.circle") }
                }
            }
            Section { Button(String(localized: "Ripristina disposizione", bundle: AppLocalization.bundle)) { draft.homeApps = HomeApplication.allCases.map(\.rawValue) } }
        }
    }

    private var presentationFields: some View {
        Section {
            Toggle(String(localized: "Usa Pagina su / Pagina giù", bundle: AppLocalization.bundle), isOn: $draft.presentationPageKeys)
            Text(String(localized: "Disattivato: invia le frecce sinistra/destra. Attivato: invia Pagina su/giù. Apri Slide sullo StopWatch collegato via USB: A torna indietro, B avanza; il comando touch Oscura / riprendi invia B. Il computer deve avere la presentazione in primo piano.", bundle: AppLocalization.bundle))
                .font(.footnote).foregroundStyle(.secondary)
        }
    }

    private var clockFields: some View {
        Group {
            Section(String(localized: "Quadrante", bundle: AppLocalization.bundle)) {
                Picker(String(localized: "Stile", bundle: AppLocalization.bundle), selection: $draft.clockFace) {
                    ForEach(Array(zip(["digital", "analog", "halo", "duo", "orbit", "segments"],
                                      [String(localized: "Digitale", bundle: AppLocalization.bundle), String(localized: "Analogico", bundle: AppLocalization.bundle), "Halo", String(localized: "Doppio", bundle: AppLocalization.bundle), String(localized: "Orbita", bundle: AppLocalization.bundle), String(localized: "Segmenti", bundle: AppLocalization.bundle)])), id: \.0) { id, name in
                        Text(name).tag(id)
                    }
                }
            }
            Section {
                Toggle(String(localized: "Display sempre acceso nell’orologio", bundle: AppLocalization.bundle), isOn: $draft.clockKeepAwake)
            } footer: {
                Text(String(localized: "Quando apri Orologio, il quadrante resta visibile e la luminosità si abbassa dopo 20 secondi senza interazioni. Timer e cronometro seguono il normale timeout.", bundle: AppLocalization.bundle))
            }
        }
    }

    private var homeFields: some View {
        Group {
            Section {
                TextField("https://home.example.com", text: $draft.haUrl).keyboardType(.URL)
                SecureField(draft.haTokenSaved ? String(localized: "Token già salvato", bundle: AppLocalization.bundle) : String(localized: "Token di accesso a lunga durata", bundle: AppLocalization.bundle), text: $draft.haToken)
            } header: { Text(String(localized: "Controlla casa dallo StopWatch", bundle: AppLocalization.bundle)) } footer: {
                Text(String(localized: "Un dominio con certificato pubblico valido funziona senza aggiungere una CA. L'indirizzo vuoto disattiva i comandi REST e rimuove il token salvato.", bundle: AppLocalization.bundle))
            }
            .textInputAutocapitalization(.never).autocorrectionDisabled()
            Section(String(localized: "Preferiti · \(draft.favorites.count)/8", bundle: AppLocalization.bundle)) {
                ForEach($draft.favorites) { $favorite in
                    VStack(alignment: .leading, spacing: 8) {
                        TextField(String(localized: "Nome", bundle: AppLocalization.bundle), text: $favorite.label)
                        TextField("light.study", text: $favorite.entity).font(.subheadline.monospaced())
                            .textInputAutocapitalization(.never).autocorrectionDisabled()
                    }.padding(.vertical, 5)
                }
                .onDelete { draft.favorites.remove(atOffsets: $0) }
                Button { draft.favorites.append(Favorite(label: "", entity: "")) } label: {
                    Label(String(localized: "Aggiungi preferito", bundle: AppLocalization.bundle), systemImage: "plus")
                }.disabled(draft.favorites.count >= 8)
            }
            Section {
                DisclosureGroup(String(localized: "CA privata · facoltativa", bundle: AppLocalization.bundle)) {
                    TextEditor(text: $draft.haCa).font(.caption.monospaced()).frame(minHeight: 140).accessibilityLabel(String(localized: "Certificato CA Home Assistant in formato PEM", bundle: AppLocalization.bundle))
                }
            }
        }
    }

    private var mqttFields: some View {
        Group {
            Section {
                TextField(String(localized: "Indirizzo broker, es. 192.168.1.10", bundle: AppLocalization.bundle), text: $draft.mqttHost)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                LabeledContent(String(localized: "Porta", bundle: AppLocalization.bundle)) { TextField("1883", value: $draft.mqttPort, format: .number.grouping(.never)).keyboardType(.numberPad).multilineTextAlignment(.trailing) }
                Toggle(String(localized: "Connessione TLS", bundle: AppLocalization.bundle), isOn: $draft.mqttTls)
                    .onChange(of: draft.mqttTls) { old, new in
                        if new && draft.mqttPort == 1883 { draft.mqttPort = 8883 }
                        if old && !new && draft.mqttPort == 8883 { draft.mqttPort = 1883 }
                    }
                TextField(String(localized: "Utente Mosquitto", bundle: AppLocalization.bundle), text: $draft.mqttUser).textInputAutocapitalization(.never).autocorrectionDisabled()
                SecureField(draft.mqttPasswordSaved ? String(localized: "Password già salvata", bundle: AppLocalization.bundle) : String(localized: "Password Mosquitto", bundle: AppLocalization.bundle), text: $draft.mqttPassword)
            } header: { Text(String(localized: "Collega StopWatch a Home Assistant", bundle: AppLocalization.bundle)) } footer: {
                Text(String(localized: "Usa il broker già collegato a Home Assistant. La discovery MQTT crea automaticamente dispositivo, sensori e comandi. Svuota il broker per disattivare il collegamento. TCP senza TLS è adatto alla tua rete locale fidata.", bundle: AppLocalization.bundle))
            }
            if draft.mqttTls {
                Section {
                    DisclosureGroup(String(localized: "CA privata · facoltativa", bundle: AppLocalization.bundle)) {
                        TextEditor(text: $draft.mqttCa).font(.caption.monospaced()).frame(minHeight: 140).accessibilityLabel(String(localized: "Certificato CA MQTT in formato PEM", bundle: AppLocalization.bundle))
                    }
                } footer: { Text(String(localized: "Lascia vuota la CA con un certificato pubblico valido. Nome del broker e certificato devono corrispondere.", bundle: AppLocalization.bundle)) }
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
                    Section(String(localized: "Collegamenti", bundle: AppLocalization.bundle)) {
                        LabeledContent("Bluetooth", value: connection.phase.title)
                        LabeledContent("Wi-Fi", value: status.wifi ? status.ip : String(localized: "Non connesso", bundle: AppLocalization.bundle))
                        if let rssi = status.rssi { LabeledContent(String(localized: "Segnale", bundle: AppLocalization.bundle), value: "\(rssi) dBm") }
                        LabeledContent("Home Assistant MQTT", value: status.mqttConnected ? String(localized: "Connesso", bundle: AppLocalization.bundle) : String(localized: "Non connesso", bundle: AppLocalization.bundle))
                        LabeledContent(String(localized: "iPhone abbinati", bundle: AppLocalization.bundle), value: "\(status.bondedPhones)")
                    }
                    Section(String(localized: "Hardware rilevato", bundle: AppLocalization.bundle)) {
                        LabeledContent(String(localized: "Microfono", bundle: AppLocalization.bundle), value: status.microphone ? String(localized: "Configurato", bundle: AppLocalization.bundle) : String(localized: "Non disponibile", bundle: AppLocalization.bundle))
                        LabeledContent(String(localized: "Movimento / IMU", bundle: AppLocalization.bundle), value: status.imu ? String(localized: "Rilevato", bundle: AppLocalization.bundle) : String(localized: "Non disponibile", bundle: AppLocalization.bundle))
                        if status.imu, status.acceleration.count == 3 {
                            LabeledContent(String(localized: "Accelerazione", bundle: AppLocalization.bundle), value: status.acceleration.map { String(format: "%.2f", $0) }.joined(separator: " · "))
                        }
                        LabeledContent(String(localized: "Orologio RTC", bundle: AppLocalization.bundle), value: status.rtc ? String(localized: "Rilevato", bundle: AppLocalization.bundle) : String(localized: "Non disponibile", bundle: AppLocalization.bundle))
                        LabeledContent("Touch", value: status.touch ? String(localized: "Configurato", bundle: AppLocalization.bundle) : String(localized: "Non disponibile", bundle: AppLocalization.bundle))
                        LabeledContent(String(localized: "Pulsante A", bundle: AppLocalization.bundle), value: status.buttonA ? String(localized: "Premuto", bundle: AppLocalization.bundle) : String(localized: "Rilasciato", bundle: AppLocalization.bundle))
                        LabeledContent(String(localized: "Pulsante B", bundle: AppLocalization.bundle), value: status.buttonB ? String(localized: "Premuto", bundle: AppLocalization.bundle) : String(localized: "Rilasciato", bundle: AppLocalization.bundle))
                    }
                    Section(String(localized: "Archivio", bundle: AppLocalization.bundle)) {
                        LabeledContent(String(localized: "Note", bundle: AppLocalization.bundle), value: "\(status.notes)")
                        LabeledContent("LittleFS", value: status.storageReady ? ByteCountFormatter.string(fromByteCount: Int64(status.storageUsed), countStyle: .file) + String(localized: " usati", bundle: AppLocalization.bundle) : String(localized: "Da inizializzare sul dispositivo", bundle: AppLocalization.bundle))
                        LabeledContent(String(localized: "PSRAM libera", bundle: AppLocalization.bundle), value: ByteCountFormatter.string(fromByteCount: Int64(status.psramFree), countStyle: .memory))
                        LabeledContent(String(localized: "Registrazione", bundle: AppLocalization.bundle), value: status.recording ? String(localized: "In corso", bundle: AppLocalization.bundle) : String(localized: "In attesa", bundle: AppLocalization.bundle))
                    }
                    Section(String(localized: "Prova sul dispositivo", bundle: AppLocalization.bundle)) {
                        Button(String(localized: "Vibrazione", bundle: AppLocalization.bundle)) { connection.output("vibrate") }
                        Button(String(localized: "Tono altoparlante", bundle: AppLocalization.bundle)) { connection.output("beep") }
                        Button(String(localized: "Accendi / spegni LED", bundle: AppLocalization.bundle)) { connection.output("led") }
                    }.disabled(connection.working || status.recording)
                } else {
                    ContentUnavailableView(String(localized: "Collega StopWatch", bundle: AppLocalization.bundle), systemImage: "link", description: Text(String(localized: "La diagnostica mostra le letture reali del dispositivo connesso.", bundle: AppLocalization.bundle)))
                }
                Section {
                    Text(String(localized: "Rilevare un componente non ne conferma il funzionamento fisico. Per touch, display, pulsanti e audio usa anche Diagnostica sullo StopWatch.", bundle: AppLocalization.bundle))
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle(String(localized: "Diagnostica", bundle: AppLocalization.bundle)).navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button(String(localized: "Fine", bundle: AppLocalization.bundle)) { dismiss() } } }
            .refreshable { await connection.refresh() }
        }
        .preferredColorScheme(.light).tint(.primary)
    }
}
