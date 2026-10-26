import Foundation
import Security
import ImageIO
import UniformTypeIdentifiers

struct RelayAccount: Codable, Sendable {
    var url: String
    var token: String
    var enabled = true

    var endpoint: URL {
        get throws {
            guard url.utf8.count <= 256, token.utf8.count <= 2048, !token.isEmpty,
                  !url.contains(where: { $0.isWhitespace }),
                  var parts = URLComponents(string: url), parts.scheme == "https",
                  let host = parts.host, !host.isEmpty, parts.user == nil, parts.password == nil,
                  parts.query == nil, parts.fragment == nil, !parts.path.contains("..") else {
                throw CompanionError.message("Inserisci l’indirizzo HTTPS di Home Assistant e un token valido.")
            }
            parts.scheme = "wss"
            parts.path = parts.path.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
            parts.path = (parts.path.isEmpty ? "" : "/" + parts.path) + "/api/websocket"
            guard let endpoint = parts.url else { throw CompanionError.message("Indirizzo Home Assistant non valido.") }
            return endpoint
        }
    }
}

enum RelayKind: String, Codable, Sendable, Hashable { case favorite, music, artwork }
struct RelayCommand: Decodable, Sendable {
    let request: UInt32
    let entity: String?
    let action: Bool?
    let command: String?
    let source: String?
    let playlist: String?
    let offset: Int?
    let sentAt: Int64?
    let url: String?
    enum CodingKeys: String, CodingKey {
        case request, entity, action, command, source, playlist, offset, url
        case sentAt = "sent_at"
    }
}
struct RelayEvent: Decodable, Sendable {
    let kind: RelayKind
    let payload: String
    let ageMS: UInt32
    enum CodingKeys: String, CodingKey { case kind, payload; case ageMS = "age_ms" }

    func validated(now: Date = Date()) throws -> RelayCommand {
        guard ageMS < 12_000, payload.utf8.count <= 2048 else { throw CompanionError.message("Richiesta scaduta.") }
        let value = try JSONDecoder().decode(RelayCommand.self, from: Data(payload.utf8))
        switch kind {
        case .favorite:
            guard let entity = value.entity, entity.range(of: "^[a-z0-9_]+\\.[a-z0-9_]+$", options: .regularExpression) != nil,
                  entity.utf8.count <= 96, value.action != nil else { throw CompanionError.message("Preferito non valido.") }
        case .music:
            guard let command = value.command,
                  ["refresh", "play", "pause", "next", "previous", "source", "playlists", "playlist"].contains(command),
                  command != "source" || (value.source?.utf8.count ?? 0) > 0 && (value.source?.utf8.count ?? 999) <= 128,
                  command != "playlist" || value.playlist?.range(of: "^spotify:playlist:[A-Za-z0-9]{22}$", options: .regularExpression) != nil,
                  command != "playlists" || value.offset.map({ $0 >= 0 && $0 <= 10_000 && $0 % 8 == 0 }) == true else {
                throw CompanionError.message("Comando Spotify non valido.")
            }
        case .artwork:
            guard let url = value.url, url.utf8.count <= 1280 else { throw CompanionError.message("Copertina non valida.") }
        }
        let changesState = kind == .favorite ? value.action == true : kind == .music && value.command != "refresh" && value.command != "playlists"
        if changesState {
            guard let sent = value.sentAt, (0...5).contains(now.timeIntervalSince1970 - Double(sent)) else {
                throw CompanionError.message("Comando scaduto: verifica lo stato prima di riprovare.")
            }
        }
        return value
    }

    func topic(deviceID: String, suffix: String) throws -> String {
        guard deviceID.range(of: "^stopwatch_[0-9a-f]{12}$", options: .regularExpression) != nil,
              kind != .artwork, ["request", "result"].contains(suffix) else { throw CompanionError.message("Identità StopWatch non valida.") }
        return "stopwatch/\(deviceID)/\(kind.rawValue)/\(suffix)"
    }

    func failure(_ message: String) throws -> String {
        struct Failure: Encodable {
            let request: UInt32
            let success = false
            let entity: String?
            let state = "unknown"
            let error: String
        }
        let command = try JSONDecoder().decode(RelayCommand.self, from: Data(payload.utf8))
        let result = Failure(request: command.request, entity: command.entity, error: String(message.prefix(160)))
        return String(decoding: try JSONEncoder().encode(result), as: UTF8.self)
    }
}
struct RelayReply: Encodable { let kind: RelayKind; let payload: String }
struct RelayEnabled: Codable { let enabled: Bool }

enum RelayKeychain {
    private static let service = "dev.stopwatch.companion.home-relay"
    private static func query(_ device: UUID) -> [String: Any] {
        [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service,
         kSecAttrAccount as String: device.uuidString]
    }
    static func load(_ device: UUID) throws -> RelayAccount? {
        var request = query(device)
        request[kSecReturnData as String] = true
        request[kSecMatchLimit as String] = kSecMatchLimitOne
        var value: CFTypeRef?
        let status = SecItemCopyMatching(request as CFDictionary, &value)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data = value as? Data else { throw CompanionError.message("Portachiavi non disponibile. Sblocca l’iPhone una volta dopo il riavvio.") }
        return try JSONDecoder().decode(RelayAccount.self, from: data)
    }
    static func save(_ account: RelayAccount, device: UUID) throws {
        _ = try account.endpoint
        let attributes: [String: Any] = [kSecValueData as String: try JSONEncoder().encode(account),
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly]
        var status = SecItemUpdate(query(device) as CFDictionary, attributes as CFDictionary)
        if status == errSecItemNotFound {
            status = SecItemAdd(query(device).merging(attributes) { _, new in new } as CFDictionary, nil)
        }
        guard status == errSecSuccess else { throw CompanionError.message("Impossibile salvare il ponte nel portachiavi.") }
    }
    static func remove(_ device: UUID) { SecItemDelete(query(device) as CFDictionary) }
}

private final class RelayRedirects: NSObject, URLSessionTaskDelegate, @unchecked Sendable {
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) { completionHandler(nil) }
}

struct HomeRelay {
    let account: RelayAccount

    struct Frame: Decodable {
        let id: Int?
        let type: String
        let success: Bool?
        let event: Event?
        struct Event: Decodable {
            let variables: Variables
            struct Variables: Decodable {
                let trigger: Trigger
                struct Trigger: Decodable { let topic: String; let payload: String }
            }
        }
        func echoes(_ payload: String, topic: String) -> Bool {
            type == "event" && id == 1 && event?.variables.trigger.topic == topic && event?.variables.trigger.payload == payload
        }
        func result(event request: RelayEvent, deviceID: String, requestID: UInt32) throws -> String? {
            guard type == "event", id == 1, let trigger = event?.variables.trigger,
                  trigger.topic == (try request.topic(deviceID: deviceID, suffix: "result")) else { return nil }
            guard trigger.payload.utf8.count <= (request.kind == .favorite ? 2048 : 4096) else { throw CompanionError.message("Risposta Home Assistant troppo grande.") }
            struct Header: Decodable { let request: UInt32; let success: Bool }
            guard let header = try? JSONDecoder().decode(Header.self, from: Data(trigger.payload.utf8)) else { return nil }
            return header.request == requestID ? trigger.payload : nil
        }
    }
    private struct Command: Encodable {
        var type: String
        var id: Int? = nil
        var access_token: String? = nil
        var trigger: Trigger? = nil
        var domain: String? = nil
        var service: String? = nil
        var service_data: Publish? = nil
        struct Trigger: Encodable { let platform = "mqtt"; let topic: String; let qos = 0 }
        struct Publish: Encodable { let topic: String; let payload: String; let qos = 0; let retain = false }
    }

    private func session() -> URLSession {
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 8
        config.timeoutIntervalForResource = 9
        config.waitsForConnectivity = false
        config.httpShouldSetCookies = false
        config.urlCache = nil
        return URLSession(configuration: config, delegate: RelayRedirects(), delegateQueue: nil)
    }

    func check(deviceID: String) async throws { _ = try await exchange(nil, deviceID: deviceID) }

    func perform(_ event: RelayEvent, deviceID: String) async throws -> String {
        _ = try event.validated()
        return event.kind == .artwork ? try await artwork(event) : try await exchange(event, deviceID: deviceID)
    }

    private func exchange(_ event: RelayEvent?, deviceID: String) async throws -> String {
        let endpoint = try account.endpoint
        let client = session()
        let socket = client.webSocketTask(with: endpoint)
        socket.maximumMessageSize = 32 * 1024
        let deadline = Task {
            try? await Task.sleep(for: .seconds(8))
            if !Task.isCancelled { socket.cancel(with: .goingAway, reason: nil) }
        }
        defer { deadline.cancel();socket.cancel(with: .normalClosure, reason: nil);client.invalidateAndCancel() }
        return try await withTaskCancellationHandler {
            socket.resume()
            func receive() async throws -> Frame {
                let message = try await socket.receive()
                let data: Data
                switch message { case .string(let text): data = Data(text.utf8); case .data(let bytes): data = bytes; @unknown default: throw CompanionError.message("Risposta Home Assistant non valida.") }
                return try JSONDecoder().decode(Frame.self, from: data)
            }
            func send(_ command: Command) async throws {
                try Task.checkCancellation()
                try await socket.send(.string(String(decoding: try JSONEncoder().encode(command), as: UTF8.self)))
            }
            guard try await receive().type == "auth_required", let origin = socket.response?.url,
                  origin.host == endpoint.host, origin.port == endpoint.port, origin.scheme == endpoint.scheme || origin.scheme == "https" else {
                throw CompanionError.message("Connessione Home Assistant non valida.")
            }
            try await send(Command(type: "auth", access_token: account.token))
            guard try await receive().type == "auth_ok" else { throw CompanionError.message("Home Assistant: token non valido.") }
            let subscription = try event?.topic(deviceID: deviceID, suffix: "result") ?? "stopwatch/\(deviceID)/favorite/result"
            guard deviceID.range(of: "^stopwatch_[0-9a-f]{12}$", options: .regularExpression) != nil else { throw CompanionError.message("Identità StopWatch non valida.") }
            try await send(Command(type: "subscribe_trigger", id: 1, trigger: .init(topic: subscription)))
            let subscribed = try await receive()
            guard subscribed.id == 1, subscribed.type == "result", subscribed.success == true else {
                throw CompanionError.message("Home Assistant: il ponte richiede un token amministratore e MQTT attivo.")
            }
            // HA batches MQTT subscriptions; its WebSocket ACK can precede the broker SUBACK.
            let probe = "{\"relay_probe\":\"\(UUID().uuidString)\"}"
            let probes = Task {
                for id in 2...6 {
                    try Task.checkCancellation()
                    let command = Command(type: "call_service", id: id, domain: "mqtt", service: "publish",
                        service_data: .init(topic: subscription, payload: probe))
                    try await socket.send(.string(String(decoding: JSONEncoder().encode(command), as: UTF8.self)))
                    try await Task.sleep(for: .milliseconds(250))
                }
            }
            defer { probes.cancel() }
            while true {
                let frame = try await receive()
                if frame.echoes(probe, topic: subscription) { break }
                if let id = frame.id, (2...6).contains(id), frame.type == "result", frame.success != true {
                    throw CompanionError.message("Il broker MQTT non ha confermato il ponte.")
                }
            }
            probes.cancel()
            _ = try? await probes.value
            guard let event else { return "" }
            let command = try event.validated()
            // Subscribe first; publish once. An uncertain outcome must never replay a toggle or skip.
            try await send(Command(type: "call_service", id: 7, domain: "mqtt", service: "publish",
                service_data: .init(topic: try event.topic(deviceID: deviceID, suffix: "request"), payload: event.payload)))
            while true {
                let frame = try await receive()
                if frame.id == 7, frame.type == "result", frame.success != true { throw CompanionError.message("Home Assistant non ha confermato l’invio. Verifica lo stato.") }
                if let result = try frame.result(event: event, deviceID: deviceID, requestID: command.request) { return result }
            }
        } onCancel: { socket.cancel(with: .goingAway, reason: nil) }
    }

    func artworkURL(_ value: String) throws -> (URL, Bool) {
        guard value.utf8.count <= 1280, !value.contains(where: { $0.isWhitespace }), !value.contains("\\"),
              let source = URLComponents(string: value), source.user == nil, source.password == nil, source.fragment == nil else {
            throw CompanionError.message("Indirizzo copertina non valido.")
        }
        if source.path.hasPrefix("/api/media_player_proxy/media_player."), !source.path.contains(".."), source.query != nil {
            var target = URLComponents(url: try account.endpoint, resolvingAgainstBaseURL: false)!
            target.scheme = "https"
            target.path = String(target.path.dropLast("/api/websocket".count)) + source.path
            target.query = source.query
            guard let url = target.url else { throw CompanionError.message("Copertina Home Assistant non valida.") }
            return (url, true)
        }
        guard source.scheme == "https", source.port == nil,
              ["i.scdn.co", "image-cdn-ak.spotifycdn.com", "image-cdn-fa.spotifycdn.com"].contains(source.host),
              source.path.hasPrefix("/image/"), source.path.count > 7, let url = source.url else {
            throw CompanionError.message("Origine copertina non consentita.")
        }
        return (url, false)
    }

    private func artwork(_ event: RelayEvent) async throws -> String {
        let command = try event.validated()
        let (url, authorize) = try artworkURL(command.url ?? "")
        var request = URLRequest(url: url)
        if authorize { request.setValue("Bearer " + account.token, forHTTPHeaderField: "Authorization") }
        let client = session()
        defer { client.invalidateAndCancel() }
        let (stream, response) = try await client.bytes(for: request)
        guard let response = response as? HTTPURLResponse, response.statusCode == 200,
              response.expectedContentLength <= 384 * 1024 else { throw CompanionError.message("Copertina non disponibile.") }
        var bytes = Data()
        for try await byte in stream {
            guard bytes.count < 384 * 1024 else { throw CompanionError.message("Copertina troppo grande.") }
            bytes.append(byte)
        }
        guard let source = CGImageSourceCreateWithData(bytes as CFData, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? Int, let height = properties[kCGImagePropertyPixelHeight] as? Int,
              width > 0, height > 0, width <= 2048, height <= 2048,
              let thumbnail = CGImageSourceCreateThumbnailAtIndex(source, 0, [kCGImageSourceCreateThumbnailFromImageAlways: true,
                kCGImageSourceCreateThumbnailWithTransform: true, kCGImageSourceThumbnailMaxPixelSize: 188] as CFDictionary) else {
            throw CompanionError.message("Immagine copertina non valida.")
        }
        for quality in [0.65, 0.45, 0.25, 0.1] {
            let data = NSMutableData()
            guard let encoder = CGImageDestinationCreateWithData(data, UTType.jpeg.identifier as CFString, 1, nil) else { break }
            CGImageDestinationAddImage(encoder, thumbnail, [kCGImageDestinationLossyCompressionQuality: quality] as CFDictionary)
            guard CGImageDestinationFinalize(encoder), data.length <= 6000 else { continue }
            struct Artwork: Encodable { let request: UInt32; let success = true; let jpeg: String }
            let jsonEncoder = JSONEncoder()
            jsonEncoder.outputFormatting = .withoutEscapingSlashes
            return String(decoding: try jsonEncoder.encode(Artwork(request: command.request, jpeg: (data as Data).base64EncodedString())), as: UTF8.self)
        }
        throw CompanionError.message("Copertina non comprimibile entro il limite Bluetooth.")
    }
}
