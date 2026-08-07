package com.colbt.im.core

import android.os.Handler
import android.os.Looper

/**
 * 线程安全的 Kotlin 封装：
 * - 所有方法可任意线程调用（C++ 层本身线程安全）
 * - 所有回调被编组到主线程（通过 Handler.post），UI 可直接使用
 */
class ImCore(private val listener: ImListener) {

    private val main = Handler(Looper.getMainLooper())
    private var handle: Long = 0

    private val bridge = object : ImListener {
        override fun onConnectionChanged(connected: Boolean) {
        main.post { listener.onConnectionChanged(connected) }
    }
        override fun onLoginResult(code: Int, msg: String, me: ImBuddy) {
        main.post { listener.onLoginResult(code, msg, me) }
    }
        override fun onRegisterResult(code: Int, msg: String) {
        main.post { listener.onRegisterResult(code, msg) }
    }
        override fun onContactsLoaded(buddies: List<ImBuddy>, groups: List<ImGroup>) {
        main.post { listener.onContactsLoaded(buddies, groups) }
    }
        override fun onSessionsLoaded(sessions: List<ImSession>) {
        main.post { listener.onSessionsLoaded(sessions) }
    }
        override fun onHistoryLoaded(targetId: Long, targetType: Int, msgs: List<ImMessage>) {
        main.post { listener.onHistoryLoaded(targetId, targetType, msgs) }
    }
        override fun onMessage(msg: ImMessage) {
        main.post { listener.onMessage(msg) }
    }
        override fun onMessageSent(msg: ImMessage) {
        main.post { listener.onMessageSent(msg) }
    }
        override fun onMessageRecalled(msgId: Long, targetId: Long, targetType: Int) {
        main.post { listener.onMessageRecalled(msgId, targetId, targetType) }
    }
        override fun onFriendDeleted(friendId: Long, name: String) {
        main.post { listener.onFriendDeleted(friendId, name) }
    }
        override fun onGroupUpdated(groupId: Long) {
        main.post { listener.onGroupUpdated(groupId) }
    }
        override fun onSearchResults(msgs: List<ImMessage>) {
        main.post { listener.onSearchResults(msgs) }
    }
        override fun onTyping(fromId: Long, targetId: Long, targetType: Int) {
        main.post { listener.onTyping(fromId, targetId, targetType) }
    }
        override fun onMessagesRead(peerId: Long, targetType: Int) {
        main.post { listener.onMessagesRead(peerId, targetType) }
    }
        override fun onReadReceipt(peerId: Long, targetType: Int) {
        main.post { listener.onReadReceipt(peerId, targetType) }
    }
        override fun onFileDownloaded(fileId: String, name: String, size: Long, mime: String, data: ByteArray) {
        main.post { listener.onFileDownloaded(fileId, name, size, mime, data) }
    }
        override fun onPresenceChanged(userId: Long, online: Boolean) {
        main.post { listener.onPresenceChanged(userId, online) }
    }
        override fun onFriendAdded(buddy: ImBuddy) {
        main.post { listener.onFriendAdded(buddy) }
    }
        override fun onGroupCreated(group: ImGroup) {
        main.post { listener.onGroupCreated(group) }
    }
        override fun onGroupMembersLoaded(groupId: Long, members: List<ImMember>) {
        main.post { listener.onGroupMembersLoaded(groupId, members) }
    }
        override fun onError(code: Int, msg: String) {
        main.post { listener.onError(code, msg) }
    }
    }

    fun start(host: String, port: Int): Boolean {
        // 每次连接前重建 native 对象，确保重连/退出后能再次使用
        if (handle != 0L) {
            NativeImClient.nativeStop(handle)
            NativeImClient.nativeDestroy(handle)
            handle = 0
        }
        handle = NativeImClient.nativeCreate(bridge)
        return NativeImClient.nativeStart(handle, host, port)
    }

    fun stop() {
        if (handle != 0L) {
            NativeImClient.nativeStop(handle)
            NativeImClient.nativeDestroy(handle)
            handle = 0
        }
    }

    fun login(user: String, pass: String) = guard { NativeImClient.nativeLogin(it, user, pass) }
    fun register(user: String, pass: String, nick: String) =
        guard { NativeImClient.nativeRegister(it, user, pass, nick) }
    fun logout() = guard { NativeImClient.nativeLogout(it) }
    fun loadContacts() = guard { NativeImClient.nativeLoadContacts(it) }
    fun loadSessions() = guard { NativeImClient.nativeLoadSessions(it) }
    fun loadHistory(targetId: Long, targetType: Int, limit: Int = 50) =
        guard { NativeImClient.nativeLoadHistory(it, targetId, targetType, limit) }
    fun sendText(targetId: Long, targetType: Int, text: String) =
        guard { NativeImClient.nativeSendText(it, targetId, targetType, text) }
    fun sendReply(targetId: Long, targetType: Int, replyToId: Long, text: String) =
        guard { NativeImClient.nativeSendReply(it, targetId, targetType, replyToId, text) }
    fun markRead(peerId: Long, targetType: Int) =
        guard { NativeImClient.nativeMarkRead(it, peerId, targetType) }
    fun recallMessage(msgId: Long, targetId: Long, targetType: Int) =
        guard { NativeImClient.nativeRecallMessage(it, msgId, targetId, targetType) }
    fun deleteFriend(friendId: Long) = guard { NativeImClient.nativeDeleteFriend(it, friendId) }
    fun addFriend(username: String) = guard { NativeImClient.nativeAddFriend(it, username) }
    fun kickMember(groupId: Long, memberId: Long) =
        guard { NativeImClient.nativeKickMember(it, groupId, memberId) }
    fun leaveGroup(groupId: Long) = guard { NativeImClient.nativeLeaveGroup(it, groupId) }
    fun dismissGroup(groupId: Long) = guard { NativeImClient.nativeDismissGroup(it, groupId) }
    fun renameGroup(groupId: Long, name: String) =
        guard { NativeImClient.nativeRenameGroup(it, groupId, name) }
    fun searchMessages(keyword: String) = guard { NativeImClient.nativeSearchMessages(it, keyword) }
    fun sendTyping(targetId: Long, targetType: Int) =
        guard { NativeImClient.nativeSendTyping(it, targetId, targetType) }
    fun sendImage(targetId: Long, targetType: Int, path: String) =
        guard { NativeImClient.nativeSendFileMessage(it, targetId, targetType, 1, path) }
    fun sendFile(targetId: Long, targetType: Int, path: String) =
        guard { NativeImClient.nativeSendFileMessage(it, targetId, targetType, 2, path) }
    fun downloadFile(fileId: String) = guard { NativeImClient.nativeDownloadFile(it, fileId) }

    private inline fun guard(block: (Long) -> Unit) {
        if (handle != 0L) block(handle)
    }
}
