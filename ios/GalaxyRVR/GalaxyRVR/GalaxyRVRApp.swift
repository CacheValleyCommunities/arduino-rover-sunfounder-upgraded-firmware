import SwiftUI

@main
struct GalaxyRVRApp: App {
    var body: some Scene {
        WindowGroup {
            ScanView()
                .preferredColorScheme(.dark)
                .statusBarHidden(true)
        }
    }
}
