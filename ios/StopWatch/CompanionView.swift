import SwiftUI
import AVFoundation

enum Palette {
    static let background = Color(red: 0.055, green: 0.055, blue: 0.06)
    static let paper = Color(red: 0.98, green: 0.98, blue: 0.97)
    static let ink = Color(red: 0.06, green: 0.06, blue: 0.065)
    static let secondary = Color(white: 0.39)
    static let orange = Color(red: 1, green: 0.31, blue: 0.06)
}

struct CompanionView: View {
    @EnvironmentObject private var connection: WatchConnection
    @State private var tab = ProcessInfo.processInfo.arguments.contains("--badge") ? 3 : 0
    @State private var pairing = false
    @State private var settings = false
    @State private var diagnostics = false
    @State private var mqttSettings = false
    @ScaledMetric(relativeTo: .largeTitle) private var headline = 44

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    HStack(alignment: .center) {
                        VStack(alignment: .leading, spacing: 6) {
                            Text("STOPWATCH").font(.caption.weight(.semibold)).tracking(3).foregroundStyle(.white.opacity(0.65))
                            Text(tab == 0 ? "A portata\ndi mano." : tab == 1 ? "Un'idea.\nÈ già qui." : tab == 2 ? "La tua casa,\ncon te." : "Il tuo stile.\nIn vista.")
                                .font(.system(size: headline, weight: .medium, design: .rounded)).tracking(-1.5)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                        Spacer(minLength: 8)
                        CircleButton(symbol: "slider.horizontal.3", label: "Impostazioni") { settings = true }
                    }
                    HStack(spacing: 8) {
                        Image(systemName: connection.ready ? "link" : "link.badge.plus")
                            .foregroundStyle(connection.ready ? Palette.orange : .white.opacity(0.7))
                        Text("Bluetooth · " + connection.phase.title).font(.subheadline)
                        Spacer()
                        if connection.preview { Text("ANTEPRIMA").font(.caption2).foregroundStyle(.white.opacity(0.6)) }
                    }
                    .foregroundStyle(.white.opacity(0.85))
                    if let error = connection.error {
                        HStack(alignment: .top) {
                            Image(systemName: "exclamationmark.circle")
                            Text(error).font(.subheadline)
                            Spacer(minLength: 0)
                            Button { connection.error = nil } label: { Image(systemName: "xmark").frame(width: 32, height: 44) }
                                .accessibilityLabel("Chiudi messaggio")
                        }
                        .padding(18).background(.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 24))
                    }
                    if tab == 0 { devicePage }
                    else if tab == 1 { NotesPage() }
                    else if tab == 2 { homePage }
                    else { BadgePage() }
                }
                .padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 20)
                .frame(maxWidth: 640)
                .frame(maxWidth: .infinity)
            }
            .background(Palette.background)
            .toolbar(.hidden, for: .navigationBar)
            .safeAreaInset(edge: .bottom, spacing: 0) {
                HStack(spacing: 0) {
                    CircleButton(symbol: "antenna.radiowaves.left.and.right", label: "Collega StopWatch", diameter: 44) { pairing = true }
                    Spacer(minLength: 4)
                    HStack(spacing: 2) {
                        tabButton(0, "stopwatch", "Dispositivo")
                        tabButton(1, "waveform", "Note vocali")
                        tabButton(2, "house", "Casa")
                        tabButton(3, "photo", "Badge")
                    }
                    .padding(5).background(Color(white: 0.17), in: Capsule())
                    Spacer(minLength: 4)
                    CircleButton(symbol: "heart.text.square", label: "Diagnostica hardware", diameter: 44) { diagnostics = true }
                }
                .padding(.horizontal, 12).padding(.top, 12).padding(.bottom, 8)
                .background(Palette.background)
            }
            .sheet(isPresented: $pairing) { PairingView() }
            .sheet(isPresented: $settings) { SettingsSheet() }
            .sheet(isPresented: $diagnostics) { DiagnosticsView() }
            .sheet(isPresented: $mqttSettings) {
                NavigationStack {
                    if let configuration = connection.configuration {
                        ConfigurationEditor(part: .mqtt, configuration: configuration)
                            .toolbar { ToolbarItem(placement: .topBarLeading) { Button("Annulla") { mqttSettings = false } } }
                    }
                }
                .preferredColorScheme(.light).tint(.primary)
            }
        }
        .tint(Palette.orange)
    }

    private func tabButton(_ index: Int, _ symbol: String, _ label: String) -> some View {
        Button { tab = index } label: {
            Image(systemName: symbol).font(.system(size: 20, weight: .medium))
                .frame(width: 44, height: 44)
                .foregroundStyle(tab == index ? Palette.ink : .white)
                .background(tab == index ? Palette.paper : .clear, in: Circle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
        .accessibilityAddTraits(tab == index ? .isSelected : [])
    }

    private var devicePage: some View {
        VStack(spacing: 12) {
            PaperCard {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("IL TUO DISPOSITIVO").font(.caption.weight(.medium)).tracking(1.4).foregroundStyle(Palette.secondary)
                        Text("StopWatch").font(.title.weight(.medium))
                    }
                    Spacer()
                    Image(systemName: connection.ready ? "checkmark" : "plus").font(.title3)
                        .frame(width: 44, height: 44).background(.black.opacity(0.035), in: Circle())
                        .accessibilityHidden(true)
                }
                WatchDrawing(connected: connection.ready).frame(height: 184).padding(.vertical, 4)
                if connection.ready {
                    HStack {
                        Text(connection.status?.name ?? "StopWatch").font(.subheadline).foregroundStyle(Palette.secondary)
                        Spacer()
                        Text("Bluetooth").font(.subheadline.weight(.medium))
                    }
                } else {
                    Text("Un codice, una volta sola.")
                        .font(.title3.weight(.medium))
                    Text("Abbina il dispositivo e ritrova qui impostazioni, comandi e registrazioni.")
                        .font(.subheadline).foregroundStyle(Palette.secondary).fixedSize(horizontal: false, vertical: true)
                    Button { pairing = true } label: {
                        HStack { Text(connection.hasRememberedDevice ? "Ricollega StopWatch" : "Abbina StopWatch"); Spacer(); Image(systemName: "arrow.up.right") }
                            .font(.headline).padding(.vertical, 15).padding(.horizontal, 20)
                            .foregroundStyle(.white).background(Palette.ink, in: Capsule())
                    }
                    .buttonStyle(.plain)
                    .padding(.top, 8)
                }
            }
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: 12)], spacing: 12) {
                MetricCard(title: "Batteria", value: batteryText, detail: connection.status?.charging == true ? "In carica" : "Sul dispositivo", symbol: "battery.75percent")
                Button { tab = 1 } label: {
                    MetricCard(title: "Note vocali", value: connection.status.map { String($0.notes) } ?? "—", detail: "Idee da riascoltare", symbol: "waveform")
                }.buttonStyle(.plain)
            }
            PaperCard {
                Text("Fatti sentire.").font(.title2.weight(.medium))
                HStack(spacing: 12) {
                    OutputButton(symbol: "waveform.path", title: "Vibra", action: "vibrate")
                    OutputButton(symbol: "speaker.wave.2", title: "Suona", action: "beep")
                    OutputButton(symbol: "lightbulb", title: "LED", action: "led")
                }
                .padding(.top, 8)
            }
            HStack(alignment: .top, spacing: 12) {
                Image(systemName: "record.circle").foregroundStyle(Palette.orange).font(.title2)
                Text("Tieni premuto B. Attendi REC, parla e rilascia: la tua idea resta sullo StopWatch, anche offline.")
                    .font(.subheadline).foregroundStyle(.white.opacity(0.75))
            }.padding(.horizontal, 8).padding(.top, 12)
        }
    }

    private var batteryText: String {
        guard let value = connection.status?.battery, (0...100).contains(value) else { return "—" }
        return "\(value)%"
    }

    private var homePage: some View {
        VStack(spacing: 12) {
            PaperCard {
                HStack {
                    Text("HOME ASSISTANT").font(.caption.weight(.medium)).tracking(1.3).foregroundStyle(Palette.secondary)
                    Spacer()
                    Image(systemName: "house").font(.title2).accessibilityHidden(true)
                }
                Text(connection.status?.mqttConnected == true ? "Tutto collegato." : "Un posto anche\nper StopWatch.")
                    .font(.largeTitle.weight(.medium)).padding(.top, 20)
                Text("In casa usi il Wi-Fi diretto. Fuori casa il Companion può inoltrare i comandi di Casa e Spotify con i dati dell’iPhone.")
                    .font(.body).foregroundStyle(Palette.secondary).padding(.top, 4)
                Divider().padding(.vertical, 8)
                LabeledContent("Wi-Fi StopWatch", value: connection.status?.wifi == true ? "Connesso" : "Non connesso")
                    .font(.subheadline)
                LabeledContent("Mosquitto / MQTT", value: mqttState)
                    .font(.subheadline.weight(.medium))
                if let broker = connection.configuration?.mqttHost, !broker.isEmpty {
                    Text(broker).font(.caption.monospaced()).foregroundStyle(Palette.secondary)
                }
                LabeledContent("Ponte iPhone", value: connection.relayEnabled ? "Abilitato" : "Da configurare")
                    .font(.subheadline)
                NavigationLink { RelaySettingsView().preferredColorScheme(.light).tint(.primary) } label: {
                    Label("Internet tramite iPhone", systemImage: "iphone.radiowaves.left.and.right")
                        .font(.headline).foregroundStyle(Palette.ink).frame(minHeight: 44)
                }
                Button { mqttSettings = true } label: {
                    Label("Configura integrazione", systemImage: "arrow.up.right")
                        .font(.headline).foregroundStyle(Palette.ink).frame(minHeight: 44)
                }.buttonStyle(.plain).padding(.top, 8)
                    .disabled(!connection.ready || connection.working || connection.configuration == nil)
            }
            PaperCard {
                HStack { Text("Sul dispositivo").font(.title2.weight(.medium)); Spacer(); Image(systemName: "star") }
                Text("Scegli fino a otto preferiti da richiamare con i due pulsanti.")
                    .font(.subheadline).foregroundStyle(Palette.secondary)
                if let favorites = connection.configuration?.favorites, !favorites.isEmpty {
                    ForEach(Array(favorites.enumerated()), id: \.offset) { _, favorite in
                        Divider().padding(.vertical, 4)
                        HStack { Image(systemName: "circle.grid.2x2"); Text(favorite.label); Spacer() }
                    }
                } else {
                    Text("I tuoi preferiti compariranno qui.").font(.subheadline).foregroundStyle(Palette.secondary).padding(.vertical, 16)
                }
            }
        }
    }

    private var mqttState: String {
        guard connection.ready else { return "StopWatch scollegato" }
        if connection.status?.mqttConnected == true { return "Connesso" }
        if connection.configuration?.mqttHost.isEmpty != false { return "Da configurare" }
        if connection.status?.wifi == false { return "In attesa del Wi-Fi" }
        return "Non connesso"
    }
}

struct PaperCard<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 12) { content }
            .frame(maxWidth: .infinity, alignment: .leading).padding(24)
            .foregroundStyle(Palette.ink)
            .background(Palette.paper, in: RoundedRectangle(cornerRadius: 34, style: .continuous))
    }
}

private struct CircleButton: View {
    let symbol: String
    let label: String
    var diameter: CGFloat = 52
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Image(systemName: symbol).font(.system(size: 20, weight: .medium))
                .frame(width: diameter, height: diameter).foregroundStyle(Palette.ink)
                .background(Palette.paper, in: Circle())
        }.buttonStyle(.plain).accessibilityLabel(label)
    }
}

private struct MetricCard: View {
    let title: String
    let value: String
    let detail: String
    let symbol: String
    @ScaledMetric(relativeTo: .largeTitle) private var numberSize = 42
    var body: some View {
        PaperCard {
            HStack { Text(title).font(.subheadline); Spacer(minLength: 0); Image(systemName: symbol).accessibilityHidden(true) }
            Text(value).font(.system(size: numberSize, weight: .regular)).monospacedDigit().padding(.top, 20)
            Text(detail).font(.caption).foregroundStyle(Palette.secondary)
        }
    }
}

private struct OutputButton: View {
    @EnvironmentObject private var connection: WatchConnection
    let symbol: String
    let title: String
    let action: String
    var body: some View {
        Button { connection.output(action) } label: {
            VStack(spacing: 10) {
                Image(systemName: symbol).font(.title2).frame(width: 58, height: 58)
                    .background(.black.opacity(0.045), in: Circle())
                Text(title).font(.caption.weight(.medium))
            }.frame(maxWidth: .infinity, minHeight: 84)
        }
        .buttonStyle(.plain)
        .disabled(!connection.ready || connection.working || connection.status?.recording == true)
        .opacity(connection.ready ? 1 : 0.4)
        .accessibilityHint("Esegue il comando sullo StopWatch")
    }
}

private struct NotesPage: View {
    @EnvironmentObject private var connection: WatchConnection
    @State private var deleting: VoiceNote?
    @State private var playback: AudioFile?
    @State private var transfer: Task<Void, Never>?
    struct AudioFile: Identifiable { let id = UUID(); let url: URL }

    var body: some View {
        VStack(spacing: 12) {
            PaperCard {
                Label("Le idee diventano testo.", systemImage: "text.bubble").font(.headline)
                Text("Le note usano il Wi-Fi quando raggiungibile e Bluetooth fuori rete. La ricezione può continuare a schermo bloccato; apri l'app per la trascrizione in italiano. Audio e testo restano disponibili offline.")
                    .font(.subheadline).foregroundStyle(Palette.secondary)
            }
            if let progress = connection.downloadProgress {
                PaperCard {
                    Text(connection.receivingViaWiFi ? "La tua nota arriva via Wi-Fi." : "La tua nota sta arrivando.").font(.title2.weight(.medium))
                    ProgressView(value: progress).tint(Palette.ink)
                    HStack {
                        Text(progress, format: .percent.precision(.fractionLength(0))).monospacedDigit()
                        Spacer()
                        Button("Annulla") { transfer?.cancel(); connection.cancelNoteSync() }.frame(minHeight: 44)
                    }
                    Text("La copia originale rimane sul dispositivo.").font(.caption).foregroundStyle(Palette.secondary)
                }
            }
            if connection.notes.isEmpty {
                PaperCard {
                    Image(systemName: "waveform").font(.system(size: 42, weight: .ultraLight)).padding(.vertical, 24)
                    Text("Spazio alle idee.").font(.largeTitle.weight(.medium))
                    Text(connection.ready ? "Tieni premuto B sul dispositivo, attendi REC e parla. Al rilascio la nota arriva qui automaticamente." : "Collega StopWatch per ritrovare le tue registrazioni e trasformarle in testo.")
                        .font(.body).foregroundStyle(Palette.secondary)
                }
            }
            ForEach(connection.notes) { note in
                PaperCard {
                    HStack {
                        Image(systemName: "waveform").font(.title2).frame(width: 44, height: 44).background(.black.opacity(0.04), in: Circle())
                        VStack(alignment: .leading, spacing: 4) {
                            Text(note.title).font(.headline)
                            Text(note.duration + " · " + ByteCountFormatter.string(fromByteCount: Int64(note.bytes), countStyle: .file))
                                .font(.caption).foregroundStyle(Palette.secondary)
                        }
                        Spacer(minLength: 0)
                    }
                    if let seconds = connection.transferSeconds[note.name] {
                        Text("\(connection.transferMethods[note.name] ?? "Bluetooth") · ricevuta in \(seconds, specifier: "%.1f") s")
                            .font(.caption).foregroundStyle(Palette.secondary)
                    }
                    if let text = connection.transcripts[note.name] {
                        Text(text).font(.body).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading)
                        ShareLink(item: text) { Label("Condividi testo", systemImage: "square.and.arrow.up") }
                            .font(.subheadline.weight(.medium)).tint(Palette.ink).frame(minHeight: 44)
                    } else if connection.syncingNote == note.name {
                        HStack {
                            ProgressView().tint(Palette.ink)
                            Text(connection.downloadProgress == nil ? "Trascrizione in corso…" : "Ricezione audio…")
                        }.font(.subheadline).foregroundStyle(Palette.secondary)
                    } else if let error = connection.transcriptionErrors[note.name] {
                        Text(error).font(.subheadline).foregroundStyle(Palette.secondary)
                        Button("Riprova trascrizione") { connection.retryTranscription(note) }
                            .font(.subheadline.weight(.medium)).tint(Palette.ink).frame(minHeight: 44)
                    } else {
                        Text("In attesa della trascrizione automatica…").font(.subheadline).foregroundStyle(Palette.secondary)
                    }
                    HStack {
                        if let url = connection.localURL(for: note) {
                            Button { playback = AudioFile(url: url) } label: { Label("Ascolta", systemImage: "play.fill").frame(minHeight: 44) }
                            Spacer()
                            ShareLink(item: url) { Image(systemName: "square.and.arrow.up").frame(width: 44, height: 44) }
                                .accessibilityLabel("Condividi nota")
                        } else {
                            Button { download(note) } label: { Label("Scarica", systemImage: "arrow.down").frame(minHeight: 44) }
                                .disabled(!connection.ready || connection.working || connection.syncingNote != nil)
                            Spacer()
                        }
                        Button(role: .destructive) { deleting = note } label: { Image(systemName: "trash").frame(width: 44, height: 44) }
                            .disabled(!connection.isOnDevice(note) || connection.working || connection.syncingNote == note.name).accessibilityLabel("Elimina nota dallo StopWatch")
                    }
                    .font(.subheadline.weight(.medium)).tint(Palette.ink)
                }
            }
            Button { Task { await connection.refreshNotes() } } label: {
                Label("Aggiorna note", systemImage: "arrow.clockwise").frame(minHeight: 48)
            }.disabled(!connection.ready || connection.working)
        }
        .task { await connection.refreshNotes() }
        .confirmationDialog("Eliminare la nota dallo StopWatch?", isPresented: Binding(get: { deleting != nil }, set: { if !$0 { deleting = nil } })) {
            if let note = deleting {
                Button("Elimina dal dispositivo", role: .destructive) { Task { await connection.delete(note) }; deleting = nil }
            }
        } message: { Text("La cancellazione sullo StopWatch è definitiva. Una copia già scaricata su iPhone viene conservata.") }
        .sheet(item: $playback) { file in AudioPlayerView(url: file.url) }
        .onDisappear { transfer?.cancel() }
    }

    private func download(_ note: VoiceNote) {
        transfer = Task {
            do { playback = AudioFile(url: try await connection.download(note)) }
            catch is CancellationError { }
            catch { connection.error = error.localizedDescription }
        }
    }
}

private struct AudioPlayerView: View {
    let url: URL
    @State private var player: AVAudioPlayer?
    @State private var playing = false
    @State private var error: String?
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(spacing: 28) {
                Image(systemName: "waveform").font(.system(size: 72, weight: .ultraLight))
                Text("La tua idea.").font(.largeTitle.weight(.medium))
                Text(url.lastPathComponent).font(.caption).foregroundStyle(.secondary)
                if let error { Text(error).font(.subheadline) }
                TimelineView(.periodic(from: .now, by: 0.5)) { _ in
                    VStack(spacing: 28) {
                        Text(String(format: "%0.0f / %0.0f s", player?.currentTime ?? 0, player?.duration ?? 0)).monospacedDigit()
                        Button {
                            do {
                                if player?.isPlaying == true { player?.pause(); playing = false }
                                else {
                                    try AVAudioSession.sharedInstance().setCategory(.playback, mode: .spokenAudio)
                                    try AVAudioSession.sharedInstance().setActive(true)
                                    playing = player?.play() == true
                                }
                            } catch { self.error = error.localizedDescription }
                        } label: {
                            Label(player?.isPlaying == true ? "Pausa" : "Riproduci", systemImage: player?.isPlaying == true ? "pause.fill" : "play.fill")
                                .frame(minWidth: 160, minHeight: 54)
                        }
                        .buttonStyle(.borderedProminent).tint(.white).foregroundStyle(.black)
                    }
                    .accessibilityValue(playing ? "Riproduzione" : "In pausa")
                }
                ShareLink("Condividi audio", item: url)
            }
            .padding(24).frame(maxWidth: .infinity, maxHeight: .infinity)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Fine") { dismiss() } } }
        }
        .onAppear {
            do { player = try AVAudioPlayer(contentsOf: url); player?.prepareToPlay() }
            catch { self.error = error.localizedDescription }
        }
        .onDisappear { player?.stop(); try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation) }
    }
}

#Preview { CompanionView().environmentObject(WatchConnection(preview: true)).preferredColorScheme(.dark) }
