import SwiftUI

struct LookPad: View {
    var onLookRate: (Double) -> Void

    @State private var knobY: CGFloat = 0
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
                Capsule()
                    .fill(Color.white.opacity(0.08))
                    .frame(width: 6, height: radius * 1.35)
                Circle()
                    .fill(Color.white.opacity(0.88))
                    .frame(width: knobSize, height: knobSize)
                    .offset(y: knobY)
                    .shadow(color: .black.opacity(0.35), radius: 3, y: 1)
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .contentShape(Circle())
            .gesture(drag(center: center))
        }
        .frame(width: radius * 2 + 8, height: radius * 2 + 8)
    }

    private func drag(center: CGPoint) -> some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { value in
                let dy = value.location.y - center.y
                knobY = min(max(dy, -radius), radius)
                // Screen Y grows downward; dragging up should look up.
                onLookRate(Double(knobY / radius))
            }
            .onEnded { _ in
                onLookRate(0)
                withAnimation(.easeOut(duration: 0.12)) {
                    knobY = 0
                }
            }
    }
}
