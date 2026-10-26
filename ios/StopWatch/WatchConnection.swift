import Foundation
import CoreBluetooth
import Combine
import UIKit

@MainActor
final class WatchConnection: NSObject, ObservableObject, @preconcurrency CBCentralManagerDelegate, @preconcurrency CBPeripheralDelegate {
    enum Phase {
        case idle, scanning, connecting, authorizing, connected, reconnecting, bluetoothOff, unavailable
        var title: String {
            switch self {
            case .idle: "Da collegare"
            case .scanning: "Cerco StopWatch…"
            case .connecting: "Connessione…"
            case .authorizing: "Verifica abbinamento…"
            case .connected: "Connesso"
            case .reconnecting: "In attesa di StopWatch"
            case .bluetoothOff: "Bluetooth spento"
            case .unavailable: "Bluetooth non disponibile"
            }
        }
    }
    struct DiscoveredWatch: Identifiable { let id: UUID; let name: String }
    private static let service = CBUUID(string: "75f3a100-9e9a-4b9c-9d73-c85f44e6a001")
    private static let rxID = CBUUID(string: "75f3a101-9e9a-4b9c-9d73-c85f44e6a001")
    private static let txID = CBUUID(string: "75f3a102-9e9a-4b9c-9d73-c85f44e6a001")
    private static let rememberedKey = "pairedStopWatchIdentifier"

    @Published private(set) var phase: Phase = .idle
    @Published private(set) var devices: [DiscoveredWatch] = []
    @Published private(set) var status: DeviceStatus?
    @Published private(set) var configuration: DeviceConfiguration?
    @Published private(set) var notes: [VoiceNote] = []
    @Published private(set) var working = false
    @Published private(set) var downloadProgress: Double?
    @Published private(set) var lastDownloadedURL: URL?
    @Published private(set) var transcripts: [String: String] = [:]
    @Published private(set) var transcriptionErrors: [String: String] = [:]
    @Published private(set) var syncingNote: String?
    @Published private(set) var transferSeconds: [String: TimeInterval] = [:]
    @Published private(set) var transferMethods: [String: String] = [:]
    @Published private(set) var receivingViaWiFi = false
    @Published private(set) var badgeProgress: Double?
    @Published private(set) var badgeTransferMethod = ""
    @Published private(set) var badgeTransferDetail = "Preparazione…"
    @Published private(set) var badgeTransferIssue: String?
    @Published private(set) var badgeTransferSeconds: TimeInterval?
    @Published private(set) var badgeState: BadgeStatus?
    @Published var error: String?
    @Published private(set) var relayURL = ""
    @Published private(set) var relayConfigured = false
    @Published private(set) var relayEnabled = false
    @Published private(set) var relayMessage = "Da configurare" {
        didSet { if oldValue != relayMessage { UserDefaults.standard.set(relayMessage, forKey: "relayStatus") } }
    }
    @Published private(set) var relayLastActivity: RelayActivity?
    var ready: Bool { phase == .connected }
    var hasRememberedDevice: Bool { rememberedID != nil }

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var found: [UUID: CBPeripheral] = [:]
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private var rememberedID: UUID?
    private var securityRead = false
    private var frames = FrameBuffer()
    private var nextID: UInt32 = 0
    private var pending: (id: UInt32, continuation: CheckedContinuation<Data, Error>)?
    private var timeout: Task<Void, Never>?
    private var connectionTimeout: Task<Void, Never>?
    private var handshake: Task<Void, Never>?
    private var handshakeBackgroundTask: UIBackgroundTaskIdentifier = .invalid
    private var outgoing: [Data] = []
    private var writeIndex = 0
    private var badgeWiFiAttempt: UUID?
    private var shouldReconnect = true
    private var remoteNotes: [VoiceNote] = []
    private let transcriber = NoteTranscriber()
    private var noteSync: Task<Void, Never>?
    private var synchronization: Task<Void, Never>?
    private var notesRefreshNeeded = false
    private var noteBackgroundTask: UIBackgroundTaskIdentifier = .invalid
    private var relayTasks: [RelayKind: Task<Void, Never>] = [:]
    private var relayBackgroundTasks: [RelayKind: UIBackgroundTaskIdentifier] = [:]
    private var relayGeneration = UUID()
    private var archive: NoteArchive? { rememberedID.map { NoteArchive(deviceID: $0) } }
    var localBadgeURL: URL? {
        guard let rememberedID, let directory=FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else { return nil }
        return directory.appendingPathComponent("Badges",isDirectory: true).appendingPathComponent(rememberedID.uuidString+".swb")
    }
    let preview: Bool

    init(preview: Bool = false) {
        self.preview = preview
        rememberedID = UserDefaults.standard.string(forKey: Self.rememberedKey).flatMap(UUID.init(uuidString:))
        super.init()
        restoreLocalNotes()
        if let data = UserDefaults.standard.data(forKey: "relayLastActivity") {
            relayLastActivity = try? JSONDecoder().decode(RelayActivity.self, from: data)
        }
        if !preview {
            central = CBCentralManager(delegate: self, queue: nil, options: [
                CBCentralManagerOptionRestoreIdentifierKey: "StopWatchCompanion",
                CBCentralManagerOptionShowPowerAlertKey: true
            ])
        }
    }

    func scan() {
        guard !preview, central.state == .poweredOn else { return }
        if ready { return }
        shouldReconnect = false
        if let peripheral { central.cancelPeripheralConnection(peripheral) }
        devices = []; found = [:]
        phase = .scanning
        error = nil
        central.scanForPeripherals(withServices: [Self.service], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func connect(_ id: UUID) {
        guard let device = found[id] else { return }
        shouldReconnect = true
        central.stopScan()
        beginConnection(device)
    }

    func reconnect() {
        guard !preview, central.state == .poweredOn else { return }
        shouldReconnect = true
        if let rememberedID, let device = central.retrievePeripherals(withIdentifiers: [rememberedID]).first {
            beginConnection(device)
        } else { scan() }
    }

    func stopScanning() {
        guard !preview else { return }
        central.stopScan()
        if phase == .scanning { phase = .idle }
    }

    func forget() {
        if let rememberedID { RelayKeychain.remove(rememberedID) }
        synchronization?.cancel()
        pauseNoteSync()
        shouldReconnect = false
        rememberedID = nil
        UserDefaults.standard.removeObject(forKey: Self.rememberedKey)
        if !preview {
            central.stopScan()
            if let peripheral { central.cancelPeripheralConnection(peripheral) }
        }
        resetConnection()
        configuration = nil; notes = []; remoteNotes = []; transcripts = [:]; transcriptionErrors = [:]; transferSeconds = [:]; transferMethods = [:]
        relayConfigured = false; relayEnabled = false; relayURL = ""; relayMessage = "Da configurare"
        relayLastActivity = nil; UserDefaults.standard.removeObject(forKey: "relayLastActivity")
        phase = .idle
    }

    private func beginConnection(_ device: CBPeripheral) {
        guard phase != .connecting || peripheral?.identifier != device.identifier else { return }
        error = nil
        peripheral = device
        device.delegate = self
        phase = .connecting
        central.connect(device)
        connectionTimeout?.cancel()
        connectionTimeout = Task { [weak self] in
            try? await Task.sleep(for: .seconds(60))
            guard !Task.isCancelled, let self, !self.ready else { return }
            self.error = "Connessione non completata. Sullo StopWatch apri iPhone Bluetooth e premi B per abbinare."
            self.central.cancelPeripheralConnection(device)
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: if rememberedID != nil { reconnect() } else { phase = .idle }
        case .poweredOff: resetConnection(); phase = .bluetoothOff
        case .unauthorized, .unsupported: resetConnection(); phase = .unavailable
        default: break
        }
    }

    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        guard let restored = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral],
              let device = restored.first(where: { $0.identifier == rememberedID }) else { return }
        peripheral = device
        device.delegate = self
        if device.state == .connected { self.centralManager(central, didConnect: device) }
        else { beginConnection(device) }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        found[peripheral.identifier] = peripheral
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? peripheral.name ?? "StopWatch"
        if !devices.contains(where: { $0.id == peripheral.identifier }) {
            devices.append(DiscoveredWatch(id: peripheral.identifier, name: name))
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard self.peripheral?.identifier == peripheral.identifier else { central.cancelPeripheralConnection(peripheral); return }
        self.peripheral = peripheral
        peripheral.delegate = self
        phase = .authorizing
        rx = nil; tx = nil; frames = FrameBuffer()
        peripheral.discoverServices([Self.service])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        resetConnection()
        phase = .idle
        self.error = error?.localizedDescription ?? "Impossibile collegare StopWatch."
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        let scanning = phase == .scanning
        resetConnection()
        if scanning { phase = .scanning; return }
        if shouldReconnect, rememberedID == peripheral.identifier, central.state == .poweredOn {
            phase = .reconnecting
            central.connect(peripheral)
        } else { phase = .idle }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        if let error { fail(error.localizedDescription); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.service }) else {
            fail("Questo firmware non contiene il companion Bluetooth."); return
        }
        peripheral.discoverCharacteristics([Self.rxID, Self.txID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        if let error { fail(error.localizedDescription); return }
        rx = service.characteristics?.first(where: { $0.uuid == Self.rxID })
        tx = service.characteristics?.first(where: { $0.uuid == Self.txID })
        guard rx != nil, let tx else { fail("Protocollo Bluetooth non disponibile."); return }
        // Reading this encrypted characteristic invokes iOS's own passkey dialog.
        securityRead = true
        peripheral.readValue(for: tx)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        if let error { fail("Abbinamento non riuscito: " + error.localizedDescription); return }
        guard characteristic.uuid == Self.txID, characteristic.isNotifying else { return }
        handshake?.cancel()
        endHandshakeBackgroundTask()
        handshake = Task {
            let generation = relayGeneration
            handshakeBackgroundTask = UIApplication.shared.beginBackgroundTask(withName: "Riconnessione StopWatch") { [weak self] in
                if self?.relayGeneration == generation {
                    self?.handshake?.cancel()
                    self?.endHandshakeBackgroundTask()
                }
            }
            let background = handshakeBackgroundTask
            defer { if handshakeBackgroundTask == background { endHandshakeBackgroundTask() } }
            do {
                let value: DeviceStatus = try await request("status", body: EmptyMessage())
                guard value.protocol == 1 else { throw CompanionError.message("Aggiorna il firmware dello StopWatch.") }
                status = value
                if rememberedID != peripheral.identifier {
                    pauseNoteSync()
                    notes = []; remoteNotes = []; transcripts = [:]; transcriptionErrors = [:]; transferSeconds = [:]; transferMethods = [:]
                }
                rememberedID = peripheral.identifier
                restoreLocalNotes()
                UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: Self.rememberedKey)
                connectionTimeout?.cancel()
                let _: EmptyMessage? = try? await request("time.set", body: EpochMessage(epoch: Int64(Date().timeIntervalSince1970)))
                configuration = try await request("config.get", body: EmptyMessage())
                try? await loadNotes()
                phase = .connected
                await prepareRelay()
                startNoteSynchronization()
            } catch { if !Task.isCancelled { fail(error.localizedDescription) } }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        if let error { fail(error.localizedDescription); return }
        if securityRead {
            securityRead = false
            peripheral.setNotifyValue(true, for: characteristic)
            return
        }
        guard characteristic.uuid == Self.txID, let bytes = characteristic.value else { return }
        do {
            for line in try frames.append(bytes) {
                let header = try JSONDecoder().decode(ReplyHeader.self, from: line)
                if header.id == 0, header.ok, header.event == "relay.request" {
                    let event = try JSONDecoder().decode(ReplyMessage<RelayEvent>.self, from: line).data
                    handleRelay(event)
                    continue
                }
                if header.id == 0, header.ok, header.event == "notes.changed" {
                    notesRefreshNeeded = true
                    startNoteSynchronization()
                    continue
                }
                if header.id == pending?.id {
                    let request = pending
                    pending = nil; timeout?.cancel(); outgoing = []
                    if header.ok { request?.continuation.resume(returning: line) }
                    else { request?.continuation.resume(throwing: CompanionError.message(header.error ?? "Operazione non riuscita.")) }
                }
            }
        } catch { fail("Risposta Bluetooth non valida. Riconnetti il dispositivo.") }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        guard characteristic.uuid == Self.rxID, pending != nil else { return }
        if let error { fail(error.localizedDescription); return }
        writeIndex += 1
        writeNext()
    }

    private func writeNext() {
        guard let peripheral, let rx, writeIndex < outgoing.count else { return }
        peripheral.writeValue(outgoing[writeIndex], for: rx, type: .withResponse)
    }

    private func request<Body: Encodable, Result: Decodable>(_ op: String, body: Body) async throws -> Result {
        try Task.checkCancellation()
        // A user action may arrive while the periodic status read is finishing.
        while pending != nil || (!op.hasPrefix("relay.") && !relayTasks.isEmpty) { try await Task.sleep(for: .milliseconds(20)) }
        guard let peripheral, let rx, tx?.isNotifying == true, peripheral.state == .connected else {
            throw CompanionError.message("Collegamento occupato o dispositivo non connesso.")
        }
        nextID &+= 1
        let id = nextID
        var bytes = try RequestMessage(id: id, op: op, data: body).encoded()
        guard bytes.count <= FrameBuffer.maximumBytes else { throw CompanionError.message("Configurazione troppo grande.") }
        bytes.append(10)
        let size = min(512, peripheral.maximumWriteValueLength(for: .withResponse))
        guard size > 0, rx.properties.contains(.write) else { throw CompanionError.message("Scrittura Bluetooth non disponibile.") }
        outgoing = stride(from: 0, to: bytes.count, by: size).map { Data(bytes[$0..<min($0 + size, bytes.count)]) }
        writeIndex = 0
        let response: Data = try await withCheckedThrowingContinuation { continuation in
            pending = (id, continuation)
            timeout = Task { [weak self] in
                try? await Task.sleep(for: .seconds(12))
                guard !Task.isCancelled, let self, self.pending?.id == id else { return }
                self.fail("Nessuna conferma dal dispositivo. Riconnetti e verifica l'esito prima di ripetere il comando.")
            }
            writeNext()
        }
        return try JSONDecoder().decode(ReplyMessage<Result>.self, from: response).data
    }

    private func fail(_ message: String) {
        error = message
        if let peripheral { central.cancelPeripheralConnection(peripheral) }
        resetConnection()
        phase = .idle
    }

    private func resetConnection() {
        relayGeneration = UUID()
        for task in relayTasks.values { task.cancel() }
        relayTasks.removeAll()
        for identifier in relayBackgroundTasks.values where identifier != .invalid { UIApplication.shared.endBackgroundTask(identifier) }
        relayBackgroundTasks.removeAll()
        handshake?.cancel()
        endHandshakeBackgroundTask()
        connectionTimeout?.cancel()
        timeout?.cancel()
        let continuation = pending?.continuation
        pending = nil
        continuation?.resume(throwing: CompanionError.message("Connessione interrotta. Verifica l'esito prima di ripetere il comando."))
        rx = nil; tx = nil; status = nil; badgeState = nil; securityRead = false
        frames = FrameBuffer(); outgoing = []
    }

    func refresh() async {
        guard ready, !working, pending == nil else { return }
        do {
            status = try await request("status", body: EmptyMessage())
            badgeState=status?.badge
            if let status, !status.recording, status.notes != remoteNotes.count { try await loadNotes() }
        }
        catch { self.error = error.localizedDescription }
    }

    func loadNotes() async throws {
        let list: NoteList = try await request("notes.list", body: EmptyMessage())
        guard list.notes.count <= 64, list.notes.allSatisfy(\.valid), Set(list.notes.map(\.name)).count == list.notes.count else {
            throw CompanionError.message("Elenco note non valido.")
        }
        remoteNotes = list.notes
        restoreLocalNotes()
    }

    private func restoreLocalNotes() {
        do {
            let local = try archive?.notes() ?? []
            notes = (remoteNotes + local.filter { local in !remoteNotes.contains(where: { $0.name == local.name }) }).sorted { $0.name < $1.name }
            transcripts = Dictionary(uniqueKeysWithValues: notes.compactMap { note in
                archive?.transcript(for: note).map { (note.name, $0) }
            })
        } catch { self.error = "Impossibile leggere le note salvate su iPhone: " + error.localizedDescription }
    }

    func isOnDevice(_ note: VoiceNote) -> Bool { ready && remoteNotes.contains(where: { $0.name == note.name }) }

    func startNoteSynchronization() {
        guard synchronization == nil, !preview,
              ready || notes.contains(where: { localURL(for: $0) != nil && transcripts[$0.name] == nil && transcriptionErrors[$0.name] == nil }) else { return }
        synchronization = Task { [self] in
            noteBackgroundTask = UIApplication.shared.beginBackgroundTask(withName: "Ricezione nota StopWatch") { [weak self] in
                self?.synchronization?.cancel()
                self?.pauseNoteSync()
                self?.endNoteBackgroundTask()
            }
            defer {
                endNoteBackgroundTask()
                synchronization = nil
            }
            repeat {
                let reload = notesRefreshNeeded
                notesRefreshNeeded = false
                do {
                    while working || pending != nil { try await Task.sleep(for: .milliseconds(20)) }
                    try Task.checkCancellation()
                    await refresh()
                    if reload { await refreshNotes() }
                    while !Task.isCancelled, await synchronizeNextNote() { }
                } catch is CancellationError { return }
                catch { self.error = error.localizedDescription }
            } while notesRefreshNeeded && !Task.isCancelled
        }
    }

    private func endNoteBackgroundTask() {
        if noteBackgroundTask != .invalid {
            UIApplication.shared.endBackgroundTask(noteBackgroundTask)
            noteBackgroundTask = .invalid
        }
    }

    private func endHandshakeBackgroundTask() {
        if handshakeBackgroundTask != .invalid {
            UIApplication.shared.endBackgroundTask(handshakeBackgroundTask)
            handshakeBackgroundTask = .invalid
        }
    }

    private func synchronizeNextNote() async -> Bool {
        guard noteSync == nil, !working, status?.recording != true, let archive,
              let note = notes.first(where: {
                  transcripts[$0.name] == nil && transcriptionErrors[$0.name] == nil &&
                  (localURL(for: $0) != nil || isOnDevice($0)) &&
                  (UIApplication.shared.applicationState == .active || localURL(for: $0) == nil)
              }) else { return false }
        syncingNote = note.name
        let task = Task {
            defer { syncingNote = nil; noteSync = nil }
            do {
                let url: URL
                if let local = archive.audioURL(for: note) { url = local }
                else { url = try await download(note) }
                try Task.checkCancellation()
                // Keep the audio received while locked; request Speech access only in the foreground.
                guard UIApplication.shared.applicationState == .active else { return }
                let text = try await transcriber.transcribe(url)
                try Task.checkCancellation()
                try archive.saveTranscript(text, for: note)
                transcripts[note.name] = archive.transcript(for: note)
            } catch is CancellationError { }
            catch { transcriptionErrors[note.name] = error.localizedDescription }
        }
        noteSync = task
        await withTaskCancellationHandler { await task.value } onCancel: { task.cancel() }
        return true
    }

    func pauseNoteSync() { noteSync?.cancel() }

    func cancelNoteSync() {
        if let syncingNote { transcriptionErrors[syncingNote] = "Elaborazione interrotta. Tocca Riprova per continuare." }
        pauseNoteSync()
    }

    func retryTranscription(_ note: VoiceNote) { transcriptionErrors[note.name] = nil }

    func refreshNotes() async {
        guard ready, !working, pending == nil, status?.recording != true else { return }
        working = true
        defer { working = false }
        do { try await loadNotes() } catch { self.error = error.localizedDescription }
    }

    func save(_ value: DeviceConfiguration, part: SettingsPart) async throws {
        guard ready, !working else { throw CompanionError.message("Attendi la connessione al dispositivo.") }
        working = true
        defer { working = false }
        // HA can change favorites while an editor is open on the phone.
        var latest: DeviceConfiguration = try await request("config.get", body: EmptyMessage())
        latest.apply(value, part: part)
        configuration = try await request("config.set", body: latest)
    }

    func output(_ action: String) {
        guard ready, !working else { return }
        working = true
        Task {
            defer { working = false }
            do { let _: EmptyMessage = try await request("output", body: OutputMessage(action: action)) }
            catch { self.error = error.localizedDescription }
        }
    }

    struct RelayActivity: Codable {
        let date: Date
        let kind: String
        let success: Bool
        let background: Bool
        let seconds: TimeInterval
    }

    private func prepareRelay() async {
        guard status?.relaySupported == true, let rememberedID else { relayMessage = "Aggiorna il firmware per usare il ponte"; return }
        do {
            var account = try RelayKeychain.load(rememberedID)
            if account == nil, configuration?.haUrl.hasPrefix("https://") == true, configuration?.haTokenSaved == true {
                let imported: RelayAccount = try await request("relay.credentials", body: EmptyMessage())
                try RelayKeychain.save(imported, device: rememberedID)
                account = imported
            }
            relayConfigured = account != nil; relayURL = account?.url ?? configuration?.haUrl ?? ""
            relayEnabled = account?.enabled == true
            let _: RelayEnabled = try await request("relay.configure", body: RelayEnabled(enabled: relayEnabled))
            relayMessage = relayEnabled ? "Abilitato · usa i dati dell’iPhone quando serve" : "Da configurare o disattivato"
            if relayEnabled { try await testRelay() }
        } catch { relayMessage = error.localizedDescription }
    }

    func saveRelay(url: String, token: String, enabled: Bool) async throws {
        guard let rememberedID, let deviceID = status?.mqttDeviceId, ready, status?.relaySupported == true else {
            throw CompanionError.message("Collega lo StopWatch aggiornato prima di configurare il ponte.")
        }
        let old = try RelayKeychain.load(rememberedID)
        let url = url.trimmingCharacters(in: .whitespacesAndNewlines).trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        let secret = token.isEmpty && old?.url == url ? old?.token ?? "" : token
        let account = RelayAccount(url: url, token: secret, enabled: enabled)
        _ = try account.endpoint
        if enabled { try await HomeRelay(account: account).check(deviceID: deviceID) }
        try RelayKeychain.save(account, device: rememberedID)
        await prepareRelay()
    }

    func testRelay() async throws {
        guard let rememberedID, let account = try RelayKeychain.load(rememberedID), let deviceID = status?.mqttDeviceId else {
            throw CompanionError.message("Configura prima l’accesso remoto a Home Assistant.")
        }
        try await HomeRelay(account: account).check(deviceID: deviceID)
        relayMessage = "Home Assistant raggiungibile · ponte pronto"
    }

    private func handleRelay(_ event: RelayEvent) {
        guard relayTasks[event.kind] == nil, let rememberedID, let deviceID = status?.mqttDeviceId else { return }
        let generation = relayGeneration
        let started = Date()
        let background = UIApplication.shared.applicationState != .active
        relayBackgroundTasks[event.kind] = UIApplication.shared.beginBackgroundTask(withName: "Ponte StopWatch \(event.kind.rawValue)") { [weak self] in
            guard let self, self.relayGeneration == generation else { return }
            self.relayTasks[event.kind]?.cancel()
            if let identifier = self.relayBackgroundTasks.removeValue(forKey: event.kind), identifier != .invalid {
                UIApplication.shared.endBackgroundTask(identifier)
            }
        }
        relayTasks[event.kind] = Task { [self] in
            defer {
                if generation == relayGeneration {
                    relayTasks[event.kind] = nil
                    if let identifier = relayBackgroundTasks.removeValue(forKey: event.kind), identifier != .invalid {
                        UIApplication.shared.endBackgroundTask(identifier)
                    }
                }
            }
            var succeeded = false
            do {
                let result: String
                do {
                    guard let account = try RelayKeychain.load(rememberedID), account.enabled else { throw CompanionError.message("Ponte iPhone disattivato.") }
                    _ = try event.validated()
                    result = try await HomeRelay(account: account).perform(event, deviceID: deviceID)
                    succeeded = true
                } catch is CancellationError { return }
                catch {
                    relayMessage = "Ponte: " + error.localizedDescription
                    result = try event.failure("Ponte iPhone non riuscito: verifica Home Assistant e la rete dati")
                }
                try Task.checkCancellation()
                guard generation == relayGeneration, self.rememberedID == rememberedID else { return }
                let _: EmptyMessage = try await request("relay.reply", body: RelayReply(kind: event.kind, payload: result))
                if succeeded { relayMessage = background ? "Ultimo inoltro riuscito in background" : "Ultimo inoltro riuscito" }
            } catch { if !Task.isCancelled { relayMessage = error.localizedDescription;succeeded = false } }
            if generation == relayGeneration {
                let activity = RelayActivity(date: Date(), kind: event.kind.rawValue, success: succeeded,
                    background: background, seconds: Date().timeIntervalSince(started))
                relayLastActivity = activity
                if let data = try? JSONEncoder().encode(activity) {
                    UserDefaults.standard.set(data, forKey: "relayLastActivity")
                    UserDefaults.standard.set(data, forKey: "relayLastActivity." + event.kind.rawValue)
                }
            }
        }
    }

    func uploadBadge(_ asset: BadgeAsset) async throws {
        guard ready, !working else {
            throw CompanionError.message("Collega lo StopWatch con il firmware Badge aggiornato e termina eventuali registrazioni.")
        }
        working=true; badgeProgress=0; badgeTransferMethod=""; badgeTransferDetail="Preparazione…"; badgeTransferIssue=nil; badgeTransferSeconds=nil
        let started=ProcessInfo.processInfo.systemUptime
        defer { working=false; badgeProgress=nil; badgeWiFiAttempt=nil }
        // BLE may reconnect before Wi-Fi; cached status is not a transfer endpoint.
        var current: DeviceStatus=try await request("status",body: EmptyMessage())
        status=current; badgeState=current.badge
        guard !current.recording, current.badge != nil else {
            throw CompanionError.message("Termina la registrazione e verifica il firmware Badge sullo StopWatch.")
        }
        var transaction: BadgeUpload = try await request("badge.begin",body: BadgeBegin(bytes: asset.data.count,digest: asset.digest))
        do {
            var sent=false
            var attemptedWiFi=false
            if current.wifi {
                for attempt in 0..<2 {
                    if attempt>0 {
                        current=try await request("status",body: EmptyMessage());status=current
                        guard !current.recording else { throw CompanionError.message("Registrazione in corso: invio Badge interrotto.") }
                        if !current.wifi { badgeTransferIssue="Lo StopWatch si è scollegato dal Wi-Fi.";break }
                        transaction=try await request("badge.begin",body: BadgeBegin(bytes:asset.data.count,digest:asset.digest))
                    }
                    attemptedWiFi=true; badgeTransferMethod="Wi-Fi"; badgeProgress=0
                    badgeTransferDetail=attempt==0 ? "Invio via Wi-Fi…" : "Invio via Wi-Fi · tentativo 2 di 2…"
                    do {
                        let offer: NoteTransferOffer=try await request("badge.transfer",body: transaction)
                        if !offer.available { badgeTransferIssue="Il servizio di trasferimento Wi-Fi non è disponibile sullo StopWatch.";break }
                        var request=try offer.request(expectedHost: current.ip,path: "badge")
                        request.httpMethod="POST"; request.setValue("application/octet-stream",forHTTPHeaderField: "Content-Type")
                        guard let encoded=offer.certificate, let certificate=Data(base64Encoded: encoded) else { throw CompanionError.message("Certificato Wi-Fi non valido.") }
                        let attemptID=UUID();badgeWiFiAttempt=attemptID
                        let client=WiFiNoteDownload(host: current.ip,certificate: certificate) { [weak self] progress in
                            Task { @MainActor in
                                guard let self, self.badgeWiFiAttempt==attemptID, self.badgeProgress != nil else { return }
                                // URLSession may replay the body; keep progress within this attempt monotonic.
                                self.badgeProgress=max(self.badgeProgress ?? 0,min(progress,1))
                                if progress>=1 { self.badgeTransferDetail="Attendo conferma dallo StopWatch…" }
                            }
                        }
                        try await client.upload(request,data: asset.data)
                        badgeWiFiAttempt=nil;badgeProgress=1;sent=true;badgeTransferIssue=nil;break
                    } catch is CancellationError { throw CancellationError() }
                    catch {
                        badgeWiFiAttempt=nil
                        try Task.checkCancellation()
                        let remote: BadgeStatus?=try? await request("badge.status",body:EmptyMessage())
                        let failure=error as NSError
                        NSLog("StopWatch Badge Wi-Fi failed: %@/%ld; attempt=%ld received=%ld — %@",failure.domain,failure.code,attempt+1,remote?.uploadReceived ?? -1,failure.localizedDescription)
                        if attempt==0 && WiFiNoteDownload.retryableUploadError(error) { continue }
                        badgeTransferIssue=(remote?.uploadError?.isEmpty == false ? remote?.uploadError : nil) ?? error.localizedDescription
                        break
                    }
                }
            } else { badgeTransferIssue="Lo StopWatch non è collegato al Wi-Fi." }
            if !sent {
                if attemptedWiFi { transaction=try await request("badge.begin",body:BadgeBegin(bytes:asset.data.count,digest:asset.digest)) }
                badgeProgress=0
                badgeTransferMethod="Bluetooth"
                badgeTransferDetail="Invio via Bluetooth…"
                for offset in stride(from: 0,to: asset.data.count,by: 4096) {
                    try Task.checkCancellation()
                    let end=min(offset+4096,asset.data.count)
                    let _: EmptyMessage=try await request("badge.write",body: BadgeChunk(upload: transaction.upload,offset: offset,chunk: asset.data.subdata(in: offset..<end).base64EncodedString()))
                    badgeProgress=Double(end)/Double(asset.data.count)
                }
            }
            badgeTransferDetail="Verifico e salvo il Badge…"
            let saved: BadgeStatus=try await request("badge.commit",body: transaction)
            guard saved.available, saved.digest==asset.digest, saved.bytes==asset.data.count else { throw CompanionError.message("Il dispositivo non ha confermato questo Badge.") }
            badgeState=saved
            badgeTransferSeconds=ProcessInfo.processInfo.systemUptime-started
            if let url=localBadgeURL {
                try FileManager.default.createDirectory(at: url.deletingLastPathComponent(),withIntermediateDirectories: true)
                try asset.data.write(to: url,options: .atomic)
            }
            status=try await request("status",body: EmptyMessage())
        } catch {
            if ready { let _: EmptyMessage?=try? await request("badge.cancel",body: transaction) }
            throw error
        }
    }

    func localURL(for note: VoiceNote) -> URL? {
        archive?.audioURL(for: note)
    }

    func download(_ note: VoiceNote) async throws -> URL {
        guard ready, !working, note.valid, let archive else { throw CompanionError.message("Nota o collegamento non disponibile.") }
        working = true
        let started = ProcessInfo.processInfo.systemUptime
        downloadProgress = 0
        defer { working = false; downloadProgress = nil; receivingViaWiFi = false }
        let current: DeviceStatus=try await request("status",body: EmptyMessage())
        status=current; badgeState=current.badge
        guard !current.recording else { throw CompanionError.message("Termina la registrazione prima di ricevere la nota.") }
        var audio = Data()
        audio.reserveCapacity(note.bytes)
        var method = "Bluetooth"
        if current.wifi {
            do {
                let offer: NoteTransferOffer = try await request("notes.transfer", body: NoteName(name: note.name))
                if offer.available {
                    let request = try offer.request(expectedHost: current.ip)
                    guard let encoded = offer.certificate, let certificate = Data(base64Encoded: encoded) else {
                        throw CompanionError.message("Certificato Wi-Fi non valido.")
                    }
                    receivingViaWiFi = true
                    audio = try await WiFiNoteDownload(host: current.ip, certificate: certificate).download(request, expectedBytes: note.bytes)
                    method = "Wi-Fi"
                    downloadProgress = 1
                }
            } catch {
                try Task.checkCancellation()
                NSLog("StopWatch Wi-Fi transfer failed: %@", error.localizedDescription)
                audio = Data()
                receivingViaWiFi = false
            }
        }
        while audio.count < note.bytes {
            try Task.checkCancellation()
            let chunk: AudioChunk = try await request("notes.read", body: NoteRead(name: note.name, offset: audio.count))
            audio.append(try chunk.decoded(expectedOffset: audio.count, totalBytes: note.bytes))
            downloadProgress = Double(audio.count) / Double(note.bytes)
        }
        try Task.checkCancellation()
        let file = try archive.saveAudio(audio, for: note)
        transferSeconds[note.name] = ProcessInfo.processInfo.systemUptime - started
        transferMethods[note.name] = method
        lastDownloadedURL = file
        return file
    }

    func delete(_ note: VoiceNote) async {
        guard isOnDevice(note), !working, syncingNote != note.name, note.valid else { return }
        working = true
        defer { working = false }
        do {
            let _: EmptyMessage = try await request("notes.delete", body: NoteName(name: note.name))
            try await loadNotes()
        } catch { self.error = error.localizedDescription }
    }
}
