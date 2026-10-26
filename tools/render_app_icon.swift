import AppKit
import SwiftUI

// swiftc ios/StopWatch/WatchDrawing.swift tools/render_app_icon.swift -o /tmp/render-stopwatch-icon
@main
struct RenderAppIcon {
    @MainActor
    static func main() throws {
        let output = CommandLine.arguments.dropFirst().first ?? "ios/StopWatch/Assets.xcassets/AppIcon.appiconset/AppIcon.png"
        let paper = Color(red: 0.98, green: 0.98, blue: 0.97)
        let icon = ZStack {
            paper
            WatchDrawing(connected: true).frame(width: 840, height: 840)
        }.frame(width: 1024, height: 1024)
        try render(icon, to: output, size: CGSize(width: 1024, height: 1024))
        for connected in [true, false] {
            let preview = WatchDrawing(connected: connected)
                .frame(width: 360, height: 184).background(paper)
            try render(preview, to: "/tmp/stopwatch-artwork-\(connected ? "connected" : "disconnected").png", size: CGSize(width: 360, height: 184))
        }
    }

    @MainActor
    private static func render<V: View>(_ view: V, to path: String, size: CGSize) throws {
        let renderer = ImageRenderer(content: view.environment(\.colorScheme, .light))
        renderer.proposedSize = ProposedViewSize(size)
        renderer.scale = 1
        renderer.isOpaque = true
        guard let image = renderer.cgImage else { throw CocoaError(.fileWriteUnknown) }
        let bitmap = NSBitmapImageRep(cgImage: image)
        guard image.width == Int(size.width), image.height == Int(size.height), !bitmap.hasAlpha,
              let data = bitmap.representation(using: .png, properties: [:]) else { throw CocoaError(.fileWriteUnknown) }
        try data.write(to: URL(fileURLWithPath: path), options: .atomic)
        print("Rendered \(path) — \(image.width) × \(image.height), opaque")
    }
}
