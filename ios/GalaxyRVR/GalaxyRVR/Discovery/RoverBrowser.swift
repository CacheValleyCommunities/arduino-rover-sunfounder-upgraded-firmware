import Darwin
import Foundation
import Network
import NetworkExtension

@MainActor
@Observable
final class RoverBrowser {
    var rovers: [DiscoveredRover] = []
    var isScanning = false
    var status = "Idle"
    var hotspotError: String?

    private let client = RoverUDPClient()
    private var browser: NWBrowser?
    private var probeTimer: Timer?
    private var seen: [String: DiscoveredRover] = [:]

    private static let apIP = "192.168.4.1"
    private static let udpPort: UInt16 = 30103

    func start() {
        stop()
        isScanning = true
        status = "Scanning…"
        hotspotError = nil
        seen.removeAll()
        rovers.removeAll()

        client.onIdentity = { [weak self] identity, host in
            self?.ingest(identity: identity, sourceHost: host, source: "UDP")
        }
        do {
            try client.startListening(port: Self.udpPort)
        } catch {
            do {
                try client.startListening(port: 0)
                status = "Scanning (ephemeral UDP)…"
            } catch {
                status = "UDP listen failed"
                isScanning = false
                return
            }
        }

        startBonjour()
        probeKnownHosts()
        probeTimer = Timer.scheduledTimer(withTimeInterval: 0.75, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.probeKnownHosts()
            }
        }
    }

    func stop() {
        probeTimer?.invalidate()
        probeTimer = nil
        browser?.cancel()
        browser = nil
        client.stop()
        isScanning = false
        if status.hasPrefix("Scanning") {
            status = rovers.isEmpty ? "No rovers found" : "Found \(rovers.count)"
        }
    }

    func joinRoverHotspot() {
        status = "Joining GalaxyRVR Wi-Fi…"
        hotspotError = nil
        let config = NEHotspotConfiguration(
            ssidPrefix: "GalaxyRVR",
            passphrase: "12345678",
            isWEP: false
        )
        config.joinOnce = false
        NEHotspotConfigurationManager.shared.apply(config) { [weak self] error in
            Task { @MainActor in
                guard let self else { return }
                if let error {
                    let nsError = error as NSError
                    if nsError.domain == NEHotspotConfigurationErrorDomain,
                       nsError.code == NEHotspotConfigurationError.alreadyAssociated.rawValue {
                        self.status = "Already on GalaxyRVR Wi-Fi"
                    } else {
                        self.hotspotError = error.localizedDescription
                        self.status = "Hotspot join failed"
                        return
                    }
                } else {
                    self.status = "Joined rover hotspot"
                }
                self.offerAPFallback()
                self.probeKnownHosts()
                try? await Task.sleep(nanoseconds: 1_500_000_000)
                self.probeKnownHosts()
            }
        }
    }

    private func startBonjour() {
        // Browse only. Never NWConnection-resolve the service: that queries
        // GalaxyRVR.local:30103 and ESP32 mDNS rarely returns A+AAAA in time.
        let descriptor = NWBrowser.Descriptor.bonjourWithTXTRecord(
            type: "_galaxyrvr._udp",
            domain: "local."
        )
        let parameters = NWParameters.udp
        parameters.includePeerToPeer = true
        let nwBrowser = NWBrowser(for: descriptor, using: parameters)
        nwBrowser.browseResultsChangedHandler = { [weak self] results, _ in
            Task { @MainActor in
                self?.handleBonjour(results)
            }
        }
        nwBrowser.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                if case .failed(let error) = state {
                    self?.status = "Bonjour: \(error.localizedDescription)"
                }
            }
        }
        browser = nwBrowser
        nwBrowser.start(queue: .main)
    }

    private func handleBonjour(_ results: Set<NWBrowser.Result>) {
        for result in results {
            if case .hostPort(let host, let port) = result.endpoint,
               case .ipv4(let address) = host {
                let udpPort = port.rawValue == 0 ? Self.udpPort : port.rawValue
                client.probe(host: "\(address)", port: udpPort)
            }

            if case .bonjour(let txt) = result.metadata {
                ingestBonjourTXT(txt)
            }
        }
    }

    private func ingestBonjourTXT(_ txt: NWTXTRecord) {
        let fields = txt.dictionary
        let name = fields["name"] ?? "GalaxyRVR"
        let type = fields["type"] ?? name
        let port = UInt16(fields["udp"] ?? "") ?? Self.udpPort
        for key in ["ap", "sta"] {
            guard let host = fields[key], isPlausibleIPv4(host) else { continue }
            client.probe(host: host, port: port)
            remember(
                DiscoveredRover(
                    name: name,
                    type: type,
                    ip: host,
                    udpPort: port,
                    videoURL: URL(string: "http://\(host):9000/mjpg"),
                    source: "Bonjour"
                )
            )
        }
    }

    private func probeKnownHosts() {
        client.probeBroadcast(port: Self.udpPort)
        client.probe(host: Self.apIP, port: Self.udpPort)
        for ip in localIPv4Addresses() {
            if ip.hasPrefix("192.168.4.") {
                offerAPFallback()
            }
            if let gateway = subnetGateway(for: ip) {
                client.probe(host: gateway, port: Self.udpPort)
            }
        }
    }

    private func offerAPFallback() {
        remember(
            DiscoveredRover(
                name: "GalaxyRVR",
                type: "GalaxyRVR",
                ip: Self.apIP,
                udpPort: Self.udpPort,
                videoURL: URL(string: "http://\(Self.apIP):9000/mjpg"),
                source: "AP"
            )
        )
    }

    private func ingest(identity: RoverIdentity, sourceHost: String, source: String) {
        var hosts = [sourceHost, identity.ap]
        if let sta = identity.sta, sta.isEmpty == false, sta != "0.0.0.0" {
            hosts.append(sta)
        }
        let video = URL(string: identity.video)
        let port = UInt16(clamping: identity.udp)
        for host in Set(hosts) where isPlausibleIPv4(host) {
            remember(
                DiscoveredRover(
                    name: identity.name,
                    type: identity.type,
                    ip: host,
                    udpPort: port == 0 ? Self.udpPort : port,
                    videoURL: videoHostReplaced(video, with: host) ?? video,
                    source: source
                )
            )
        }
    }

    private func remember(_ rover: DiscoveredRover) {
        if let existing = seen[rover.id], existing.source == "UDP", rover.source != "UDP" {
            return
        }
        seen[rover.id] = rover
        rovers = seen.values.sorted { $0.ip < $1.ip }
        status = "Found \(rovers.count)"
    }

    private func videoHostReplaced(_ url: URL?, with host: String) -> URL? {
        guard var components = url.flatMap({ URLComponents(url: $0, resolvingAgainstBaseURL: false) }) else {
            return URL(string: "http://\(host):9000/mjpg")
        }
        components.host = host
        return components.url
    }

    private func isPlausibleIPv4(_ host: String) -> Bool {
        let parts = host.split(separator: ".")
        guard parts.count == 4 else { return false }
        return parts.allSatisfy { UInt8($0) != nil }
    }

    private func subnetGateway(for ip: String) -> String? {
        let parts = ip.split(separator: ".")
        guard parts.count == 4 else { return nil }
        let gateway = "\(parts[0]).\(parts[1]).\(parts[2]).1"
        return gateway == ip ? nil : gateway
    }

    private func localIPv4Addresses() -> [String] {
        var addresses: [String] = []
        var ifaddrList: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&ifaddrList) == 0 else { return [] }
        defer { freeifaddrs(ifaddrList) }

        var cursor = ifaddrList
        while let iface = cursor {
            defer { cursor = iface.pointee.ifa_next }
            guard let addr = iface.pointee.ifa_addr,
                  addr.pointee.sa_family == sa_family_t(AF_INET)
            else { continue }

            var hostname = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            let result = getnameinfo(
                addr,
                socklen_t(MemoryLayout<sockaddr_in>.size),
                &hostname,
                socklen_t(hostname.count),
                nil,
                0,
                NI_NUMERICHOST
            )
            guard result == 0 else { continue }
            let ip = String(cString: hostname)
            if ip.hasPrefix("127.") || ip.hasPrefix("169.254.") { continue }
            addresses.append(ip)
        }
        return addresses
    }
}
