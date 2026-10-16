import SwiftUI

struct VirtualJoystick: View {
    var enabled: Bool = true
    var onChange: (Double, Double) -> Void
    var onEnd: () -> Void

    @State private var knobOffset: CGSize = .zero
    private let radius: CGFloat = 56
    private let knobSize: CGFloat = 40

    var body: some View {
        GeometryReader { geo in
            let center = CGPoint(x: geo.size.width / 2, y: geo.size.height / 2)
            ZStack {
                Circle()
                    .fill(Color.black.opacity(0.32))
                    .overlay(Circle().stroke(Color.white.opacity(0.4), lineWidth: 1.5))
                    .frame(width: radius * 2, height: radius * 2)
                Circle()
                    .stroke(Color.white.opacity(0.12), lineWidth: 1)
                    .frame(width: radius, height: radius)
                Circle()
                    .fill(Color.white.opacity(0.88))
                    .frame(width: knobSize, height: knobSize)
                    .offset(knobOffset)
                    .shadow(color: .black.opacity(0.35), radius: 3, y: 1)
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .contentShape(Circle())
            .gesture(drag(center: center))
        }
        .frame(width: radius * 2 + 8, height: radius * 2 + 8)
        .opacity(enabled ? 1 : 0.32)
        .allowsHitTesting(enabled)
    }

    private func drag(center: CGPoint) -> some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { value in
                let dx = value.location.x - center.x
                let dy = value.location.y - center.y
                let distance = hypot(dx, dy)
                let clamped = min(distance, radius)
                let angle = atan2(dy, dx)
                knobOffset = CGSize(
                    width: cos(angle) * clamped,
                    height: sin(angle) * clamped
                )
                onChange(
                    Double(knobOffset.width / radius),
                    Double(-knobOffset.height / radius)
                )
            }
            .onEnded { _ in
                withAnimation(.easeOut(duration: 0.12)) {
                    knobOffset = .zero
                }
                onEnd()
            }
    }
}
