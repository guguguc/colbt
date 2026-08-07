#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "im/core/icclient.h"

// Qt 友好的数据结构（用于跨线程信号传递）
struct QtUser {
    qint64 id = 0;
    QString username;
    QString nickname;
    QString avatar;
    int online = 0;
};

struct QtBuddy {
    QtUser user;
    QString remark;
};

struct QtMember {
    QtUser user;
    QString groupNick;
};

struct QtGroup {
    qint64 id = 0;
    QString name;
    qint64 ownerId = 0;
    QVector<QtMember> members;
};

struct QtMessage {
    qint64 id = 0;
    qint64 fromId = 0;
    qint64 targetId = 0;
    int targetType = 0; // 0好友 1群
    int msgType = 0;
    QString content;
    qint64 timestamp = 0;
    int direction = 0; // 0发出 1接收
    QString senderName;
    int read = 0; // 1=已读
    qint64 replyToId = 0;
    QString replyContent;
};

struct QtSession {
    qint64 targetId = 0;
    int targetType = 0;
    QString title;
    QString avatar;
    QString lastContent;
    qint64 lastTime = 0;
    int unread = 0;
};

// UI 与逻辑层的桥接：持有逻辑库 ClientCore，将回调编组到 UI 线程
class AppContext : public QObject, public im::IClientListener {
    Q_OBJECT

public:
    explicit AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    // ---- UI 调用逻辑层（可在 UI 线程调用） ----
    void connectToServer(const QString& host, int port);
    void login(const QString& username, const QString& password);
    void registerUser(const QString& username, const QString& password, const QString& nickname);
    void loadContacts();
    void loadSessions();
    void openSession(qint64 targetId, int targetType);
    void sendText(qint64 targetId, int targetType, const QString& text);
    void sendReply(qint64 targetId, int targetType, qint64 replyToId, const QString& text);
    void markRead(qint64 peerId, int targetType);
    void recallMessage(qint64 msgId, qint64 targetId, int targetType);
    void deleteFriend(qint64 friendId);
    void kickMember(qint64 groupId, qint64 memberId);
    void leaveGroup(qint64 groupId);
    void dismissGroup(qint64 groupId);
    void renameGroup(qint64 groupId, const QString& name);
    void searchMessages(const QString& keyword);
    void sendTyping(qint64 targetId, int targetType);
    void sendFile(qint64 targetId, int targetType, int msgType, const QString& path);
    void downloadFile(const QString& fileId);
    void addFriend(const QString& username);
    void createGroup(const QString& name, const QVector<qint64>& memberIds);
    void loadGroupMembers(qint64 groupId);
    void updateProfile(const QString& nickname, const QString& avatarPath,
                       const QString& oldPassword, const QString& newPassword);
    void logout();
    void disconnectAll();

    qint64 myId() const { return myId_; }
    const QtUser& me() const { return me_; }

signals:
    void connectionChanged(bool connected);
    void loginResult(int code, const QString& msg, const QtUser& me);
    void registerResult(int code, const QString& msg);
    void contactsReady(const QVector<QtBuddy>& buddies, const QVector<QtGroup>& groups);
    void sessionsReady(const QVector<QtSession>& sessions);
    void historyReady(qint64 targetId, int targetType, const QVector<QtMessage>& msgs);
    void messageArrived(const QtMessage& msg);
    void messageSent(const QtMessage& msg);
    void messageRecalled(qint64 msgId, qint64 targetId, int targetType);
    void friendDeleted(qint64 friendId, const QString& name);
    void groupUpdated(qint64 groupId);
    void searchResults(const QVector<QtMessage>& msgs);
    void typing(int64_t fromId, qint64 targetId, int targetType);
    void messagesRead(qint64 peerId, int targetType);
    void readReceipt(qint64 peerId, int targetType);
    void fileDownloaded(const QString& fileId, const QString& name, qint64 size,
                        const QString& mime, const QByteArray& data);
    void presenceChanged(qint64 userId, bool online);
    void friendAdded(const QtBuddy& buddy);
    void groupCreated(const QtGroup& group);
    void groupMembersReady(qint64 groupId, const QVector<QtMember>& members);
    void errorOccurred(int code, const QString& msg);
    void profileUpdated(int code, const QString& msg, const QtUser& me);
    void profileChanged(qint64 userId, const QString& nickname, const QString& avatar);

private:
    // im::IClientListener 回调（逻辑层工作线程调用，信号经队列投递到UI线程）
    void onConnectionChanged(bool connected) override;
    void onLoginResult(int code, const std::string& msg, const im::UserInfo& me) override;
    void onRegisterResult(int code, const std::string& msg) override;
    void onContactsLoaded(const std::vector<im::BuddyInfo>& buddies,
                          const std::vector<im::GroupInfo>& groups) override;
    void onSessionsLoaded(const std::vector<im::SessionInfo>& sessions) override;
    void onHistoryLoaded(int64_t targetId, int targetType,
                         const std::vector<im::MessageInfo>& msgs) override;
    void onMessage(const im::MessageInfo& msg) override;
    void onMessageSent(const im::MessageInfo& msg) override;
    void onMessageRecalled(int64_t msgId, int64_t targetId, int targetType) override;
    void onFriendDeleted(int64_t friendId, const std::string& name) override;
    void onGroupUpdated(int64_t groupId) override;
    void onSearchResults(const std::vector<im::MessageInfo>& msgs) override;
    void onTyping(int64_t fromId, int64_t targetId, int targetType) override;
    void onMessagesRead(int64_t peerId, int targetType) override;
    void onReadReceipt(int64_t peerId, int targetType) override;
    void onFileDownloaded(const std::string& fileId, const std::string& name, int64_t size,
                          const std::string& mime,
                          const std::vector<uint8_t>& data) override;
    void onPresenceChanged(int64_t userId, bool online) override;
    void onFriendAdded(const im::BuddyInfo& buddy) override;
    void onGroupCreated(const im::GroupInfo& group) override;
    void onGroupMembersLoaded(int64_t groupId,
                              const std::vector<im::MemberInfo>& members) override;
    void onError(int code, const std::string& msg) override;
    void onProfileUpdated(int code, const std::string& msg, const im::UserInfo& me) override;
    void onProfileChanged(int64_t userId, const std::string& nickname,
                          const std::string& avatar) override;

    std::unique_ptr<im::ClientCore> core_;
    qint64 myId_ = 0;
    QtUser me_;
};

// 类型转换
QtUser toQtUser(const im::UserInfo& u);
QtMessage toQtMessage(const im::MessageInfo& m);
QtBuddy toQtBuddy(const im::BuddyInfo& b);
QtGroup toQtGroup(const im::GroupInfo& g);
QtMember toQtMember(const im::MemberInfo& m);
QtSession toQtSession(const im::SessionInfo& s);
