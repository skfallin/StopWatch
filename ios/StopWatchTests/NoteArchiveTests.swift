import XCTest
@testable import StopWatch

final class NoteArchiveTests: XCTestCase {
    private func audio() -> Data {
        var data = Data(repeating: 0, count: 64)
        data.replaceSubrange(0..<4, with: Data("RIFF".utf8))
        data.replaceSubrange(8..<12, with: Data("WAVE".utf8))
        return data
    }

    func testAudioAndItalianTranscriptSurviveReopening() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let device = UUID()
        let archive = NoteArchive(deviceID: device, root: root)
        let note = VoiceNote(name: "note-0123456789abcdef.wav", bytes: 64, seconds: 20.0 / 32_000)
        XCTAssertTrue(try archive.notes().isEmpty)
        XCTAssertNil(archive.audioURL(for: note))
        _ = try archive.saveAudio(audio(), for: note)
        try archive.saveTranscript("  Un'idea: caffè e più tempo per me.\n", for: note)
        let reopened = NoteArchive(deviceID: device, root: root)
        XCTAssertEqual(try reopened.notes(), [note])
        XCTAssertEqual(reopened.transcript(for: note), "Un'idea: caffè e più tempo per me.")
        XCTAssertNotNil(reopened.audioURL(for: note))
        let otherDevice = NoteArchive(deviceID: UUID(), root: root)
        XCTAssertTrue(try otherDevice.notes().isEmpty)
        XCTAssertNil(otherDevice.transcript(for: note))
    }

    func testTitlesSurviveReopeningWithoutRenamingOrChangingAudio() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let id = UUID()
        let archive = NoteArchive(deviceID: id, root: root)
        let note = VoiceNote(name: "note-0123456789abcdef.wav", bytes: 64, seconds: 0.000625)
        XCTAssertThrowsError(try archive.saveTitle("Idea", for: note))
        let url = try archive.saveAudio(audio(), for: note)
        try archive.saveTranscript("Un'idea da sviluppare", for: note)
        try archive.saveTitle("  Caffè domani  ", for: note)
        XCTAssertEqual(archive.title(for: note), "Caffè domani")
        let reopened = NoteArchive(deviceID: id, root: root)
        XCTAssertEqual(reopened.title(for: note), "Caffè domani")
        XCTAssertThrowsError(try archive.saveTitle(String(repeating: "a", count: 121), for: note))
        XCTAssertThrowsError(try archive.saveTitle("Due\nrighe", for: note))
        XCTAssertEqual(archive.title(for: note), "Caffè domani")
        XCTAssertEqual(try Data(contentsOf: url), audio())
        XCTAssertEqual(archive.transcript(for: note), "Un'idea da sviluppare")
        XCTAssertEqual(try archive.notes(), [note])
        try archive.saveTitle("", for: note)
        XCTAssertNil(archive.title(for: note))
    }

    func testInvalidAudioAndEmptyRecognitionPreserveExistingFiles() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let archive = NoteArchive(deviceID: UUID(), root: root)
        let note = VoiceNote(name: "note-0123456789abcdef.wav", bytes: 64, seconds: 0.000625)
        XCTAssertThrowsError(try archive.saveTranscript("Testo senza audio", for: note))
        let url = try archive.saveAudio(audio(), for: note)
        try archive.saveTranscript("Idea conservata", for: note)
        XCTAssertThrowsError(try archive.saveTranscript(" \n ", for: note))
        XCTAssertThrowsError(try archive.saveAudio(Data(repeating: 0, count: 64), for: note))
        XCTAssertThrowsError(try archive.saveAudio(audio().prefix(20), for: note))
        XCTAssertEqual(try Data(contentsOf: url), audio())
        XCTAssertEqual(archive.transcript(for: note), "Idea conservata")
        let traversal = VoiceNote(name: "../note-0123456789abcdef.wav", bytes: 64, seconds: 0)
        XCTAssertThrowsError(try archive.saveAudio(audio(), for: traversal))
        XCTAssertThrowsError(try archive.saveTranscript("Fuori archivio", for: traversal))
        XCTAssertNil(archive.audioURL(for: traversal))
    }
}
