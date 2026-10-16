import Darwin
import Foundation

enum UDPDatagramError: Error {
    case socketFailed
    case bindFailed
    case sendFailed
}

/// POSIX UDP socket used for broadcast discovery and unicast control.
final class UDPDatagramSocket {
    private var fd: Int32 = -1
    private var source: DispatchSourceRead?
    private let queue = DispatchQueue(label: "com.galaxyrvr.udp")

    var onDatagram: ((Data, String, UInt16) -> Void)?

    deinit {
        close()
    }

    func open(port: UInt16 = 0, broadcast: Bool = true) throws {
        close()
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard fd >= 0 else { throw UDPDatagramError.socketFailed }

        var yes: Int32 = 1
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, socklen_t(MemoryLayout<Int32>.size))
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, socklen_t(MemoryLayout<Int32>.size))
        if broadcast {
            setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, socklen_t(MemoryLayout<Int32>.size))
        }

        var flags = fcntl(fd, F_GETFL, 0)
        if flags >= 0 {
            _ = fcntl(fd, F_SETFL, flags | O_NONBLOCK)
        }

        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        addr.sin_addr = in_addr(s_addr: INADDR_ANY.bigEndian)

        let bindResult = withUnsafePointer(to: &addr) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                Darwin.bind(fd, sockaddrPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard bindResult == 0 else {
            Darwin.close(fd)
            fd = -1
            throw UDPDatagramError.bindFailed
        }

        startReader()
    }

    func send(_ data: Data, to host: String, port: UInt16) throws {
        guard fd >= 0 else { throw UDPDatagramError.socketFailed }
        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        guard inet_pton(AF_INET, host, &addr.sin_addr) == 1 else {
            throw UDPDatagramError.sendFailed
        }

        let sent = data.withUnsafeBytes { rawBuffer in
            withUnsafePointer(to: &addr) { pointer in
                pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                    sendto(
                        fd,
                        rawBuffer.baseAddress,
                        rawBuffer.count,
                        0,
                        sockaddrPtr,
                        socklen_t(MemoryLayout<sockaddr_in>.size)
                    )
                }
            }
        }
        guard sent >= 0 else { throw UDPDatagramError.sendFailed }
    }

    func close() {
        source?.cancel()
        source = nil
        if fd >= 0 {
            Darwin.close(fd)
            fd = -1
        }
    }

    private func startReader() {
        let capturedFD = fd
        let reader = DispatchSource.makeReadSource(fileDescriptor: capturedFD, queue: queue)
        reader.setEventHandler { [weak self] in
            self?.drain()
        }
        reader.setCancelHandler { }
        source = reader
        reader.resume()
    }

    private func drain() {
        var buffer = [UInt8](repeating: 0, count: 2048)
        var addr = sockaddr_in()
        var addrLen = socklen_t(MemoryLayout<sockaddr_in>.size)
        while true {
            let received = withUnsafeMutablePointer(to: &addr) { pointer in
                pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockaddrPtr in
                    recvfrom(
                        fd,
                        &buffer,
                        buffer.count,
                        0,
                        sockaddrPtr,
                        &addrLen
                    )
                }
            }
            if received < 0 {
                break
            }
            var hostBytes = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
            inet_ntop(AF_INET, &addr.sin_addr, &hostBytes, socklen_t(INET_ADDRSTRLEN))
            let host = String(cString: hostBytes)
            let port = UInt16(bigEndian: addr.sin_port)
            let data = Data(buffer.prefix(received))
            DispatchQueue.main.async { [weak self] in
                self?.onDatagram?(data, host, port)
            }
        }
    }
}
