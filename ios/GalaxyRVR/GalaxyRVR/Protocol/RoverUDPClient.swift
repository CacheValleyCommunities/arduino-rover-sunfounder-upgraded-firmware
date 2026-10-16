import Foundation

struct RoverIdentity: Decodable, Equatable {
    var name: String
    var type: String
    var udp: Int
    var video: String
    var ap: String
    var sta: String?

    enum CodingKeys: String, CodingKey {
        case name = "Name"
        case type = "Type"
        case udp
        case video
        case ap
        case sta
    }
}

struct DiscoveredRover: Identifiable, Equatable, Hashable {
    var id: String { "\(ip):\(udpPort)" }
    var name: String
    var type: String
    var ip: String
    var udpPort: UInt16
    var videoURL: URL?
    var source: String
}

@MainActor
@Observable
final class RoverUDPClient {
    private let socket = UDPDatagramSocket()
    private(set) var telemetry = RoverTelemetry()
    private(set) var isLinked = false
    private var roverIP = ""
    private var roverPort: UInt16 = 30103
    private var keepaliveTimer: Timer?
    private var motorTimer: Timer?
    private var pendingLeft: Int8 = 0
    private var pendingRight: Int8 = 0
    private var motorsDirty = false
    private var lastSentLeft: Int8?
    private var lastSentRight: Int8?

    var onIdentity: ((RoverIdentity, String) -> Void)?

    func startListening(port: UInt16 = 0) throws {
        socket.onDatagram = { [weak self] data, host, _ in
            Task { @MainActor in
                self?.handleDatagram(data, from: host)
            }
        }
        try socket.open(port: port, broadcast: true)
    }

    func connect(to rover: DiscoveredRover) throws {
        stop()
        roverIP = rover.ip
        roverPort = rover.udpPort
        try startListening()
        isLinked = true
        send(BinaryProtocol.ping)
        startKeepalive()
        startMotorLoop()
    }

    func stop() {
        keepaliveTimer?.invalidate()
        keepaliveTimer = nil
        motorTimer?.invalidate()
        motorTimer = nil
        if isLinked {
            send(BinaryProtocol.motors(left: 0, right: 0))
            send(BinaryProtocol.obstacle(mode: .drive))
        }
        isLinked = false
        socket.close()
    }

    func sendMotors(left: Int8, right: Int8) {
        pendingLeft = left
        pendingRight = right
        motorsDirty = true
        if left == 0 && right == 0 {
            flushMotors(force: true)
        }
    }

    func sendServo(_ angle: UInt8) {
        send(BinaryProtocol.servo(angle))
    }

    func sendLamp(_ on: Bool) {
        send(BinaryProtocol.lamp(on))
    }

    func sendMode(_ mode: RoverDriveMode) {
        send(BinaryProtocol.obstacle(mode: mode))
    }

    func sendRGB(red: UInt8, green: UInt8, blue: UInt8) {
        send(BinaryProtocol.rgb(red: red, green: green, blue: blue))
    }

    func probe(host: String, port: UInt16 = 30103) {
        try? socket.send(BinaryProtocol.probe, to: host, port: port)
    }

    func probeBroadcast(port: UInt16 = 30103) {
        try? socket.send(BinaryProtocol.probe, to: "255.255.255.255", port: port)
    }

    private func startKeepalive() {
        keepaliveTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.send(BinaryProtocol.ping)
            }
        }
    }

    private func startMotorLoop() {
        motorTimer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.flushMotors(force: false)
            }
        }
    }

    private func flushMotors(force: Bool) {
        guard isLinked, motorsDirty || force else { return }
        if !force, lastSentLeft == pendingLeft, lastSentRight == pendingRight {
            return
        }
        send(BinaryProtocol.motors(left: pendingLeft, right: pendingRight))
        lastSentLeft = pendingLeft
        lastSentRight = pendingRight
        if pendingLeft == 0 && pendingRight == 0 {
            motorsDirty = false
        }
    }

    private func send(_ data: Data) {
        guard isLinked || roverIP.isEmpty == false else { return }
        try? socket.send(data, to: roverIP, port: roverPort)
    }

    private func handleDatagram(_ data: Data, from host: String) {
        if let identity = try? JSONDecoder().decode(RoverIdentity.self, from: data) {
            onIdentity?(identity, host)
            return
        }
        if let parsed = BinaryProtocol.parseTelemetry(data) {
            telemetry = parsed
        }
    }
}
