import SwiftUI

@main
struct ColbtApp: App {
    var body: some Scene {
        WindowGroup {
            RootView()
        }
    }
}

struct RootView: View {
    @StateObject private var core = IMCore()

    var body: some View {
        if core.loggedIn {
            MainView(core: core)
        } else {
            LoginView(core: core)
        }
    }
}
