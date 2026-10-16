// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "GalaxyRVRProtocol",
    platforms: [
        .macOS(.v13),
        .iOS(.v17),
    ],
    products: [
        .library(name: "GalaxyRVRProtocol", targets: ["GalaxyRVRProtocol"]),
    ],
    targets: [
        .target(
            name: "GalaxyRVRProtocol",
            path: "GalaxyRVR/GalaxyRVR/Protocol",
            exclude: [
                "RoverUDPClient.swift",
                "UDPDatagramSocket.swift",
            ]
        ),
        .testTarget(
            name: "GalaxyRVRProtocolTests",
            dependencies: ["GalaxyRVRProtocol"],
            path: "GalaxyRVRProtocolTests"
        ),
    ]
)
