import SwiftUI

struct MessagesView: View {
    @ObservedObject var core: IMCore
    @State private var showSearch = false

    var body: some View {
        NavigationStack {
            Group {
                if core.sessions.isEmpty {
                    ContentUnavailableView("暂无会话", systemImage: "tray", description: Text("添加好友或开始聊天后会出现在这里"))
                } else {
                    List(core.sessions, id: \.targetId) { session in
                        NavigationLink(value: session) {
                            SessionRow(session: session)
                        }
                    }
                    .listStyle(.plain)
                }
            }
            .navigationTitle("消息")
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button(role: .destructive) { core.logout() } label: { Text("退出") }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button { showSearch = true } label: { Image(systemName: "magnifyingglass") }
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
    let session: ImSession

    var body: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle()
                    .fill(Color.blue.opacity(0.85))
                    .frame(width: 44, height: 44)
                Text(String(session.title.prefix(1)))
                    .font(.headline)
                    .foregroundColor(.white)
            }
            VStack(alignment: .leading, spacing: 3) {
                Text(session.title)
                    .font(.body)
                    .lineLimit(1)
                Text(session.lastContent.isEmpty ? " " : session.lastContent)
                    .font(.footnote)
                    .foregroundColor(.secondary)
                    .lineLimit(1)
            }
            Spacer()
            if session.unread > 0 {
                Text(session.unread > 99 ? "99+" : "\(session.unread)")
                    .font(.caption2)
                    .foregroundColor(.white)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Capsule().fill(Color.red))
            }
        }
        .padding(.vertical, 2)
    }
}
