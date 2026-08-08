#pragma once

#include "im/core/protocol.h"
#include "im/core/types.h"

namespace im {

// =====================================================================
// 客户端逻辑层接口
//
// ClientCore 在独立工作线程中收发数据，所有结果都通过 IClientListener
// 回调（在逻辑线程中调用）。各端 UI 的桥接层实现该接口，并把回调
// 编组回 UI 线程（Qt 信号队列 / Android Handler / iOS 主队列）。
// =====================================================================
class IClientListener {
public:
    virtual ~IClientListener() = default;

    // 网络连接状态变化（回调在逻辑层工作线程）
    virtual void onConnectionChanged(bool connected) = 0;
    // 登录结果；code==0 成功，me 携带登录用户信息
    virtual void onLoginResult(int code, const std::string& msg, const UserInfo& me) = 0;
    virtual void onRegisterResult(int code, const std::string& msg) = 0;
    // 联系人（好友 + 群）拉取完成
    virtual void onContactsLoaded(const std::vector<BuddyInfo>& buddies,
                                  const std::vector<GroupInfo>& groups) = 0;
    virtual void onSessionsLoaded(const std::vector<SessionInfo>& sessions) = 0;
    // 某个会话的历史消息
    virtual void onHistoryLoaded(int64_t targetId, int targetType,
                                 const std::vector<MessageInfo>& msgs) = 0;
    virtual void onMessage(const MessageInfo& msg) = 0;         // 收到新消息
    virtual void onMessageSent(const MessageInfo& msg) = 0;     // 消息已送达确认
    virtual void onMessageRecalled(int64_t msgId, int64_t targetId, int targetType) = 0;
    virtual void onFriendDeleted(int64_t friendId, const std::string& name) = 0;
    virtual void onGroupUpdated(int64_t groupId) = 0;
    virtual void onSearchResults(const std::vector<MessageInfo>& msgs) = 0;
    // "对方正在输入"
    virtual void onTyping(int64_t fromId, int64_t targetId, int targetType) = 0;
    virtual void onMessagesRead(int64_t peerId, int targetType) = 0;   // 我已读某会话（回执）
    virtual void onReadReceipt(int64_t peerId, int targetType) = 0;    // 对方已读我发的消息
    // 文件/图片下载完成（data 为完整二进制）
    virtual void onFileDownloaded(const std::string& fileId, const std::string& name,
                                  int64_t size, const std::string& mime,
                                  const std::vector<uint8_t>& data) = 0;
    virtual void onPresenceChanged(int64_t userId, bool online) = 0;
    virtual void onFriendAdded(const BuddyInfo& buddy) = 0;
    virtual void onGroupCreated(const GroupInfo& group) = 0;
    virtual void onGroupMembersLoaded(int64_t groupId,
                                      const std::vector<MemberInfo>& members) = 0;
    virtual void onError(int code, const std::string& msg) = 0;
    // 我自己的资料更新结果
    virtual void onProfileUpdated(int code, const std::string& msg, const UserInfo& me) = 0;
    // 好友/群成员资料变更推送（昵称/头像）
    virtual void onProfileChanged(int64_t userId, const std::string& nickname,
                                  const std::string& avatar) = 0;
};

// =====================================================================
// 客户端逻辑核心（纯C++，不依赖Qt）
//
// 线程模型：start() 启动一个工作线程，负责连接、收包、分发、心跳。
// 所有调用方法线程安全（内部加锁），未连接时命令会进入待发队列，
// 连接建立后自动补发。数据通过 IClientListener 回调通知上层。
// =====================================================================
class ClientCore {
public:
    ClientCore();
    ~ClientCore();

    ClientCore(const ClientCore&) = delete;
    ClientCore& operator=(const ClientCore&) = delete;

    // 启动工作线程并连接服务器；成功返回 true（会先断开旧连接）
    bool start(const std::string& host, uint16_t port, IClientListener* listener);
    // 停止工作线程并断开连接（阻塞直至线程退出）
    void stop();

    bool isConnected() const;
    const UserInfo& me() const { return me_; }

    // 以下接口线程安全，均可从任意线程调用
    void login(const std::string& username, const std::string& password);
    void registerUser(const std::string& username, const std::string& password,
                      const std::string& nickname);
    void logout();
    void loadContacts();
    void loadSessions();
    void loadHistory(int64_t targetId, int targetType, int limit = 50);
    void sendText(int64_t targetId, int targetType, const std::string& text);
    void sendReply(int64_t targetId, int targetType, int64_t replyToId, const std::string& text);
    void markRead(int64_t peerId, int targetType);
    void recallMessage(int64_t msgId, int64_t targetId, int targetType);
    void deleteFriend(int64_t friendId);
    void kickMember(int64_t groupId, int64_t memberId);
    void leaveGroup(int64_t groupId);
    void dismissGroup(int64_t groupId);
    void renameGroup(int64_t groupId, const std::string& name);
    void searchMessages(const std::string& keyword, int limit = 50);
    void sendTyping(int64_t targetId, int targetType);
    // 发送图片(MSG_IMAGE)/文件(MSG_FILE)：内部先上传再发消息
    void sendFileMessage(int64_t targetId, int targetType, int msgType, const std::string& path);
    // 下载服务器文件（图片消息会自动下载）
    void downloadFile(const std::string& fileId);
    void addFriend(const std::string& username, const std::string& remark);
    void createGroup(const std::string& name, const std::vector<int64_t>& memberIds);
    void loadGroupMembers(int64_t groupId);

    // 修改资料：非空字段生效；改密码需 oldPassword
    void updateProfile(const std::string& nickname, const std::string& avatar,
                       const std::string& oldPassword, const std::string& newPassword);
    // 修改资料并上传新头像：path 为空则只改昵称/密码
    void updateProfileWithAvatarUpload(const std::string& path, const std::string& nickname,
                                       const std::string& oldPassword,
                                       const std::string& newPassword);

public:
    struct Impl;
    Impl* impl_;
    UserInfo me_;
};

} // namespace im
