import SwiftUI
import UIKit

struct USBControlPage: View {
    @EnvironmentObject private var connection: WatchConnection
    @Environment(\.scenePhase) private var scenePhase
    @AppStorage("usbKeyboardLayout") private var layout = "us"
    @State private var keyboard = false
    private var enabled: Bool { connection.ready && connection.usbState?.ready == true && !connection.working && scenePhase == .active }

    var body: some View {
        VStack(spacing: 12) {
            PaperCard {
                HStack {
                    Label(String(localized: "PONTE USB", bundle: AppLocalization.bundle), systemImage: "cable.connector").font(.caption.weight(.semibold)).tracking(1.3)
                    Spacer()
                    Circle().fill(enabled ? Palette.orange : .gray).frame(width: 8, height: 8)
                }
                Text(connection.usbState?.title ?? (connection.ready ? String(localized: "Verifica collegamento…", bundle: AppLocalization.bundle) : String(localized: "Collega lo StopWatch via Bluetooth", bundle: AppLocalization.bundle)))
                    .font(.title3.weight(.medium)).fixedSize(horizontal: false, vertical: true)
                Text(String(localized: "Collega StopWatch alla porta USB del computer e apri Ponte USB sul dispositivo.", bundle: AppLocalization.bundle))
                    .font(.subheadline).foregroundStyle(Palette.secondary)
                if enabled {
                    Text(connection.usbFastPointer ? String(localized: "Puntatore · Bluetooth rapido", bundle: AppLocalization.bundle) : String(localized: "Puntatore · compatibilità", bundle: AppLocalization.bundle))
                        .font(.caption).foregroundStyle(Palette.secondary)
                }
                if let issue = connection.usbIssue {
                    Label(issue, systemImage: "exclamationmark.circle").font(.subheadline).foregroundStyle(.red)
                }
            }
            PaperCard {
                ZStack {
                    RoundedRectangle(cornerRadius: 26).fill(.black.opacity(0.035))
                    VStack(spacing: 14) {
                        Image(systemName: "cursorarrow").font(.system(size: 30, weight: .light))
                        Text(String(localized: "Sposta un dito.\nScorri con due.", bundle: AppLocalization.bundle)).font(.subheadline).multilineTextAlignment(.center)
                    }
                    .foregroundStyle(Palette.secondary).allowsHitTesting(false).accessibilityHidden(true)
                    USBTrackpad(
                        move: { x, y, wheel in connection.moveUSB(x: x, y: y, wheel: wheel) },
                        click: { connection.enqueueUSB(.click($0)) }
                    )
                    .accessibilityLabel(String(localized: "Trackpad del computer", bundle: AppLocalization.bundle))
                    .accessibilityHint(String(localized: "Sposta un dito per muovere il puntatore, due dita per scorrere. Un tocco fa clic sinistro, due dita fanno clic destro.", bundle: AppLocalization.bundle))
                }
                .frame(height: keyboard ? 130 : 250)
                .disabled(!enabled).opacity(enabled ? 1 : 0.5)
                HStack(spacing: 12) {
                    clickButton(String(localized: "Sinistro", bundle: AppLocalization.bundle), symbol: "computermouse", button: "left")
                    clickButton(String(localized: "Destro", bundle: AppLocalization.bundle), symbol: "computermouse.fill", button: "right")
                }
                HStack {
                    Button {
                        keyboard.toggle()
                    } label: {
                        Label(keyboard ? String(localized: "Chiudi tastiera", bundle: AppLocalization.bundle) : String(localized: "Apri tastiera", bundle: AppLocalization.bundle), systemImage: "keyboard")
                            .font(.headline).frame(maxWidth: .infinity, minHeight: 50)
                    }
                    .buttonStyle(.plain).disabled(!enabled)
                }
                if keyboard {
                    Text(String(localized: "Scrivi nel campo selezionato sul computer.", bundle: AppLocalization.bundle))
                        .font(.caption).foregroundStyle(Palette.secondary)
                    HStack(spacing: 6) {
                        keyButton("Esc", key: "escape")
                        keyButton("Tab", key: "tab")
                        keyButton("←", key: "left")
                        keyButton("↑", key: "up")
                        keyButton("↓", key: "down")
                        keyButton("→", key: "right")
                    }
                }
                USBKeyboardCapture(active: keyboard && enabled,
                    insert: { connection.enqueueUSB(.text($0, layout: layout)) },
                    delete: { connection.enqueueUSB(.key("backspace")) })
                    .frame(height: 1).accessibilityHidden(true)
            }
            PaperCard {
                Picker(String(localized: "Tastiera del computer", bundle: AppLocalization.bundle), selection: $layout) {
                    Text(String(localized: "Italiano (PC)", bundle: AppLocalization.bundle)).tag("it")
                    Text(String(localized: "Inglese (US / ABC)", bundle: AppLocalization.bundle)).tag("us")
                }.tint(Palette.ink)
                Text(String(localized: "Scegli lo stesso layout attivo sul computer. Su Mac usa Italiano — PC oppure ABC. Gli emoji e i caratteri assenti nel layout non vengono inviati.", bundle: AppLocalization.bundle))
                    .font(.caption).foregroundStyle(Palette.secondary)
            }
        }
        .task(id: scenePhase) {
            guard scenePhase == .active else { return }
            await connection.monitorUSB()
        }
        .onChange(of: enabled) { _, value in if !value { keyboard = false } }
        .onDisappear { keyboard = false }
    }

    private func clickButton(_ title: String, symbol: String, button: String) -> some View {
        Button { connection.enqueueUSB(.click(button)) } label: {
            Label(title, systemImage: symbol).font(.subheadline.weight(.medium))
                .frame(maxWidth: .infinity, minHeight: 52)
                .background(.black.opacity(0.055), in: Capsule())
        }.buttonStyle(.plain).disabled(!enabled)
    }
    private func keyButton(_ title: String, key: String) -> some View {
        Button(title) { connection.enqueueUSB(.key(key)) }
            .font(.system(.subheadline, design: .monospaced)).buttonStyle(.plain)
            .frame(maxWidth: .infinity, minHeight: 44)
            .background(.black.opacity(0.055), in: RoundedRectangle(cornerRadius: 12))
            .disabled(!enabled)
            .accessibilityLabel(["escape": "Escape", "tab": "Tab", "left": String(localized: "Freccia sinistra", bundle: AppLocalization.bundle), "right": String(localized: "Freccia destra", bundle: AppLocalization.bundle), "up": String(localized: "Freccia su", bundle: AppLocalization.bundle), "down": String(localized: "Freccia giù", bundle: AppLocalization.bundle)][key] ?? title)
    }
}

private struct USBTrackpad: UIViewRepresentable {
    let move: (Double, Double, Double) -> Void
    let click: (String) -> Void
    func makeCoordinator() -> Coordinator { Coordinator(self) }
    func makeUIView(context: Context) -> UIView {
        let view = UIView()
        view.isMultipleTouchEnabled = true
        let pan = UIPanGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.pan(_:)))
        pan.minimumNumberOfTouches = 1; pan.maximumNumberOfTouches = 2
        let left = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.left))
        let right = UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.right))
        right.numberOfTouchesRequired = 2
        left.require(toFail: right); left.require(toFail: pan); right.require(toFail: pan)
        for gesture in [pan, left, right] { view.addGestureRecognizer(gesture) }
        return view
    }
    func updateUIView(_ view: UIView, context: Context) { context.coordinator.parent = self }
    final class Coordinator: NSObject {
        var parent: USBTrackpad
        init(_ parent: USBTrackpad) { self.parent = parent }
        @objc func pan(_ gesture: UIPanGestureRecognizer) {
            guard gesture.state == .began || gesture.state == .changed else { return }
            let delta = gesture.translation(in: gesture.view)
            gesture.setTranslation(.zero, in: gesture.view)
            if gesture.numberOfTouches == 2 { parent.move(0, 0, -Double(delta.y) / 10) }
            else { parent.move(Double(delta.x) * 1.6, Double(delta.y) * 1.6, 0) }
        }
        @objc func left() { parent.click("left") }
        @objc func right() { parent.click("right") }
    }
}

private struct USBKeyboardCapture: UIViewRepresentable {
    let active: Bool
    let insert: (String) -> Void
    let delete: () -> Void
    func makeUIView(context: Context) -> Input { Input() }
    func updateUIView(_ view: Input, context: Context) {
        view.insert = insert; view.delete = delete
        if active && !view.isFirstResponder { view.becomeFirstResponder() }
        else if !active && view.isFirstResponder { view.resignFirstResponder() }
    }
    static func dismantleUIView(_ view: Input, coordinator: ()) { view.resignFirstResponder() }
    final class Input: UIView, UIKeyInput {
        var insert: ((String) -> Void)?
        var delete: (() -> Void)?
        override var canBecomeFirstResponder: Bool { true }
        var hasText: Bool { true }
        var autocorrectionType: UITextAutocorrectionType = .no
        var autocapitalizationType: UITextAutocapitalizationType = .none
        var smartQuotesType: UITextSmartQuotesType = .no
        var smartDashesType: UITextSmartDashesType = .no
        var spellCheckingType: UITextSpellCheckingType = .no
        var keyboardAppearance: UIKeyboardAppearance = .dark
        func insertText(_ text: String) { insert?(text) }
        func deleteBackward() { delete?() }
    }
}
