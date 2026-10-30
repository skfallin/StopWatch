import XCTest
@testable import StopWatch

final class USBControlTests: XCTestCase {
    func testFastMotionPacketUsesBoundedSignedDeltasAndLittleEndianSession() throws {
        let input = USBInput(session: 0x12345678, kind: "move", x: 12, y: -4, wheel: 1)
        XCTAssertEqual(input.motionPacket(), Data([1,0x78,0x56,0x34,0x12,12,252,1]))
        XCTAssertNil(USBInput(kind: "move", x: 128, y: 0, wheel: 0).motionPacket())
        XCTAssertNil(USBInput.click("left").motionPacket())
    }
    func testMovementCoalescesWithoutCrossingClicks() throws {
        var buffer = USBInputBuffer()
        buffer.move(x: 20, y: -10); buffer.move(x: 5, y: -6)
        try buffer.append(.click("left"))
        buffer.move(x: 2, y: 3, wheel: -1)
        XCTAssertEqual(buffer.items.count, 3)
        let first = try XCTUnwrap(buffer.pop(session: 42))
        XCTAssertEqual(first.x, 25); XCTAssertEqual(first.y, -16); XCTAssertEqual(first.session, 42)
        XCTAssertEqual(buffer.pop(session: 42)?.button, "left")
        XCTAssertEqual(buffer.pop(session: 42)?.wheel, -1)
        XCTAssertNil(buffer.pop(session: 42))
    }

    func testFractionalMovementAndLimits() throws {
        var buffer = USBInputBuffer()
        buffer.move(x: .infinity, y: .nan); XCTAssertTrue(buffer.items.isEmpty)
        buffer.move(x: 0.25, y: 0.25, wheel: 0.25); XCTAssertTrue(buffer.items.isEmpty)
        for _ in 0..<3 { buffer.move(x: 0.25, y: 0.25, wheel: 0.25) }
        XCTAssertEqual(buffer.pop(session: 1)?.x, 1)
        for _ in 0..<1_000 { buffer.move(x: 100, y: -100, wheel: 4) }
        XCTAssertEqual(buffer.items.count, 1)
        let movement = try XCTUnwrap(buffer.pop(session: 1))
        XCTAssertEqual(movement.x, 127); XCTAssertEqual(movement.y, -127); XCTAssertEqual(movement.wheel, 127)
        for _ in 0..<32 { try buffer.append(.key("enter")) }
        XCTAssertThrowsError(try buffer.append(.key("enter")))
        buffer.clear(); XCTAssertTrue(buffer.items.isEmpty)
        buffer.move(x: 0.75, y: 0.75); XCTAssertTrue(buffer.items.isEmpty)
    }

    func testTextValidationMatchesFirmwareBeforeEnqueueing() throws {
        var buffer = USBInputBuffer()
        try buffer.append(.text("Caffè è già qui: 10€", layout: "it"))
        try buffer.append(.text("Hello @world ~{}", layout: "us"))
        try buffer.append(.text(String(repeating: "a", count: 96), layout: "us"))
        let count = buffer.items.count
        for (text, layout) in [("😀", "it"), ("a\u{0}b", "it"), ("è", "us"), ("`", "it"), ("~", "it"), ("abc", "unknown"), ("", "us"), ("\r", "us"), (String(repeating: "a", count: 97), "it")] {
            XCTAssertThrowsError(try buffer.append(.text(text, layout: layout)))
            XCTAssertEqual(buffer.items.count, count)
        }
    }

    func testWireFormatIsSmallAndUsesSession() throws {
        var buffer = USBInputBuffer()
        try buffer.append(.text("Hello", layout: "us"))
        let input = try XCTUnwrap(buffer.pop(session: 123))
        let bytes = try RequestMessage(id: 8, op: "usb.input", data: input).encoded()
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: bytes) as? [String: Any])
        let body = try XCTUnwrap(json["data"] as? [String: Any])
        XCTAssertEqual(json["op"] as? String, "usb.input")
        XCTAssertEqual(body["session"] as? Int, 123)
        XCTAssertEqual(body["text"] as? String, "Hello")
        XCTAssertNil(body["x"]); XCTAssertLessThan(bytes.count, 512)
    }

    func testStatusDistinguishesPhysicalActivationFromCable() throws {
        let bytes = Data(#"{"supported":true,"active":false,"connected":true,"ready":false,"busy":false,"session":3,"fault":false,"queued":0}"#.utf8)
        let status = try JSONDecoder().decode(USBControlStatus.self, from: bytes)
        XCTAssertEqual(status.title, String(localized: "Apri Ponte USB sullo StopWatch", bundle: AppLocalization.bundle))
        XCTAssertFalse(status.ready)
    }
}
