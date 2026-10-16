import Foundation

enum RoverEntity: UInt8 {
    case motors = 0x01
    case rgb = 0x02
    case servo = 0x03
    case lamp = 0x04
    case obstacle = 0x05
    case ultrasonic = 0x81
    case ir = 0x82
    case battery = 0x83
    case config = 0x85
}

enum RoverDriveMode: Equatable {
    case drive
    case avoid
    case follow

    /// Matches UNO `0x05` encoding: state=0 is app control; state≠0 + mode==0 is avoid.
    var obstacleFrame: (state: UInt8, mode: UInt8, power: UInt8) {
        switch self {
        case .drive:
            return (0, 0, 65)
        case .avoid:
            return (1, 0, 65)
        case .follow:
            return (1, 1, 65)
        }
    }
}

struct RoverTelemetry: Equatable {
    var distanceMM: Int?
    var irLeft: Bool = false
    var irRight: Bool = false
    var batteryRaw: UInt8?

    var distanceCM: Double? {
        guard let distanceMM, distanceMM > 0, distanceMM < 30000 else { return nil }
        return Double(distanceMM) / 10.0
    }

    /// CAM sends `round((volts - 6) * 100)`. Pack voltage ~6.6–8.4 V.
    var batteryFraction: Double? {
        guard let batteryRaw else { return nil }
        let volts = 6.0 + Double(batteryRaw) / 100.0
        let fraction = (volts - 6.6) / (8.4 - 6.6)
        return min(max(fraction, 0), 1)
    }
}

enum BinaryProtocol {
    static let startBit: UInt8 = 0xA0
    static let endBit: UInt8 = 0xA1
    static let probe = Data("GRVR?".utf8)
    static let ping = Data("ping".utf8)

    static func motors(left: Int8, right: Int8) -> Data {
        Data([RoverEntity.motors.rawValue, UInt8(bitPattern: left), UInt8(bitPattern: right)])
    }

    static func rgb(red: UInt8, green: UInt8, blue: UInt8) -> Data {
        Data([RoverEntity.rgb.rawValue, red, green, blue])
    }

    static func servo(_ angle: UInt8) -> Data {
        Data([RoverEntity.servo.rawValue, min(angle, 140)])
    }

    static func lamp(_ on: Bool) -> Data {
        Data([RoverEntity.lamp.rawValue, on ? 1 : 0])
    }

    static func obstacle(mode: RoverDriveMode) -> Data {
        let frame = mode.obstacleFrame
        return Data([RoverEntity.obstacle.rawValue, frame.state, frame.mode, frame.power])
    }

    static func arcadeMix(x: Double, y: Double, maxPower: Double = 100) -> (Int8, Int8) {
        let throttle = min(max(y, -1), 1)
        let turn = min(max(x, -1), 1)
        var left = throttle + turn
        var right = throttle - turn
        let peak = max(abs(left), abs(right), 1)
        if peak > 1 {
            left /= peak
            right /= peak
        }
        return (
            Int8(clamping: Int((left * maxPower).rounded())),
            Int8(clamping: Int((right * maxPower).rounded()))
        )
    }

    static func parseTelemetry(_ packet: Data) -> RoverTelemetry? {
        guard packet.count >= 11,
              packet.first == startBit,
              packet.last == endBit
        else { return nil }

        var checksum: UInt8 = 0
        for byte in packet.dropLast() {
            checksum ^= byte
        }
        guard checksum == 0 else { return nil }

        var telemetry = RoverTelemetry()
        var index = 3
        let bytes = [UInt8](packet)
        while index < bytes.count - 1 {
            switch bytes[index] {
            case RoverEntity.ultrasonic.rawValue:
                guard index + 2 < bytes.count - 1 else { return telemetry }
                let mm = (UInt16(bytes[index + 1]) << 8) | UInt16(bytes[index + 2])
                telemetry.distanceMM = Int(mm)
                index += 3
            case RoverEntity.ir.rawValue:
                guard index + 1 < bytes.count - 1 else { return telemetry }
                let bits = bytes[index + 1]
                telemetry.irLeft = (bits & 0b10) != 0
                telemetry.irRight = (bits & 0b01) != 0
                index += 2
            case RoverEntity.battery.rawValue:
                guard index + 1 < bytes.count - 1 else { return telemetry }
                telemetry.batteryRaw = bytes[index + 1]
                index += 2
            case RoverEntity.config.rawValue:
                guard index + 18 < bytes.count - 1 else { return telemetry }
                index += 19
            default:
                index += 1
            }
        }
        return telemetry
    }
}
