import SwiftUI

struct SearchView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var keyword = ""
    @State private var searched = false

    var body: some View {
        NavigationStack {
            List(core.searchResults, id: \.id) { message in
                VStack(alignment: .leading, spacing: 3) {
                    Text(message.content)
                        .font(.body)
                        .lineLimit(2)
                    Text("来自 \(message.senderName) · \(timeString(message.timestamp))")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            .overlay {
                if searched && core.searchResults.isEmpty {
                    ContentUnavailableView("未找到结果", systemImage: "magnifyingglass")
                }
            }
            .navigationTitle("搜索消息")
            .navigationBarTitleDisplayMode(.inline)
            .searchable(text: $keyword, placement: .navigationBarDrawer(displayMode: .always))
            .onSubmit(of: .search) {
                searched = true
                core.searchMessages(keyword: keyword.trimmingCharacters(in: .whitespaces))
            }
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("关闭") { dismiss() }
                }
            }
        }
    }

    private func timeString(_ ts: Int64) -> String {
        let f = DateFormatter()
        f.dateFormat = "MM-dd HH:mm"
        return f.string(from: Date(timeIntervalSince1970: TimeInterval(ts)))
    }
}
