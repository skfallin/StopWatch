import XCTest
@testable import StopWatch

final class ProtocolTests: XCTestCase {
    func testHomeAndPresentationEditsPreserveOtherConfiguration() throws {
        var current = DeviceConfiguration(), edited = DeviceConfiguration()
        current.volume = 180; current.clockFace = "halo"
        current.favorites = [Favorite(label: "Studio", entity: "light.studio")]
        edited.homeApps = ["system", "spotify", "home"]
        current.apply(edited, part: .launcher)
        XCTAssertEqual(current.homeApps, edited.homeApps)
        XCTAssertEqual(current.volume, 180); XCTAssertEqual(current.clockFace, "halo")
        XCTAssertEqual(current.favorites.first?.entity, "light.studio")
        edited.presentationPageKeys = true; current.apply(edited, part: .presentation)
        let restored = try JSONDecoder().decode(DeviceConfiguration.self, from: JSONEncoder().encode(current))
        XCTAssertTrue(restored.presentationPageKeys); XCTAssertEqual(restored.homeApps, edited.homeApps)
        var old = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(current)) as? [String: Any])
        old.removeValue(forKey: "home_apps"); old.removeValue(forKey: "presentation_page_keys")
        let legacy = try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: old))
        XCTAssertEqual(legacy.homeApps, HomeApplication.allCases.map(\.rawValue)); XCTAssertFalse(legacy.presentationPageKeys)
    }

    func testMicroclickMigrationPreservesSilenceAndOtherSettings() throws {
        var settings = DeviceConfiguration()
        settings.ssid = "Casa"; settings.volume = 140; settings.noteVolume = 60
        settings.clockFace = "halo"; settings.badge.keepAwake = true
        var wire = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(settings)) as? [String: Any])
        for legacy in ["minimal", "soft", "glass", "arcade", "mechanical", "organic", "dreamy",
                       "scifi", "rubber", "cinematic", "studio", "zen", "microclick", "off"] {
            wire["sound_pack"] = legacy
            var expected = settings
            expected.soundPack = legacy == "off" ? "off" : "microclick"
            let restored = try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: wire))
            XCTAssertEqual(restored, expected)
        }
        wire.removeValue(forKey: "sound_pack")
        XCTAssertEqual(try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: wire)), settings)
    }

    func testClockConfigurationWireFormatAndLegacyDefaults() throws {
        for face in ["digital", "analog", "halo", "duo", "orbit", "segments"] {
            var configuration = DeviceConfiguration()
            configuration.clockFace = face; configuration.clockKeepAwake = true
            let data = try JSONEncoder().encode(configuration)
            let wire = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
            XCTAssertEqual(wire["clock_face"] as? String, face)
            XCTAssertEqual(wire["clock_keep_awake"] as? Bool, true)
            XCTAssertEqual(try JSONDecoder().decode(DeviceConfiguration.self, from: data), configuration)
        }
        var legacy = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(DeviceConfiguration())) as? [String: Any])
        legacy.removeValue(forKey: "clock_face"); legacy.removeValue(forKey: "clock_keep_awake")
        let restored = try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: legacy))
        XCTAssertEqual(restored.clockFace, "digital"); XCTAssertFalse(restored.clockKeepAwake)
        legacy["clock_keep_awake"] = "true"
        XCTAssertThrowsError(try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: legacy)))
    }

    func testClockEditsOnlyChangeClockAndOtherSectionsPreserveIt() {
        var current = DeviceConfiguration()
        current.ssid = "Casa"; current.volume = 140; current.badge.keepAwake = true
        current.favorites = [Favorite(label: "Studio", entity: "light.studio")]
        var draft = DeviceConfiguration()
        draft.clockFace = "halo"; draft.clockKeepAwake = true
        current.apply(draft, part: .clock)
        XCTAssertEqual(current.clockFace, "halo"); XCTAssertTrue(current.clockKeepAwake)
        XCTAssertEqual(current.ssid, "Casa"); XCTAssertEqual(current.volume, 140)
        XCTAssertTrue(current.badge.keepAwake); XCTAssertEqual(current.favorites.first?.entity, "light.studio")
        draft.clockFace = "digital"; draft.clockKeepAwake = false
        for part in [SettingsPart.device, .wifi, .home, .mqtt, .badge] {
            current.apply(draft, part: part)
            XCTAssertEqual(current.clockFace, "halo"); XCTAssertTrue(current.clockKeepAwake)
        }
    }

    func testIndependentInteractionAndNoteVolumesOnWire() throws {
        var settings=DeviceConfiguration()
        settings.volume=200;settings.noteVolume=60
        let data=try JSONEncoder().encode(settings)
        let wire=try XCTUnwrap(JSONSerialization.jsonObject(with:data) as? [String:Any])
        XCTAssertEqual(wire["volume"] as? Int,200)
        XCTAssertEqual(wire["note_volume"] as? Int,60)
        let restored=try JSONDecoder().decode(DeviceConfiguration.self,from:data)
        XCTAssertEqual(restored.volume,200);XCTAssertEqual(restored.noteVolume,60)
        settings.noteVolume=0
        XCTAssertEqual(try JSONDecoder().decode(DeviceConfiguration.self,from:JSONEncoder().encode(settings)).noteVolume,0)
    }

    func testLegacyVolumeMigrationAndUnrelatedEditsPreserveNoteVolume() throws {
        var settings=DeviceConfiguration();settings.volume=160
        var legacy=try XCTUnwrap(JSONSerialization.jsonObject(with:JSONEncoder().encode(settings)) as? [String:Any])
        legacy.removeValue(forKey:"note_volume")
        let migrated=try JSONDecoder().decode(DeviceConfiguration.self,from:JSONSerialization.data(withJSONObject:legacy))
        XCTAssertEqual(migrated.noteVolume,160)
        settings.noteVolume=40
        var draft=DeviceConfiguration();draft.ssid="Casa"
        settings.apply(draft,part:.wifi)
        XCTAssertEqual(settings.volume,160);XCTAssertEqual(settings.noteVolume,40)
        draft.volume=120;draft.noteVolume=180;settings.apply(draft,part:.device)
        XCTAssertEqual(settings.volume,120);XCTAssertEqual(settings.noteVolume,180)
        XCTAssertEqual(settings.ssid,"Casa")
    }

    func testPhoneEditsPreserveFavoritesChangedInHomeAssistant() {
        var current = DeviceConfiguration()
        current.favorites = [Favorite(label: "Studio", entity: "light.studio")]
        current.volume = 140
        var oldDraft = DeviceConfiguration()
        oldDraft.ssid = "Casa"
        oldDraft.wifiPassword = "password-modificata"
        current.apply(oldDraft, part: .wifi)
        XCTAssertEqual(current.favorites.first?.entity, "light.studio")
        XCTAssertEqual(current.volume, 140)
        XCTAssertEqual(current.ssid, "Casa")
        oldDraft.brightness = 64
        current.apply(oldDraft, part: .device)
        XCTAssertEqual(current.ssid, "Casa")
        XCTAssertEqual(current.favorites.first?.entity, "light.studio")
        XCTAssertEqual(current.brightness, 64)
    }
    func testUTF8SplitAcrossBluetoothFrames() throws {
        let message = Data("{\"id\":1,\"ok\":true,\"data\":{\"name\":\"Caffè\"}}\n".utf8)
        var buffer = FrameBuffer()
        var frames: [Data] = []
        for byte in message { frames += try buffer.append(Data([byte])) }
        XCTAssertEqual(frames.count, 1)
        XCTAssertEqual(try JSONDecoder().decode(ReplyHeader.self, from: frames[0]).id, 1)
        XCTAssertEqual(String(decoding: frames[0], as: UTF8.self) + "\n", String(decoding: message, as: UTF8.self))
    }

    func testFrameBoundsAndMultipleMessages() throws {
        var buffer = FrameBuffer()
        XCTAssertEqual(try buffer.append(Data("{}\n{}\n".utf8)).count, 2)
        XCTAssertThrowsError(try buffer.append(Data([0])))
        buffer = FrameBuffer()
        XCTAssertTrue(try buffer.append(Data(repeating: 120, count: FrameBuffer.maximumBytes)).isEmpty)
        XCTAssertThrowsError(try buffer.append(Data([120])))
        buffer = FrameBuffer()
        XCTAssertThrowsError(try buffer.append(Data([10])))
    }

    func testAudioChunksMustMatchRequestedFileAndOffset() throws {
        let chunk = AudioChunk(offset: 44, total: 100, audio: Data(repeating: 7, count: 56).base64EncodedString())
        XCTAssertEqual(try chunk.decoded(expectedOffset: 44, totalBytes: 100).count, 56)
        XCTAssertThrowsError(try chunk.decoded(expectedOffset: 0, totalBytes: 100))
        XCTAssertThrowsError(try chunk.decoded(expectedOffset: 44, totalBytes: 99))
        XCTAssertThrowsError(try AudioChunk(offset: 0, total: 100, audio: "").decoded(expectedOffset: 0, totalBytes: 100))
        XCTAssertThrowsError(try AudioChunk(offset: 0, total: 5000, audio: Data(repeating: 0, count: 4097).base64EncodedString()).decoded(expectedOffset: 0, totalBytes: 5000))
    }

    func testLargerAudioChunksAndLegacyResponses() throws {
        let request = try JSONSerialization.jsonObject(with: JSONEncoder().encode(NoteRead(name: "note-0123456789abcdef.wav", offset: 0))) as? [String: Any]
        XCTAssertEqual(request?["length"] as? Int, 4096)
        for size in [20, 768, 4096] {
            let bytes = Data(repeating: 7, count: size)
            XCTAssertEqual(try AudioChunk(offset: 0, total: size, audio: bytes.base64EncodedString()).decoded(expectedOffset: 0, totalBytes: size), bytes)
        }
        XCTAssertThrowsError(try AudioChunk(offset: 4096, total: 5000, audio: Data(repeating: 1, count: 4096).base64EncodedString()).decoded(expectedOffset: 4096, totalBytes: 5000))
    }

    func testNoteEventAndLargeReplyShareNotificationStream() throws {
        let bytes = Data(repeating: 9, count: 4096)
        let reply = "{\"id\":42,\"ok\":true,\"data\":{\"offset\":0,\"total\":4096,\"audio\":\"\(bytes.base64EncodedString())\"}}\n"
        let wire = Data(("{\"id\":0,\"ok\":true,\"event\":\"notes.changed\"}\n" + reply).utf8)
        for size in [20, 182, 244, 512] {
            var buffer = FrameBuffer()
            var lines: [Data] = []
            for offset in stride(from: 0, to: wire.count, by: size) {
                lines += try buffer.append(Data(wire[offset..<min(offset + size, wire.count)]))
            }
            XCTAssertEqual(lines.count, 2)
            XCTAssertEqual(try JSONDecoder().decode(ReplyHeader.self, from: lines[0]).event, "notes.changed")
            XCTAssertEqual(try JSONDecoder().decode(ReplyHeader.self, from: lines[1]).id, 42)
            let decoded = try JSONDecoder().decode(ReplyMessage<AudioChunk>.self, from: lines[1]).data
            XCTAssertEqual(try decoded.decoded(expectedOffset: 0, totalBytes: 4096), bytes)
        }
    }

    func testNotePathsAndMaximumSize() {
        XCTAssertTrue(VoiceNote(name: "note-0123456789abcdef.wav", bytes: 1_920_044, seconds: 60).valid)
        XCTAssertFalse(VoiceNote(name: "../notes/note-0123456789abcdef.wav", bytes: 50, seconds: 1).valid)
        XCTAssertFalse(VoiceNote(name: "note-0123456789abcdef.wav", bytes: 1_920_045, seconds: 60).valid)
        XCTAssertFalse(VoiceNote(name: "note-0123456789abcdef.wav", bytes: 50, seconds: .nan).valid)
    }

    func testConfigurationPreservesBlankSecretsOnWire() throws {
        var configuration = DeviceConfiguration()
        configuration.wifiPasswordSaved = true
        configuration.haTokenSaved = true
        configuration.mqttPasswordSaved = true
        configuration.favorites = [Favorite(label: "Cucina", entity: "light.cucina")]
        let data = try JSONEncoder().encode(configuration)
        let restored = try JSONDecoder().decode(DeviceConfiguration.self, from: data)
        XCTAssertEqual(restored.wifiPassword, "")
        XCTAssertEqual(restored.haToken, "")
        XCTAssertEqual(restored.mqttPassword, "")
        XCTAssertEqual(restored.favorites.first?.entity, "light.cucina")
        XCTAssertTrue(restored.wifiPasswordSaved)
    }
}
