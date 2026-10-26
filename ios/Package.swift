// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "StopWatchProtocolChecks",
    platforms: [.macOS(.v13), .iOS(.v17)],
    targets: [
        .target(name: "StopWatch", path: "StopWatch",
                exclude: ["StopWatchApp.swift", "WatchConnection.swift", "CompanionView.swift", "WatchDrawing.swift", "SettingsView.swift", "BadgeView.swift", "NoteTranscriber.swift", "Info.plist", "Assets.xcassets"],
                sources: ["Models.swift", "NoteArchive.swift", "WiFiNoteDownload.swift", "BadgeAsset.swift", "HomeRelay.swift"],
                linkerSettings: [.linkedLibrary("z")]),
        .testTarget(name: "StopWatchTests", dependencies: ["StopWatch"], path: "StopWatchTests", resources: [.copy("Fixtures")])
    ]
)
