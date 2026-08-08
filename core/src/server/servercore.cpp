// 服务器核心实现
// 架构：
//   accept 线程：循环 accept 新连接，为每个连接创建 Session（读+写两线程）
//   Session：见 session.cpp，负责该连接的收帧分发与队列发送
//   Impl：持有会话注册表 byUser/allSessions、Storage 持久化、文件目录
// 所有 onXxx 处理器在对应 Session 的读线程中串行执行，
// 跨会话推送通过 broadcastToUser 写入目标会话的发送队列。
#include "server/servercore_impl.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include <winsock2.h>
#define MKDIR(p) _mkdir(p)
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) ::mkdir(p, 0755)
#endif

#include "protocol/codec.h"

namespace im {

ServerCore::ServerCore() : impl_(new Impl) {}
ServerCore::~ServerCore() {
    stop();
    delete impl_;
    impl_ = nullptr;
}

// 启动：打开数据库 -> 监听端口 -> 启动 accept 线程（阻塞直到 stop）
int ServerCore::run(uint16_t port, const std::string& dbPath) {
    impl_->dbPath = dbPath;
    if (!impl_->storage.open(dbPath)) {
        std::cerr << "[server] 打开数据库失败: " << dbPath << std::endl;
        return 1;
    }
    impl_->ensureFileDir();
    if (!impl_->listenSock.listen(port)) {
        std::cerr << "[server] 监听端口 " << port << " 失败" << std::endl;
        return 1;
    }
    impl_->running.store(true);
    std::cout << "[server] IM 服务器已启动: 端口 " << port << " 数据库 " << dbPath << std::endl;

    // accept 线程：每秒 select 一次检查新连接（带超时便于优雅退出）
    impl_->acceptThread = std::thread([this] {
        while (impl_->running.load()) {
            // select 带超时，便于停止
            fd_set rfds;
            FD_ZERO(&rfds);
            int fd = impl_->listenSock.fd();
            FD_SET(fd, &rfds);
            struct timeval tv{1, 0};
            int rc = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
            if (rc <= 0) continue;
            Socket s = impl_->listenSock.accept();
            if (!s.isValid()) continue;
            auto* sess = new Session(impl_, std::move(s));
            {
                std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
                impl_->allSessions.insert(sess);
            }
            sess->start();
            std::cout << "[server] 新连接 " << sess->userId() << " (fd=" << s.fd() << ")"
                      << std::endl;
        }
    });

    // 阻塞等待 stop() 被调用（running 是 atomic，无需加锁）
    while (impl_->running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    impl_->stop();
    return 0;
}

void ServerCore::stop() { impl_->stop(); }

// 优雅停止：停止 accept，关监听，停止并回收所有会话
void ServerCore::Impl::stop() {
    if (!running.exchange(false)) return;

    std::vector<Session*> sessions;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto* s : allSessions) sessions.push_back(s);
    }
    for (auto* s : sessions) s->stop();

    listenSock.close();
    if (acceptThread.joinable()) acceptThread.join();

    for (auto* s : sessions) {
        s->join();
        delete s;
    }
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        allSessions.clear();
        byUser.clear();
    }
    storage.close();
    std::cout << "[server] 服务器已停止" << std::endl;
}

// 查某用户的连接会话（不存在返回 nullptr）
Session* ServerCore::Impl::sessionOf(int64_t userId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = byUser.find(userId);
    return it == byUser.end() ? nullptr : it->second;
}

// 登录成功后注册会话；同一账号再次登录则顶掉旧会话（顶号）
void ServerCore::Impl::registerSession(int64_t userId, Session* s) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = byUser.find(userId);
    if (it != byUser.end() && it->second != s) {
        // 顶掉旧会话
        it->second->stop();
    }
    byUser[userId] = s;
    s->setUserId(userId);
}

// 会话关闭时从注册表移除
void ServerCore::Impl::unregisterSession(Session* s) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    if (s->userId() >= 0) {
        auto it = byUser.find(s->userId());
        if (it != byUser.end() && it->second == s) byUser.erase(it);
    }
    allSessions.erase(s);
}

// 向某用户推送一帧（该用户在线才发送）
void ServerCore::Impl::broadcastToUser(int64_t userId, const Packet& pkt) {
    if (Session* s = sessionOf(userId); s && s->isAlive()) s->enqueue(pkt);
}

// 用户所有在线好友的 userId 列表
std::vector<int64_t> ServerCore::Impl::onlineFriendsOf(int64_t userId) {
    std::vector<int64_t> out;
    auto friends = storage.getFriends(userId);
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (const auto& b : friends) {
            if (byUser.count(b.user.id)) out.push_back(b.user.id);
        }
    }
    return out;
}

// 连接关闭回调：注销会话并通知其在线好友"下线"
void ServerCore::Impl::onSessionClosed(Session* s) {
    int64_t uid = s->userId();
    bool wasKnown = false;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        wasKnown = allSessions.count(s) > 0;
    }
    if (!wasKnown) return;
    unregisterSession(s);
    if (uid >= 0) {
        std::cout << "[server] 用户 " << uid << " 下线" << std::endl;
        Packet pkt;
        pkt.cmd = CMD_PRESENCE_PUSH;
        Writer w;
        w.i64(uid);
        w.u8(0);
        pkt.body = w.data();
        for (auto f : onlineFriendsOf(uid)) broadcastToUser(f, pkt);
    }
}

// 命令分发总入口：按命令字调用对应处理器；解析异常回 CMD_ERROR
void ServerCore::Impl::handleMessage(Session* s, const Packet& pkt) {
    try {
        switch (pkt.cmd) {
        case CMD_HEARTBEAT:
            break;
        case CMD_LOGIN_REQ:
            onLogin(s, pkt);
            break;
        case CMD_REGISTER_REQ:
            onRegister(s, pkt);
            break;
        case CMD_LOGOUT_REQ:
            s->stop();
            break;
        case CMD_GET_CONTACTS_REQ:
            onGetContacts(s);
            break;
        case CMD_ADD_FRIEND_REQ:
            onAddFriend(s, pkt);
            break;
        case CMD_CREATE_GROUP_REQ:
            onCreateGroup(s, pkt);
            break;
        case CMD_GET_GROUP_MEMBERS_REQ:
            onGetGroupMembers(s, pkt);
            break;
        case CMD_GET_SESSIONS_REQ:
            onGetSessions(s);
            break;
        case CMD_GET_HISTORY_REQ:
            onGetHistory(s, pkt);
            break;
        case CMD_SEND_MSG_REQ:
            onSendMessage(s, pkt);
            break;
        case CMD_MARK_READ_REQ:
            onMarkRead(s, pkt);
            break;
        case CMD_UPLOAD_FILE_REQ:
            onUploadFile(s, pkt);
            break;
        case CMD_DOWNLOAD_FILE_REQ:
            onDownloadFile(s, pkt);
            break;
        case CMD_RECALL_MSG_REQ:
            onRecallMsg(s, pkt);
            break;
        case CMD_DELETE_FRIEND_REQ:
            onDeleteFriend(s, pkt);
            break;
        case CMD_KICK_MEMBER_REQ:
            onKickMember(s, pkt);
            break;
        case CMD_LEAVE_GROUP_REQ:
            onLeaveGroup(s, pkt);
            break;
        case CMD_DISMISS_GROUP_REQ:
            onDismissGroup(s, pkt);
            break;
        case CMD_RENAME_GROUP_REQ:
            onRenameGroup(s, pkt);
            break;
        case CMD_SEARCH_MSGS_REQ:
            onSearchMsgs(s, pkt);
            break;
        case CMD_TYPING_REQ:
            onTyping(s, pkt);
            break;
        case CMD_UPDATE_PROFILE_REQ:
            onUpdateProfile(s, pkt);
            break;
        default:
            pushError(s, 1, "未知命令");
            break;
    }
    } catch (const std::exception& e) {
        pushError(s, 2, std::string("协议解析错误: ") + e.what());
    }
}

// 已读上报：标记与对方会话中对方发来的消息为已读，并回执本人、通知对方
void ServerCore::Impl::onMarkRead(Session* s, const Packet& pkt) {
    int64_t peerId;
    int targetType;
    decodeMarkReadReq(pkt.body, peerId, targetType);

    int64_t uid = s->userId();
    int changed = 0;
    if (targetType == TARGET_FRIEND) {
        if (storage.areFriends(uid, peerId) || peerId == uid)
            changed = storage.markMessagesRead(uid, peerId, TARGET_FRIEND);
    } else if (targetType == TARGET_GROUP) {
        if (storage.isGroupMember(peerId, uid))
            changed = storage.markMessagesRead(uid, peerId, TARGET_GROUP);
    }

    // 回执给本人
    Packet resp;
    resp.cmd = CMD_MARK_READ_RESP;
    Writer w;
    w.i64(peerId);
    w.u8(static_cast<uint8_t>(targetType));
    resp.body = w.data();
    s->enqueue(resp);

    // 单聊：通知对方"我已读"，对方可在自己发出的消息上显示已读
    if (targetType == TARGET_FRIEND && changed > 0) {
        Packet push;
        push.cmd = CMD_READ_PUSH;
        Writer w2;
        w2.i64(uid); // 谁读的
        w2.u8(static_cast<uint8_t>(TARGET_FRIEND));
        push.body = w2.data();
        broadcastToUser(peerId, push);
    }
}

void ServerCore::Impl::ensureFileDir() {
    size_t slash = dbPath.find_last_of("/\\");
    std::string dir = slash == std::string::npos ? std::string(".") : dbPath.substr(0, slash);
    fileDir = dir + "/files";
    MKDIR(fileDir.c_str());
}

std::string ServerCore::Impl::filePathOf(const std::string& fileId) const {
    return fileDir + "/" + fileId;
}

// 上传文件：校验大小，写入磁盘并登记 files 表，返回 fileId
void ServerCore::Impl::onUploadFile(Session* s, const Packet& pkt) {
    Packet resp;
    resp.cmd = CMD_UPLOAD_FILE_RESP;
    Writer w;

    std::string name, mime;
    int64_t size;
    std::vector<uint8_t> data;
    try {
        decodeUploadFileReq(pkt.body, name, size, mime, data);
    } catch (...) {
        w.u8(1);
        w.str("解析失败");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }

    if (name.empty() || data.empty() ||
        static_cast<uint64_t>(data.size()) > kMaxFileSize) {
        w.u8(2);
        w.str("文件为空或超过大小限制");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }

    // 生成唯一 fileId
    std::string fileId = std::to_string(time(nullptr)) + "_" +
                         std::to_string(static_cast<unsigned long long>(
                             reinterpret_cast<uintptr_t>(s)));
    std::string path = filePathOf(fileId);
    {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            w.u8(3);
            w.str("存储失败");
            resp.body = w.data();
            s->enqueue(resp);
            return;
        }
        ofs.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
        ofs.close();
    }
    storage.createFile(fileId, name, size, mime);

    w.u8(0);
    w.str(fileId);
    resp.body = w.data();
    s->enqueue(resp);
    std::cout << "[server] 文件上传: " << name << " (" << data.size() << "B) -> " << fileId
              << std::endl;
}

// 下载文件：按 fileId 读磁盘并返回数据
void ServerCore::Impl::onDownloadFile(Session* s, const Packet& pkt) {
    Packet resp;
    resp.cmd = CMD_DOWNLOAD_FILE_RESP;
    Writer w;

    std::string fileId;
    try {
        decodeDownloadFileReq(pkt.body, fileId);
    } catch (...) {
        w.u8(1);
        w.str("解析失败");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }

    std::string name, mime;
    int64_t size;
    if (!storage.findFile(fileId, name, size, mime)) {
        w.u8(2);
        w.str("文件不存在");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }

    std::ifstream ifs(filePathOf(fileId), std::ios::binary);
    if (!ifs) {
        w.u8(3);
        w.str("读取失败");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());

    w.u8(0);
    w.str(fileId);
    w.str(name);
    w.i64(size);
    w.str(mime);
    w.bytes(data);
    resp.body = w.data();
    s->enqueue(resp);
    std::cout << "[server] 文件下载: " << fileId << " -> " << name << " (" << data.size()
              << "B)" << std::endl;
}

// 撤回消息：仅发送者 2 分钟内可撤回，成功后推送撤回给会话成员
void ServerCore::Impl::onRecallMsg(Session* s, const Packet& pkt) {
    int64_t msgId, targetId;
    int targetType;
    decodeRecallMsgReq(pkt.body, msgId, targetId, targetType);
    int64_t uid = s->userId();

    MessageInfo ref;
    if (!storage.findMessage(msgId, ref)) {
        pushError(s, 1, "消息不存在或已被撤回");
        return;
    }
    if (ref.fromId != uid) {
        pushError(s, 2, "只能撤回自己发送的消息");
        return;
    }
    if (!storage.recallMessage(msgId, uid)) {
        pushError(s, 3, "撤回失败");
        return;
    }

    // 通知相关方删除该消息
    Packet push;
    push.cmd = CMD_RECALL_MSG_PUSH;
    Writer w;
    w.i64(msgId);
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
    push.body = w.data();

    if (targetType == TARGET_GROUP) {
        auto members = storage.getGroupMembers(targetId);
        for (const auto& member : members)
            if (member.user.id != uid) broadcastToUser(member.user.id, push);
    } else {
        broadcastToUser(targetId, push);
    }
    s->enqueue(push); // 也通知本人
}

// 删除好友：双向删除并推送通知
void ServerCore::Impl::onDeleteFriend(Session* s, const Packet& pkt) {
    int64_t friendId;
    decodeDeleteFriendReq(pkt.body, friendId);
    int64_t uid = s->userId();
    storage.deleteFriend(uid, friendId);

    UserInfo me;
    storage.findUserById(uid, me);
    // 通知双方刷新联系人
    Packet p1;
    p1.cmd = CMD_DELETE_FRIEND_PUSH;
    {
        Writer w;
        w.i64(friendId);
        w.str(me.nickname);
        p1.body = w.data();
    }
    s->enqueue(p1);
    Packet p2;
    p2.cmd = CMD_DELETE_FRIEND_PUSH;
    {
        Writer w;
        w.i64(uid);
        w.str(me.nickname);
        p2.body = w.data();
    }
    broadcastToUser(friendId, p2);
    std::cout << "[server] 删除好友: " << uid << " <-> " << friendId << std::endl;
}

// 踢人：仅群主可踢，踢出后推送群更新
void ServerCore::Impl::onKickMember(Session* s, const Packet& pkt) {
    int64_t groupId, memberId;
    decodeKickMemberReq(pkt.body, groupId, memberId);
    int64_t uid = s->userId();
    GroupInfo g;
    if (storage.findGroupById(groupId, g) != groupId) {
        pushError(s, 1, "群不存在");
        return;
    }
    if (g.ownerId != uid) {
        pushError(s, 2, "仅群主可踢人");
        return;
    }
    storage.kickMember(groupId, memberId);
    // 通知所有成员刷新
    Packet push;
    push.cmd = CMD_GROUP_UPDATED_PUSH;
    Writer w;
    w.i64(groupId);
    push.body = w.data();
    for (const auto& member : g.members) broadcastToUser(member.user.id, push);
}

// 退群：移除群成员并推送群更新
void ServerCore::Impl::onLeaveGroup(Session* s, const Packet& pkt) {
    int64_t groupId;
    decodeGroupIdReq(pkt.body, groupId);
    int64_t uid = s->userId();
    GroupInfo g;
    if (storage.findGroupById(groupId, g) != groupId) {
        pushError(s, 1, "群不存在");
        return;
    }
    storage.leaveGroup(groupId, uid);
    Packet push;
    push.cmd = CMD_GROUP_UPDATED_PUSH;
    Writer w;
    w.i64(groupId);
    push.body = w.data();
    for (const auto& member : g.members) broadcastToUser(member.user.id, push);
}

// 解散群：仅群主可解散，移除所有成员并推送
void ServerCore::Impl::onDismissGroup(Session* s, const Packet& pkt) {
    int64_t groupId;
    decodeGroupIdReq(pkt.body, groupId);
    int64_t uid = s->userId();
    GroupInfo g;
    if (storage.findGroupById(groupId, g) != groupId) {
        pushError(s, 1, "群不存在");
        return;
    }
    if (g.ownerId != uid) {
        pushError(s, 2, "仅群主可解散群");
        return;
    }
    storage.dismissGroup(groupId);
    Packet push;
    push.cmd = CMD_GROUP_UPDATED_PUSH;
    Writer w;
    w.i64(groupId);
    push.body = w.data();
    for (const auto& member : g.members) broadcastToUser(member.user.id, push);
}

// 改群名：仅群主可改，推送群更新
void ServerCore::Impl::onRenameGroup(Session* s, const Packet& pkt) {
    int64_t groupId;
    std::string name;
    decodeRenameGroupReq(pkt.body, groupId, name);
    GroupInfo g;
    if (storage.findGroupById(groupId, g) != groupId) {
        pushError(s, 1, "群不存在");
        return;
    }
    if (name.empty() || name.size() > 32) {
        pushError(s, 2, "群名无效");
        return;
    }
    storage.renameGroup(groupId, name);
    Packet push;
    push.cmd = CMD_GROUP_UPDATED_PUSH;
    Writer w;
    w.i64(groupId);
    push.body = w.data();
    for (const auto& member : g.members) broadcastToUser(member.user.id, push);
}

// 搜索消息：在本人参与的所有会话中按关键词搜索
void ServerCore::Impl::onSearchMsgs(Session* s, const Packet& pkt) {
    std::string keyword;
    int limit;
    decodeSearchReq(pkt.body, keyword, limit);
    if (limit <= 0 || limit > 200) limit = 50;
    auto msgs = storage.searchMessages(s->userId(), keyword, limit);
    Packet resp;
    resp.cmd = CMD_SEARCH_MSGS_RESP;
    Writer w;
    w.u16(static_cast<uint16_t>(msgs.size()));
    for (const auto& m : msgs) writeMessage(w, m);
    resp.body = w.data();
    s->enqueue(resp);
}

// 输入中：把"正在输入"推送给会话对端/群成员
void ServerCore::Impl::onTyping(Session* s, const Packet& pkt) {
    int64_t targetId;
    int targetType;
    decodeTypingReq(pkt.body, targetId, targetType);
    int64_t uid = s->userId();
    Packet push;
    push.cmd = CMD_TYPING_PUSH;
    Writer w;
    w.i64(uid);
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
    push.body = w.data();

    if (targetType == TARGET_GROUP) {
        auto members = storage.getGroupMembers(targetId);
        for (const auto& member : members)
            if (member.user.id != uid) broadcastToUser(member.user.id, push);
    } else {
        broadcastToUser(targetId, push);
    }
}

// 修改资料：改密码需旧密码校验，成功后回传新资料并推送好友/群成员
void ServerCore::Impl::onUpdateProfile(Session* s, const Packet& pkt) {
    int64_t uid = s->userId();
    std::string nickname, avatar, oldPassword, newPassword;
    decodeUpdateProfileReq(pkt.body, nickname, avatar, oldPassword, newPassword);

    Packet resp;
    resp.cmd = CMD_UPDATE_PROFILE_RESP;
    Writer w;

    auto fail = [&](const std::string& msg) {
        w.u8(1);
        w.str(msg);
        resp.body = w.data();
        s->enqueue(resp);
    };

    if (!nickname.empty() && (nickname.size() > 32)) {
        fail("昵称过长(≤32字符)");
        return;
    }
    if (!avatar.empty() && avatar.size() > 128) {
        fail("头像标识过长");
        return;
    }
    if (!newPassword.empty() && newPassword.size() < 4) {
        fail("新密码至少4位");
        return;
    }

    std::string newPwdHash;
    if (!newPassword.empty()) {
        UserInfo cur;
        std::string curHash;
        if (!storage.findUserById(uid, cur) || !storage.findUserByName(cur.username, cur, curHash) ||
            curHash != sha256Hex(oldPassword)) {
            fail("旧密码错误");
            return;
        }
        newPwdHash = sha256Hex(newPassword);
    }

    std::string err;
    if (!storage.updateUser(uid, nickname, avatar, newPwdHash, err)) {
        fail("保存失败: " + err);
        return;
    }

    UserInfo me;
    if (!storage.findUserById(uid, me)) {
        fail("读取用户失败");
        return;
    }
    me.online = 1;

    w.u8(0);
    w.str("ok");
    writeUser(w, me);
    resp.body = w.data();
    s->enqueue(resp);
    std::cout << "[server] 用户 " << uid << " 更新资料" << std::endl;

    // 推送资料变更给在线好友 + 共同群成员（让客户端刷新列表/头像）
    Packet push;
    push.cmd = CMD_PROFILE_UPDATED_PUSH;
    Writer wp;
    encodeProfileUpdatedPush(wp, uid, me.nickname, me.avatar);
    push.body = wp.data();
    for (auto f : onlineFriendsOf(uid)) broadcastToUser(f, push);
    for (const auto& g : storage.getGroups(uid)) {
        for (const auto& m : g.members)
            if (m.user.id != uid) broadcastToUser(m.user.id, push);
    }
}

void ServerCore::Impl::pushError(Session* s, int code, const std::string& msg) {
    Packet pkt;
    pkt.cmd = CMD_ERROR;
    Writer w;
    w.u8(static_cast<uint8_t>(code));
    w.str(msg);
    pkt.body = w.data();
    s->enqueue(pkt);
}

// 登录：校验密码，注册会话（顶号），回传用户信息并同步在线状态
void ServerCore::Impl::onLogin(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    std::string username = r.str();
    std::string password = r.str();

    Packet resp;
    resp.cmd = CMD_LOGIN_RESP;
    Writer w;
    UserInfo u;
    std::string pwdHash;
    if (!storage.findUserByName(username, u, pwdHash) || pwdHash != sha256Hex(password)) {
        w.u8(1);
        w.str("用户名或密码错误");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    registerSession(u.id, s);
    u.online = 1;
    w.u8(0);
    w.str("ok");
    w.i64(u.id);
    w.str(u.username);
    w.str(u.nickname);
    w.str(u.avatar);
    w.u8(1);
    resp.body = w.data();
    s->enqueue(resp);
    std::cout << "[server] 用户登录: " << username << " (id=" << u.id << ")" << std::endl;

    // 上线通知：告知其在线好友 + 推送在线好友给自己
    for (auto f : onlineFriendsOf(u.id)) {
        Packet pToFriend;
        pToFriend.cmd = CMD_PRESENCE_PUSH;
        Writer wF;
        wF.i64(u.id);
        wF.u8(1);
        pToFriend.body = wF.data();
        broadcastToUser(f, pToFriend);

        Packet pToMe;
        pToMe.cmd = CMD_PRESENCE_PUSH;
        Writer wM;
        wM.i64(f);
        wM.u8(1);
        pToMe.body = wM.data();
        s->enqueue(pToMe);
    }
}

// 注册：创建用户（用户名唯一），回传结果
void ServerCore::Impl::onRegister(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    std::string username = r.str();
    std::string password = r.str();
    std::string nickname = r.str();
    if (nickname.empty()) nickname = username;

    Packet resp;
    resp.cmd = CMD_REGISTER_RESP;
    Writer w;
    if (username.empty() || username.size() > 32 || password.size() < 4) {
        w.u8(1);
        w.str("用户名不能为空(≤32字符)，密码至少4位");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    std::string err;
    int64_t id = storage.createUser(username, sha256Hex(password), nickname, err);
    if (id < 0) {
        w.u8(1);
        w.str(err.find("UNIQUE") != std::string::npos ? "用户名已被占用" : "注册失败");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    w.u8(0);
    w.str("注册成功");
    resp.body = w.data();
    s->enqueue(resp);
    std::cout << "[server] 新用户注册: " << username << " (id=" << id << ")" << std::endl;
}

// 拉取好友列表 + 群列表（含在线状态）
void ServerCore::Impl::onGetContacts(Session* s) {
    int64_t uid = s->userId();
    Packet resp;
    resp.cmd = CMD_GET_CONTACTS_RESP;
    Writer w;
    auto buddies = storage.getFriends(uid);
    w.u16(static_cast<uint16_t>(buddies.size()));
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto& b : buddies) {
            b.user.online = byUser.count(b.user.id) ? 1 : 0;
            writeUser(w, b.user);
            w.str(b.remark);
        }
    }
    auto groups = storage.getGroups(uid);
    w.u16(static_cast<uint16_t>(groups.size()));
    for (auto& g : groups) {
        w.i64(g.id);
        w.str(g.name);
        w.i64(g.ownerId);
        w.u16(static_cast<uint16_t>(g.members.size()));
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            for (auto& m : g.members) {
                m.user.online = byUser.count(m.user.id) ? 1 : 0;
                writeUser(w, m.user);
                w.str(m.groupNick);
            }
        }
    }
    resp.body = w.data();
    s->enqueue(resp);
}

// 添加好友：按用户名查找并建立双向好友关系，推送对方
void ServerCore::Impl::onAddFriend(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    std::string username = r.str();
    std::string remark = r.str();

    Packet resp;
    resp.cmd = CMD_ADD_FRIEND_RESP;
    Writer w;
    UserInfo target;
    std::string hash;
    if (!storage.findUserByName(username, target, hash)) {
        w.u8(2);
        w.str("用户不存在");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    if (target.id == s->userId()) {
        w.u8(3);
        w.str("不能添加自己为好友");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    storage.addFriend(s->userId(), target.id, remark);
    target.online = 0;
    if (Session* ts = sessionOf(target.id)) {
        target.online = 1;
        (void)ts;
    }
    w.u8(0);
    w.str("ok");
    writeUser(w, target);
    w.str(remark);
    resp.body = w.data();
    s->enqueue(resp);

    // 通知对方
    Packet push;
    push.cmd = CMD_FRIEND_ADDED_PUSH;
    Writer w2;
    UserInfo me;
    storage.findUserById(s->userId(), me);
    me.online = 1;
    writeUser(w2, me);
    w2.str("");
    push.body = w2.data();
    broadcastToUser(target.id, push);
}

// 创建群：建群 + 拉入成员 + 回传群信息
void ServerCore::Impl::onCreateGroup(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    std::string name = r.str();
    uint16_t n = r.u16();
    std::vector<int64_t> memberIds;
    for (uint16_t i = 0; i < n; ++i) memberIds.push_back(r.i64());

    int64_t owner = s->userId();
    Packet resp;
    resp.cmd = CMD_CREATE_GROUP_RESP;
    Writer w;
    std::string err;
    int64_t gid = storage.createGroup(name, owner, err);
    if (gid < 0) {
        w.u8(1);
        w.str("创建群失败");
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    storage.addGroupMember(gid, owner, "");
    for (auto id : memberIds) {
        if (id != owner) storage.addGroupMember(gid, id, "");
    }
    GroupInfo g;
    storage.findGroupById(gid, g);
    g.ownerId = owner;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto& m : g.members) m.user.online = byUser.count(m.user.id) ? 1 : 0;
    }
    w.u8(0);
    w.str("ok");
    w.i64(g.id);
    w.str(g.name);
    w.i64(g.ownerId);
    w.u16(static_cast<uint16_t>(g.members.size()));
    for (auto& m : g.members) {
        writeUser(w, m.user);
        w.str(m.groupNick);
    }
    resp.body = w.data();
    s->enqueue(resp);

    // 向群内广播一条系统消息
    UserInfo me;
    storage.findUserById(owner, me);
    std::string sys = "「" + me.nickname + "」创建了群聊 " + name;
    int64_t mid = storage.saveMessage(owner, gid, TARGET_GROUP, MSG_SYSTEM, sys, time(nullptr));
    Packet push;
    push.cmd = CMD_MSG_PUSH;
    Writer w2;
    MessageInfo m;
    m.id = mid;
    m.fromId = owner;
    m.targetId = gid;
    m.targetType = TARGET_GROUP;
    m.msgType = MSG_SYSTEM;
    m.content = sys;
    m.timestamp = time(nullptr);
    m.senderName = me.nickname;
    writeMessage(w2, m);
    push.body = w2.data();
    for (const auto& member : g.members) {
        if (member.user.id != owner) broadcastToUser(member.user.id, push);
    }
}

// 拉取群成员列表
void ServerCore::Impl::onGetGroupMembers(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    int64_t gid = r.i64();
    Packet resp;
    resp.cmd = CMD_GET_GROUP_MEMBERS_RESP;
    Writer w;
    auto members = storage.getGroupMembers(gid);
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto& m : members) m.user.online = byUser.count(m.user.id) ? 1 : 0;
    }
    w.i64(gid);
    w.u16(static_cast<uint16_t>(members.size()));
    for (auto& m : members) {
        writeUser(w, m.user);
        w.str(m.groupNick);
    }
    resp.body = w.data();
    s->enqueue(resp);
}

// 拉取最近会话列表（按最后消息时间倒序）
void ServerCore::Impl::onGetSessions(Session* s) {
    Packet resp;
    resp.cmd = CMD_GET_SESSIONS_RESP;
    Writer w;
    auto sessions = storage.getSessions(s->userId(), 100);
    w.u16(static_cast<uint16_t>(sessions.size()));
    for (auto& ss : sessions) {
        w.i64(ss.targetId);
        w.u8(static_cast<uint8_t>(ss.targetType));
        w.str(ss.title);
        w.str(ss.avatar);
        w.str(ss.lastContent);
        w.i64(ss.lastTime);
        w.u32(static_cast<uint32_t>(ss.unread));
    }
    resp.body = w.data();
    s->enqueue(resp);
}

// 拉取某个会话的历史消息（分页）
void ServerCore::Impl::onGetHistory(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    int64_t targetId = r.i64();
    int targetType = r.u8();
    int limit = r.u16();
    if (limit <= 0 || limit > 200) limit = 50;

    int64_t uid = s->userId();
    Packet resp;
    resp.cmd = CMD_GET_HISTORY_RESP;
    Writer w;
    if (targetType == TARGET_FRIEND && !storage.areFriends(uid, targetId) && targetId != uid) {
        w.i64(targetId);
        w.u8(static_cast<uint8_t>(targetType));
        w.u16(0);
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    if (targetType == TARGET_GROUP && !storage.isGroupMember(targetId, uid)) {
        w.i64(targetId);
        w.u8(static_cast<uint8_t>(targetType));
        w.u16(0);
        resp.body = w.data();
        s->enqueue(resp);
        return;
    }
    auto msgs = storage.getHistory(uid, targetId, targetType, limit, 0);
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
    w.u16(static_cast<uint16_t>(msgs.size()));
    for (auto& m : msgs) writeMessage(w, m);
    resp.body = w.data();
    s->enqueue(resp);
}

// 发送消息：落库并回执发送方，推送给单聊对方或群成员
void ServerCore::Impl::onSendMessage(Session* s, const Packet& pkt) {
    Reader r(pkt.body);
    int64_t targetId = r.i64();
    int targetType = r.u8();
    int msgType = r.u8();
    std::string content = r.str();
    int64_t replyToId = 0;
    if (!r.atEnd()) replyToId = r.i64(); // 兼容无引用字段的请求

    int64_t uid = s->userId();
    Packet ack;
    ack.cmd = CMD_SEND_MSG_RESP;
    Writer wa;

    if (content.empty()) {
        wa.i64(-1);
        wa.i64(0);
        wa.str("");
        ack.body = wa.data();
        s->enqueue(ack);
        return;
    }

    int64_t now = time(nullptr);
    bool allowed = false;
    if (targetType == TARGET_FRIEND) {
        allowed = storage.areFriends(uid, targetId) || targetId == uid;
    } else if (targetType == TARGET_GROUP) {
        allowed = storage.isGroupMember(targetId, uid);
    }
    if (!allowed) {
        wa.i64(-1);
        wa.i64(0);
        wa.str("");
        ack.body = wa.data();
        s->enqueue(ack);
        return;
    }

    int64_t mid = storage.saveMessage(uid, targetId, targetType, msgType, content, now, 0,
                                      replyToId);

    UserInfo me;
    storage.findUserById(uid, me);

    MessageInfo m;
    m.id = mid;
    m.fromId = uid;
    m.targetId = targetId;
    m.targetType = targetType;
    m.msgType = msgType;
    m.content = content;
    m.timestamp = now;
    m.senderName = me.nickname;
    m.replyToId = replyToId;
    if (replyToId > 0) {
        MessageInfo ref;
        if (storage.findMessage(replyToId, ref)) m.replyContent = ref.content;
    }

    writeMessage(wa, m);
    ack.body = wa.data();
    s->enqueue(ack);

    Packet push;
    push.cmd = CMD_MSG_PUSH;
    Writer wp;
    writeMessage(wp, m);
    push.body = wp.data();

    if (targetType == TARGET_FRIEND) {
        if (targetId != uid) broadcastToUser(targetId, push);
    } else {
        auto members = storage.getGroupMembers(targetId);
        for (const auto& member : members) {
            if (member.user.id != uid) broadcastToUser(member.user.id, push);
        }
    }
}

} // namespace im
