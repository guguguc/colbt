import Foundation
import Combine
import UIKit

/// 已下载的文件（含图片/普通文件），用于 UI 展示与保存
struct DownloadedFile {
    let fileId: String
    let name: String
    let size: Int64
    let mime: String
    let data: Data
}

/// 线程安全的 Swift 封装：C++ 回调由 ObjC 桥接层统一转发到主队列，UI 可直接使用。
final class IMCore: NSObject, ObservableObject {

    @Published var connected = false
    @Published var loggedIn = false
    @Published var me: ImUser?
    @Published var sessions: [ImSession] = []
    @Published var buddies: [ImBuddy] = []
    @Published var groups: [ImGroup] = []
    @Published var messages: [ImMessage] = []
    @Published var status = ""
    @Published var typing = false
    @Published var searchResults: [ImMessage] = []
    @Published var groupMembers: [ImMember] = []
    @Published var groupMembersGroupId: Int64 = 0

    private(set) var activeTargetId: Int64 = 0
    private(set) var activeTargetType = 0

    /// fileId -> 已下载的图片数据（图片消息在加载历史/收到推送时自动下载）
    private(set) var imageCache: [String: Data] = [:]

    /// fileId -> 已下载的文件（手动下载或图片自动下载）
    @Published private(set) var downloadedFiles: [String: DownloadedFile] = [:]

    private var tempFileURLs: [String: URL] = [:]

    private var bridge: ImClientBridge?
    private var typingTask: Task<Void, Never>?

    override init() {
        super.init()
    }

    // MARK: - 连接与账号

    func start(host: String, port: Int) {
        bridge?.stop()
        let b = ImClientBridge(listener: self)
        bridge = b
        _ = b.start(host, port: Int32(port))
    }

    func stop() {
        bridge?.stop()
    }

    func login(username: String, password: String) {
        bridge?.login(username, password: password)
    }

    func register(username: String, password: String, nickname: String) {
        bridge?.registerUser(username, password: password, nickname: nickname)
    }

    func logout() {
        bridge?.logout()
        loggedIn = false
        sessions = []
        buddies = []
        groups = []
        messages = []
        me = nil
    }

    // MARK: - 数据加载

    func loadContacts() { bridge?.loadContacts() }
    func loadSessions() { bridge?.loadSessions() }

    func openSession(targetId: Int64, targetType: Int) {
        activeTargetId = targetId
        activeTargetType = targetType
        messages = []
        bridge?.loadHistory(targetId, targetType: Int32(targetType), limit: 50)
        bridge?.markRead(targetId, targetType: Int32(targetType))
    }

    func closeSession() {
        activeTargetId = 0
        activeTargetType = 0
        messages = []
        typing = false
    }

    // MARK: - 消息

    func sendText(targetId: Int64, targetType: Int, text: String) {
        bridge?.sendText(targetId, targetType: Int32(targetType), text: text)
    }

    func sendReply(targetId: Int64, targetType: Int, replyToId: Int64, text: String) {
        bridge?.sendReply(targetId, targetType: Int32(targetType), replyToId: replyToId, text: text)
    }

    func recall(messageId: Int64, targetId: Int64, targetType: Int) {
        bridge?.recallMessage(messageId, targetId: targetId, targetType: Int32(targetType))
    }

    func sendTyping(targetId: Int64, targetType: Int) {
        bridge?.sendTyping(targetId, targetType: Int32(targetType))
    }

    func sendImage(targetId: Int64, targetType: Int, path: String) {
        bridge?.sendImage(targetId, targetType: Int32(targetType), path: path)
    }

    func sendFile(targetId: Int64, targetType: Int, path: String) {
        bridge?.sendFile(targetId, targetType: Int32(targetType), path: path)
    }

    func searchMessages(keyword: String) {
        bridge?.searchMessages(keyword)
    }

    /// 下载文件（收到文件消息后调用；图片消息会自动下载）
    func downloadFile(fileId: String) {
        guard !fileId.isEmpty else { return }
        bridge?.downloadFile(fileId)
    }

    /// 按 fileId 取已下载的头像/图片（用于头像渲染）
    func avatarImage(fileId: String?) -> UIImage? {
        guard let fileId, let f = downloadedFiles[fileId] else { return nil }
        return UIImage(data: f.data)
    }

    /// 把已下载文件写入临时目录，供 ShareLink / 系统分享保存到"文件"
    func tempFileURL(for file: DownloadedFile) -> URL? {
        if let cached = tempFileURLs[file.fileId] { return cached }
        let dir = FileManager.default.temporaryDirectory
            .appendingPathComponent("colbt_files", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let safeName = file.name.isEmpty ? file.fileId : file.name
        let url = dir.appendingPathComponent(safeName)
        do {
            try file.data.write(to: url)
            tempFileURLs[file.fileId] = url
            return url
        } catch {
            return nil
        }
    }

    // MARK: - 好友 / 群

    func addFriend(username: String) { bridge?.addFriend(username) }
    func deleteFriend(friendId: Int64) { bridge?.deleteFriend(friendId) }
    func createGroup(name: String, memberIds: [Int64]) {
        bridge?.createGroup(name, memberIds: memberIds.map { NSNumber(value: $0) })
    }
    func loadGroupMembers(groupId: Int64) {
        bridge?.loadGroupMembers(groupId)
    }
    func renameGroup(groupId: Int64, name: String) {
        bridge?.renameGroup(groupId, name: name)
    }
    func kickMember(groupId: Int64, memberId: Int64) {
        bridge?.kickMember(groupId, memberId: memberId)
    }
    func leaveGroup(groupId: Int64) { bridge?.leaveGroup(groupId) }
    func dismissGroup(groupId: Int64) { bridge?.dismissGroup(groupId) }

    // MARK: - 资料

    /// 修改昵称/头像/密码；avatarPath 为空则不改头像；newPassword 为空则不改密码
    func updateProfile(nickname: String, avatarPath: String,
                       oldPassword: String, newPassword: String) {
        bridge?.updateProfile(nickname, avatarPath: avatarPath,
                              oldPassword: oldPassword, newPassword: newPassword)
    }

    // MARK: - 工具

    /// 保存下载的图片数据到临时目录，返回本地 URL
    func saveImageData(_ data: Data, fileId: String) -> URL? {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("colbt_images", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let url = dir.appendingPathComponent("\(fileId).img")
        do {
            try data.write(to: url)
            return url
        } catch {
            return nil
        }
    }
}

// MARK: - ObjC 桥接回调（主队列）

extension IMCore: ImClientListener {

    func onConnectionChanged(_ connected: Bool) {
        self.connected = connected
        if !connected && !loggedIn {
            status = "无法连接服务器"
        }
    }

    func onLoginResult(_ code: Int32, message: String, me: ImUser) {
        if code == 0 {
            loggedIn = true
            self.me = me
            status = ""
            loadSessions()
            loadContacts()
        } else {
            status = message
        }
    }

    func onRegisterResult(_ code: Int32, message: String) {
        status = message
    }

    func onContactsLoaded(_ buddies: [ImBuddy], groups: [ImGroup]) {
        self.buddies = buddies
        self.groups = groups
    }

    func onSessionsLoaded(_ sessions: [ImSession]) {
        self.sessions = sessions
    }

    func onHistoryLoaded(_ targetId: Int64, targetType: Int32, messages: [ImMessage]) {
        guard activeTargetId == targetId && activeTargetType == Int(targetType) else { return }
        self.messages = messages
    }

    func onMessage(_ message: ImMessage) {
        if Int(message.targetType) == activeTargetType && message.targetId == activeTargetId {
            messages.append(message)
        } else {
            loadSessions()
        }
        if message.msgType == 1, let fileId = message.fileId {
            // 图片已自动下载，刷新气泡
            refreshImages()
        }
    }

    func onMessageSent(_ message: ImMessage) {
        if message.targetId == activeTargetId {
            messages.append(message)
        }
        loadSessions()
        if message.msgType == 1, let fileId = message.fileId {
            refreshImages()
        }
    }

    func onMessageRecalled(_ messageId: Int64, targetId: Int64, targetType: Int32) {
        if targetId == activeTargetId {
            messages.removeAll { $0.id == messageId }
        }
        loadSessions()
    }

    func onFriendDeleted(_ friendId: Int64, name: String) {
        loadContacts()
        loadSessions()
    }

    func onGroupUpdated(_ groupId: Int64) {
        loadContacts()
        loadSessions()
    }

    func onSearchResults(_ messages: [ImMessage]) {
        searchResults = messages
    }

    func onTyping(_ fromId: Int64, targetId: Int64, targetType: Int32) {
        let type = Int(targetType)
        let matches = activeTargetType == type &&
            (type == 1 ? activeTargetId == targetId : activeTargetId == fromId)
        guard matches else { return }
        typing = true
        typingTask?.cancel()
        typingTask = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            guard !Task.isCancelled else { return }
            self?.typing = false
        }
    }

    func onMessagesRead(_ peerId: Int64, targetType: Int32) {}

    func onReadReceipt(_ peerId: Int64, targetType: Int32) {
        let type = Int(targetType)
        if type == 1 {
            messages = messages.map { $0.read = 1; return $0 }
        } else if activeTargetId == peerId {
            messages = messages.map { $0.read = 1; return $0 }
        }
    }

    func onFileDownloaded(_ fileId: String, name: String, size: Int64, mime: String, data: Data) {
        // 图片消息：存入图片缓存用于气泡展示
        if mime.hasPrefix("image/") {
            imageCache[fileId] = data
            refreshImages()
        }
        // 所有下载都记录，供"保存到文件"使用
        var d = downloadedFiles
        d[fileId] = DownloadedFile(fileId: fileId, name: name, size: size, mime: mime, data: data)
        downloadedFiles = d
    }

    func onPresenceChanged(_ userId: Int64, online: Bool) {
        buddies = buddies.map { buddy in
            guard buddy.user.id == userId else { return buddy }
            buddy.user.online = online ? 1 : 0
            return buddy
        }
    }

    func onFriendAdded(_ buddy: ImBuddy) {
        status = "添加好友成功: \(buddy.displayName)"
        loadContacts()
    }

    func onGroupCreated(_ group: ImGroup) {
        loadContacts()
        loadSessions()
    }

    func onGroupMembersLoaded(_ groupId: Int64, members: [ImMember]) {
        groupMembers = members
        groupMembersGroupId = groupId
    }

    func onError(_ code: Int32, message: String) {
        status = message
    }

    func onProfileUpdated(_ code: Int32, message: String, me: ImUser) {
        if code == 0 {
            self.me = me
            status = "资料已更新"
        } else {
            status = message
        }
    }

    func onProfileChanged(_ userId: Int64, nickname: String, avatar: String) {
        loadContacts()
    }

    // MARK: 私有

    private func refreshImages() {
        messages = messages.map { $0 }
    }
}
