#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "im/core/icserver.h"
#include "im/core/protocol.h"
#include "im/core/types.h"
#include "net/socket.h"
#include "server/storage.h"

namespace im {

// 单个客户端连接会话：读线程 + 写线程
class Session {
public:
    Session(ServerCore::Impl* server, Socket&& sock);
    ~Session();

    void start();
    void stop();
    void join();

    void enqueue(const Packet& pkt);
    void handlePacket(const Packet& pkt);

    int64_t userId() const { return userId_; }
    void setUserId(int64_t id) { userId_ = id; }
    bool isAlive() const { return alive_.load(); }

private:
    void readerLoop();
    void writerLoop();
    void close();

    ServerCore::Impl* server_;
    Socket sock_;
    int64_t userId_ = -1;

    std::thread reader_;
    std::thread writer_;
    std::atomic<bool> alive_{false};

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::vector<Packet> outQueue_;
    bool closeRequested_ = false;
};

class ServerCore::Impl {
public:
    Storage storage;
    std::string dbPath;

    Socket listenSock;
    std::thread acceptThread;
    std::atomic<bool> running{false};

    std::mutex sessionsMutex;
    std::map<int64_t, Session*> byUser;       // userId -> session
    std::set<Session*> allSessions;           // 全部连接

    int run(uint16_t port, const std::string& dbPath);
    void stop();

    Session* sessionOf(int64_t userId);
    void registerSession(int64_t userId, Session* s);
    void unregisterSession(Session* s);
    void broadcastToUser(int64_t userId, const Packet& pkt);
    std::vector<int64_t> onlineFriendsOf(int64_t userId);

    void handleMessage(Session* s, const Packet& pkt);
    void onSessionClosed(Session* s);

    // 命令处理
    void onLogin(Session* s, const Packet& pkt);
    void onRegister(Session* s, const Packet& pkt);
    void onGetContacts(Session* s);
    void onAddFriend(Session* s, const Packet& pkt);
    void onCreateGroup(Session* s, const Packet& pkt);
    void onGetGroupMembers(Session* s, const Packet& pkt);
    void onGetSessions(Session* s);
    void onGetHistory(Session* s, const Packet& pkt);
    void onSendMessage(Session* s, const Packet& pkt);
    void onMarkRead(Session* s, const Packet& pkt);
    void onUploadFile(Session* s, const Packet& pkt);
    void onDownloadFile(Session* s, const Packet& pkt);
    void onRecallMsg(Session* s, const Packet& pkt);
    void onDeleteFriend(Session* s, const Packet& pkt);
    void onKickMember(Session* s, const Packet& pkt);
    void onLeaveGroup(Session* s, const Packet& pkt);
    void onDismissGroup(Session* s, const Packet& pkt);
    void onRenameGroup(Session* s, const Packet& pkt);
    void onSearchMsgs(Session* s, const Packet& pkt);
    void onTyping(Session* s, const Packet& pkt);

    // 文件存储目录（<db目录>/files）
    std::string fileDir;
    void ensureFileDir();
    std::string filePathOf(const std::string& fileId) const;

    void pushError(Session* s, int code, const std::string& msg);
};

} // namespace im
