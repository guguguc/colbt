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

// 单个客户端连接会话：读线程 + 写线程（详见 session.cpp）
class Session {
public:
    Session(ServerCore::Impl* server, Socket&& sock);
    ~Session();

    void start(); // 启动读写线程
    void stop();  // 请求关闭（唤醒写线程 + 关 socket）
    void join();  // 等待线程退出

    void enqueue(const Packet& pkt); // 往发送队列投递一帧
    void handlePacket(const Packet& pkt); // 交给服务器核心处理

    int64_t userId() const { return userId_; }
    void setUserId(int64_t id) { userId_ = id; } // 登录成功后绑定
    bool isAlive() const { return alive_.load(); }

private:
    void readerLoop(); // 读线程：收帧 -> 分发
    void writerLoop(); // 写线程：从队列取帧发送
    void close();      // 立即关闭（幂等）

    ServerCore::Impl* server_;
    Socket sock_;
    int64_t userId_ = -1; // 未登录为 -1

    std::thread reader_;
    std::thread writer_;
    std::atomic<bool> alive_{false};

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::vector<Packet> outQueue_; // 待发送队列
    bool closeRequested_ = false;
};

// 服务器核心实现细节（Pimpl）：监听、会话注册表、各命令处理器、存储
class ServerCore::Impl {
public:
    Storage storage;     // SQLite 持久化
    std::string dbPath;

    Socket listenSock;   // 监听 socket
    std::thread acceptThread; // accept 线程
    std::atomic<bool> running{false};

    std::mutex sessionsMutex;
    std::map<int64_t, Session*> byUser; // userId -> session（在线用户）
    std::set<Session*> allSessions;     // 全部连接（含未登录）

    int run(uint16_t port, const std::string& dbPath); // 启动监听（阻塞）
    void stop(); // 停止并回收所有会话

    Session* sessionOf(int64_t userId);            // 取某用户的连接
    void registerSession(int64_t userId, Session* s); // 登录成功注册
    void unregisterSession(Session* s);            // 下线/断开注销
    void broadcastToUser(int64_t userId, const Packet& pkt); // 向在线用户推送
    std::vector<int64_t> onlineFriendsOf(int64_t userId);    // 在线好友列表

    void handleMessage(Session* s, const Packet& pkt); // 命令分发总入口
    void onSessionClosed(Session* s);                  // 连接关闭回调

    // 命令处理（每个对应一种请求，见 servercore.cpp）
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
    void onUpdateProfile(Session* s, const Packet& pkt);

    // 文件存储目录（<db目录>/files）
    std::string fileDir;
    void ensureFileDir();
    std::string filePathOf(const std::string& fileId) const;

    void pushError(Session* s, int code, const std::string& msg); // 回 CMD_ERROR
};

} // namespace im
