// 客户端逻辑核心实现
// 线程模型：
//   start() 启动一个工作线程运行 run()——负责连接、收发、心跳；
//   handlePacket() 在逻辑线程内分发并回调 IClientListener；
//   公开 API 线程安全，未连接时的命令进入 pendingQueue 待连接后补发。
// 内部实现细节（Impl）见 clientcore_impl.h。
#include "client/clientcore_impl.h"

#include <chrono>
#include <fstream>

#include "protocol/codec.h"

namespace im {

ClientCore::ClientCore() : impl_(new Impl) {}
ClientCore::~ClientCore() {
    stop();
    delete impl_;
    impl_ = nullptr;
}

// 发送一帧数据；未连接或发送失败时缓存到待发队列（最多 128 条，避免无限堆积）
bool ClientCore::Impl::sendPacket(const Packet& pkt) {
    std::lock_guard<std::mutex> lock(sendMutex);
    if (!connected.load()) {
        // 连接未就绪：缓存，待连接建立后补发
        if (pendingQueue.size() < 128) pendingQueue.push_back(pkt);
        return false;
    }
    auto bytes = encodePacket(pkt);
    if (!sock.sendAll(bytes.data(), bytes.size())) {
        // 发送失败也缓存，等待重连/补发
        pendingQueue.push_back(pkt);
        return false;
    }
    return true;
}

// 工作线程主循环：连接 -> 补发队列 -> 循环（心跳 + 收帧 + 分发）
void ClientCore::Impl::run() {
    if (!sock.connect(host, port, 8000)) {
        if (listener) listener->onConnectionChanged(false);
        connected.store(false);
        running.store(false);
        return;
    }
    connected.store(true);

    // 补发连接建立前缓存的命令
    {
        std::lock_guard<std::mutex> lock(sendMutex);
        for (const auto& pkt : pendingQueue) {
            auto bytes = encodePacket(pkt);
            if (!sock.sendAll(bytes.data(), bytes.size())) break;
        }
        pendingQueue.clear();
    }

    if (listener) listener->onConnectionChanged(true);

    sock.setRecvTimeout(3000);
    sock.setSendTimeout(10000);

    auto lastBeat = std::chrono::steady_clock::now();
    auto lastData = lastBeat;
    uint8_t hdr[12];
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        // 心跳
        if (now - lastBeat > std::chrono::seconds(kHeartbeatIntervalSec)) {
            Packet hb;
            hb.cmd = CMD_HEARTBEAT;
            sendPacket(hb);
            lastBeat = now;
        }
        // 超过服务器判定时限未收到任何数据 -> 认为连接失效
        if (now - lastData > std::chrono::seconds(kServerDeadlineSec)) break;

        int rc = sock.recvExact(hdr, 12);
        if (rc <= 0) {
            if (rc == 0) break;        // 对端关闭/致命错误
            continue;                  // 超时：继续循环
        }
        uint32_t blen = 0;
        for (int i = 0; i < 4; ++i) blen |= static_cast<uint32_t>(hdr[8 + i]) << (i * 8);
        if (blen > kMaxBodyLen) break;

        Packet pkt;
        pkt.cmd = static_cast<uint16_t>(hdr[5]) | (static_cast<uint16_t>(hdr[6]) << 8);
        if (blen > 0) {
            pkt.body.resize(blen);
            if (sock.recvExact(pkt.body.data(), blen) != 1) break;
        }
        lastData = std::chrono::steady_clock::now();
        try {
            handlePacket(pkt);
        } catch (const std::exception& e) {
            if (listener) listener->onError(2, std::string("协议解析错误: ") + e.what());
        }
    }

    bool wasConnected = connected.exchange(false);
    if (wasConnected && listener) listener->onConnectionChanged(false);
}

// 按命令字分发服务器返回的帧，解码后调用对应回调（均在逻辑线程）
void ClientCore::Impl::handlePacket(const Packet& pkt) {
    switch (pkt.cmd) {
        case CMD_LOGIN_RESP: {
            int code;
            std::string msg;
            UserInfo me;
            decodeLoginResp(pkt.body, code, msg, me);
            if (code == 0) {
                myId.store(me.id);
                autoDownloadAvatar(me.avatar);
            }
            if (listener) listener->onLoginResult(code, msg, me);
            break;
        }
        case CMD_REGISTER_RESP: {
            int code;
            std::string msg;
            decodeRegisterResp(pkt.body, code, msg);
            if (listener) listener->onRegisterResult(code, msg);
            break;
        }
        case CMD_GET_CONTACTS_RESP: {
            std::vector<BuddyInfo> buddies;
            std::vector<GroupInfo> groups;
            decodeContactsResp(pkt.body, buddies, groups);
            for (const auto& b : buddies) autoDownloadAvatar(b.user.avatar);
            for (const auto& g : groups)
                for (const auto& m : g.members) autoDownloadAvatar(m.user.avatar);
            if (listener) listener->onContactsLoaded(buddies, groups);
            break;
        }
        case CMD_GET_SESSIONS_RESP: {
            std::vector<SessionInfo> sessions;
            decodeSessionsResp(pkt.body, sessions);
            if (listener) listener->onSessionsLoaded(sessions);
            break;
        }
        case CMD_GET_HISTORY_RESP: {
            int64_t targetId;
            int targetType;
            std::vector<MessageInfo> msgs;
            decodeHistoryResp(pkt.body, targetId, targetType, msgs);
            autoDownloadImages(msgs);
            if (listener) listener->onHistoryLoaded(targetId, targetType, msgs);
            break;
        }
        case CMD_SEND_MSG_RESP: {
            MessageInfo msg;
            decodeSendMsgResp(pkt.body, msg);
            msg.direction = DIRECTION_OUT;
            if (msg.msgType == MSG_IMAGE) {
                std::string fileId;
                if (parseFileContent(msg.content, fileId)) requestDownload(fileId);
            }
            if (listener) listener->onMessageSent(msg);
            break;
        }
        case CMD_MSG_PUSH: {
            MessageInfo msg;
            decodeMsgPush(pkt.body, msg);
            msg.direction = DIRECTION_IN;
            if (msg.msgType == MSG_IMAGE) {
                std::string fileId;
                if (parseFileContent(msg.content, fileId)) requestDownload(fileId);
            }
            if (listener) listener->onMessage(msg);
            break;
        }
        case CMD_PRESENCE_PUSH: {
            int64_t userId;
            bool online;
            decodePresencePush(pkt.body, userId, online);
            if (listener) listener->onPresenceChanged(userId, online);
            break;
        }
        case CMD_RECALL_MSG_PUSH: {
            int64_t msgId, targetId;
            int targetType;
            decodeRecallMsgPush(pkt.body, msgId, targetId, targetType);
            if (listener) listener->onMessageRecalled(msgId, targetId, targetType);
            break;
        }
        case CMD_DELETE_FRIEND_PUSH: {
            int64_t friendId;
            std::string name;
            decodeDeleteFriendPush(pkt.body, friendId, name);
            if (listener) listener->onFriendDeleted(friendId, name);
            break;
        }
        case CMD_GROUP_UPDATED_PUSH: {
            int64_t groupId;
            decodeGroupUpdatedPush(pkt.body, groupId);
            if (listener) listener->onGroupUpdated(groupId);
            break;
        }
        case CMD_SEARCH_MSGS_RESP: {
            std::vector<MessageInfo> msgs;
            decodeSearchResp(pkt.body, msgs);
            if (listener) listener->onSearchResults(msgs);
            break;
        }
        case CMD_TYPING_PUSH: {
            int64_t fromId, targetId;
            int targetType;
            decodeTypingPush(pkt.body, fromId, targetId, targetType);
            if (listener) listener->onTyping(fromId, targetId, targetType);
            break;
        }
        case CMD_UPDATE_PROFILE_RESP: {
            int code;
            std::string msg;
            UserInfo me;
            decodeUpdateProfileResp(pkt.body, code, msg, me);
            if (code == 0) {
                myId.store(me.id);
                autoDownloadAvatar(me.avatar);
            }
            if (listener) listener->onProfileUpdated(code, msg, me);
            break;
        }
        case CMD_PROFILE_UPDATED_PUSH: {
            int64_t userId;
            std::string nickname, avatar;
            decodeProfileUpdatedPush(pkt.body, userId, nickname, avatar);
            autoDownloadAvatar(avatar);
            if (listener) listener->onProfileChanged(userId, nickname, avatar);
            break;
        }
        case CMD_MARK_READ_RESP: {
            int64_t peerId;
            int targetType;
            decodeReadPush(pkt.body, peerId, targetType);
            if (listener) listener->onMessagesRead(peerId, targetType);
            break;
        }
        case CMD_READ_PUSH: {
            int64_t peerId;
            int targetType;
            decodeReadPush(pkt.body, peerId, targetType);
            if (listener) listener->onReadReceipt(peerId, targetType);
            break;
        }
        case CMD_UPLOAD_FILE_RESP: {
            int code;
            std::string fileId;
            decodeUploadFileResp(pkt.body, code, fileId);
            {
                std::lock_guard<std::mutex> lock(profileMutex);
                if (pendingProfile.waitingUpload) {
                    PendingProfile pp = pendingProfile;
                    pendingProfile = {};
                    if (code != 0) {
                        if (listener) listener->onError(code, "头像上传失败");
                    } else {
                        // 头像上传成功：补发资料更新请求
                        Writer w;
                        encodeUpdateProfileReq(w, pp.nickname, fileId, pp.oldPassword,
                                               pp.newPassword);
                        Packet p;
                        p.cmd = CMD_UPDATE_PROFILE_REQ;
                        p.body = w.data();
                        sendPacket(p);
                    }
                    break;
                }
            }
            PendingUpload up;
            {
                std::lock_guard<std::mutex> lock(uploadMutex);
                up = pendingUpload;
            }
            if (code != 0) {
                if (listener) listener->onError(code, "文件上传失败");
                break;
            }
            // 上传成功后发送携带文件引用的消息
            std::string name = up.name;
            for (auto& ch : name)
                if (ch == '|') ch = '_';
            std::string content = fileId + "|" + name + "|" + std::to_string(up.size) + "|" +
                                  up.mime;
            Writer w;
            encodeSendMsgReq(w, up.targetId, up.targetType, up.msgType, content);
            Packet msg;
            msg.cmd = CMD_SEND_MSG_REQ;
            msg.body = w.data();
            sendPacket(msg);
            break;
        }
        case CMD_DOWNLOAD_FILE_RESP: {
            int code;
            std::string fileId, name, mime;
            int64_t size;
            std::vector<uint8_t> data;
            decodeDownloadFileResp(pkt.body, code, fileId, name, size, mime, data);
            if (code != 0) {
                {
                    std::lock_guard<std::mutex> lock(avatarMutex);
                    avatarRequests.erase(fileId);
                }
                if (listener) listener->onError(code, "文件下载失败");
                break;
            }
            if (listener) listener->onFileDownloaded(fileId, name, size, mime, data);
            break;
        }
        case CMD_ADD_FRIEND_RESP: {
            int code;
            std::string msg;
            BuddyInfo buddy;
            decodeAddFriendResp(pkt.body, code, msg, buddy);
            if (code != 0) {
                if (listener) listener->onError(code, msg);
            } else {
                if (listener) listener->onFriendAdded(buddy);
            }
            break;
        }
        case CMD_FRIEND_ADDED_PUSH: {
            BuddyInfo buddy;
            decodeFriendAddedPush(pkt.body, buddy);
            if (listener) listener->onFriendAdded(buddy);
            break;
        }
        case CMD_CREATE_GROUP_RESP: {
            int code;
            std::string msg;
            GroupInfo group;
            decodeCreateGroupResp(pkt.body, code, msg, group);
            if (code != 0) {
                if (listener) listener->onError(code, msg);
            } else {
                if (listener) listener->onGroupCreated(group);
            }
            break;
        }
        case CMD_GET_GROUP_MEMBERS_RESP: {
            int64_t groupId;
            std::vector<MemberInfo> members;
            decodeGroupMembersResp(pkt.body, groupId, members);
            for (const auto& m : members) autoDownloadAvatar(m.user.avatar);
            if (listener) listener->onGroupMembersLoaded(groupId, members);
            break;
        }
        case CMD_ERROR: {
            int code;
            std::string msg;
            decodeAck(pkt.body, code, msg);
            if (listener) listener->onError(code, msg);
            break;
        }
        default:
            break;
    }
}

bool ClientCore::Impl::start() {
    if (running.load()) return false;
    running.store(true);
    worker = std::thread([this] { run(); });
    return true;
}

void ClientCore::Impl::stop() {
    if (!running.load()) return;
    running.store(false);
    sock.close();
    if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(sendMutex);
        pendingQueue.clear();
    }
    connected.store(false);
}

// ---- ClientCore 公共接口 ----
// 以下方法均为"编码请求帧 + sendPacket"的模式：
//   1. 用 codec 的 encodeXxxReq 把参数写入 Writer
//   2. 设置命令字 pkt.cmd
//   3. 交给 sendPacket（未连接时自动进入待发队列，连接后补发）

// 启动工作线程并连接；内部先 stop() 清空旧状态再建新连接
bool ClientCore::start(const std::string& host, uint16_t port, IClientListener* listener) {
    stop();
    // 每次重新连接都允许重新下载头像；头像图片缓存由 UI 层维护，不能依赖旧连接的请求状态
    {
        std::lock_guard<std::mutex> lock(impl_->avatarMutex);
        impl_->avatarRequests.clear();
    }
    impl_->host = host;
    impl_->port = port;
    impl_->listener = listener;
    impl_->myId.store(0);
    return impl_->start();
}

void ClientCore::stop() { impl_->stop(); }

bool ClientCore::isConnected() const { return impl_->connected.load(); }

void ClientCore::login(const std::string& username, const std::string& password) {
    Writer w;
    encodeLoginReq(w, username, password);
    Packet pkt;
    pkt.cmd = CMD_LOGIN_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::registerUser(const std::string& username, const std::string& password,
                              const std::string& nickname) {
    Writer w;
    encodeRegisterReq(w, username, password, nickname);
    Packet pkt;
    pkt.cmd = CMD_REGISTER_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::logout() {
    Packet pkt;
    pkt.cmd = CMD_LOGOUT_REQ;
    impl_->sendPacket(pkt);
}

void ClientCore::loadContacts() {
    Writer w;
    encodeSimpleReq(w);
    Packet pkt;
    pkt.cmd = CMD_GET_CONTACTS_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::loadSessions() {
    Writer w;
    encodeSimpleReq(w);
    Packet pkt;
    pkt.cmd = CMD_GET_SESSIONS_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::loadHistory(int64_t targetId, int targetType, int limit) {
    Writer w;
    encodeHistoryReq(w, targetId, targetType, limit);
    Packet pkt;
    pkt.cmd = CMD_GET_HISTORY_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::sendText(int64_t targetId, int targetType, const std::string& text) {
    sendReply(targetId, targetType, 0, text);
}

void ClientCore::sendReply(int64_t targetId, int targetType, int64_t replyToId,
                           const std::string& text) {
    Writer w;
    encodeSendMsgReq(w, targetId, targetType, MSG_TEXT, text, replyToId);
    Packet pkt;
    pkt.cmd = CMD_SEND_MSG_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::recallMessage(int64_t msgId, int64_t targetId, int targetType) {
    Writer w;
    encodeRecallMsgReq(w, msgId, targetId, targetType);
    Packet pkt;
    pkt.cmd = CMD_RECALL_MSG_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::deleteFriend(int64_t friendId) {
    Writer w;
    encodeDeleteFriendReq(w, friendId);
    Packet pkt;
    pkt.cmd = CMD_DELETE_FRIEND_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::kickMember(int64_t groupId, int64_t memberId) {
    Writer w;
    encodeKickMemberReq(w, groupId, memberId);
    Packet pkt;
    pkt.cmd = CMD_KICK_MEMBER_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::leaveGroup(int64_t groupId) {
    Writer w;
    encodeGroupIdReq(w, groupId);
    Packet pkt;
    pkt.cmd = CMD_LEAVE_GROUP_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::dismissGroup(int64_t groupId) {
    Writer w;
    encodeGroupIdReq(w, groupId);
    Packet pkt;
    pkt.cmd = CMD_DISMISS_GROUP_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::renameGroup(int64_t groupId, const std::string& name) {
    Writer w;
    encodeRenameGroupReq(w, groupId, name);
    Packet pkt;
    pkt.cmd = CMD_RENAME_GROUP_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::searchMessages(const std::string& keyword, int limit) {
    Writer w;
    encodeSearchReq(w, keyword, limit);
    Packet pkt;
    pkt.cmd = CMD_SEARCH_MSGS_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::sendTyping(int64_t targetId, int targetType) {
    Writer w;
    encodeTypingReq(w, targetId, targetType);
    Packet pkt;
    pkt.cmd = CMD_TYPING_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::markRead(int64_t peerId, int targetType) {
    Writer w;
    encodeMarkReadReq(w, peerId, targetType);
    Packet pkt;
    pkt.cmd = CMD_MARK_READ_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

namespace {
std::string baseName(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}
std::string mimeFor(const std::string& name) {
    size_t p = name.find_last_of('.');
    std::string ext = p == std::string::npos ? "" : name.substr(p);
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".webp") return "image/webp";
    if (ext == ".txt") return "text/plain";
    if (ext == ".pdf") return "application/pdf";
    return "application/octet-stream";
}
} // namespace

bool ClientCore::Impl::parseFileContent(const std::string& content, std::string& fileId) {
    size_t p = content.find('|');
    if (p == std::string::npos) return false;
    fileId = content.substr(0, p);
    return !fileId.empty();
}

void ClientCore::Impl::requestDownload(const std::string& fileId) {
    if (fileId.empty()) return;
    Writer w;
    encodeDownloadFileReq(w, fileId);
    Packet pkt;
    pkt.cmd = CMD_DOWNLOAD_FILE_REQ;
    pkt.body = w.data();
    sendPacket(pkt);
}

void ClientCore::Impl::autoDownloadAvatar(const std::string& fileId) {
    if (fileId.empty()) return;
    {
        std::lock_guard<std::mutex> lock(avatarMutex);
        if (!avatarRequests.insert(fileId).second) return; // 已请求过
    }
    requestDownload(fileId);
}

void ClientCore::Impl::autoDownloadImages(const std::vector<MessageInfo>& msgs) {
    for (const auto& m : msgs) {
        if (m.msgType != MSG_IMAGE) continue;
        std::string fileId;
        if (parseFileContent(m.content, fileId)) requestDownload(fileId);
    }
}

void ClientCore::sendFileMessage(int64_t targetId, int targetType, int msgType,
                                 const std::string& path) {
    // 文件消息流程：先读文件并上传，收到 CMD_UPLOAD_FILE_RESP 拿到 fileId
    // 后，再把 "fileId|name|size|mime" 作为消息内容发出（见 handlePacket）。
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        impl_->listener->onError(10, "无法读取文件: " + path);
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    if (data.empty()) {
        impl_->listener->onError(11, "文件为空");
        return;
    }
    if (data.size() > kMaxFileSize) {
        impl_->listener->onError(12, "文件超过 32MB 限制");
        return;
    }

    std::string name = baseName(path);
    {
        std::lock_guard<std::mutex> lock(impl_->uploadMutex);
        impl_->pendingUpload = {targetId, targetType, msgType, name,
                                static_cast<int64_t>(data.size()), mimeFor(name), path};
    }
    Writer w;
    encodeUploadFileReq(w, name, static_cast<int64_t>(data.size()), mimeFor(name), data);
    Packet pkt;
    pkt.cmd = CMD_UPLOAD_FILE_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::downloadFile(const std::string& fileId) {
    if (fileId.empty()) return;
    Writer w;
    encodeDownloadFileReq(w, fileId);
    Packet pkt;
    pkt.cmd = CMD_DOWNLOAD_FILE_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::addFriend(const std::string& username, const std::string& remark) {
    Writer w;
    encodeAddFriendReq(w, username, remark);
    Packet pkt;
    pkt.cmd = CMD_ADD_FRIEND_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::createGroup(const std::string& name, const std::vector<int64_t>& memberIds) {
    Writer w;
    encodeCreateGroupReq(w, name, memberIds);
    Packet pkt;
    pkt.cmd = CMD_CREATE_GROUP_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::loadGroupMembers(int64_t groupId) {
    Writer w;
    encodeGroupMembersReq(w, groupId);
    Packet pkt;
    pkt.cmd = CMD_GET_GROUP_MEMBERS_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::updateProfile(const std::string& nickname, const std::string& avatar,
                               const std::string& oldPassword, const std::string& newPassword) {
    {
        std::lock_guard<std::mutex> lock(impl_->profileMutex);
        impl_->pendingProfile = {};
    }
    Writer w;
    encodeUpdateProfileReq(w, nickname, avatar, oldPassword, newPassword);
    Packet pkt;
    pkt.cmd = CMD_UPDATE_PROFILE_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

void ClientCore::updateProfileWithAvatarUpload(const std::string& path,
                                               const std::string& nickname,
                                               const std::string& oldPassword,
                                               const std::string& newPassword) {
    // 带头像的资料修改：先上传头像文件，上传成功后（handlePacket 里
    // 检测到 pendingProfile.waitingUpload）自动补发 CMD_UPDATE_PROFILE_REQ，
    // 把 fileId 填进 avatar 字段。
    if (path.empty()) {
        updateProfile(nickname, "", oldPassword, newPassword);
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        if (impl_->listener) impl_->listener->onError(10, "无法读取头像文件: " + path);
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    if (data.empty() || data.size() > kMaxFileSize) {
        if (impl_->listener) impl_->listener->onError(12, "头像文件为空或超过 32MB 限制");
        return;
    }
    std::string name = "avatar_" + std::to_string(impl_->myId.load()) + ".img";
    {
        std::lock_guard<std::mutex> lock(impl_->profileMutex);
        impl_->pendingProfile = {true, nickname, oldPassword, newPassword};
    }
    Writer w;
    encodeUploadFileReq(w, name, static_cast<int64_t>(data.size()), "image/*", data);
    Packet pkt;
    pkt.cmd = CMD_UPLOAD_FILE_REQ;
    pkt.body = w.data();
    impl_->sendPacket(pkt);
}

} // namespace im
