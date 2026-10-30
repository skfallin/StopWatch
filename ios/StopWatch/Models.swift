import Foundation

enum AppLocalization {
    static var bundle: Bundle {
        #if SWIFT_PACKAGE
        return .module
        #else
        return .main
        #endif
    }
}

enum SettingsPart: String, CaseIterable, Identifiable {
    case device, wifi, home, mqtt, badge, clock, launcher, presentation, regional
    var id: Self { self }
    var title: String {
        switch self { case .regional: String(localized: "Lingua e fuso orario", bundle: AppLocalization.bundle); case .device: String(localized: "Display e feedback", bundle: AppLocalization.bundle); case .wifi: "Wi-Fi"; case .home: String(localized: "Preferiti Home Assistant", bundle: AppLocalization.bundle); case .mqtt: String(localized: "Integrazione Home Assistant", bundle: AppLocalization.bundle); case .badge: "Badge"; case .clock: String(localized: "Orologio", bundle: AppLocalization.bundle); case .launcher: String(localized: "Home e applicazioni", bundle: AppLocalization.bundle); case .presentation: String(localized: "Presentazioni", bundle: AppLocalization.bundle) }
    }
    var symbol: String {
        switch self { case .regional: "globe"; case .device: "slider.horizontal.3"; case .wifi: "wifi"; case .home: "star"; case .mqtt: "house"; case .badge: "photo.badge.checkmark"; case .clock: "clock"; case .launcher: "circle.grid.2x2"; case .presentation: "display" }
    }
}

struct Favorite: Codable, Identifiable, Equatable {
    var id = UUID()
    var label: String
    var entity: String
    enum CodingKeys: String, CodingKey { case label, entity }
}

struct BadgeConfiguration: Codable, Equatable, Sendable {
    var autoRotate = true
    var animate = true
    var keepAwake = false
    enum CodingKeys: String, CodingKey { case autoRotate = "auto_rotate", animate, keepAwake = "keep_awake" }
}
struct BadgeStatus: Decodable, Equatable, Sendable {
    let available: Bool
    let frames: Int
    let bytes: Int
    let durationMS: Int
    let digest: String
    let error: String
    let uploading: Bool
    let uploadReceived: Int?
    let uploadError: String?
    enum CodingKeys: String, CodingKey {
        case available, frames, bytes, digest, error, uploading
        case durationMS = "duration_ms", uploadReceived = "upload_received", uploadError = "upload_error"
    }
}
struct BadgeBegin: Encodable { let bytes: Int; let digest: String }
struct BadgeUpload: Codable { let upload: String }
struct BadgeChunk: Encodable { let upload: String; let offset: Int; let chunk: String }

enum HomeApplication: String, CaseIterable, Identifiable {
    case home, spotify, notes, badge, clock, focus, system, usb, key, slides
    var id: String { rawValue }
    var title: String {
        switch self { case .home: String(localized: "Casa", bundle: AppLocalization.bundle); case .spotify: "Spotify"; case .notes: String(localized: "Note", bundle: AppLocalization.bundle); case .badge: "Badge"; case .clock: String(localized: "Orologio", bundle: AppLocalization.bundle); case .focus: "Focus"; case .system: String(localized: "Sistema", bundle: AppLocalization.bundle); case .usb: String(localized: "Ponte", bundle: AppLocalization.bundle); case .key: String(localized: "Chiave", bundle: AppLocalization.bundle); case .slides: "Slide" }
    }
    var group: String {
        switch self { case .home, .spotify: String(localized: "Casa", bundle: AppLocalization.bundle); case .notes, .badge, .clock, .focus: String(localized: "Tasca", bundle: AppLocalization.bundle); default: String(localized: "Strumenti", bundle: AppLocalization.bundle) }
    }
}

struct DeviceConfiguration: Codable, Equatable {
    var language = "en"
    var timeZone = "UTC"
    var timeZones: [String] = []
    var homeApps = HomeApplication.allCases.map(\.rawValue)
    var presentationPageKeys = false
    var brightness = 160
    var volume = 80
    var noteVolume = 80
    var soundPack = "microclick"
    var colorTheme = "lime"
    var badge = BadgeConfiguration()
    var clockFace = "digital"
    var clockKeepAwake = false
    var vibration = 120
    var screenTimeout = 45
    var ssid = ""
    var wifiPassword = ""
    var wifiPasswordSaved = false
    var haUrl = ""
    var haToken = ""
    var haTokenSaved = false
    var haCa = ""
    var favorites: [Favorite] = []
    var mqttHost = ""
    var mqttPort = 1883
    var mqttTls = false
    var mqttUser = ""
    var mqttPassword = ""
    var mqttPasswordSaved = false
    var mqttCa = ""

    enum CodingKeys: String, CodingKey {
        case language, timeZone = "time_zone", timeZones = "time_zones"
        case homeApps = "home_apps", presentationPageKeys = "presentation_page_keys"
        case brightness, volume, vibration, ssid, favorites
        case noteVolume = "note_volume"
        case soundPack = "sound_pack", colorTheme = "color_theme", badge
        case clockFace = "clock_face", clockKeepAwake = "clock_keep_awake"
        case screenTimeout = "screen_timeout", wifiPassword = "wifi_password", wifiPasswordSaved = "wifi_password_saved"
        case haUrl = "ha_url", haToken = "ha_token", haTokenSaved = "ha_token_saved", haCa = "ha_ca"
        case mqttHost = "mqtt_host", mqttPort = "mqtt_port", mqttTls = "mqtt_tls", mqttUser = "mqtt_user"
        case mqttPassword = "mqtt_password", mqttPasswordSaved = "mqtt_password_saved", mqttCa = "mqtt_ca"
    }

    init() {}

    mutating func apply(_ edited: DeviceConfiguration, part: SettingsPart) {
        switch part {
        case .regional: language = edited.language; timeZone = edited.timeZone
        case .launcher: homeApps = edited.homeApps
        case .presentation: presentationPageKeys = edited.presentationPageKeys
        case .device:
            brightness = edited.brightness; volume = edited.volume
            noteVolume = edited.noteVolume
            vibration = edited.vibration; screenTimeout = edited.screenTimeout
            soundPack = edited.soundPack; colorTheme = edited.colorTheme
        case .wifi:
            ssid = edited.ssid; wifiPassword = edited.wifiPassword
        case .home:
            haUrl = edited.haUrl; haToken = edited.haToken; haCa = edited.haCa
            favorites = edited.favorites
        case .mqtt:
            mqttHost = edited.mqttHost; mqttPort = edited.mqttPort; mqttTls = edited.mqttTls
            mqttUser = edited.mqttUser; mqttPassword = edited.mqttPassword; mqttCa = edited.mqttCa
        case .badge: badge = edited.badge
        case .clock:
            clockFace = edited.clockFace; clockKeepAwake = edited.clockKeepAwake
        }
    }
    init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        language = try values.decodeIfPresent(String.self, forKey: .language) ?? "it"
        timeZone = try values.decodeIfPresent(String.self, forKey: .timeZone) ?? "Europe/Rome"
        timeZones = try values.decodeIfPresent([String].self, forKey: .timeZones) ?? []
        homeApps = try values.decodeIfPresent([String].self, forKey: .homeApps) ?? HomeApplication.allCases.map(\.rawValue)
        presentationPageKeys = try values.decodeIfPresent(Bool.self, forKey: .presentationPageKeys) ?? false
        brightness = try values.decode(Int.self, forKey: .brightness)
        volume = try values.decode(Int.self, forKey: .volume)
        noteVolume = try values.decodeIfPresent(Int.self, forKey: .noteVolume) ?? volume
        soundPack = try values.decodeIfPresent(String.self, forKey: .soundPack) ?? "microclick"
        if ["minimal", "soft", "glass", "arcade", "mechanical", "organic", "dreamy", "scifi",
            "rubber", "cinematic", "studio", "zen"].contains(soundPack) { soundPack = "microclick" }
        colorTheme = try values.decodeIfPresent(String.self, forKey: .colorTheme) ?? "lime"
        badge = try values.decodeIfPresent(BadgeConfiguration.self, forKey: .badge) ?? BadgeConfiguration()
        clockFace = try values.decodeIfPresent(String.self, forKey: .clockFace) ?? "digital"
        clockKeepAwake = try values.decodeIfPresent(Bool.self, forKey: .clockKeepAwake) ?? false
        vibration = try values.decode(Int.self, forKey: .vibration)
        screenTimeout = try values.decode(Int.self, forKey: .screenTimeout)
        ssid = try values.decode(String.self, forKey: .ssid)
        wifiPasswordSaved = try values.decode(Bool.self, forKey: .wifiPasswordSaved)
        haUrl = try values.decode(String.self, forKey: .haUrl)
        haTokenSaved = try values.decode(Bool.self, forKey: .haTokenSaved)
        haCa = try values.decode(String.self, forKey: .haCa)
        favorites = try values.decode([Favorite].self, forKey: .favorites)
        mqttHost = try values.decode(String.self, forKey: .mqttHost)
        mqttPort = try values.decode(Int.self, forKey: .mqttPort)
        mqttTls = try values.decode(Bool.self, forKey: .mqttTls)
        mqttUser = try values.decode(String.self, forKey: .mqttUser)
        mqttPasswordSaved = try values.decode(Bool.self, forKey: .mqttPasswordSaved)
        mqttCa = try values.decode(String.self, forKey: .mqttCa)
        // Secrets remain on the device; an empty editor value preserves them there.
    }
}

struct DeviceStatus: Decodable {
    let `protocol`: Int
    let name: String
    let battery: Int
    let charging: Bool
    let wifi: Bool
    let rssi: Int?
    let ip: String
    let storageReady: Bool
    let storageUsed: Int
    let storageTotal: Int
    let psramFree: Int
    let notes: Int
    let recording: Bool
    let noteSeconds: Double
    let bondedPhones: Int
    let microphone: Bool
    let imu: Bool
    let rtc: Bool
    let touch: Bool
    let buttonA: Bool
    let buttonB: Bool
    let mqttConnected: Bool
    let mqttDeviceId: String
    let relaySupported: Bool?
    let relayEnabled: Bool?
    let connectionRoute: String?
    let acceleration: [Double]
    let badge: BadgeStatus?
    let usb: USBControlStatus?

    enum CodingKeys: String, CodingKey {
        case `protocol`, name, battery, charging, wifi, rssi, ip, notes, recording, microphone, imu, rtc, touch, acceleration, badge, usb
        case storageReady = "storage_ready", storageUsed = "storage_used", storageTotal = "storage_total", psramFree = "psram_free"
        case noteSeconds = "note_seconds", bondedPhones = "bonded_phones", buttonA = "button_a", buttonB = "button_b"
        case mqttConnected = "mqtt_connected", mqttDeviceId = "mqtt_device_id"
        case relaySupported = "relay_supported", relayEnabled = "relay_enabled", connectionRoute = "connection_route"
    }
}

struct VoiceNote: Decodable, Identifiable, Equatable {
    let name: String
    let bytes: Int
    let seconds: Double
    var id: String { name }
    var title: String { String(localized: "Nota · ", bundle: AppLocalization.bundle) + String(name.dropFirst(5).prefix(6)).uppercased() }
    var duration: String { String(format: "%d:%02d", Int(seconds) / 60, Int(seconds) % 60) }
    var valid: Bool {
        name.range(of: #"^note-[0-9a-f]{16}\.wav$"#, options: .regularExpression) != nil &&
        (44...1_920_044).contains(bytes) && seconds.isFinite && (0...60).contains(seconds)
    }
}

struct NoteList: Decodable { let notes: [VoiceNote] }
struct EmptyMessage: Codable {}
struct EpochMessage: Encodable { let epoch: Int64 }
struct OutputMessage: Encodable { let action: String }
struct NoteRead: Encodable { let name: String; let offset: Int; let length: Int = 4096 }
struct NoteName: Encodable { let name: String }
struct NoteTransferOffer: Decodable {
    let available: Bool
    let host: String?
    let port: Int?
    let ticket: String?
    let certificate: String?

    func request(expectedHost: String, path: String = "note") throws -> URLRequest {
        guard ["note","badge"].contains(path), available, let host, host == expectedHost,
              host.split(separator: ".", omittingEmptySubsequences: false).count == 4,
              host.split(separator: ".", omittingEmptySubsequences: false).allSatisfy({ !$0.isEmpty && $0.allSatisfy(\.isNumber) && UInt8($0) != nil }),
              port == 8443, let ticket, ticket.range(of: #"^[0-9a-f]{64}$"#, options: .regularExpression) != nil,
              let certificate, let decoded = Data(base64Encoded: certificate), (100...1536).contains(decoded.count) else {
            throw CompanionError.message(String(localized: "Parametri del trasferimento Wi-Fi non validi.", bundle: AppLocalization.bundle))
        }
        var components = URLComponents()
        components.scheme = "https"; components.host = host; components.port = port; components.path = "/" + path
        guard let url = components.url else { throw CompanionError.message(String(localized: "Indirizzo Wi-Fi non valido.", bundle: AppLocalization.bundle)) }
        var request = URLRequest(url: url)
        request.setValue("Bearer " + ticket, forHTTPHeaderField: "Authorization")
        return request
    }
}
struct AudioChunk: Decodable {
    let offset: Int
    let total: Int
    let audio: String

    func decoded(expectedOffset: Int, totalBytes: Int) throws -> Data {
        guard offset == expectedOffset, total == totalBytes,
              let bytes = Data(base64Encoded: audio), !bytes.isEmpty, bytes.count <= 4096,
              expectedOffset >= 0, expectedOffset + bytes.count <= totalBytes else {
            throw CompanionError.message(String(localized: "Trasferimento audio incompleto. L'originale è ancora sullo StopWatch.", bundle: AppLocalization.bundle))
        }
        return bytes
    }
}

enum CompanionError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case let .message(text) = self { return text }; return nil }
}

struct RequestMessage<Body: Encodable>: Encodable {
    let id: UInt32; let op: String; let data: Body
    func encoded() throws -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = .withoutEscapingSlashes
        return try encoder.encode(self)
    }
}
struct ReplyHeader: Decodable { let id: UInt32; let ok: Bool; let error: String?; let event: String? }
struct ReplyMessage<Body: Decodable>: Decodable { let data: Body }

struct FrameBuffer {
    static let maximumBytes = 12_288
    private var bytes = Data()
    mutating func append(_ chunk: Data) throws -> [Data] {
        var lines: [Data] = []
        for byte in chunk {
            if byte == 10 {
                guard !bytes.isEmpty else { throw CompanionError.message(String(localized: "Risposta Bluetooth non valida.", bundle: AppLocalization.bundle)) }
                lines.append(bytes)
                bytes = Data()
            } else {
                guard byte != 0, bytes.count < Self.maximumBytes else {
                    throw CompanionError.message(String(localized: "Risposta Bluetooth troppo grande o non valida.", bundle: AppLocalization.bundle))
                }
                bytes.append(byte)
            }
        }
        return lines
    }
}
