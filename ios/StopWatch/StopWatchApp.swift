import SwiftUI

@main
struct StopWatchApp: App {
    @StateObject private var connection = WatchConnection(preview: ProcessInfo.processInfo.arguments.contains("--preview"))
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            CompanionView()
                .environmentObject(connection)
                .preferredColorScheme(.dark)
                .task(id: scenePhase) {
                    guard scenePhase == .active else { return }
                    while !Task.isCancelled {
                        connection.startNoteSynchronization()
                        try? await Task.sleep(for: .seconds(5))
                    }
                }
        }
    }
}
