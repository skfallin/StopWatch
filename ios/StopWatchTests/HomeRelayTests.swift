import XCTest
@testable import StopWatch

final class HomeRelayTests: XCTestCase {
    func testBrightnessCommandsAreBoundedLightActions() throws {
        let now = Date(timeIntervalSince1970: 2_000_000_000)
        for level in [0, 1, 62, 100] {
            let request = RelayEvent(kind: .favorite, payload: "{\"request\":12,\"entity\":\"light.camera\",\"action\":true,\"brightness\":\(level),\"sent_at\":2000000000}", ageMS: 0)
            XCTAssertEqual(try request.validated(now: now).brightness, level)
            XCTAssertThrowsError(try request.validated(now: now.addingTimeInterval(6)))
        }
        for pair in [("light.camera", "true", "-1"), ("light.camera", "true", "101"),
                     ("light.camera", "false", "50"), ("switch.presa", "true", "50"),
                     ("light.camera", "true", "true"), ("light.camera", "true", "1.5")] {
            let request = RelayEvent(kind: .favorite, payload: "{\"request\":12,\"entity\":\"\(pair.0)\",\"action\":\(pair.1),\"brightness\":\(pair.2),\"sent_at\":2000000000}", ageMS: 0)
            XCTAssertThrowsError(try request.validated(now: now))
        }
    }
    private let device = "stopwatch_ec6244858428"
    private let account = RelayAccount(url: "https://casa.example.it", token: "test-token")
    private func event(_ payload: String, kind: RelayKind = .music, age: UInt32 = 0) -> RelayEvent {
        RelayEvent(kind: kind, payload: payload, ageMS: age)
    }

    func testHTTPSOriginAndWebSocketPath() throws {
        XCTAssertEqual(try account.endpoint.absoluteString, "wss://casa.example.it/api/websocket")
        XCTAssertEqual(try RelayAccount(url: "https://casa.example.it/ha/", token: "x").endpoint.path, "/ha/api/websocket")
        for url in ["http://casa.example.it", "wss://casa.example.it", "https://user:pass@casa.example.it", "https://casa.example.it?token=x", "https://casa.example.it#x", "https://casa.example.it/../x", "https://casa.example.it/\n"] {
            XCTAssertThrowsError(try RelayAccount(url: url, token: "x").endpoint)
        }
        XCTAssertThrowsError(try RelayAccount(url: account.url, token: "").endpoint)
    }

    func testOnlyExpectedDeviceTopicsCanBePublished() throws {
        let request = event("{\"request\":12,\"command\":\"refresh\"}")
        XCTAssertEqual(try request.topic(deviceID: device, suffix: "request"), "stopwatch/\(device)/music/request")
        for id in ["stopwatch_+/set/beep", "stopwatch_123", "another_device", "stopwatch_EC6244858428"] {
            XCTAssertThrowsError(try request.topic(deviceID: id, suffix: "request"))
        }
        XCTAssertThrowsError(try request.topic(deviceID: device, suffix: "set/beep"))
    }

    func testExpiredActionsAreNotPublished() throws {
        let now = Date(timeIntervalSince1970: 2_000_000_000)
        let action = event("{\"request\":12,\"entity\":\"light.camera\",\"action\":true,\"sent_at\":2000000000}", kind: .favorite)
        XCTAssertNoThrow(try action.validated(now: now))
        XCTAssertThrowsError(try action.validated(now: now.addingTimeInterval(5.01)))
        XCTAssertThrowsError(try action.validated(now: now.addingTimeInterval(-1)))
        let read = event("{\"request\":12,\"entity\":\"light.camera\",\"action\":false}", kind: .favorite)
        XCTAssertNoThrow(try read.validated(now: now))
        XCTAssertThrowsError(try event(read.payload, kind: .favorite, age: 12_000).validated(now: now))
        XCTAssertThrowsError(try event("{\"request\":true,\"command\":\"refresh\"}").validated())
    }

    func testMusicCommandsAndCatalogBounds() throws {
        for offset in [0, 8, 10_000] {
            XCTAssertNoThrow(try event("{\"request\":1,\"command\":\"playlists\",\"offset\":\(offset)}").validated())
        }
        for payload in ["{\"request\":1,\"command\":\"set_volume\"}", "{\"request\":1,\"command\":\"playlists\",\"offset\":7}", "{\"request\":1,\"command\":\"playlists\",\"offset\":10008}", "{\"request\":1,\"command\":\"source\",\"source\":\"\"}"] {
            XCTAssertThrowsError(try event(payload).validated())
        }
        XCTAssertThrowsError(try event(String(repeating: "x", count: 2049)).validated())
    }

    func testResultCorrelationBeforeReturningItToWatch() throws {
        let request = event("{\"request\":12,\"command\":\"refresh\"}")
        func frame(id: Int = 1, topic: String? = nil, payload: String = "{\"request\":12,\"success\":true}") throws -> HomeRelay.Frame {
            let json: [String: Any] = ["type": "event", "id": id, "event": ["variables": ["trigger": ["topic": topic ?? "stopwatch/\(device)/music/result", "payload": payload]]]]
            return try JSONDecoder().decode(HomeRelay.Frame.self, from: JSONSerialization.data(withJSONObject: json))
        }
        XCTAssertNotNil(try frame().result(event: request, deviceID: device, requestID: 12))
        XCTAssertNil(try frame(id: 2).result(event: request, deviceID: device, requestID: 12))
        XCTAssertNil(try frame(topic: "stopwatch/another/music/result").result(event: request, deviceID: device, requestID: 12))
        XCTAssertNil(try frame(payload: "{\"request\":11,\"success\":true}").result(event: request, deviceID: device, requestID: 12))
        let probe = "{\"relay_probe\":\"unique-readiness-marker\"}"
        let readiness = try frame(payload: probe)
        XCTAssertTrue(readiness.echoes(probe, topic: "stopwatch/\(device)/music/result"))
        XCTAssertFalse(readiness.echoes("another marker", topic: "stopwatch/\(device)/music/result"))
        XCTAssertFalse(readiness.echoes(probe, topic: "stopwatch/another/music/result"))
        XCTAssertNil(try readiness.result(event: request, deviceID: device, requestID: 12))
        XCTAssertThrowsError(try frame(payload: String(repeating: "a", count: 4097)).result(event: request, deviceID: device, requestID: 12))
    }

    func testFailureKeepsFavoriteIdentityAndUncertainOutcome() throws {
        let request = event("{\"request\":12,\"entity\":\"light.camera\",\"action\":true}", kind: .favorite)
        let result = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(request.failure("Esito incerto").utf8)) as? [String: Any])
        XCTAssertEqual(result["request"] as? Int, 12)
        XCTAssertEqual(result["entity"] as? String, "light.camera")
        XCTAssertEqual(result["success"] as? Bool, false)
        XCTAssertEqual(result["state"] as? String, "unknown")
    }

    func testCoverProxyUsesRemoteHomeAssistantAndNeverAuthenticatesSpotifyCDN() throws {
        let relay = HomeRelay(account: account)
        let (proxy, authorized) = try relay.artworkURL("http://192.168.0.150:8123/api/media_player_proxy/media_player.spotify?token=example")
        XCTAssertEqual(proxy.absoluteString, "https://casa.example.it/api/media_player_proxy/media_player.spotify?token=example")
        XCTAssertTrue(authorized)
        let (cdn, cdnAuthorization) = try relay.artworkURL("https://i.scdn.co/image/123")
        XCTAssertEqual(cdn.host, "i.scdn.co"); XCTAssertFalse(cdnAuthorization)
        for url in ["https://i.scdn.co.attacker.example/image/123", "http://i.scdn.co/image/123", "https://example.com/image/123", "https://user:pass@i.scdn.co/image/123", "https://i.scdn.co/image/123#x"] {
            XCTAssertThrowsError(try relay.artworkURL(url))
        }
    }

    func testArtworkReplyFitsExistingEncryptedBluetoothFrames() throws {
        let payload = "{\"request\":12,\"success\":true,\"jpeg\":\"" + Data(repeating: 0xff, count: 6000).base64EncodedString() + "\"}"
        let reply = try RequestMessage(id: 12, op: "relay.reply", data: RelayReply(kind: .artwork, payload: payload)).encoded()
        XCTAssertLessThan(reply.count, FrameBuffer.maximumBytes)
        var buffer = FrameBuffer()
        var result: [Data] = []
        for offset in stride(from: 0, to: reply.count, by: 185) {
            result += try buffer.append(Data(reply[offset..<min(offset + 185, reply.count)]))
        }
        result += try buffer.append(Data([10]))
        XCTAssertEqual(result, [reply])
    }
}
