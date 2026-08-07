package com.colbt.im.core

// JNI 桥：native 方法与 ImListener 一一对应（C++ 侧通过 ImListener 回调）
object NativeImClient {
    init {
        System.loadLibrary("imclient")
    }

    external fun nativeCreate(listener: ImListener): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeStart(handle: Long, host: String, port: Int): Boolean
    external fun nativeStop(handle: Long)
    external fun nativeLogin(handle: Long, user: String, pass: String)
    external fun nativeRegister(handle: Long, user: String, pass: String, nick: String)
    external fun nativeLogout(handle: Long)
    external fun nativeLoadContacts(handle: Long)
    external fun nativeLoadSessions(handle: Long)
    external fun nativeLoadHistory(handle: Long, targetId: Long, targetType: Int, limit: Int)
    external fun nativeSendText(handle: Long, targetId: Long, targetType: Int, text: String)
    external fun nativeSendReply(handle: Long, targetId: Long, targetType: Int, replyToId: Long, text: String)
    external fun nativeMarkRead(handle: Long, peerId: Long, targetType: Int)
    external fun nativeRecallMessage(handle: Long, msgId: Long, targetId: Long, targetType: Int)
    external fun nativeDeleteFriend(handle: Long, friendId: Long)
    external fun nativeAddFriend(handle: Long, username: String)
    external fun nativeKickMember(handle: Long, groupId: Long, memberId: Long)
    external fun nativeLeaveGroup(handle: Long, groupId: Long)
    external fun nativeDismissGroup(handle: Long, groupId: Long)
    external fun nativeRenameGroup(handle: Long, groupId: Long, name: String)
    external fun nativeSearchMessages(handle: Long, keyword: String)
    external fun nativeSendTyping(handle: Long, targetId: Long, targetType: Int)
    external fun nativeSendFileMessage(handle: Long, targetId: Long, targetType: Int, msgType: Int, path: String)
    external fun nativeDownloadFile(handle: Long, fileId: String)
    external fun nativeUpdateProfile(handle: Long, nickname: String, avatarPath: String,
                                     oldPass: String, newPass: String)
}
