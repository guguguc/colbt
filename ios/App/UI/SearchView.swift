import SwiftUI

struct SearchView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var keyword = ""
    @State private var searched = false

    var body: some View {
        NavigationStack {
            List(core.searchResults, id: \.id) { message in
                VStack(alignment: .leading, spacing: 4) {
                    Text(message.content)
                        .font(.system(size: 15))
                        .foregroundColor(DTheme.textMain)
                        .lineLimit(2)
                    HStack {
                        Text("来自 \(message.senderName)")
                        Text("·")
                        Text(timeString(message.timestamp))
                    }
                    .font(.system(size: 12))
                    .foregroundColor(DTheme.textMuted)
                }
                .listRowBackground(DTheme.bg2)
            }
            .listStyle(.plain)
            .discordList(DTheme.bg1)
            .overlay {
                if searched && core.searchResults.isEmpty {
                    ContentUnavailableView("未找到结果", systemImage: "magnifyingglass")
                        .foregroundStyle(DTheme.textMuted)
                }
            }
            .navigationTitle("搜索消息")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .searchable(text: $keyword, placement: .navigationBarDrawer(displayMode: .always))
            .onSubmit(of: .search) {
                searched = true
                core.searchMessages(keyword: keyword.trimmingCharacters(in: .whitespaces))
            }
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("关闭") { dismiss() }
                        .foregroundColor(DTheme.textSub)
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    private func timeString(_ ts: Int64) -> String {
        let f = DateFormatter()
        f.dateFormat = "MM-dd HH:mm"
        return f.string(from: Date(timeIntervalSince1970: TimeInterval(ts)))
    }
}
