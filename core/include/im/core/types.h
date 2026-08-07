#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace im {

// 目标类型：单聊 / 群聊
enum TargetType : int {
    TARGET_FRIEND = 0,
    TARGET_GROUP = 1,
};

// 消息类型
enum MsgType : int {
    MSG_TEXT = 0,   // 文本
    MSG_IMAGE = 1,  // 图片（内容为路径/URL）
    MSG_FILE = 2,   // 文件（内容为路径/URL）
    MSG_SYSTEM = 3, // 系统消息
};

// 消息方向（客户端本地标记）
enum MsgDirection : int {
    DIRECTION_OUT = 0, // 发出
    DIRECTION_IN = 1,  // 接收
};

struct UserInfo {
    int64_t id = 0;
    std::string username;
    std::string nickname;
    std::string avatar;
    int online = 0; // 0 离线 1 在线
};

struct BuddyInfo {
    UserInfo user;
    std::string remark;
};

struct MemberInfo {
    UserInfo user;
    std::string groupNick;
};

struct GroupInfo {
    int64_t id = 0;
    std::string name;
    int64_t ownerId = 0;
    std::vector<MemberInfo> members;
};

struct MessageInfo {
    int64_t id = 0;        // 服务端消息ID
    int64_t fromId = 0;    // 发送者用户ID
    int64_t targetId = 0;  // 单聊=对方用户ID，群聊=群ID
    int targetType = TARGET_FRIEND;
    int msgType = MSG_TEXT;
    std::string content;
    int64_t timestamp = 0; // Unix秒
    int direction = DIRECTION_OUT;
    std::string senderName;
    int read = 0;          // 1=对方(或本人)已读
    int64_t replyToId = 0; // 引用的消息ID，0=无
    std::string replyContent; // 引用消息的原文快照
};

struct SessionInfo {
    int64_t targetId = 0;
    int targetType = TARGET_FRIEND;
    std::string title;
    std::string avatar;
    std::string lastContent;
    int64_t lastTime = 0;
    int unread = 0;
};

} // namespace im
