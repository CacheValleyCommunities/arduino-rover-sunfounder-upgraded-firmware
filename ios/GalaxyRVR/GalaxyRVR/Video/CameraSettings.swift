import Foundation

enum CameraFramesize: String, CaseIterable, Identifiable {
    case qvga = "QVGA"
    case hvga = "HVGA"
    case vga = "VGA"

    var id: String { rawValue }

    var label: String {
        switch self {
        case .qvga: return "QVGA"
        case .hvga: return "HVGA"
        case .vga: return "VGA"
        }
    }

    var detail: String {
        switch self {
        case .qvga: return "320×240"
        case .hvga: return "480×320"
        case .vga: return "640×480"
        }
    }

    static func parse(_ raw: String?) -> CameraFramesize {
        guard let raw else { return .hvga }
        return CameraFramesize(rawValue: raw.uppercased()) ?? .hvga
    }
}

struct CameraStreamSettings: Equatable {
    static let qualityMin = 8
    static let qualityMax = 20
    static let qualityDefault = 15

    var jpegQuality: Int
    var framesize: CameraFramesize

    static func clampQuality(_ value: Int) -> Int {
        min(max(value, qualityMin), qualityMax)
    }
}

struct CameraSettingsClient {
    let host: String
    private let session: URLSession

    init(host: String) {
        self.host = host
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 4
        config.timeoutIntervalForResource = 6
        config.waitsForConnectivity = false
        session = URLSession(configuration: config)
    }

    func fetch() async throws -> CameraStreamSettings {
        let (data, response) = try await session.data(from: url("/settings"))
        try Self.throwIfHTTPError(response, data: data)
        let payload = try JSONDecoder().decode(SettingsPayload.self, from: data)
        return CameraStreamSettings(
            jpegQuality: CameraStreamSettings.clampQuality(
                payload.cameraJpegQuality ?? CameraStreamSettings.qualityDefault
            ),
            framesize: CameraFramesize.parse(payload.cameraFramesize)
        )
    }

    func setJpegQuality(_ value: Int) async throws {
        let quality = CameraStreamSettings.clampQuality(value)
        try await post(path: "/set-cameraJpegQuality", field: "camJpegQuality", value: "\(quality)")
    }

    func setFramesize(_ size: CameraFramesize) async throws {
        try await post(path: "/set-cameraFramesize", field: "camFramesize", value: size.rawValue)
    }

    private func post(path: String, field: String, value: String) async throws {
        var request = URLRequest(url: url(path))
        request.httpMethod = "POST"
        request.setValue(
            "application/x-www-form-urlencoded",
            forHTTPHeaderField: "Content-Type"
        )
        request.httpBody = "\(field)=\(value)".data(using: .utf8)
        let (data, response) = try await session.data(for: request)
        try Self.throwIfHTTPError(response, data: data)
    }

    private func url(_ path: String) -> URL {
        URL(string: "http://\(host)\(path)")!
    }

    private static func throwIfHTTPError(_ response: URLResponse, data: Data) throws {
        guard let http = response as? HTTPURLResponse else { return }
        guard (200..<300).contains(http.statusCode) else {
            let body = String(data: data, encoding: .utf8) ?? ""
            throw CameraSettingsError.http(http.statusCode, body)
        }
    }
}

enum CameraSettingsError: LocalizedError {
    case http(Int, String)

    var errorDescription: String? {
        switch self {
        case .http(let code, _) where code == 404:
            return "Camera firmware needs the quality API. Reflash the ESP32-CAM."
        case .http(let code, let body):
            return body.isEmpty ? "Camera settings failed (\(code))" : body
        }
    }
}

private struct SettingsPayload: Decodable {
    var cameraJpegQuality: Int?
    var cameraFramesize: String?
}
