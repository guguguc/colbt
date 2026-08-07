import SwiftUI

struct MessagesView: View {
    @ObservedObject var core: IMCore
    @State private var showSearch = false

    var body: some View {
        NavigationStack {
            Group {
                if core.sessions.isEmpty {
                    ContentUnavailableView("暂无会话", systemImage: "tray",
                                            description: Text("添加好友或开始聊天后会出现在这里"))
                        .foregroundStyle(DTheme.textMuted)
                } else {
                    List(core.sessions, id: \.targetId) { session in
                        NavigationLink(value: session) {
                            SessionRow(core: core, session: session)
                        }
                        .listRowBackground(DTheme.bg2)
                        .listRowSeparator(.hidden)
                    }
                    .listStyle(.plain)
                    .discordList(DTheme.bg1)
                }
            }
            .navigationTitle("消息")
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button(role: .destructive) { core.logout() } label: {
                        Image(systemName: "rectangle.portrait.and.arrow.right")
                            .foregroundColor(DTheme.textSub)
                    }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button { showSearch = true } label: {
                        Image(systemName: "magnifyingglass")
                            .foregroundColor(DTheme.textSub)
                    }
                }
            }
            .sheet(isPresented: $showSearch) { SearchView(core: core) }
            .navigationDestination(for: ImSession.self) { session in
                ChatView(core: core, session: session)
            }
        }
    }
}

struct SessionRow: View {
    @ObservedObject var core: IMCore
    let session: ImSession

    var body: some View {
        HStack(spacing: 12) {
            DiscordAvatar(name: session.title, size: 44,
                          uiImage: core.avatarImage(fileId: session.avatar))
            VStack(alignment: .leading, spacing: 3) {
                Text(session.title)
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundColor(DTheme.textMain)
                    .lineLimit(1)
                Text(session.lastContent.isEmpty ? " " : session.lastContent)
                    .font(.system(size: 13))
                    .foregroundColor(DTheme.textMuted)
                    .lineLimit(1)
            }
            Spacer()
            VStack(alignment: .trailing, spacing: 4) {
                if session.unread > 0 {
                    Text(session.unread > 99 ? "99+" : "\(session.unread)")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundColor(.white)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 2)
                        .background(Capsule().fill(DTheme.danger))
                } else {
                    Text(timeString(session.lastTime))
                        .font(.system(size: 11))
                        .foregroundColor(DTheme.textMuted)
                }
            }
        }
        .padding(.vertical, 4)
    }

    private func timeString(_ ts: Int64) -> String {
        guard ts > 0 else { return "" }
        let f = DateFormatter()
        f.dateFormat = "HH:mm"
        return f.string(from: Date(timeIntervalSince1970: TimeInterval(ts)))
    }
}
