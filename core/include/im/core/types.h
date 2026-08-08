#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace im {

// =====================================================================
// 数据类型定义（协议层/业务层共用）
//
// 这些结构体是客户端与服务端之间传递数据的"语言"，通过 codec 序列化为
// 二进制协议帧，也可直接在各端 UI 桥接层中转换为平台对象。
// =====================================================================

// 目标类型：一条消息/会话的"对方"是什么
enum TargetType : int {
    TARGET_FRIEND = 0, // 单聊（对方是一个用户）
    TARGET_GROUP = 1,  // 群聊（对方是一个群）
};

// 消息类型
enum MsgType : int {
    MSG_TEXT = 0,   // 文本消息，content 为纯文本
    MSG_IMAGE = 1,  // 图片消息，content 为 "fileId|文件名|大小|mime"
    MSG_FILE = 2,   // 文件消息，content 格式同图片
    MSG_SYSTEM = 3, // 系统消息（如群成员变动提示）
};

// 消息方向（仅在客户端本地标记，不参与协议传输）
enum MsgDirection : int {
    DIRECTION_OUT = 0, // 我发出的
    DIRECTION_IN = 1,  // 我收到的
};

// 用户基本信息
struct UserInfo {
    int64_t id = 0;            // 服务器分配的唯一用户ID
    std::string username;      // 登录用户名（唯一）
    std::string nickname;      // 显示昵称
    std::string avatar;        // 头像：服务器文件 ID 或空（客户端下载后展示）
    int online = 0;            // 在线状态：0 离线 1 在线
};

// 好友（我的好友列表中的一项）
struct BuddyInfo {
    UserInfo user;    // 对方用户信息
    std::string remark; // 备注名（空则显示对方昵称）
};

// 群成员
struct MemberInfo {
    UserInfo user;      // 成员用户信息
    std::string groupNick; // 群内昵称（空则显示用户昵称）
};

// 群信息
struct GroupInfo {
    int64_t id = 0;            // 群ID
    std::string name;          // 群名称
    int64_t ownerId = 0;       // 群主用户ID
    std::vector<MemberInfo> members; // 成员列表
};

// 一条消息
struct MessageInfo {
    int64_t id = 0;            // 服务端消息ID（0 表示尚未确认）
    int64_t fromId = 0;        // 发送者用户ID
    int64_t targetId = 0;      // 目标：单聊=对方用户ID，群聊=群ID
    int targetType = TARGET_FRIEND;
    int msgType = MSG_TEXT;    // 见 MsgType
    std::string content;       // 文本内容或 文件/图片 的 fileId 描述串
    int64_t timestamp = 0;     // Unix 秒
    int direction = DIRECTION_OUT; // 客户端本地标记方向
    std::string senderName;    // 发送者显示名（群聊展示用）
    int read = 0;              // 已读标记：1=对方(或本人)已读
    int64_t replyToId = 0;     // 引用的消息ID，0=无引用
    std::string replyContent;  // 被引用消息的原文快照
};

// 会话（最近会话列表中的一项）
struct SessionInfo {
    int64_t targetId = 0;   // 会话对象：用户ID 或 群ID
    int targetType = TARGET_FRIEND;
    std::string title;      // 显示标题（对方昵称或群名）
    std::string avatar;     // 头像 fileId（空则用首字母占位）
    std::string lastContent; // 最后一条消息预览
    int64_t lastTime = 0;   // 最后消息时间（Unix 秒）
    int unread = 0;         // 未读数
};

} // namespace im
