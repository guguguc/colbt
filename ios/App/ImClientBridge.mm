#import "ImClientBridge.h"

#include <cstdint>
#include <string>
#include <vector>

#include "im/core/icclient.h"

namespace {

NSString *toNS(const std::string &s) {
    return [NSString stringWithUTF8String:s.c_str()];
}

std::string toStd(NSString *s) {
    if (!s) return std::string();
    const char *c = s.UTF8String;
    return c ? std::string(c) : std::string();
}

ImUser *toImUser(const im::UserInfo &u) {
    return [[ImUser alloc] initWithId:u.id
                             username:toNS(u.username)
                             nickname:toNS(u.nickname)
                               avatar:toNS(u.avatar)
                               online:u.online];
}

ImBuddy *toImBuddy(const im::BuddyInfo &b) {
    return [[ImBuddy alloc] initWithUser:toImUser(b.user) remark:toNS(b.remark)];
}

ImMember *toImMember(const im::MemberInfo &m) {
    return [[ImMember alloc] initWithUser:toImUser(m.user) groupNick:toNS(m.groupNick)];
}

ImGroup *toImGroup(const im::GroupInfo &g) {
    NSMutableArray<ImMember *> *ms = [NSMutableArray arrayWithCapacity:g.members.size()];
    for (const auto &m : g.members) {
        [ms addObject:toImMember(m)];
    }
    return [[ImGroup alloc] initWithId:g.id name:toNS(g.name) ownerId:g.ownerId members:ms];
}

ImMessage *toImMessage(const im::MessageInfo &m) {
    return [[ImMessage alloc] initWithId:m.id
                                  fromId:m.fromId
                                targetId:m.targetId
                              targetType:m.targetType
                                 msgType:m.msgType
                                 content:toNS(m.content)
                               timestamp:m.timestamp
                               direction:m.direction
                              senderName:toNS(m.senderName)
                                    read:m.read
                                replyToId:m.replyToId
                            replyContent:toNS(m.replyContent)];
}

ImSession *toImSession(const im::SessionInfo &s) {
    return [[ImSession alloc] initWithTargetId:s.targetId
                                    targetType:s.targetType
                                         title:toNS(s.title)
                                        avatar:toNS(s.avatar)
                                   lastContent:toNS(s.lastContent)
                                      lastTime:s.lastTime
                                        unread:s.unread];
}

NSMutableArray<ImMessage *> *messagesToNS(const std::vector<im::MessageInfo> &msgs) {
    NSMutableArray<ImMessage *> *arr = [NSMutableArray arrayWithCapacity:msgs.size()];
    for (const auto &m : msgs) [arr addObject:toImMessage(m)];
    return arr;
}

// 重要：C++ 回调对象只在本次调用期间存活，必须在派发到主队列之前
// 全部转成 ObjC 对象，再在 block 中只捕获 ObjC 对象与基本类型。
void dispatchToMain(void (^block)(void)) {
    dispatch_async(dispatch_get_main_queue(), block);
}

// C++ 监听适配器：回调在 C++ 工作线程，统一转到主队列，再以 ObjC 协议转发给 UI 层
class BridgeListener final : public im::IClientListener {
public:
    void setDelegate(id<ImClientListener> d) { delegate_ = d; }

    void onConnectionChanged(bool connected) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onConnectionChanged:connected]; });
    }
    void onLoginResult(int code, const std::string &msg, const im::UserInfo &me) override {
        ImUser *u = toImUser(me);
        NSString *nsMsg = toNS(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onLoginResult:code message:nsMsg me:u]; });
    }
    void onRegisterResult(int code, const std::string &msg) override {
        NSString *nsMsg = toNS(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onRegisterResult:code message:nsMsg]; });
    }
    void onContactsLoaded(const std::vector<im::BuddyInfo> &buddies,
                          const std::vector<im::GroupInfo> &groups) override {
        NSMutableArray<ImBuddy *> *b = [NSMutableArray arrayWithCapacity:buddies.size()];
        for (const auto &x : buddies) [b addObject:toImBuddy(x)];
        NSMutableArray<ImGroup *> *g = [NSMutableArray arrayWithCapacity:groups.size()];
        for (const auto &x : groups) [g addObject:toImGroup(x)];
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onContactsLoaded:b groups:g]; });
    }
    void onSessionsLoaded(const std::vector<im::SessionInfo> &sessions) override {
        NSMutableArray<ImSession *> *s = [NSMutableArray arrayWithCapacity:sessions.size()];
        for (const auto &x : sessions) [s addObject:toImSession(x)];
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onSessionsLoaded:s]; });
    }
    void onHistoryLoaded(int64_t targetId, int targetType,
                         const std::vector<im::MessageInfo> &msgs) override {
        NSMutableArray<ImMessage *> *m = messagesToNS(msgs);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onHistoryLoaded:targetId targetType:targetType messages:m]; });
    }
    void onMessage(const im::MessageInfo &msg) override {
        ImMessage *m = toImMessage(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onMessage:m]; });
    }
    void onMessageSent(const im::MessageInfo &msg) override {
        ImMessage *m = toImMessage(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onMessageSent:m]; });
    }
    void onMessageRecalled(int64_t msgId, int64_t targetId, int targetType) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onMessageRecalled:msgId targetId:targetId targetType:targetType]; });
    }
    void onFriendDeleted(int64_t friendId, const std::string &name) override {
        NSString *nsName = toNS(name);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onFriendDeleted:friendId name:nsName]; });
    }
    void onGroupUpdated(int64_t groupId) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onGroupUpdated:groupId]; });
    }
    void onSearchResults(const std::vector<im::MessageInfo> &msgs) override {
        NSMutableArray<ImMessage *> *m = messagesToNS(msgs);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onSearchResults:m]; });
    }
    void onTyping(int64_t fromId, int64_t targetId, int targetType) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onTyping:fromId targetId:targetId targetType:targetType]; });
    }
    void onMessagesRead(int64_t peerId, int targetType) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onMessagesRead:peerId targetType:targetType]; });
    }
    void onReadReceipt(int64_t peerId, int targetType) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onReadReceipt:peerId targetType:targetType]; });
    }
    void onFileDownloaded(const std::string &fileId, const std::string &name, int64_t size,
                          const std::string &mime, const std::vector<uint8_t> &data) override {
        NSString *nsFileId = toNS(fileId);
        NSString *nsName = toNS(name);
        NSString *nsMime = toNS(mime);
        NSData *d = [NSData dataWithBytes:data.data() length:data.size()];
        __weak id<ImClientListener> l = delegate_;
        dispatchToMain(^{ [l onFileDownloaded:nsFileId name:nsName size:size mime:nsMime data:d]; });
    }
    void onPresenceChanged(int64_t userId, bool online) override {
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onPresenceChanged:userId online:online]; });
    }
    void onFriendAdded(const im::BuddyInfo &buddy) override {
        ImBuddy *b = toImBuddy(buddy);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onFriendAdded:b]; });
    }
    void onGroupCreated(const im::GroupInfo &group) override {
        ImGroup *g = toImGroup(group);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onGroupCreated:g]; });
    }
    void onGroupMembersLoaded(int64_t groupId,
                              const std::vector<im::MemberInfo> &members) override {
        NSMutableArray<ImMember *> *m = [NSMutableArray arrayWithCapacity:members.size()];
        for (const auto &x : members) [m addObject:toImMember(x)];
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onGroupMembersLoaded:groupId members:m]; });
    }
    void onError(int code, const std::string &msg) override {
        NSString *nsMsg = toNS(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onError:code message:nsMsg]; });
    }
    void onProfileUpdated(int code, const std::string &msg, const im::UserInfo &me) override {
        ImUser *u = toImUser(me);
        NSString *nsMsg = toNS(msg);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onProfileUpdated:code message:nsMsg me:u]; });
    }
    void onProfileChanged(int64_t userId, const std::string &nickname,
                          const std::string &avatar) override {
        NSString *nsNick = toNS(nickname);
        NSString *nsAvatar = toNS(avatar);
        __weak id<ImClientListener> d = delegate_;
        dispatchToMain(^{ [d onProfileChanged:userId nickname:nsNick avatar:nsAvatar]; });
    }

private:
    __weak id<ImClientListener> delegate_ = nil;
};

} // namespace

@implementation ImClientBridge {
    im::ClientCore *core_;
    BridgeListener listener_;
}

- (instancetype)initWithListener:(id<ImClientListener>)listener {
    self = [super init];
    if (self) {
        core_ = new im::ClientCore();
        listener_.setDelegate(listener);
    }
    return self;
}

- (void)dealloc {
    if (core_) {
        core_->stop();
        delete core_;
        core_ = nullptr;
    }
}

- (BOOL)start:(NSString *)host port:(int)port {
    if (!core_) return NO;
    return core_->start(toStd(host), static_cast<uint16_t>(port), &listener_);
}

- (void)stop { if (core_) core_->stop(); }

- (void)login:(NSString *)username password:(NSString *)password {
    if (core_) core_->login(toStd(username), toStd(password));
}
- (void)registerUser:(NSString *)username password:(NSString *)password nickname:(NSString *)nickname {
    if (core_) core_->registerUser(toStd(username), toStd(password), toStd(nickname));
}
- (void)logout { if (core_) core_->logout(); }
- (void)loadContacts { if (core_) core_->loadContacts(); }
- (void)loadSessions { if (core_) core_->loadSessions(); }
- (void)loadHistory:(int64_t)targetId targetType:(int)targetType limit:(int)limit {
    if (core_) core_->loadHistory(targetId, targetType, limit);
}
- (void)sendText:(int64_t)targetId targetType:(int)targetType text:(NSString *)text {
    if (core_) core_->sendText(targetId, targetType, toStd(text));
}
- (void)sendReply:(int64_t)targetId targetType:(int)targetType replyToId:(int64_t)replyToId text:(NSString *)text {
    if (core_) core_->sendReply(targetId, targetType, replyToId, toStd(text));
}
- (void)markRead:(int64_t)peerId targetType:(int)targetType {
    if (core_) core_->markRead(peerId, targetType);
}
- (void)recallMessage:(int64_t)messageId targetId:(int64_t)targetId targetType:(int)targetType {
    if (core_) core_->recallMessage(messageId, targetId, targetType);
}
- (void)deleteFriend:(int64_t)friendId { if (core_) core_->deleteFriend(friendId); }
- (void)addFriend:(NSString *)username { if (core_) core_->addFriend(toStd(username), ""); }
- (void)kickMember:(int64_t)groupId memberId:(int64_t)memberId {
    if (core_) core_->kickMember(groupId, memberId);
}
- (void)leaveGroup:(int64_t)groupId { if (core_) core_->leaveGroup(groupId); }
- (void)dismissGroup:(int64_t)groupId { if (core_) core_->dismissGroup(groupId); }
- (void)renameGroup:(int64_t)groupId name:(NSString *)name {
    if (core_) core_->renameGroup(groupId, toStd(name));
}
- (void)searchMessages:(NSString *)keyword {
    if (core_) core_->searchMessages(toStd(keyword), 50);
}
- (void)sendTyping:(int64_t)targetId targetType:(int)targetType {
    if (core_) core_->sendTyping(targetId, targetType);
}
- (void)sendImage:(int64_t)targetId targetType:(int)targetType path:(NSString *)path {
    if (core_) core_->sendFileMessage(targetId, targetType, im::MSG_IMAGE, toStd(path));
}
- (void)sendFile:(int64_t)targetId targetType:(int)targetType path:(NSString *)path {
    if (core_) core_->sendFileMessage(targetId, targetType, im::MSG_FILE, toStd(path));
}
- (void)downloadFile:(NSString *)fileId { if (core_) core_->downloadFile(toStd(fileId)); }
- (void)createGroup:(NSString *)name memberIds:(NSArray<NSNumber *> *)memberIds {
    if (!core_) return;
    std::vector<int64_t> ids;
    ids.reserve(memberIds.count);
    for (NSNumber *n in memberIds) ids.push_back(n.longLongValue);
    core_->createGroup(toStd(name), ids);
}
- (void)loadGroupMembers:(int64_t)groupId { if (core_) core_->loadGroupMembers(groupId); }

- (void)updateProfile:(NSString *)nickname avatarPath:(NSString *)avatarPath oldPassword:(NSString *)oldPassword newPassword:(NSString *)newPassword {
    if (!core_) return;
    if (avatarPath.length == 0) {
        core_->updateProfile(toStd(nickname), "", toStd(oldPassword), toStd(newPassword));
    } else {
        core_->updateProfileWithAvatarUpload(toStd(avatarPath), toStd(nickname),
                                             toStd(oldPassword), toStd(newPassword));
    }
}

@end
