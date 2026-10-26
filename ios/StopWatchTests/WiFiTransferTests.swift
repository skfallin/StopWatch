import XCTest
import Security
@testable import StopWatch

final class WiFiTransferTests: XCTestCase {
    func testOnlyTransientConnectionFailuresAreRetried() {
        for code in [URLError.Code.networkConnectionLost,.timedOut,.cannotConnectToHost] {
            XCTAssertTrue(WiFiNoteDownload.retryableUploadError(URLError(code)))
        }
        for code in [URLError.Code.cancelled,.notConnectedToInternet,.serverCertificateUntrusted,.secureConnectionFailed,.badServerResponse] {
            XCTAssertFalse(WiFiNoteDownload.retryableUploadError(URLError(code)))
        }
        XCTAssertFalse(WiFiNoteDownload.retryableUploadError(NSError(domain:"other",code:-1005)))
        XCTAssertFalse(WiFiNoteDownload.retryableUploadError(CompanionError.message("HTTP 403")))
    }

    func testUploadDiagnosticsRemainOptionalForPreviousFirmware() throws {
        var payload:[String:Any] = ["available":true,"frames":1,"bytes":100,"duration_ms":1000,"digest":"test","error":"","uploading":true]
        let old=try JSONDecoder().decode(BadgeStatus.self,from:JSONSerialization.data(withJSONObject:payload))
        XCTAssertNil(old.uploadReceived);XCTAssertNil(old.uploadError)
        payload["upload_received"]=4096;payload["upload_error"]="Pausa troppo lunga"
        let current=try JSONDecoder().decode(BadgeStatus.self,from:JSONSerialization.data(withJSONObject:payload))
        XCTAssertEqual(current.uploadReceived,4096);XCTAssertEqual(current.uploadError,"Pausa troppo lunga")
    }

    func testHTTPFailuresKeepTheirCauseAndRedirectsAreRejected() throws {
        let url=try XCTUnwrap(URL(string:"https://192.168.0.105:8443/badge"))
        try WiFiNoteDownload.requireSuccess(XCTUnwrap(HTTPURLResponse(url:url,statusCode:200,httpVersion:nil,headerFields:nil)))
        for status in [302,400,403,404,408,500,503] {
            let response=try XCTUnwrap(HTTPURLResponse(url:url,statusCode:status,httpVersion:nil,headerFields:nil))
            XCTAssertThrowsError(try WiFiNoteDownload.requireSuccess(response)) { error in
                XCTAssertTrue(error.localizedDescription.contains("HTTP \(status)"))
            }
        }
        XCTAssertThrowsError(try WiFiNoteDownload.requireSuccess(URLResponse(url:url,mimeType:nil,expectedContentLength:0,textEncodingName:nil)))
    }

    private let certificate = Data(base64Encoded: "MIIBiTCCATCgAwIBAgIUNIr4UIkNlUyJR/3YQMkGnu/Jp9cwCgYIKoZIzj0EAwIwFDESMBAGA1UEAwwJU3RvcFdhdGNoMB4XDTI2MTAyNDA5MzI1M1oXDTM2MTAyMTA5MzI1M1owFDESMBAGA1UEAwwJU3RvcFdhdGNoMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEG6gOKKT72VpyZcTTSYquOYC9KA2ljgLx93Mm+Hj6CcV4bOv63rE1YtDDy0FKu6Y9rxRIAyw9mUaI+03xeyvpvKNgMF4wHQYDVR0OBBYEFMYrqdsQtV7IrtQ/s36TwSudiKoMMB8GA1UdIwQYMBaAFMYrqdsQtV7IrtQ/s36TwSudiKoMMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQDAgeAMAoGCCqGSM49BAMCA0cAMEQCICsMfD3RLJWm+onWQ+h6BDuq+jz3K1rErx/3NMOQAc09AiBL2xFi7HgaST4hXArCeeBeWNpcXMTCEZvXQze5azysGg==")!

    func testExactCertificateHostAndPortAreRequired() throws {
        let leaf = try XCTUnwrap(SecCertificateCreateWithData(nil, certificate as CFData))
        var trust: SecTrust?
        XCTAssertEqual(SecTrustCreateWithCertificates(leaf, SecPolicyCreateBasicX509(), &trust), errSecSuccess)
        let value = try XCTUnwrap(trust)
        let client = WiFiNoteDownload(host: "192.168.0.105", certificate: certificate)
        XCTAssertTrue(client.accepts(value, host: "192.168.0.105", port: 8443))
        XCTAssertFalse(client.accepts(value, host: "192.168.0.150", port: 8443))
        XCTAssertFalse(client.accepts(value, host: "192.168.0.105", port: 443))
        XCTAssertFalse(WiFiNoteDownload(host: "192.168.0.105", certificate: Data([1, 2, 3])).accepts(value, host: "192.168.0.105", port: 8443))
    }

    func testTransferOfferCannotChangeHostPortOrInjectAuthorization() throws {
        let ticket = String(repeating: "a", count: 64)
        let encoded = certificate.base64EncodedString()
        func offer(host: String = "192.168.0.105", port: Int = 8443, secret: String? = nil, cert: String? = nil) -> NoteTransferOffer {
            NoteTransferOffer(available: true, host: host, port: port, ticket: secret ?? ticket, certificate: cert ?? encoded)
        }
        let request = try offer().request(expectedHost: "192.168.0.105")
        XCTAssertEqual(request.url?.absoluteString, "https://192.168.0.105:8443/note")
        XCTAssertEqual(request.value(forHTTPHeaderField: "Authorization"), "Bearer " + ticket)
        XCTAssertNil(request.url?.query)
        XCTAssertEqual(try offer().request(expectedHost:"192.168.0.105",path:"badge").url?.path,"/badge")
        XCTAssertThrowsError(try offer().request(expectedHost:"192.168.0.105",path:"../config"))
        XCTAssertThrowsError(try offer().request(expectedHost: "192.168.0.150"))
        XCTAssertThrowsError(try offer(port: 80).request(expectedHost: "192.168.0.105"))
        XCTAssertThrowsError(try offer(secret: ticket + "\r\nOther: injected").request(expectedHost: "192.168.0.105"))
        XCTAssertThrowsError(try offer(cert: "not-base64").request(expectedHost: "192.168.0.105"))
        XCTAssertThrowsError(try offer(host: "192..168.0.105").request(expectedHost: "192..168.0.105"))
        XCTAssertThrowsError(try offer(host: "999.168.0.105").request(expectedHost: "999.168.0.105"))
        let unavailable = try JSONDecoder().decode(NoteTransferOffer.self, from: Data("{\"available\":false}".utf8))
        XCTAssertThrowsError(try unavailable.request(expectedHost: "192.168.0.105"))
    }
}
