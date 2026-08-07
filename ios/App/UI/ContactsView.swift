import SwiftUI

struct ContactsView: View {
    @ObservedObject var core: IMCore
    @State private var showAddFriend = false
    @State private var showCreateGroup = false
    @State private var renameGroup: ImGroup?

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Button {
                        showAddFriend = true
                    } label: {
                        Label("添加好友", systemImage: "person.badge.plus")
                            .foregroundColor(.blue)
                    }
                }

                Section("我的好友 (\(core.buddies.count))") {
                    if core.buddies.isEmpty {
                        Text("还没有好友，点击上方添加")
                            .font(.footnote)
                            .foregroundColor(.secondary)
                    }
                    ForEach(core.buddies, id: \.user.id) { buddy in
                        NavigationLink(value: buddy) {
                            ContactRow(buddy: buddy)
                        }
                        .swipeActions {
                            Button(role: .destructive) {
                                core.deleteFriend(friendId: buddy.user.id)
                            } label: {
                                Label("删除", systemImage: "trash")
                            }
                        }
                    }
                }

                Section("我的群组 (\(core.groups.count))") {
                    if core.groups.isEmpty {
                        Text("还没有群聊")
                            .font(.footnote)
                            .foregroundColor(.secondary)
                    }
                    ForEach(core.groups, id: \.id) { group in
                        NavigationLink(value: group) {
                            HStack(spacing: 12) {
                                ZStack {
                                    Circle().fill(Color.purple.opacity(0.85)).frame(width: 44, height: 44)
                                    Text(String(group.name.prefix(1))).font(.headline).foregroundColor(.white)
                                }
                                Text(group.name)
                            }
                            .padding(.vertical, 2)
                        }
                        .swipeActions {
                            Button {
                                renameGroup = group
                            } label: {
                                Label("改名", systemImage: "pencil")
                            }
                            .tint(.orange)
                            Button(role: .destructive) {
                                core.leaveGroup(groupId: group.id)
                            } label: {
                                Label("退群", systemImage: "rectangle.portrait.and.arrow.right")
                            }
                        }
                    }
                }

                Section {
                    Button {
                        showCreateGroup = true
                    } label: {
                        Label("创建群聊", systemImage: "person.3")
                            .foregroundColor(.blue)
                    }
                }
            }
            .navigationTitle("好友")
            .sheet(isPresented: $showAddFriend) { AddFriendView(core: core) }
            .sheet(isPresented: $showCreateGroup) { CreateGroupView(core: core) }
            .sheet(item: $renameGroup) { group in
                RenameGroupView(core: core, group: group)
            }
            .navigationDestination(for: ImBuddy.self) { buddy in
                let session = ImSession(targetId: buddy.user.id,
                                        targetType: 0,
                                        title: buddy.displayName,
                                        avatar: "",
                                        lastContent: "",
                                        lastTime: 0,
                                        unread: 0)
                ChatView(core: core, session: session)
            }
            .navigationDestination(for: ImGroup.self) { group in
                let session = ImSession(targetId: group.id,
                                        targetType: 1,
                                        title: group.name,
                                        avatar: "",
                                        lastContent: "",
                                        lastTime: 0,
                                        unread: 0)
                ChatView(core: core, session: session)
            }
        }
    }
}

struct ContactRow: View {
    let buddy: ImBuddy

    var body: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle()
                    .fill(buddy.isOnline ? Color.green.opacity(0.85) : Color.gray.opacity(0.6))
                    .frame(width: 40, height: 40)
                Text(String(buddy.displayName.prefix(1)))
                    .font(.subheadline)
                    .foregroundColor(.white)
            }
            Text(buddy.displayName)
                .foregroundColor(buddy.isOnline ? .primary : .secondary)
        }
        .padding(.vertical, 2)
    }
}
