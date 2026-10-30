import Foundation

struct USBControlStatus: Decodable, Equatable {
    let supported: Bool
    let active: Bool
    let connected: Bool
    let ready: Bool
    let busy: Bool
    let session: UInt32
    let fault: Bool
    let queued: Int

    var title: String {
        if !supported { return String(localized: "Aggiorna il firmware", bundle: AppLocalization.bundle) }
        if fault { return String(localized: "Riprendi Ponte USB sullo StopWatch", bundle: AppLocalization.bundle) }
        if !active { return String(localized: "Apri Ponte USB sullo StopWatch", bundle: AppLocalization.bundle) }
        if !connected { return String(localized: "Collega il cavo USB-C al computer", bundle: AppLocalization.bundle) }
        return ready ? String(localized: "Mouse e tastiera pronti", bundle: AppLocalization.bundle) : String(localized: "Preparazione…", bundle: AppLocalization.bundle)
    }
}

struct USBInput: Encodable, Equatable {
    var session: UInt32 = 0
    let kind: String
    var x: Int? = nil
    var y: Int? = nil
    var wheel: Int? = nil
    var button: String? = nil
    var text: String? = nil
    var layout: String? = nil
    var key: String? = nil

    static func click(_ button: String) -> Self { Self(kind: "click", button: button) }
    static func key(_ name: String) -> Self { Self(kind: "key", key: name) }
    static func text(_ value: String, layout: String) -> Self { Self(kind: "text", text: value, layout: layout) }
    func motionPacket() -> Data? {
        guard kind == "move", let x, let y, let wheel,
              (-127...127).contains(x), (-127...127).contains(y), (-127...127).contains(wheel) else { return nil }
        return Data([1, UInt8(truncatingIfNeeded: session), UInt8(truncatingIfNeeded: session >> 8),
                     UInt8(truncatingIfNeeded: session >> 16), UInt8(truncatingIfNeeded: session >> 24),
                     UInt8(bitPattern: Int8(x)), UInt8(bitPattern: Int8(y)), UInt8(bitPattern: Int8(wheel))])
    }
}

struct USBInputBuffer {
    private(set) var items: [USBInput] = []
    private var fractionX = 0.0, fractionY = 0.0, fractionWheel = 0.0

    mutating func clear() { items.removeAll(); fractionX = 0; fractionY = 0; fractionWheel = 0 }

    mutating func move(x: Double, y: Double, wheel: Double = 0) {
        guard x.isFinite, y.isFinite, wheel.isFinite else { return }
        fractionX += max(-127, min(127, x)); fractionY += max(-127, min(127, y))
        fractionWheel += max(-127, min(127, wheel))
        let dx = Int(fractionX), dy = Int(fractionY), dw = Int(fractionWheel)
        fractionX -= Double(dx); fractionY -= Double(dy); fractionWheel -= Double(dw)
        guard dx != 0 || dy != 0 || dw != 0 else { return }
        if items.last?.kind == "move", let last = items.popLast() {
            items.append(USBInput(kind: "move", x: clamp((last.x ?? 0) + dx), y: clamp((last.y ?? 0) + dy), wheel: clamp((last.wheel ?? 0) + dw)))
        } else if items.count < 32 {
            items.append(USBInput(kind: "move", x: clamp(dx), y: clamp(dy), wheel: clamp(dw)))
        }
    }

    mutating func append(_ input: USBInput) throws {
        guard items.count < 32 else { throw CompanionError.message(String(localized: "Attendi l’invio dei tasti precedenti.", bundle: AppLocalization.bundle)) }
        if input.kind == "text" {
            guard let text = input.text, let layout = input.layout, ["it", "us"].contains(layout),
                  !text.isEmpty, text.unicodeScalars.count <= 96,
                  text.unicodeScalars.allSatisfy({ scalar in
                      let c = scalar.value
                      if c == 9 || c == 10 { return true }
                      if (32...126).contains(c) { return layout == "us" || (c != 96 && c != 126) }
                      return layout == "it" && [0xe0, 0xe8, 0xe9, 0xec, 0xf2, 0xf9, 0xa3, 0x20ac].contains(c)
                  }) else {
                throw CompanionError.message(String(localized: "Carattere non disponibile nel layout scelto. Usa lettere, numeri e simboli della tastiera del computer; gli emoji non sono supportati.", bundle: AppLocalization.bundle))
            }
        }
        items.append(input)
    }

    mutating func pop(session: UInt32) -> USBInput? {
        guard !items.isEmpty else { return nil }
        var input = items.removeFirst(); input.session = session; return input
    }
    private func clamp(_ value: Int) -> Int { max(-127, min(127, value)) }
}
