import Foundation
import Speech

@MainActor
final class NoteTranscriber {
    private var recognizer: SFSpeechRecognizer?
    private var recognition: SFSpeechRecognitionTask?
    private var continuation: CheckedContinuation<String, Error>?
    private var timeout: Task<Void, Never>?
    private var requestID: UUID?

    func transcribe(_ url: URL) async throws -> String {
        let authorization = await withCheckedContinuation { continuation in
            SFSpeechRecognizer.requestAuthorization { continuation.resume(returning: $0) }
        }
        try Task.checkCancellation()
        guard authorization == .authorized else {
            throw CompanionError.message(String(localized: "Consenti Riconoscimento vocale in Impostazioni iPhone → StopWatch, poi tocca Riprova.", bundle: AppLocalization.bundle))
        }
        guard let recognizer = SFSpeechRecognizer(locale: Locale(identifier: UserDefaults.standard.string(forKey: "transcriptionLanguage") ?? "en-US")), recognizer.isAvailable else {
            throw CompanionError.message(String(localized: "Riconoscimento nella lingua scelta momentaneamente non disponibile. Riprova più tardi.", bundle: AppLocalization.bundle))
        }
        guard recognizer.supportsOnDeviceRecognition else {
            throw CompanionError.message(String(localized: "La trascrizione locale nella lingua scelta non è disponibile su questo iPhone.", bundle: AppLocalization.bundle))
        }
        guard continuation == nil else { throw CompanionError.message(String(localized: "Una trascrizione è già in corso.", bundle: AppLocalization.bundle)) }
        let id = UUID()
        let request = SFSpeechURLRecognitionRequest(url: url)
        request.requiresOnDeviceRecognition = true
        request.shouldReportPartialResults = false
        request.addsPunctuation = true
        request.taskHint = .dictation
        self.recognizer = recognizer
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                self.continuation = continuation
                requestID = id
                guard !Task.isCancelled else { finish(.failure(CancellationError()), id: id); return }
                recognition = recognizer.recognitionTask(with: request) { [weak self] result, error in
                    let text = result?.isFinal == true ? result?.bestTranscription.formattedString : nil
                    Task { @MainActor in
                        if let text { self?.finish(.success(text), id: id) }
                        else if let error { self?.finish(.failure(error), id: id) }
                    }
                }
                timeout = Task { [weak self] in
                    try? await Task.sleep(for: .seconds(120))
                    guard !Task.isCancelled else { return }
                    self?.finish(.failure(CompanionError.message(String(localized: "La trascrizione sta impiegando troppo tempo. L'audio è salvo: tocca Riprova.", bundle: AppLocalization.bundle))), id: id)
                }
            }
        } onCancel: {
            Task { @MainActor [weak self] in self?.finish(.failure(CancellationError()), id: id) }
        }
    }

    private func finish(_ result: Result<String, Error>, id: UUID) {
        guard requestID == id, let continuation else { return }
        self.continuation = nil
        requestID = nil
        timeout?.cancel(); timeout = nil
        recognition?.cancel(); recognition = nil
        recognizer = nil
        continuation.resume(with: result)
    }
}
