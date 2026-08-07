import SwiftUI

struct AddFriendView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var username = ""

    var body: some View {
        NavigationStack {
            ZStack {
                DTheme.bg1.ignoresSafeArea()
                VStack(spacing: 16) {
                    DiscordTextField(placeholder: "输入对方用户名", text: $username)
                    DiscordButton(title: "添加好友",
                                  disabled: username.trimmingCharacters(in: .whitespaces).isEmpty) {
                        let name = username.trimmingCharacters(in: .whitespaces)
                        guard !name.isEmpty else { return }
                        core.addFriend(username: name)
                        dismiss()
                    }
                    if !core.status.isEmpty {
                        Text(core.status)
                            .font(.system(size: 13))
                            .foregroundColor(DTheme.textSub)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    Spacer()
                }
                .padding(24)
            }
            .navigationTitle("添加好友")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                        .foregroundColor(DTheme.textSub)
                }
            }
        }
        .preferredColorScheme(.dark)
    }
}
