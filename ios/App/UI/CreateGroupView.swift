import SwiftUI

struct CreateGroupView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var selected: Set<Int64> = []

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                VStack(spacing: 12) {
                    DiscordTextField(placeholder: "群聊名称", text: $name)
                    HStack {
                        Text("选择成员 (\(selected.count))")
                            .discordHeader()
                        Spacer()
                    }
                }
                .padding(16)

                if core.buddies.isEmpty {
                    Spacer()
                    Text("请先添加好友")
                        .font(.system(size: 14))
                        .foregroundColor(DTheme.textMuted)
                    Spacer()
                } else {
                    List {
                        ForEach(core.buddies, id: \.user.id) { buddy in
                            let isOn = Binding(
                                get: { selected.contains(buddy.user.id) },
                                set: { on in
                                    if on { selected.insert(buddy.user.id) }
                                    else { selected.remove(buddy.user.id) }
                                }
                            )
                            Button {
                                isOn.wrappedValue.toggle()
                            } label: {
                                HStack(spacing: 12) {
                                    DiscordAvatar(name: buddy.displayName, size: 36)
                                    Text(buddy.displayName)
                                        .font(.system(size: 15))
                                        .foregroundColor(DTheme.textMain)
                                    Spacer()
                                    Image(systemName: isOn.wrappedValue ? "checkmark.circle.fill" : "circle")
                                        .foregroundColor(isOn.wrappedValue ? DTheme.accent : DTheme.textMuted)
                                        .font(.system(size: 20))
                                }
                            }
                            .listRowBackground(DTheme.bg2)
                            .listRowSeparatorTint(DTheme.bg1)
                        }
                    }
                    .listStyle(.plain)
                    .discordList(DTheme.bg1)
                }

                DiscordButton(title: "创建群聊",
                              disabled: name.trimmingCharacters(in: .whitespaces).isEmpty || selected.isEmpty) {
                    let groupName = name.trimmingCharacters(in: .whitespaces)
                    guard !groupName.isEmpty else { return }
                    core.createGroup(name: groupName, memberIds: Array(selected))
                    dismiss()
                }
                .padding(16)
            }
            .navigationTitle("创建群聊")
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
