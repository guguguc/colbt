package com.colbt.im.core

data class ImMessage(
    val id: Long,
    val fromId: Long,
    val targetId: Long,
    val targetType: Int,
    val msgType: Int,
    val content: String,
    val timestamp: Long,
    val direction: Int,
    val read: Int,
    val senderName: String,
    val replyToId: Long,
    val replyContent: String,
)
