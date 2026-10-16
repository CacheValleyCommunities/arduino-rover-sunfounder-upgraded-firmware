import XCTest
@testable import GalaxyRVRProtocol

final class BinaryProtocolTests: XCTestCase {
    func testMotorsGoldenPacketMatchesFirmware() {
        XCTAssertEqual(
            [UInt8](BinaryProtocol.motors(left: 40, right: -100)),
            [0x01, 0x28, 0x9C]
        )
    }

    func testRgbServoLampFrames() {
        XCTAssertEqual([UInt8](BinaryProtocol.rgb(red: 1, green: 2, blue: 3)), [0x02, 1, 2, 3])
        XCTAssertEqual([UInt8](BinaryProtocol.servo(90)), [0x03, 90])
        XCTAssertEqual([UInt8](BinaryProtocol.servo(200)), [0x03, 140])
        XCTAssertEqual([UInt8](BinaryProtocol.lamp(true)), [0x04, 1])
        XCTAssertEqual([UInt8](BinaryProtocol.lamp(false)), [0x04, 0])
    }

    func testObstacleFramesMatchUnoEncoding() {
        XCTAssertEqual([UInt8](BinaryProtocol.obstacle(mode: .drive)), [0x05, 0, 0, 65])
        XCTAssertEqual([UInt8](BinaryProtocol.obstacle(mode: .avoid)), [0x05, 1, 0, 65])
        XCTAssertEqual([UInt8](BinaryProtocol.obstacle(mode: .follow)), [0x05, 1, 1, 65])
    }

    func testArcadeMixForwardAndTurn() {
        let forward = BinaryProtocol.arcadeMix(x: 0, y: 1)
        XCTAssertEqual(forward.0, 100)
        XCTAssertEqual(forward.1, 100)

        let left = BinaryProtocol.arcadeMix(x: -1, y: 0)
        XCTAssertEqual(left.0, -100)
        XCTAssertEqual(left.1, 100)

        let clamped = BinaryProtocol.arcadeMix(x: 2, y: 2)
        XCTAssertEqual(clamped.0, 100)
        XCTAssertEqual(clamped.1, 0)
    }

    func testParseTelemetryRejectsBadChecksumOrFraming() {
        XCTAssertNil(BinaryProtocol.parseTelemetry(Data([0x00])))
        var packet: [UInt8] = [
            0xA0, 0x07, 0x00,
            0x81, 0x00, 0xFA,
            0x82, 0x02,
            0x83, 0x8C,
            0xA1,
        ]
        packet[2] = 0xFF
        XCTAssertNil(BinaryProtocol.parseTelemetry(Data(packet)))
    }

    func testParseTelemetryGoldenPacketFromFirmwareLayout() {
        var packet: [UInt8] = [
            0xA0, 0x07, 0x00,
            0x81, 0x00, 0xFA, // 250 mm
            0x82, 0x02,       // left IR
            0x83, 140,        // 7.40 V
            0xA1,
        ]
        var checksum: UInt8 = 0
        for i in 0..<10 {
            checksum ^= packet[i]
        }
        packet[2] = checksum

        let telemetry = BinaryProtocol.parseTelemetry(Data(packet))
        XCTAssertNotNil(telemetry)
        XCTAssertEqual(telemetry?.distanceMM, 250)
        XCTAssertEqual(telemetry?.distanceCM, 25.0)
        XCTAssertEqual(telemetry?.irLeft, true)
        XCTAssertEqual(telemetry?.irRight, false)
        XCTAssertEqual(telemetry?.batteryRaw, 140)
        XCTAssertEqual(telemetry?.batteryFraction, (7.4 - 6.6) / (8.4 - 6.6))
    }

    func testIrBitsMatchUnoPacking() {
        func packet(ir: UInt8) -> Data {
            var bytes: [UInt8] = [
                0xA0, 0x07, 0x00,
                0x81, 0x00, 0x00,
                0x82, ir,
                0x83, 0x00,
                0xA1,
            ]
            var checksum: UInt8 = 0
            for i in 0..<10 {
                checksum ^= bytes[i]
            }
            bytes[2] = checksum
            return Data(bytes)
        }

        XCTAssertEqual(BinaryProtocol.parseTelemetry(packet(ir: 0b10))?.irLeft, true)
        XCTAssertEqual(BinaryProtocol.parseTelemetry(packet(ir: 0b10))?.irRight, false)
        XCTAssertEqual(BinaryProtocol.parseTelemetry(packet(ir: 0b01))?.irLeft, false)
        XCTAssertEqual(BinaryProtocol.parseTelemetry(packet(ir: 0b01))?.irRight, true)
    }

    func testConstantsMatchFirmware() {
        XCTAssertEqual(BinaryProtocol.startBit, 0xA0)
        XCTAssertEqual(BinaryProtocol.endBit, 0xA1)
        XCTAssertEqual(RoverEntity.motors.rawValue, 0x01)
        XCTAssertEqual(RoverEntity.ultrasonic.rawValue, 0x81)
        XCTAssertEqual(RoverEntity.config.rawValue, 0x85)
    }

    func testParseTelemetrySkipsConfigEcho() {
        var packet: [UInt8] = [
            0xA0, 26, 0x00,
            0x81, 0x00, 0xFA,
            0x82, 0x02,
            0x83, 140,
            0x85,
        ]
        packet += Array(repeating: 0, count: 18)
        packet.append(0xA1)
        packet[11] = 0x00
        packet[12] = 12
        var checksum: UInt8 = 0
        for i in 0..<(packet.count - 1) {
            checksum ^= packet[i]
        }
        packet[2] = checksum

        let telemetry = BinaryProtocol.parseTelemetry(Data(packet))
        XCTAssertEqual(telemetry?.distanceMM, 250)
        XCTAssertEqual(telemetry?.irLeft, true)
        XCTAssertEqual(telemetry?.batteryRaw, 140)
    }
}
