import Foundation
import Security

final class WiFiNoteDownload: NSObject, URLSessionDelegate, URLSessionTaskDelegate {
    private let host: String
    private let certificate: Data
    private let progress: (@Sendable (Double) -> Void)?

    init(host: String, certificate: Data, progress: (@Sendable (Double) -> Void)? = nil) {
        self.host = host
        self.certificate = certificate
        self.progress = progress
    }

    func download(_ request: URLRequest, expectedBytes: Int) async throws -> Data {
        let session = makeSession(timeout: 15, requestTimeout: 5)
        defer { session.invalidateAndCancel() }
        let (file, response) = try await session.download(for: request)
        defer { try? FileManager.default.removeItem(at: file) }
        let size = try file.resourceValues(forKeys: [.fileSizeKey]).fileSize
        try Self.requireSuccess(response)
        guard size == expectedBytes else {
            throw CompanionError.message(String(localized: "Trasferimento Wi-Fi incompleto: riprovo via Bluetooth.", bundle: AppLocalization.bundle))
        }
        return try Data(contentsOf: file)
    }

    func upload(_ request: URLRequest, data: Data) async throws {
        let session = makeSession(timeout: 120, requestTimeout: 20)
        defer { session.invalidateAndCancel() }
        var request=request
        request.timeoutInterval=20
        request.setValue(String(data.count),forHTTPHeaderField:"Content-Length")
        let (_, response) = try await session.upload(for: request, from: data)
        try Self.requireSuccess(response)
    }

    static func requireSuccess(_ response: URLResponse) throws {
        guard let http=response as? HTTPURLResponse else { throw CompanionError.message(String(localized: "Risposta Wi-Fi non valida.", bundle: AppLocalization.bundle)) }
        guard http.statusCode==200 else { throw CompanionError.message(String(localized: "Trasferimento Wi-Fi: HTTP \(http.statusCode).", bundle: AppLocalization.bundle)) }
    }

    static func retryableUploadError(_ error: Error) -> Bool {
        let error=error as NSError
        return error.domain==NSURLErrorDomain && [URLError.networkConnectionLost.rawValue, URLError.timedOut.rawValue, URLError.cannotConnectToHost.rawValue].contains(error.code)
    }

    private func makeSession(timeout: TimeInterval, requestTimeout: TimeInterval) -> URLSession {
        let config = URLSessionConfiguration.ephemeral
        config.allowsCellularAccess = false
        config.waitsForConnectivity = false
        config.timeoutIntervalForRequest = requestTimeout
        config.timeoutIntervalForResource = timeout
        config.urlCache = nil
        config.httpCookieStorage = nil
        config.urlCredentialStorage = nil
        return URLSession(configuration: config, delegate: self, delegateQueue: nil)
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didSendBodyData bytesSent: Int64,
                    totalBytesSent: Int64, totalBytesExpectedToSend: Int64) {
        if totalBytesExpectedToSend > 0 { progress?(Double(totalBytesSent)/Double(totalBytesExpectedToSend)) }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        guard let error=error as NSError? else { return }
        NSLog("StopWatch Wi-Fi task failed: %@/%ld; sent=%lld expected=%lld",error.domain,error.code,task.countOfBytesSent,task.countOfBytesExpectedToSend)
    }

    func urlSession(_ session: URLSession, didReceive challenge: URLAuthenticationChallenge,
                    completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void) {
        let space = challenge.protectionSpace
        guard space.authenticationMethod == NSURLAuthenticationMethodServerTrust,
              let trust = space.serverTrust, accepts(trust, host: space.host, port: space.port) else {
            completionHandler(.cancelAuthenticationChallenge, nil); return
        }
        completionHandler(.useCredential, URLCredential(trust: trust))
    }

    func accepts(_ trust: SecTrust, host: String, port: Int) -> Bool {
        guard host == self.host, port == 8443,
              let chain = SecTrustCopyCertificateChain(trust) as? [SecCertificate], let leaf = chain.first,
              SecCertificateCopyData(leaf) as Data == certificate else { return false }
        // The exact certificate arrives over authenticated BLE; no system trust is relaxed.
        SecTrustSetAnchorCertificates(trust, [leaf] as CFArray)
        SecTrustSetAnchorCertificatesOnly(trust, true)
        SecTrustSetPolicies(trust, SecPolicyCreateBasicX509())
        return SecTrustEvaluateWithError(trust, nil)
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
        completionHandler(nil)
    }
}
