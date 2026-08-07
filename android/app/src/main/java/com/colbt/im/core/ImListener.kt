package com.colbt.im.core

// 回调来自 C++ 工作线程，需自行编组到主线程
interface ImListener {
    fun onConnectionChanged(connected: Boolean)
    fun onLoginResult(code: Int, msg: String, me: ImBuddy)
    fun onRegisterResult(code: Int, msg: String)
    fun onContactsLoaded(buddies: List<ImBuddy>, groups: List<ImGroup>)
    fun onSessionsLoaded(sessions: List<ImSession>)
    fun onHistoryLoaded(targetId: Long, targetType: Int, msgs: List<ImMessage>)
    fun onMessage(msg: ImMessage)
    fun onMessageSent(msg: ImMessage)
    fun onMessageRecalled(msgId: Long, targetId: Long, targetType: Int)
    fun onFriendDeleted(friendId: Long, name: String)
    fun onGroupUpdated(groupId: Long)
    fun onSearchResults(msgs: List<ImMessage>)
    fun onTyping(fromId: Long, targetId: Long, targetType: Int)
    fun onMessagesRead(peerId: Long, targetType: Int)
    fun onReadReceipt(peerId: Long, targetType: Int)
    fun onFileDownloaded(fileId: String, name: String, size: Long, mime: String, data: ByteArray)
    fun onPresenceChanged(userId: Long, online: Boolean)
    fun onFriendAdded(buddy: ImBuddy)
    fun onGroupCreated(group: ImGroup)
    fun onGroupMembersLoaded(groupId: Long, members: List<ImMember>)
    fun onError(code: Int, msg: String)
    fun onProfileUpdated(code: Int, msg: String, me: ImBuddy)
    fun onProfileChanged(userId: Long, nickname: String, avatar: String)
}

// 空实现基类，方便只关心部分回调
open class ImListenerAdapter : ImListener {
    override fun onConnectionChanged(connected: Boolean) {}
    override fun onLoginResult(code: Int, msg: String, me: ImBuddy) {}
    override fun onRegisterResult(code: Int, msg: String) {}
    override fun onContactsLoaded(buddies: List<ImBuddy>, groups: List<ImGroup>) {}
    override fun onSessionsLoaded(sessions: List<ImSession>) {}
    override fun onHistoryLoaded(targetId: Long, targetType: Int, msgs: List<ImMessage>) {}
    override fun onMessage(msg: ImMessage) {}
    override fun onMessageSent(msg: ImMessage) {}
    override fun onMessageRecalled(msgId: Long, targetId: Long, targetType: Int) {}
    override fun onFriendDeleted(friendId: Long, name: String) {}
    override fun onGroupUpdated(groupId: Long) {}
    override fun onSearchResults(msgs: List<ImMessage>) {}
    override fun onTyping(fromId: Long, targetId: Long, targetType: Int) {}
    override fun onMessagesRead(peerId: Long, targetType: Int) {}
    override fun onReadReceipt(peerId: Long, targetType: Int) {}
    override fun onFileDownloaded(fileId: String, name: String, size: Long, mime: String, data: ByteArray) {}
    override fun onPresenceChanged(userId: Long, online: Boolean) {}
    override fun onFriendAdded(buddy: ImBuddy) {}
    override fun onGroupCreated(group: ImGroup) {}
    override fun onGroupMembersLoaded(groupId: Long, members: List<ImMember>) {}
    override fun onError(code: Int, msg: String) {}
    override fun onProfileUpdated(code: Int, msg: String, me: ImBuddy) {}
    override fun onProfileChanged(userId: Long, nickname: String, avatar: String) {}
}
