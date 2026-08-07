import SwiftUI

struct MainView: View {
    @ObservedObject var core: IMCore
    @State private var tab = 0

    var body: some View {
        TabView(selection: $tab) {
            MessagesView(core: core)
                .tabItem { Label("消息", systemImage: "bubble.left.and.bubble.right") }
                .tag(0)
            ContactsView(core: core)
                .tabItem { Label("好友", systemImage: "person.2") }
                .tag(1)
        }
        .toolbarBackground(DTheme.bg1, for: .tabBar)
        .toolbarBackground(.visible, for: .tabBar)
        .tint(DTheme.accent)
    }
}
