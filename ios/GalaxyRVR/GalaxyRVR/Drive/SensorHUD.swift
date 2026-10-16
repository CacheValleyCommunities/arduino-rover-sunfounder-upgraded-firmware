import SwiftUI

private let hudCyan = Color(red: 0.45, green: 0.92, blue: 1.0)

struct SensorHUD: View {
    var telemetry: RoverTelemetry
    var mode: RoverDriveMode
    var throttle: Double
    var servoAngle: Double

    var body: some View {
        ZStack {
            VStack {
                HStack {
                    Spacer()
                    ProximityRadar(
                        distanceCM: telemetry.distanceCM,
                        irLeft: telemetry.irLeft,
                        irRight: telemetry.irRight
                    )
                    .frame(width: 128, height: 118)
                    .padding(.top, 52)
                    .padding(.trailing, 14)
                }
                Spacer()
                SpeedCluster(
                    throttle: throttle,
                    distanceCM: telemetry.distanceCM,
                    auto: mode != .drive
                )
                .padding(.bottom, 22)
            }
            HStack {
                Spacer()
                PitchLadder(angle: servoAngle)
                    .padding(.trailing, 168)
            }
        }
        .allowsHitTesting(false)
    }
}

struct FuelGauge: View {
    var fraction: Double?

    var body: some View {
        HStack(spacing: 8) {
            Text("FUEL")
                .font(.system(size: 10, weight: .bold, design: .monospaced))
                .foregroundStyle(.white.opacity(0.7))
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.white.opacity(0.12))
                    Capsule()
                        .fill(fuelColor)
                        .frame(width: geo.size.width * (fraction ?? 0))
                }
                .overlay(Capsule().stroke(Color.white.opacity(0.28), lineWidth: 1))
            }
            .frame(width: 78, height: 8)
        }
    }

    private var fuelColor: Color {
        let value = fraction ?? 0
        if value < 0.2 { return .red }
        if value < 0.45 { return .orange }
        return hudCyan
    }
}

private struct SpeedCluster: View {
    var throttle: Double
    var distanceCM: Double?
    var auto: Bool

    var body: some View {
        ZStack {
            ThrottleArc()
                .stroke(Color.white.opacity(0.18), lineWidth: 8)
                .frame(width: 168, height: 168)
            ThrottleArc()
                .trim(from: 0, to: auto ? 0 : throttle)
                .stroke(
                    rangeColor,
                    style: StrokeStyle(lineWidth: 8, lineCap: .round)
                )
                .frame(width: 168, height: 168)
            VStack(spacing: 2) {
                Text(rangeLabel)
                    .font(.system(size: 28, weight: .bold, design: .monospaced))
                    .foregroundStyle(rangeColor)
                    .minimumScaleFactor(0.6)
                Text("RANGE cm")
                    .font(.system(size: 9, weight: .bold, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.45))
                Text(auto ? "AUTO" : String(format: "%02.0f THR", (throttle * 100).rounded()))
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundStyle(auto ? hudCyan : .white.opacity(0.75))
            }
            .offset(y: 18)
        }
        .frame(width: 176, height: 110)
    }

    private var rangeLabel: String {
        guard let distanceCM else { return "--" }
        return String(format: "%.0f", distanceCM)
    }

    private var rangeColor: Color {
        guard let distanceCM else { return .white.opacity(0.7) }
        if distanceCM < 20 { return .red }
        if distanceCM < 40 { return .orange }
        return hudCyan
    }
}

private struct ThrottleArc: Shape {
    func path(in rect: CGRect) -> Path {
        let inset = min(rect.width, rect.height) * 0.08
        let radius = min(rect.width, rect.height) / 2 - inset
        var path = Path()
        path.addArc(
            center: CGPoint(x: rect.midX, y: rect.midY + 18),
            radius: radius,
            startAngle: .degrees(160),
            endAngle: .degrees(20),
            clockwise: false
        )
        return path
    }
}

private struct ProximityRadar: View {
    var distanceCM: Double?
    var irLeft: Bool
    var irRight: Bool

    private let maxRange: Double = 200

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0)) { timeline in
            let pulse = (sin(timeline.date.timeIntervalSinceReferenceDate * 8) + 1) / 2
            Canvas { context, size in
                let rover = CGPoint(x: size.width / 2, y: size.height - 10)
                let radius = min(size.width, size.height) * 0.78
                let close = (distanceCM ?? .infinity) < 20
                let warn = irLeft || irRight || close

                var fan = Path()
                fan.addArc(
                    center: rover,
                    radius: radius,
                    startAngle: .degrees(180),
                    endAngle: .degrees(0),
                    clockwise: false
                )
                fan.addLine(to: rover)
                fan.closeSubpath()
                context.fill(fan, with: .color(Color.black.opacity(0.38)))
                context.stroke(fan, with: .color(.white.opacity(0.35)), lineWidth: 1)

                for ring in [0.33, 0.66, 1.0] {
                    var ringPath = Path()
                    ringPath.addArc(
                        center: rover,
                        radius: radius * ring,
                        startAngle: .degrees(180),
                        endAngle: .degrees(0),
                        clockwise: false
                    )
                    context.stroke(ringPath, with: .color(.white.opacity(0.12)), lineWidth: 1)
                }

                var heading = Path()
                heading.move(to: rover)
                heading.addLine(to: CGPoint(x: rover.x, y: rover.y - radius))
                context.stroke(heading, with: .color(.white.opacity(0.2)), lineWidth: 1)

                drawWedge(
                    context: context,
                    rover: rover,
                    radius: radius,
                    start: 188,
                    end: 236,
                    active: irLeft,
                    pulse: pulse
                )
                drawWedge(
                    context: context,
                    rover: rover,
                    radius: radius,
                    start: 304,
                    end: 352,
                    active: irRight,
                    pulse: pulse
                )

                if let distanceCM, distanceCM > 0 {
                    let pingRadius = min(distanceCM / maxRange, 1) * radius
                    var ping = Path()
                    ping.addArc(
                        center: rover,
                        radius: max(pingRadius, 6),
                        startAngle: .degrees(180),
                        endAngle: .degrees(0),
                        clockwise: false
                    )
                    let pingColor = close ? Color.red : hudCyan
                    context.stroke(
                        ping,
                        with: .color(pingColor.opacity(warn ? 0.45 + pulse * 0.55 : 0.9)),
                        lineWidth: 2
                    )
                }

                var chevron = Path()
                chevron.move(to: CGPoint(x: rover.x, y: rover.y - 9))
                chevron.addLine(to: CGPoint(x: rover.x - 6, y: rover.y + 3))
                chevron.addLine(to: CGPoint(x: rover.x + 6, y: rover.y + 3))
                chevron.closeSubpath()
                context.fill(chevron, with: .color(hudCyan))
            }
        }
    }

    private func drawWedge(
        context: GraphicsContext,
        rover: CGPoint,
        radius: CGFloat,
        start: Double,
        end: Double,
        active: Bool,
        pulse: Double
    ) {
        var wedge = Path()
        wedge.move(to: rover)
        wedge.addArc(
            center: rover,
            radius: radius,
            startAngle: .degrees(start),
            endAngle: .degrees(end),
            clockwise: false
        )
        wedge.closeSubpath()
        let opacity = active ? 0.28 + pulse * 0.45 : 0.05
        context.fill(wedge, with: .color(Color.red.opacity(opacity)))
        if active {
            context.stroke(wedge, with: .color(Color.red.opacity(0.5 + pulse * 0.5)), lineWidth: 1)
        }
    }
}

private struct PitchLadder: View {
    var angle: Double

    var body: some View {
        GeometryReader { geo in
            let height = geo.size.height
            let clamped = min(max(angle, 0), 140)
            let markerY = height * (1 - clamped / 140)
            let levelY = height * (1 - 90 / 140)

            ZStack(alignment: .leading) {
                Rectangle()
                    .fill(Color.white.opacity(0.18))
                    .frame(width: 1)
                    .frame(maxHeight: .infinity)

                ForEach(0..<7, id: \.self) { tick in
                    let fraction = CGFloat(tick) / 6
                    let y = height * (1 - fraction)
                    Rectangle()
                        .fill(Color.white.opacity(0.28))
                        .frame(width: tick == 0 || tick == 6 ? 12 : 8, height: 1)
                        .position(x: 8, y: y)
                }

                Rectangle()
                    .fill(hudCyan.opacity(0.8))
                    .frame(width: 16, height: 1)
                    .position(x: 10, y: levelY)

                Image(systemName: "play.fill")
                    .font(.system(size: 7, weight: .bold))
                    .foregroundStyle(hudCyan)
                    .rotationEffect(.degrees(180))
                    .position(x: 20, y: markerY)
            }
        }
        .frame(width: 26, height: 96)
    }
}
