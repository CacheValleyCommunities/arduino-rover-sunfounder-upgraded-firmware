import Foundation
import UIKit

final class MJPEGStream: NSObject, URLSessionDataDelegate {
    var onFrame: ((UIImage) -> Void)?
    var onError: ((String) -> Void)?

    private var session: URLSession?
    private var task: URLSessionDataTask?
    private var buffer = Data()
    private let queue = OperationQueue()
    private var lastFrameAt = Date.distantPast
    private var streamURL: URL?
    private var retryWorkItem: DispatchWorkItem?
    private var retryDelay: TimeInterval = 0.4
    private var stopped = true

    override init() {
        super.init()
        queue.maxConcurrentOperationCount = 1
        queue.name = "com.galaxyrvr.mjpeg"
    }

    func start(url: URL) {
        stop()
        stopped = false
        streamURL = url
        retryDelay = 0.4
        startTask()
    }

    func stop() {
        stopped = true
        retryWorkItem?.cancel()
        retryWorkItem = nil
        task?.cancel()
        task = nil
        session?.invalidateAndCancel()
        session = nil
        buffer.removeAll()
    }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        retryDelay = 0.4
        buffer.append(data)
        extractFrames()
        if buffer.count > 1_000_000 {
            buffer.removeFirst(buffer.count - 200_000)
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        if let error, (error as? URLError)?.code != .cancelled {
            let message = Self.userMessage(for: error)
            DispatchQueue.main.async { self.onError?(message) }
            scheduleRetry()
        }
    }

    private func startTask() {
        guard !stopped, let url = streamURL else { return }
        buffer.removeAll(keepingCapacity: true)
        if session == nil {
            let config = URLSessionConfiguration.default
            config.timeoutIntervalForRequest = 8
            config.timeoutIntervalForResource = .infinity
            config.requestCachePolicy = .reloadIgnoringLocalCacheData
            config.waitsForConnectivity = false
            session = URLSession(configuration: config, delegate: self, delegateQueue: queue)
        }
        var request = URLRequest(url: url)
        request.setValue("multipart/x-mixed-replace", forHTTPHeaderField: "Accept")
        task = session?.dataTask(with: request)
        task?.resume()
    }

    private func scheduleRetry() {
        guard !stopped, streamURL != nil else { return }
        retryWorkItem?.cancel()
        let delay = retryDelay
        retryDelay = min(retryDelay * 1.6, 3.0)
        let work = DispatchWorkItem { [weak self] in
            guard let self, !self.stopped else { return }
            self.startTask()
        }
        retryWorkItem = work
        DispatchQueue.global(qos: .userInitiated).asyncAfter(deadline: .now() + delay, execute: work)
    }

    private static func userMessage(for error: Error) -> String {
        if let urlError = error as? URLError, urlError.code == .cannotConnectToHost {
            return "Waiting for camera…"
        }
        return error.localizedDescription
    }

    private func extractFrames() {
        let soi: [UInt8] = [0xFF, 0xD8]
        let eoi: [UInt8] = [0xFF, 0xD9]
        while true {
            guard let start = buffer.range(of: Data(soi)) else {
                if buffer.count > 2 { buffer.removeAll(keepingCapacity: true) }
                return
            }
            if start.lowerBound > 0 {
                buffer.removeSubrange(..<start.lowerBound)
            }
            guard let end = buffer.range(of: Data(eoi), in: 2..<buffer.count) else { return }
            let jpeg = buffer.subdata(in: 0..<end.upperBound)
            buffer.removeSubrange(..<end.upperBound)
            guard Date().timeIntervalSince(lastFrameAt) > 0.016,
                  let image = UIImage(data: jpeg)
            else { continue }
            lastFrameAt = Date()
            DispatchQueue.main.async { self.onFrame?(image) }
        }
    }
}
