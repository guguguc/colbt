import SwiftUI
import PhotosUI

struct ProfileEditView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var nickname = ""
    @State private var oldPassword = ""
    @State private var newPassword = ""
    @State private var avatarPath = ""
    @State private var imageItem: PhotosPickerItem?

    var body: some View {
        NavigationStack {
            ZStack {
                DTheme.bg1.ignoresSafeArea()
                VStack(spacing: 16) {
                    // 当前头像预览
                    DiscordAvatar(name: nickname.isEmpty ? (core.me?.displayName ?? "?") : nickname,
                                  size: 72)

                    PhotosPicker(selection: $imageItem, matching: .images) {
                        HStack(spacing: 6) {
                            Image(systemName: "photo")
                            Text(avatarPath.isEmpty ? "选择新头像" : "已选择头像，点击更换")
                                .font(.system(size: 14))
                        }
                        .foregroundColor(DTheme.textSub)
                    }

                    DiscordTextField(placeholder: "昵称（留空不修改）", text: $nickname)
                    DiscordTextField(placeholder: "旧密码（改密码时需要）", text: $oldPassword, isSecure: true)
                    DiscordTextField(placeholder: "新密码（留空不修改）", text: $newPassword, isSecure: true)

                    if !core.status.isEmpty {
                        Text(core.status)
                            .font(.system(size: 13))
                            .foregroundColor(DTheme.textSub)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }

                    DiscordButton(title: "保存资料") {
                        if !newPassword.isEmpty && newPassword.count < 4 {
                            core.status = "新密码至少4位"
                            return
                        }
                        core.updateProfile(nickname: nickname.trimmingCharacters(in: .whitespaces),
                                           avatarPath: avatarPath,
                                           oldPassword: oldPassword,
                                           newPassword: newPassword)
                        dismiss()
                    }
                    .disabled(nickname.trimmingCharacters(in: .whitespaces).isEmpty
                              && avatarPath.isEmpty && newPassword.isEmpty)
                    Spacer()
                }
                .padding(24)
            }
            .navigationTitle("个人资料")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("关闭") { dismiss() }
                        .foregroundColor(DTheme.textSub)
                }
            }
        }
        .onChange(of: imageItem) { item in
            guard let item else { return }
            Task {
                if let data = try? await item.loadTransferable(type: Data.self) {
                    let url = FileManager.default.temporaryDirectory
                        .appendingPathComponent("avatar_\(Date().timeIntervalSince1970).img")
                    do {
                        try data.write(to: url)
                        avatarPath = url.path
                    } catch {}
                }
                imageItem = nil
            }
        }
    }
}
