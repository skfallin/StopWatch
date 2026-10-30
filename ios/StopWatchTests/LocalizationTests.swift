import XCTest
@testable import StopWatch

final class LocalizationTests: XCTestCase {
    func testBothNativeCatalogsAndFormatting() throws {
        for (language, title, seconds) in [("en", "Settings", "Turns off after 45 seconds"), ("it", "Impostazioni", "Si spegne dopo 45 secondi")] {
            let path = try XCTUnwrap(AppLocalization.bundle.path(forResource: language, ofType: "lproj"))
            let bundle = try XCTUnwrap(Bundle(path: path))
            XCTAssertEqual(bundle.localizedString(forKey: "Impostazioni", value: nil, table: nil), title)
            let format = bundle.localizedString(forKey: "Si spegne dopo %lld secondi", value: nil, table: nil)
            XCTAssertEqual(String(format: format, Int64(45)), seconds)
        }
    }

    func testRegionalEditsAndLegacyDefaults() throws {
        var current = DeviceConfiguration()
        XCTAssertEqual(current.language, "en")
        XCTAssertEqual(current.timeZone, "UTC")
        current.ssid = "Synthetic test network"
        current.volume = 130
        current.timeZones = ["UTC", "Europe/Rome"]
        var edited = current
        edited.language = "it"
        edited.timeZone = "Europe/Rome"
        edited.volume = 5
        current.apply(edited, part: .regional)
        XCTAssertEqual(current.language, "it")
        XCTAssertEqual(current.timeZone, "Europe/Rome")
        XCTAssertEqual(current.volume, 130)
        XCTAssertEqual(current.ssid, "Synthetic test network")
        edited.language = "en"
        current.apply(edited, part: .device)
        XCTAssertEqual(current.language, "it")
        var wire = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(current)) as? [String: Any])
        wire.removeValue(forKey: "language")
        wire.removeValue(forKey: "time_zone")
        wire.removeValue(forKey: "time_zones")
        let legacy = try JSONDecoder().decode(DeviceConfiguration.self, from: JSONSerialization.data(withJSONObject: wire))
        XCTAssertEqual(legacy.language, "it")
        XCTAssertEqual(legacy.timeZone, "Europe/Rome")
        XCTAssertTrue(legacy.timeZones.isEmpty)
    }
}
