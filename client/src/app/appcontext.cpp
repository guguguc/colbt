#include "app/appcontext.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>

namespace {

QtUser convert(const im::UserInfo& u) {
    QtUser q;
    q.id = u.id;
    q.username = QString::fromStdString(u.username);
    q.nickname = QString::fromStdString(u.nickname);
    q.avatar = QString::fromStdString(u.avatar);
    q.online = u.online;
    return q;
}

} // namespace

QtUser toQtUser(const im::UserInfo& u) { return convert(u); }
QtMessage toQtMessage(const im::MessageInfo& m) {
    QtMessage q;
    q.id = m.id;
    q.fromId = m.fromId;
    q.targetId = m.targetId;
    q.targetType = m.targetType;
    q.msgType = m.msgType;
    q.content = QString::fromStdString(m.content);
    q.timestamp = m.timestamp;
    q.direction = m.direction;
    q.senderName = QString::fromStdString(m.senderName);
    q.read = m.read;
    q.replyToId = m.replyToId;
    q.replyContent = QString::fromStdString(m.replyContent);
    return q;
}
QtBuddy toQtBuddy(const im::BuddyInfo& b) {
    QtBuddy q;
    q.user = convert(b.user);
    q.remark = QString::fromStdString(b.remark);
    return q;
}
QtGroup toQtGroup(const im::GroupInfo& g) {
    QtGroup q;
    q.id = g.id;
    q.name = QString::fromStdString(g.name);
    q.ownerId = g.ownerId;
    for (const auto& m : g.members) q.members.append(toQtMember(m));
    return q;
}
QtMember toQtMember(const im::MemberInfo& m) {
    QtMember q;
    q.user = convert(m.user);
    q.groupNick = QString::fromStdString(m.groupNick);
    return q;
}
QtSession toQtSession(const im::SessionInfo& s) {
    QtSession q;
    q.targetId = s.targetId;
    q.targetType = s.targetType;
    q.title = QString::fromStdString(s.title);
    q.avatar = QString::fromStdString(s.avatar);
    q.lastContent = QString::fromStdString(s.lastContent);
    q.lastTime = s.lastTime;
    q.unread = s.unread;
    return q;
}

AppContext::AppContext(QObject* parent)
    : QObject(parent), core_(std::make_unique<im::ClientCore>()) {}

AppContext::~AppContext() {
    core_->stop();
}

void AppContext::connectToServer(const QString& host, int port) {
    core_->start(host.toStdString(), static_cast<uint16_t>(port), this);
}

void AppContext::login(const QString& username, const QString& password) {
    core_->login(username.toStdString(), password.toStdString());
}

void AppContext::registerUser(const QString& username, const QString& password,
                              const QString& nickname) {
    core_->registerUser(username.toStdString(), password.toStdString(), nickname.toStdString());
}

void AppContext::loadContacts() { core_->loadContacts(); }

void AppContext::loadSessions() { core_->loadSessions(); }

void AppContext::openSession(qint64 targetId, int targetType) {
    core_->loadHistory(targetId, targetType, 50);
}

void AppContext::sendText(qint64 targetId, int targetType, const QString& text) {
    core_->sendText(targetId, targetType, text.toStdString());
}

void AppContext::sendReply(qint64 targetId, int targetType, qint64 replyToId,
                           const QString& text) {
    core_->sendReply(targetId, targetType, replyToId, text.toStdString());
}

void AppContext::recallMessage(qint64 msgId, qint64 targetId, int targetType) {
    core_->recallMessage(msgId, targetId, targetType);
}

void AppContext::deleteFriend(qint64 friendId) { core_->deleteFriend(friendId); }

void AppContext::kickMember(qint64 groupId, qint64 memberId) { core_->kickMember(groupId, memberId); }

void AppContext::leaveGroup(qint64 groupId) { core_->leaveGroup(groupId); }

void AppContext::dismissGroup(qint64 groupId) { core_->dismissGroup(groupId); }

void AppContext::renameGroup(qint64 groupId, const QString& name) {
    core_->renameGroup(groupId, name.toStdString());
}

void AppContext::searchMessages(const QString& keyword) {
    core_->searchMessages(keyword.toStdString(), 50);
}

void AppContext::sendTyping(qint64 targetId, int targetType) {
    core_->sendTyping(targetId, targetType);
}

void AppContext::markRead(qint64 peerId, int targetType) {
    core_->markRead(peerId, targetType);
}

void AppContext::sendFile(qint64 targetId, int targetType, int msgType, const QString& path) {
    core_->sendFileMessage(targetId, targetType, msgType, path.toStdString());
}

void AppContext::downloadFile(const QString& fileId) {
    core_->downloadFile(fileId.toStdString());
}

void AppContext::addFriend(const QString& username) {
    core_->addFriend(username.toStdString(), "");
}

void AppContext::createGroup(const QString& name, const QVector<qint64>& memberIds) {
    std::vector<int64_t> ids;
    for (qint64 id : memberIds) ids.push_back(id);
    core_->createGroup(name.toStdString(), ids);
}

void AppContext::loadGroupMembers(qint64 groupId) { core_->loadGroupMembers(groupId); }

void AppContext::updateProfile(const QString& nickname, const QString& avatarPath,
                               const QString& oldPassword, const QString& newPassword) {
    if (avatarPath.isEmpty())
        core_->updateProfile(nickname.toStdString(), "", oldPassword.toStdString(),
                             newPassword.toStdString());
    else
        core_->updateProfileWithAvatarUpload(avatarPath.toStdString(), nickname.toStdString(),
                                             oldPassword.toStdString(),
                                             newPassword.toStdString());
}

void AppContext::logout() { core_->logout(); }

void AppContext::disconnectAll() { core_->stop(); }

// ---- IClientListener 回调（逻辑线程）----

void AppContext::onConnectionChanged(bool connected) { emit connectionChanged(connected); }

void AppContext::onLoginResult(int code, const std::string& msg, const im::UserInfo& me) {
    if (code == 0) {
        myId_ = me.id;
        me_ = convert(me);
        avatarById_.insert(me.id, QString::fromStdString(me.avatar));
    }
    emit loginResult(code, QString::fromStdString(msg), convert(me));
}

void AppContext::onRegisterResult(int code, const std::string& msg) {
    emit registerResult(code, QString::fromStdString(msg));
}

void AppContext::onContactsLoaded(const std::vector<im::BuddyInfo>& buddies,
                                  const std::vector<im::GroupInfo>& groups) {
    for (const auto& b : buddies)
        avatarById_.insert(b.user.id, QString::fromStdString(b.user.avatar));
    for (const auto& g : groups)
        for (const auto& m : g.members)
            avatarById_.insert(m.user.id, QString::fromStdString(m.user.avatar));
    QVector<QtBuddy> qb;
    qb.reserve(static_cast<int>(buddies.size()));
    for (const auto& b : buddies) qb.append(toQtBuddy(b));
    QVector<QtGroup> qg;
    qg.reserve(static_cast<int>(groups.size()));
    for (const auto& g : groups) qg.append(toQtGroup(g));
    emit contactsReady(qb, qg);
}

void AppContext::onSessionsLoaded(const std::vector<im::SessionInfo>& sessions) {
    QVector<QtSession> qs;
    qs.reserve(static_cast<int>(sessions.size()));
    for (const auto& s : sessions) qs.append(toQtSession(s));
    emit sessionsReady(qs);
}

void AppContext::onHistoryLoaded(int64_t targetId, int targetType,
                                 const std::vector<im::MessageInfo>& msgs) {
    QVector<QtMessage> qm;
    qm.reserve(static_cast<int>(msgs.size()));
    for (const auto& m : msgs) {
        QtMessage q = toQtMessage(m);
        q.direction = (q.fromId == myId_) ? 0 : 1;
        qm.append(q);
    }
    emit historyReady(targetId, targetType, qm);
}

void AppContext::onMessage(const im::MessageInfo& msg) { emit messageArrived(toQtMessage(msg)); }

void AppContext::onMessageSent(const im::MessageInfo& msg) { emit messageSent(toQtMessage(msg)); }

void AppContext::onMessageRecalled(int64_t msgId, int64_t targetId, int targetType) {
    emit messageRecalled(msgId, targetId, targetType);
}

void AppContext::onFriendDeleted(int64_t friendId, const std::string& name) {
    emit friendDeleted(friendId, QString::fromStdString(name));
}

void AppContext::onGroupUpdated(int64_t groupId) { emit groupUpdated(groupId); }

void AppContext::onSearchResults(const std::vector<im::MessageInfo>& msgs) {
    QVector<QtMessage> qm;
    qm.reserve(static_cast<int>(msgs.size()));
    for (const auto& m : msgs) {
        QtMessage q = toQtMessage(m);
        q.direction = (q.fromId == myId_) ? 0 : 1;
        qm.append(q);
    }
    emit searchResults(qm);
}

void AppContext::onTyping(int64_t fromId, int64_t targetId, int targetType) {
    emit typing(fromId, targetId, targetType);
}

void AppContext::onMessagesRead(int64_t peerId, int targetType) {
    emit messagesRead(peerId, targetType);
}

void AppContext::onReadReceipt(int64_t peerId, int targetType) {
    emit readReceipt(peerId, targetType);
}

void AppContext::onFileDownloaded(const std::string& fileId, const std::string& name,
                                  int64_t size, const std::string& mime,
                                  const std::vector<uint8_t>& data) {
    if (mime.rfind("image/", 0) == 0 && !data.empty()) {
        // 头像/图片：落盘，供下次启动直接加载
        QDir dir(avatarCacheDir());
        dir.mkpath(QStringLiteral("."));
        QFile f(dir.filePath(QString::fromStdString(fileId)));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<qint64>(data.size()));
            f.close();
        }
    }
    emit fileDownloaded(QString::fromStdString(fileId), QString::fromStdString(name), size,
                        QString::fromStdString(mime),
                        QByteArray(reinterpret_cast<const char*>(data.data()),
                                   static_cast<int>(data.size())));
}

QString AppContext::avatarCacheDir() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.colbt");
    return dir + QStringLiteral("/avatars");
}

void AppContext::onPresenceChanged(int64_t userId, bool online) {
    emit presenceChanged(userId, online);
}

void AppContext::onFriendAdded(const im::BuddyInfo& buddy) { emit friendAdded(toQtBuddy(buddy)); }

void AppContext::onGroupCreated(const im::GroupInfo& group) { emit groupCreated(toQtGroup(group)); }

void AppContext::onGroupMembersLoaded(int64_t groupId,
                                      const std::vector<im::MemberInfo>& members) {
    QVector<QtMember> qm;
    qm.reserve(static_cast<int>(members.size()));
    for (const auto& m : members) qm.append(toQtMember(m));
    emit groupMembersReady(groupId, qm);
}

void AppContext::onError(int code, const std::string& msg) {
    emit errorOccurred(code, QString::fromStdString(msg));
}

void AppContext::onProfileUpdated(int code, const std::string& msg, const im::UserInfo& me) {
    if (code == 0) {
        me_ = convert(me);
        avatarById_.insert(me.id, QString::fromStdString(me.avatar));
    }
    emit profileUpdated(code, QString::fromStdString(msg), convert(me));
}

void AppContext::onProfileChanged(int64_t userId, const std::string& nickname,
                                  const std::string& avatar) {
    avatarById_.insert(userId, QString::fromStdString(avatar));
    emit profileChanged(userId, QString::fromStdString(nickname),
                        QString::fromStdString(avatar));
}
