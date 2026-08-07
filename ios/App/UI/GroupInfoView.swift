import SwiftUI

struct GroupInfoView: View {
    @ObservedObject var core: IMCore
    let groupId: Int64
    let groupTitle: String

    @Environment(\.dismiss) private var dismiss
    @State private var showRename = false
    @State private var confirmDismiss = false

    private var group: ImGroup? {
        core.groups.first { $0.id == groupId }
    }
    private var members: [ImMember] {
        core.groupMembersGroupId == groupId ? core.groupMembers : []
    }
    private var isOwner: Bool {
        group?.ownerId == core.me?.id
    }

    var body: some View {
        NavigationStack {
            List {
                Section {
                    ForEach(members, id: \.user.id) { member in
                        HStack(spacing: 12) {
                            DiscordAvatar(name: member.displayName, size: 36,
                                          uiImage: core.avatarImage(fileId: member.user.avatar))
                            Text(member.displayName)
                                .font(.system(size: 15))
                                .foregroundColor(DTheme.textMain)
                            if member.user.id == core.me?.id {
                                Text("我")
                                    .font(.system(size: 12))
                                    .foregroundColor(DTheme.textMuted)
                            }
                            if isOwner && member.user.id != core.me?.id {
                                Spacer()
                                Button {
                                    core.kickMember(groupId: groupId, memberId: member.user.id)
                                } label: {
                                    Image(systemName: "person.crop.circle.badge.xmark")
                                        .foregroundColor(DTheme.danger)
                                }
                            }
                        }
                        .listRowBackground(DTheme.bg2)
                    }
                } header: {
                    Text("成员 (\(members.count))").discordHeader()
                }

                Section {
                    Button { showRename = true } label: {
                        Label("重命名群聊", systemImage: "pencil")
                            .foregroundColor(DTheme.textMain)
                    }
                }
                .listRowBackground(DTheme.bg2)

                Section {
                    Button(role: .destructive) {
                        core.leaveGroup(groupId: groupId)
                        dismiss()
                    } label: {
                        Label("退出群聊", systemImage: "rectangle.portrait.and.arrow.right")
                    }
                    if isOwner {
                        Button(role: .destructive) {
                            confirmDismiss = true
                        } label: {
                            Label("解散群聊", systemImage: "trash")
                        }
                    }
                }
                .listRowBackground(DTheme.bg2)
            }
            .listStyle(.plain)
            .discordList(DTheme.bg1)
            .navigationTitle(groupTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") { dismiss() }
                        .foregroundColor(DTheme.textSub)
                }
            }
            .sheet(isPresented: $showRename) {
                RenameGroupView(core: core, groupId: groupId, currentName: groupTitle)
            }
            .confirmationDialog("确定解散该群聊？", isPresented: $confirmDismiss, titleVisibility: .visible) {
                Button("解散", role: .destructive) {
                    core.dismissGroup(groupId: groupId)
                    dismiss()
                }
                Button("取消", role: .cancel) {}
            }
        }
        .preferredColorScheme(.dark)
    }
}

struct RenameGroupView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    let groupId: Int64
    let currentName: String
    @State private var name: String

    init(core: IMCore, groupId: Int64, currentName: String) {
        self.core = core
        self.groupId = groupId
        self.currentName = currentName
        _name = State(initialValue: currentName)
    }

    init(core: IMCore, group: ImGroup) {
        self.init(core: core, groupId: group.id, currentName: group.name)
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                DiscordTextField(placeholder: "群聊名称", text: $name)
                DiscordButton(title: "保存",
                              disabled: name.trimmingCharacters(in: .whitespaces).isEmpty) {
                    let trimmed = name.trimmingCharacters(in: .whitespaces)
                    guard !trimmed.isEmpty else { return }
                    core.renameGroup(groupId: groupId, name: trimmed)
                    dismiss()
                }
                Spacer()
            }
            .padding(24)
            .background(DTheme.bg1.ignoresSafeArea())
            .navigationTitle("重命名群聊")
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
