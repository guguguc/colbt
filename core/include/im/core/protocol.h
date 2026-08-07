#pragma once

#include <cstdint>
#include <vector>

#include "im/core/types.h"

namespace im {

// 二进制协议
//  头部(12字节): magic[uint32] + version[uint8] + cmd[uint16] + reserved[uint8] + bodyLen[uint32]
//  正文: 由各命令自定义，见 codec.h
static const uint32_t kMagic = 0x21434D49; // "IMC!"
static const uint8_t kVersion = 1;
static const uint32_t kMaxBodyLen = 40 * 1024 * 1024; // 容纳最大文件包
static const uint64_t kMaxFileSize = 32 * 1024 * 1024; // 单个文件上限 32MB
static const uint16_t kHeartbeatIntervalSec = 15;
static const uint16_t kServerDeadlineSec = 60;

enum Cmd : uint16_t {
    CMD_HEARTBEAT = 0x0001,
    CMD_ACK = 0x0002,
    CMD_ERROR = 0x0003,

    CMD_LOGIN_REQ = 0x0010,
    CMD_LOGIN_RESP = 0x0011,
    CMD_REGISTER_REQ = 0x0012,
    CMD_REGISTER_RESP = 0x0013,
    CMD_LOGOUT_REQ = 0x0014,

    CMD_GET_CONTACTS_REQ = 0x0020,
    CMD_GET_CONTACTS_RESP = 0x0021,
    CMD_ADD_FRIEND_REQ = 0x0022,
    CMD_ADD_FRIEND_RESP = 0x0023,
    CMD_FRIEND_ADDED_PUSH = 0x0024,

    CMD_CREATE_GROUP_REQ = 0x0026,
    CMD_CREATE_GROUP_RESP = 0x0027,
    CMD_GET_GROUP_MEMBERS_REQ = 0x0028,
    CMD_GET_GROUP_MEMBERS_RESP = 0x0029,

    CMD_GET_SESSIONS_REQ = 0x0030,
    CMD_GET_SESSIONS_RESP = 0x0031,
    CMD_GET_HISTORY_REQ = 0x0032,
    CMD_GET_HISTORY_RESP = 0x0033,

    CMD_SEND_MSG_REQ = 0x0040,
    CMD_SEND_MSG_RESP = 0x0041,
    CMD_MSG_PUSH = 0x0042,
    CMD_PRESENCE_PUSH = 0x0043,

    CMD_MARK_READ_REQ = 0x0050,   // 客户端：我已读某会话
    CMD_MARK_READ_RESP = 0x0051,  // 服务端回执
    CMD_READ_PUSH = 0x0052,       // 服务端→发送方：对方已读

    CMD_UPLOAD_FILE_REQ = 0x0060,  // 上传文件（数据+元数据）
    CMD_UPLOAD_FILE_RESP = 0x0061, // 返回 fileId
    CMD_DOWNLOAD_FILE_REQ = 0x0062,
    CMD_DOWNLOAD_FILE_RESP = 0x0063,

    CMD_RECALL_MSG_REQ = 0x0070,
    CMD_RECALL_MSG_PUSH = 0x0071,
    CMD_DELETE_FRIEND_REQ = 0x0072,
    CMD_DELETE_FRIEND_PUSH = 0x0073,
    CMD_KICK_MEMBER_REQ = 0x0074,
    CMD_LEAVE_GROUP_REQ = 0x0075,
    CMD_DISMISS_GROUP_REQ = 0x0076,
    CMD_RENAME_GROUP_REQ = 0x0077,
    CMD_GROUP_UPDATED_PUSH = 0x0078,
    CMD_SEARCH_MSGS_REQ = 0x0079,
    CMD_SEARCH_MSGS_RESP = 0x007A,
    CMD_TYPING_REQ = 0x007B,
    CMD_TYPING_PUSH = 0x007C,
};

struct Packet {
    uint16_t cmd = 0;
    std::vector<uint8_t> body;
};

} // namespace im
