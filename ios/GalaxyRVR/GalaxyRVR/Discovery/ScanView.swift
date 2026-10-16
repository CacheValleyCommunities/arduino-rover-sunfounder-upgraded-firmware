import SwiftUI

struct ScanView: View {
    @State private var browser = RoverBrowser()
    @State private var selectedRover: DiscoveredRover?

    var body: some View {
        NavigationStack {
            ZStack {
                Color.black.ignoresSafeArea()
                VStack(spacing: 16) {
                    header
                    roverList
                    Spacer(minLength: 0)
                    controls
                }
                .padding(24)
            }
            .navigationDestination(item: $selectedRover) { rover in
                DriveView(rover: rover)
            }
            .onAppear { browser.start() }
            .onDisappear { browser.stop() }
        }
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("GALAXY RVR")
                .font(.system(size: 28, weight: .heavy, design: .monospaced))
                .foregroundStyle(.white)
            Text(browser.status)
                .font(.system(.subheadline, design: .monospaced))
                .foregroundStyle(.white.opacity(0.7))
            if let hotspotError = browser.hotspotError {
                Text(hotspotError)
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var roverList: some View {
        Group {
            if browser.rovers.isEmpty {
                VStack(spacing: 8) {
                    ProgressView()
                        .tint(.white)
                    Text("Listening for UDP beacons on :30103")
                        .font(.system(.caption, design: .monospaced))
                        .foregroundStyle(.white.opacity(0.6))
                    Text("Join rover Wi-Fi if the list stays empty")
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(.white.opacity(0.4))
                }
                .frame(maxWidth: .infinity, minHeight: 160)
            } else {
                ScrollView {
                    VStack(spacing: 10) {
                        ForEach(browser.rovers) { rover in
                            Button {
                                selectedRover = rover
                            } label: {
                                roverRow(rover)
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }
            }
        }
    }

    private func roverRow(_ rover: DiscoveredRover) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text(rover.name)
                    .font(.system(.headline, design: .monospaced))
                    .foregroundStyle(.white)
                Text("\(rover.ip):\(rover.udpPort)  \(rover.source)")
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.65))
            }
            Spacer()
            Text("DRIVE")
                .font(.system(.caption, design: .monospaced, weight: .bold))
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(Color.green.opacity(0.85))
                .foregroundStyle(.black)
                .clipShape(RoundedRectangle(cornerRadius: 4))
        }
        .padding(14)
        .background(Color.white.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(Color.white.opacity(0.15), lineWidth: 1)
        )
    }

    private var controls: some View {
        HStack(spacing: 12) {
            Button("Scan again") {
                browser.start()
            }
            .buttonStyle(MCButtonStyle())

            Button("Join rover Wi-Fi") {
                if !browser.isScanning { browser.start() }
                browser.joinRoverHotspot()
            }
            .buttonStyle(MCButtonStyle(accent: .green))
        }
    }
}

struct MCButtonStyle: ButtonStyle {
    var accent: Color = Color.white.opacity(0.18)

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(.body, design: .monospaced, weight: .semibold))
            .foregroundStyle(.white)
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
            .background(accent.opacity(configuration.isPressed ? 0.6 : 1))
            .clipShape(RoundedRectangle(cornerRadius: 6))
            .overlay(
                RoundedRectangle(cornerRadius: 6)
                    .stroke(Color.white.opacity(0.25), lineWidth: 1)
            )
    }
}

#Preview {
    ScanView()
}
