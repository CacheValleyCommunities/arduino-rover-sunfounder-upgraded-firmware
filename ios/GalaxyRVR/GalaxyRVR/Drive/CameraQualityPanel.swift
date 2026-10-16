import SwiftUI

struct CameraQualityPanel: View {
    var quality: Int
    var framesize: CameraFramesize
    var busy: Bool
    var error: String?
    var onQuality: (Int) -> Void
    var onFramesize: (CameraFramesize) -> Void

    @State private var sliderQuality: Double = Double(CameraStreamSettings.qualityDefault)

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("CAMERA")
                    .font(.system(size: 10, weight: .bold, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.7))
                Spacer()
                if busy {
                    ProgressView().tint(.white).scaleEffect(0.7)
                }
            }

            Picker("Size", selection: framesizeBinding) {
                ForEach(CameraFramesize.allCases) { size in
                    Text(size.label).tag(size)
                }
            }
            .pickerStyle(.segmented)
            .disabled(busy)

            Text(framesize.detail)
                .font(.system(size: 10, weight: .medium, design: .monospaced))
                .foregroundStyle(.white.opacity(0.5))

            HStack {
                Text("SHARPER")
                    .font(.system(size: 9, weight: .bold, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.45))
                Spacer()
                Text("FASTER")
                    .font(.system(size: 9, weight: .bold, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.45))
            }

            Slider(
                value: $sliderQuality,
                in: Double(CameraStreamSettings.qualityMin)...Double(CameraStreamSettings.qualityMax),
                step: 1
            ) { editing in
                if !editing {
                    onQuality(Int(sliderQuality.rounded()))
                }
            }
            .tint(.white)
            .disabled(busy)

            if let error, error.isEmpty == false {
                Text(error)
                    .font(.system(size: 10, weight: .medium, design: .monospaced))
                    .foregroundStyle(.orange)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(12)
        .frame(width: 260)
        .background(Color.black.opacity(0.58))
        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(Color.white.opacity(0.28), lineWidth: 1)
        )
        .colorScheme(.dark)
        .onAppear {
            sliderQuality = Double(quality)
        }
        .onChange(of: quality) { _, newValue in
            sliderQuality = Double(newValue)
        }
    }

    private var framesizeBinding: Binding<CameraFramesize> {
        Binding(
            get: { framesize },
            set: { onFramesize($0) }
        )
    }
}
