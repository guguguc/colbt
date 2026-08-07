import SwiftUI

struct AddFriendView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var username = ""

    var body: some View {
        NavigationStack {
            Form {
                Section("输入对方用户名") {
                    TextField("用户名", text: $username)
                        .autocapitalization(.none)
                        .autocorrectionDisabled()
                }
                if !core.status.isEmpty {
                    Section {
                        Text(core.status)
                            .font(.footnote)
                            .foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("添加好友")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("添加") {
                        let name = username.trimmingCharacters(in: .whitespaces)
                        guard !name.isEmpty else { return }
                        core.addFriend(username: name)
                    }
                    .disabled(username.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
    }
}
