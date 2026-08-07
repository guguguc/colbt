#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "im/core/types.h"

struct sqlite3;

namespace im {

// SQLite 持久化层（线程安全：内部加锁）
class Storage {
public:
    Storage();
    ~Storage();

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // 用户
    int64_t createUser(const std::string& username, const std::string& pwdHash,
                       const std::string& nickname, std::string& err);
    // 通过用户名查找（含密码hash）；不存在返回 false
    bool findUserByName(const std::string& username, UserInfo& out, std::string& pwdHash);
    bool findUserById(int64_t id, UserInfo& out);
    // 修改资料：非空字段才会更新（newPwdHash 为空则不改密码）
    bool updateUser(int64_t id, const std::string& nickname, const std::string& avatar,
                    const std::string& newPwdHash, std::string& err);

    // 好友
    bool addFriend(int64_t userId, int64_t friendId, const std::string& remark);
    std::vector<BuddyInfo> getFriends(int64_t userId);
    bool areFriends(int64_t a, int64_t b);
    bool deleteFriend(int64_t a, int64_t b);

    // 群组
    int64_t createGroup(const std::string& name, int64_t ownerId, std::string& err);
    bool addGroupMember(int64_t groupId, int64_t userId, const std::string& groupNick);
    std::vector<GroupInfo> getGroups(int64_t userId);
    std::vector<MemberInfo> getGroupMembers(int64_t groupId);
    bool isGroupMember(int64_t groupId, int64_t userId);
    int64_t findGroupById(int64_t id, GroupInfo& out);
    bool kickMember(int64_t groupId, int64_t memberId);
    bool leaveGroup(int64_t groupId, int64_t userId);
    bool dismissGroup(int64_t groupId);
    bool renameGroup(int64_t groupId, const std::string& name);

private:
    std::vector<MemberInfo> getGroupMembersUnlocked(int64_t groupId);

public:
    // 消息
    int64_t saveMessage(int64_t fromId, int64_t targetId, int targetType, int msgType,
                        const std::string& content, int64_t ts, int isRead = 0,
                        int64_t replyToId = 0);
    // 查询单条消息（撤回校验用）
    bool findMessage(int64_t msgId, MessageInfo& out);
    // 撤回：仅发送者可删除；返回是否成功
    bool recallMessage(int64_t msgId, int64_t fromId);
    // myUserId 用于单聊的双向查询（消息可能是自己发、也可能是对方发）
    std::vector<MessageInfo> getHistory(int64_t myUserId, int64_t targetId, int targetType,
                                        int limit, int64_t beforeId);
    // 搜索聊天记录
    std::vector<MessageInfo> searchMessages(int64_t userId, const std::string& keyword,
                                            int limit);
    // 把"我(readerId)与 peerId 会话中对方发来的消息"标记为已读；返回标记条数
    int markMessagesRead(int64_t readerId, int64_t peerId, int targetType);

    // 文件
    bool createFile(const std::string& fileId, const std::string& name, int64_t size,
                    const std::string& mime);
    bool findFile(const std::string& fileId, std::string& name, int64_t& size,
                  std::string& mime);
    std::vector<SessionInfo> getSessions(int64_t userId, int limit);

private:
    sqlite3* db_ = nullptr;
    std::mutex mutex_;

    bool exec(const std::string& sql, std::string& err);
};

std::string sha256Hex(const std::string& input);

} // namespace im
