import Foundation
import UIKit

@MainActor
@Observable
final class DriveSession {
    private static let lookDeadzone = 0.12
    private static let lookDegreesPerSecond = 32.0
    private static let lookTick: TimeInterval = 0.05

    let rover: DiscoveredRover
    let client = RoverUDPClient()
    private let video = MJPEGStream()
    private var cameraSettings: CameraSettingsClient?
    private var lookTimer: Timer?
    private var lookRate = 0.0
    private var lastSentServo: UInt8?
    private var cameraChangeToken = 0

    var frame: UIImage?
    var videoError: String?
    var mode: RoverDriveMode = .drive
    var lampOn = false
    var servoAngle: Double = 90
    var linkError: String?
    var jpegQuality = CameraStreamSettings.qualityDefault
    var framesize = CameraFramesize.hvga
    var cameraSettingsBusy = false
    var cameraSettingsError: String?
    private(set) var joystickY = 0.0

    var telemetry: RoverTelemetry { client.telemetry }

    /// Commanded forward/back effort, 0...1, for the tach cluster.
    var throttle: Double { min(abs(joystickY), 1) }

    init(rover: DiscoveredRover) {
        self.rover = rover
    }

    func start() {
        video.onFrame = { [weak self] image in
            Task { @MainActor in
                self?.frame = image
                self?.videoError = nil
            }
        }
        video.onError = { [weak self] message in
            Task { @MainActor in
                self?.videoError = message
            }
        }
        do {
            try client.connect(to: rover)
            client.sendMode(.drive)
            sendServoIfNeeded(force: true)
            linkError = nil
        } catch {
            linkError = error.localizedDescription
        }
        if let url = rover.videoURL {
            video.start(url: url)
        }
        cameraSettings = CameraSettingsClient(host: rover.ip)
        startLookLoop()
        Task { await refreshCameraSettings() }
    }

    func stop() {
        lookTimer?.invalidate()
        lookTimer = nil
        lookRate = 0
        resetJoystick()
        client.sendMotors(left: 0, right: 0)
        client.sendMode(.drive)
        client.stop()
        video.stop()
        frame = nil
    }

    func setJoystick(x: Double, y: Double) {
        joystickY = y
        guard mode == .drive else { return }
        let mixed = BinaryProtocol.arcadeMix(x: x, y: y)
        client.sendMotors(left: mixed.0, right: mixed.1)
    }

    func joystickEnded() {
        resetJoystick()
        guard mode == .drive else { return }
        client.sendMotors(left: 0, right: 0)
    }

    func setMode(_ mode: RoverDriveMode) {
        self.mode = mode
        if mode != .drive {
            resetJoystick()
        } else {
            resetJoystick()
            client.sendMotors(left: 0, right: 0)
        }
        client.sendMode(mode)
    }

    private func resetJoystick() {
        joystickY = 0
    }

    func toggleLamp() {
        lampOn.toggle()
        client.sendLamp(lampOn)
    }

    func setLookRate(_ rate: Double) {
        if abs(rate) < Self.lookDeadzone {
            lookRate = 0
            return
        }
        lookRate = min(max(rate, -1), 1)
    }

    func setJpegQuality(_ value: Int) {
        let quality = CameraStreamSettings.clampQuality(value)
        guard quality != jpegQuality else { return }
        let previous = jpegQuality
        jpegQuality = quality
        let token = nextCameraChangeToken()
        Task {
            await applyCameraChange(
                previousQuality: previous,
                previousSize: framesize,
                token: token
            )
        }
    }

    func setFramesize(_ size: CameraFramesize) {
        guard size != framesize else { return }
        let previous = framesize
        framesize = size
        let token = nextCameraChangeToken()
        Task {
            await applyCameraChange(
                previousQuality: jpegQuality,
                previousSize: previous,
                token: token
            )
        }
    }

    private func startLookLoop() {
        lookTimer?.invalidate()
        lookTimer = Timer.scheduledTimer(withTimeInterval: Self.lookTick, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.tickLook()
            }
        }
    }

    private func tickLook() {
        guard lookRate != 0 else { return }
        let delta = lookRate * Self.lookDegreesPerSecond * Self.lookTick
        servoAngle = min(max(servoAngle + delta, 0), 140)
        sendServoIfNeeded(force: false)
    }

    private func sendServoIfNeeded(force: Bool) {
        let angle = UInt8(servoAngle.rounded())
        guard force || lastSentServo != angle else { return }
        lastSentServo = angle
        client.sendServo(angle)
    }

    private func refreshCameraSettings() async {
        guard let cameraSettings else { return }
        cameraSettingsBusy = true
        defer { cameraSettingsBusy = false }
        do {
            let settings = try await cameraSettings.fetch()
            jpegQuality = settings.jpegQuality
            framesize = settings.framesize
            cameraSettingsError = nil
        } catch {
            cameraSettingsError = error.localizedDescription
        }
    }

    private func nextCameraChangeToken() -> Int {
        cameraChangeToken += 1
        return cameraChangeToken
    }

    private func applyCameraChange(
        previousQuality: Int,
        previousSize: CameraFramesize,
        token: Int
    ) async {
        guard let cameraSettings else { return }
        cameraSettingsBusy = true
        defer { cameraSettingsBusy = false }
        do {
            if jpegQuality != previousQuality {
                try await cameraSettings.setJpegQuality(jpegQuality)
            }
            if framesize != previousSize {
                try await cameraSettings.setFramesize(framesize)
            }
            if token == cameraChangeToken {
                cameraSettingsError = nil
            }
        } catch {
            guard token == cameraChangeToken else { return }
            jpegQuality = previousQuality
            framesize = previousSize
            cameraSettingsError = error.localizedDescription
        }
    }
}
