package com.colbt.im.core

data class ImBuddy(
    val id: Long,
    val username: String,
    val nickname: String,
    val avatar: String,
    val online: Int,
    val remark: String,
)

data class ImMember(
    val id: Long,
    val username: String,
    val nickname: String,
    val avatar: String,
    val online: Int,
    val groupNick: String,
)

data class ImGroup(
    val id: Long,
    val name: String,
    val ownerId: Long,
    val members: List<ImMember>,
)

data class ImSession(
    val targetId: Long,
    val targetType: Int,
    val title: String,
    val avatar: String,
    val lastContent: String,
    val lastTime: Long,
    val unread: Int,
)
