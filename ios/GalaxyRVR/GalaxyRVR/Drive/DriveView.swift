import SwiftUI

struct DriveView: View {
    let rover: DiscoveredRover
    @Environment(\.dismiss) private var dismiss
    @State private var session: DriveSession
    @State private var showCameraSettings = false
    @State private var showCode = false

    init(rover: DiscoveredRover) {
        self.rover = rover
        _session = State(initialValue: DriveSession(rover: rover))
    }

    var body: some View {
        GeometryReader { geo in
            ZStack {
                SensorHUD(
                    telemetry: session.telemetry,
                    mode: session.mode,
                    throttle: session.throttle,
                    servoAngle: session.servoAngle
                )
                .zIndex(1)

                controls(insets: geo.safeAreaInsets)
                    .zIndex(2)
            }
        }
        .background { videoLayer }
        .background(Color.black)
        .ignoresSafeArea()
        .navigationBarBackButtonHidden(true)
        .statusBarHidden(true)
        .onAppear { session.start() }
        .onDisappear { session.stop() }
        .fullScreenCover(isPresented: $showCode) {
            CodeWorkspaceView(url: c2URL)
        }
    }

    @ViewBuilder
    private var videoLayer: some View {
        GeometryReader { geo in
            Group {
                if let frame = session.frame {
                    Image(uiImage: frame)
                        .resizable()
                        .scaledToFill()
                } else {
                    Color.black
                        .overlay {
                            VStack(spacing: 8) {
                                ProgressView().tint(.white)
                                Text(session.videoError ?? "Waiting for camera…")
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.white.opacity(0.7))
                                Text("\(rover.ip):\(rover.udpPort)")
                                    .font(.system(.caption2, design: .monospaced))
                                    .foregroundStyle(.white.opacity(0.45))
                            }
                        }
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .clipped()
        }
        .allowsHitTesting(false)
    }

    private func controls(insets: EdgeInsets) -> some View {
        VStack(spacing: 0) {
            topStrip
                .padding(.leading, max(insets.leading, 14))
                .padding(.trailing, max(insets.trailing, 14))
                .padding(.top, max(insets.top, 8))

            if showCameraSettings {
                HStack {
                    CameraQualityPanel(
                        quality: session.jpegQuality,
                        framesize: session.framesize,
                        busy: session.cameraSettingsBusy,
                        error: session.cameraSettingsError,
                        onQuality: { session.setJpegQuality($0) },
                        onFramesize: { session.setFramesize($0) }
                    )
                    Spacer()
                }
                .padding(.leading, max(insets.leading, 14))
                .padding(.top, 8)
            }

            Spacer().allowsHitTesting(false)

            HStack(alignment: .bottom, spacing: 0) {
                VirtualJoystick(
                    enabled: session.mode == .drive && !showCode,
                    onChange: { x, y in session.setJoystick(x: x, y: y) },
                    onEnd: { session.joystickEnded() }
                )

                Spacer().allowsHitTesting(false)

                VStack(spacing: 12) {
                    faceButtons
                    LookPad { rate in
                        session.setLookRate(rate)
                    }
                }
            }
            .padding(.leading, max(insets.leading, 20))
            .padding(.trailing, max(insets.trailing, 20))
            .padding(.bottom, max(insets.bottom, 16))
        }
    }

    private var topStrip: some View {
        HStack(spacing: 12) {
            Button {
                session.stop()
                dismiss()
            } label: {
                Image(systemName: "pause.fill")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(.white)
                    .frame(width: 36, height: 36)
                    .background(Color.black.opacity(0.45))
                    .clipShape(Circle())
                    .overlay(Circle().stroke(Color.white.opacity(0.28), lineWidth: 1))
            }

            FuelGauge(fraction: session.telemetry.batteryFraction)

            Spacer().allowsHitTesting(false)

            Button {
                session.joystickEnded()
                showCode = true
            } label: {
                Text("CODE")
                    .font(.system(size: 10, weight: .heavy, design: .monospaced))
                    .foregroundStyle(.white)
                    .frame(width: 52, height: 36)
                    .background(Color.black.opacity(0.45))
                    .clipShape(Capsule())
                    .overlay(Capsule().stroke(Color.white.opacity(0.28), lineWidth: 1))
            }

            Button {
                showCameraSettings.toggle()
            } label: {
                Image(systemName: "camera.aperture")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(.white)
                    .frame(width: 36, height: 36)
                    .background(
                        showCameraSettings
                            ? Color.white.opacity(0.28)
                            : Color.black.opacity(0.45)
                    )
                    .clipShape(Circle())
                    .overlay(Circle().stroke(Color.white.opacity(0.28), lineWidth: 1))
            }

            if let linkError = session.linkError {
                Text(linkError)
                    .font(.system(size: 10, weight: .bold, design: .monospaced))
                    .foregroundStyle(.orange)
                    .lineLimit(1)
            }

            Text(modeLabel)
                .font(.system(size: 11, weight: .bold, design: .monospaced))
                .foregroundStyle(.white)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(Color.black.opacity(0.45))
                .clipShape(Capsule())
                .overlay(Capsule().stroke(Color.white.opacity(0.28), lineWidth: 1))
        }
    }

    private var faceButtons: some View {
        HStack(spacing: 10) {
            faceButton(
                title: "LAMP",
                active: session.lampOn,
                tint: .yellow
            ) {
                session.toggleLamp()
            }
            faceButton(
                title: "AVOID",
                active: session.mode == .avoid,
                tint: .purple
            ) {
                session.setMode(session.mode == .avoid ? .drive : .avoid)
            }
            faceButton(
                title: "FOLLOW",
                active: session.mode == .follow,
                tint: Color(red: 0.35, green: 0.7, blue: 1.0)
            ) {
                session.setMode(session.mode == .follow ? .drive : .follow)
            }
        }
    }

    private func faceButton(
        title: String,
        active: Bool,
        tint: Color,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 9, weight: .heavy, design: .monospaced))
                .foregroundStyle(.white)
                .frame(width: 52, height: 52)
                .background(active ? tint.opacity(0.8) : Color.black.opacity(0.45))
                .clipShape(Circle())
                .overlay(Circle().stroke(Color.white.opacity(0.32), lineWidth: 1.5))
        }
        .buttonStyle(.plain)
    }

    private var c2URL: URL {
        URL(string: "http://\(rover.ip)/c2")!
    }

    private var modeLabel: String {
        switch session.mode {
        case .drive: return "DRIVE"
        case .avoid: return "AVOID"
        case .follow: return "FOLLOW"
        }
    }
}
