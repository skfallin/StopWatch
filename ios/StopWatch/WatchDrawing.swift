import SwiftUI

// Shared with the app-icon renderer so the hardware silhouette stays identical.
struct WatchDrawing: View {
    let connected: Bool

    var body: some View {
        GeometryReader { geometry in
            ZStack {
                RoundedRectangle(cornerRadius: 3)
                    .fill(Color(red: 1, green: 0.77, blue: 0.12))
                    .frame(width: 28, height: 10)
                    .rotationEffect(.degrees(-45))
                    .offset(x: -59, y: -59)
                RoundedRectangle(cornerRadius: 3)
                    .fill(Color(red: 0.20, green: 0.43, blue: 0.92))
                    .frame(width: 28, height: 10)
                    .rotationEffect(.degrees(45))
                    .offset(x: 59, y: -59)
                Circle().fill(Color(white: 0.13))
                    .overlay(Circle().strokeBorder(Color(white: 0.28), lineWidth: 1))
                    .frame(width: 160, height: 160)
                    .shadow(color: .black.opacity(0.14), radius: 4, y: 3)
                Circle().fill(.black)
                    .overlay(Circle().strokeBorder(Color(white: 0.23), lineWidth: 1))
                    .frame(width: 148, height: 148)
                Circle().fill(LinearGradient(colors: [Color(white: 0.055), .black], startPoint: .topLeading, endPoint: .bottomTrailing))
                    .frame(width: 136, height: 136)
                VStack(spacing: 12) {
                    Image(systemName: "waveform")
                        .font(.system(size: 38, weight: .ultraLight))
                        .foregroundStyle(connected ? Color(red: 1, green: 0.31, blue: 0.06) : .white.opacity(0.55))
                    Text("STOPWATCH")
                        .font(.system(size: 9, weight: .medium)).tracking(2)
                        .foregroundStyle(.white.opacity(0.6))
                }
                .environment(\.dynamicTypeSize, .medium)
            }
            .frame(width: 184, height: 184)
            .scaleEffect(min(geometry.size.width, geometry.size.height) / 184)
            .position(x: geometry.size.width / 2, y: geometry.size.height / 2)
        }
        .accessibilityHidden(true)
    }
}
