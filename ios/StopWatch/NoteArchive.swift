import Foundation

struct NoteArchive {
    let folder: URL

    init(deviceID: UUID, root: URL = URL.documentsDirectory.appending(path: "Notes", directoryHint: .isDirectory)) {
        folder = root.appending(path: deviceID.uuidString, directoryHint: .isDirectory)
    }

    func audioURL(for note: VoiceNote) -> URL? {
        guard note.valid else { return nil }
        let url = folder.appendingPathComponent(note.name)
        return FileManager.default.fileExists(atPath: url.path) ? url : nil
    }

    func notes() throws -> [VoiceNote] {
        guard FileManager.default.fileExists(atPath: folder.path) else { return [] }
        return try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.fileSizeKey, .isRegularFileKey]).compactMap { url in
            guard url.pathExtension == "wav" else { return nil }
            let values = try url.resourceValues(forKeys: [.fileSizeKey, .isRegularFileKey])
            guard values.isRegularFile == true, let bytes = values.fileSize else { return nil }
            let note = VoiceNote(name: url.lastPathComponent, bytes: bytes, seconds: Double(bytes - 44) / 32_000)
            return note.valid ? note : nil
        }
    }

    func saveAudio(_ data: Data, for note: VoiceNote) throws -> URL {
        guard note.valid, data.count == note.bytes, data.count >= 44,
              data.prefix(4) == Data("RIFF".utf8), data[8..<12] == Data("WAVE".utf8) else {
            throw CompanionError.message("Audio non valido. L'originale è conservato sullo StopWatch.")
        }
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let url = folder.appendingPathComponent(note.name)
        try data.write(to: url, options: .atomic)
        return url
    }

    func transcript(for note: VoiceNote) -> String? {
        guard let audio = audioURL(for: note) else { return nil }
        return try? String(contentsOf: audio.deletingPathExtension().appendingPathExtension("txt"), encoding: .utf8)
    }

    func saveTranscript(_ text: String, for note: VoiceNote) throws {
        let text = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let audio = audioURL(for: note), !text.isEmpty else {
            throw CompanionError.message("Nessuna parola riconosciuta. Puoi riascoltare l'audio e riprovare.")
        }
        try text.write(to: audio.deletingPathExtension().appendingPathExtension("txt"), atomically: true, encoding: .utf8)
    }
}
