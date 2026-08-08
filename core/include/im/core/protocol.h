#pragma once

#include <cstdint>
#include <vector>

#include "im/core/types.h"

namespace im {

// =====================================================================
// 二进制协议定义
//
// 帧格式（12 字节定长头 + 变长正文）：
//   magic[4] | version[1] | cmd[2] | reserved[1] | bodyLen[4] | body[bodyLen]
//
//  - magic:    固定魔数 "IMC!"，用于快速识别与对齐
//  - cmd:      命令字（见 Cmd 枚举），决定正文如何编解码
//  - bodyLen:  正文长度，保护性限制见 kMaxBodyLen
//
// 每个命令的正文格式由 codec.h/codec.cpp 中的对应函数定义。
// 命名约定：_REQ=客户端请求，_RESP=服务端应答，_PUSH=服务端主动推送。
// =====================================================================

static const uint32_t kMagic = 0x21434D49;             // "IMC!"
static const uint8_t kVersion = 1;
static const uint32_t kMaxBodyLen = 40 * 1024 * 1024;  // 单帧正文上限（容纳最大文件包）
static const uint64_t kMaxFileSize = 32 * 1024 * 1024; // 单个文件大小上限 32MB
static const uint16_t kHeartbeatIntervalSec = 15;      // 客户端心跳间隔
static const uint16_t kServerDeadlineSec = 60;         // 服务端判定连接超时的静默时长

// 命令字
enum Cmd : uint16_t {
    // ---- 基础 ----
    CMD_HEARTBEAT = 0x0001, // 心跳（无正文）
    CMD_ACK = 0x0002,       // 通用应答
    CMD_ERROR = 0x0003,     // 错误通知：code(u8) + msg(str)

    // ---- 账号 ----
    CMD_LOGIN_REQ = 0x0010,      // 登录请求
    CMD_LOGIN_RESP = 0x0011,     // 登录应答（含用户信息）
    CMD_REGISTER_REQ = 0x0012,   // 注册请求
    CMD_REGISTER_RESP = 0x0013,  // 注册应答
    CMD_LOGOUT_REQ = 0x0014,     // 退出登录

    // ---- 联系人 ----
    CMD_GET_CONTACTS_REQ = 0x0020,   // 拉取好友列表 + 群列表
    CMD_GET_CONTACTS_RESP = 0x0021,
    CMD_ADD_FRIEND_REQ = 0x0022,     // 添加好友
    CMD_ADD_FRIEND_RESP = 0x0023,
    CMD_FRIEND_ADDED_PUSH = 0x0024,  // 被添加方收到的新好友推送

    // ---- 群组 ----
    CMD_CREATE_GROUP_REQ = 0x0026,     // 创建群
    CMD_CREATE_GROUP_RESP = 0x0027,
    CMD_GET_GROUP_MEMBERS_REQ = 0x0028, // 拉取群成员
    CMD_GET_GROUP_MEMBERS_RESP = 0x0029,

    // ---- 会话与历史 ----
    CMD_GET_SESSIONS_REQ = 0x0030,  // 拉取最近会话列表
    CMD_GET_SESSIONS_RESP = 0x0031,
    CMD_GET_HISTORY_REQ = 0x0032,   // 拉取某个会话的历史消息
    CMD_GET_HISTORY_RESP = 0x0033,

    // ---- 消息 ----
    CMD_SEND_MSG_REQ = 0x0040,   // 发送消息
    CMD_SEND_MSG_RESP = 0x0041,  // 发送成功回执（含服务端分配的ID）
    CMD_MSG_PUSH = 0x0042,       // 服务端推送给接收方的新消息
    CMD_PRESENCE_PUSH = 0x0043,  // 上下线状态推送

    // ---- 已读 ----
    CMD_MARK_READ_REQ = 0x0050,  // 客户端上报：我已读某会话
    CMD_MARK_READ_RESP = 0x0051, // 服务端回执
    CMD_READ_PUSH = 0x0052,      // 服务端→发送方：对方已读

    // ---- 文件 ----
    CMD_UPLOAD_FILE_REQ = 0x0060,  // 上传文件（元数据 + 二进制数据）
    CMD_UPLOAD_FILE_RESP = 0x0061, // 返回 fileId
    CMD_DOWNLOAD_FILE_REQ = 0x0062, // 按 fileId 下载
    CMD_DOWNLOAD_FILE_RESP = 0x0063,

    // ---- 群管理 / 消息管理 ----
    CMD_RECALL_MSG_REQ = 0x0070,      // 撤回消息
    CMD_RECALL_MSG_PUSH = 0x0071,     // 撤回推送
    CMD_DELETE_FRIEND_REQ = 0x0072,   // 删除好友
    CMD_DELETE_FRIEND_PUSH = 0x0073,  // 被删方收到通知
    CMD_KICK_MEMBER_REQ = 0x0074,     // 群主踢人
    CMD_LEAVE_GROUP_REQ = 0x0075,     // 退群
    CMD_DISMISS_GROUP_REQ = 0x0076,   // 群主解散群
    CMD_RENAME_GROUP_REQ = 0x0077,    // 改群名
    CMD_GROUP_UPDATED_PUSH = 0x0078,  // 群信息变更推送
    CMD_SEARCH_MSGS_REQ = 0x0079,     // 搜索聊天记录
    CMD_SEARCH_MSGS_RESP = 0x007A,
    CMD_TYPING_REQ = 0x007B,          // 我正在输入
    CMD_TYPING_PUSH = 0x007C,         // "对方正在输入"推送

    // ---- 资料 ----
    CMD_UPDATE_PROFILE_REQ = 0x0080,      // 修改昵称/头像/密码
    CMD_UPDATE_PROFILE_RESP = 0x0081,
    CMD_PROFILE_UPDATED_PUSH = 0x0082,    // 好友资料变更推送
};

// 一帧原始数据（命令字 + 正文字节流）
struct Packet {
    uint16_t cmd = 0;
    std::vector<uint8_t> body;
};

} // namespace im
