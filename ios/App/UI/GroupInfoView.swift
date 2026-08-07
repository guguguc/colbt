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
                Section("成员 (\(members.count))") {
                    ForEach(members, id: \.user.id) { member in
                        HStack(spacing: 12) {
                            ZStack {
                                Circle()
                                    .fill(Color.gray.opacity(0.5))
                                    .frame(width: 36, height: 36)
                                Text(String(member.displayName.prefix(1)))
                                    .font(.subheadline)
                                    .foregroundColor(.white)
                            }
                            Text(member.displayName)
                            if member.user.id == core.me?.id {
                                Text("我")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                            }
                            if isOwner && member.user.id != core.me?.id {
                                Spacer()
                                Button {
                                    core.kickMember(groupId: groupId, memberId: member.user.id)
                                } label: {
                                    Text("踢出")
                                        .font(.footnote)
                                        .foregroundColor(.red)
                                }
                            }
                        }
                    }
                }

                Section {
                    Button {
                        showRename = true
                    } label: {
                        Label("重命名群聊", systemImage: "pencil")
                    }
                }

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
            }
            .navigationTitle(groupTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") { dismiss() }
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
            Form {
                Section("群名称") {
                    TextField("名称", text: $name)
                }
            }
            .navigationTitle("重命名群聊")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("保存") {
                        let trimmed = name.trimmingCharacters(in: .whitespaces)
                        guard !trimmed.isEmpty else { return }
                        core.renameGroup(groupId: groupId, name: trimmed)
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
    }
}
