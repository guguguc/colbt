import SwiftUI

struct ContactsView: View {
    @ObservedObject var core: IMCore
    @State private var showAddFriend = false
    @State private var showCreateGroup = false
    @State private var showProfile = false
    @State private var renameGroup: ImGroup?

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Button { showAddFriend = true } label: {
                        Label("添加好友", systemImage: "person.badge.plus")
                            .foregroundColor(DTheme.textMain)
                    }
                    Button { showCreateGroup = true } label: {
                        Label("创建群聊", systemImage: "person.3")
                            .foregroundColor(DTheme.textMain)
                    }
                }
                .listRowBackground(DTheme.bg2)

                Section {
                    if core.buddies.isEmpty {
                        Text("还没有好友")
                            .font(.system(size: 13))
                            .foregroundColor(DTheme.textMuted)
                    }
                    ForEach(core.buddies, id: \.user.id) { buddy in
                        NavigationLink(value: buddy) {
                            ContactRow(buddy: buddy)
                        }
                        .listRowBackground(DTheme.bg2)
                        .swipeActions {
                            Button(role: .destructive) {
                                core.deleteFriend(friendId: buddy.user.id)
                            } label: {
                                Label("删除", systemImage: "trash")
                            }
                        }
                    }
                } header: {
                    Text("我的好友 (\(core.buddies.count))").discordHeader()
                }

                Section {
                    if core.groups.isEmpty {
                        Text("还没有群聊")
                            .font(.system(size: 13))
                            .foregroundColor(DTheme.textMuted)
                    }
                    ForEach(core.groups, id: \.id) { group in
                        NavigationLink(value: group) {
                            HStack(spacing: 12) {
                                DiscordAvatar(name: group.name, size: 40)
                                Text(group.name)
                                    .foregroundColor(DTheme.textMain)
                            }
                        }
                        .listRowBackground(DTheme.bg2)
                        .swipeActions {
                            Button { renameGroup = group } label: {
                                Label("改名", systemImage: "pencil")
                            }
                            .tint(DTheme.warn)
                            Button(role: .destructive) {
                                core.leaveGroup(groupId: group.id)
                            } label: {
                                Label("退群", systemImage: "rectangle.portrait.and.arrow.right")
                            }
                        }
                    }
                } header: {
                    Text("我的群组 (\(core.groups.count))").discordHeader()
                }
            }
            .listStyle(.plain)
            .discordList(DTheme.bg1)
            .navigationTitle("好友")
            .toolbarBackground(DTheme.bg2, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button { showProfile = true } label: {
                        Image(systemName: "person.crop.circle.badge.checkmark")
                            .foregroundColor(DTheme.textSub)
                    }
                }
            }
            .sheet(isPresented: $showProfile) { ProfileEditView(core: core) }
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
            DiscordAvatar(name: buddy.displayName, size: 40, online: buddy.isOnline)
            Text(buddy.displayName)
                .font(.system(size: 15))
                .foregroundColor(buddy.isOnline ? DTheme.textMain : DTheme.textMuted)
        }
        .padding(.vertical, 3)
    }
}
