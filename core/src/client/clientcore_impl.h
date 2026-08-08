#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "im/core/icclient.h"
#include "im/core/protocol.h"
#include "im/core/types.h"
#include "net/socket.h"

namespace im {

// ClientCore 的实现细节（Pimpl 手法）：把网络、线程、状态都藏在 .cpp 里，
// 公共头文件只暴露不透明的 Impl*，降低头文件依赖。
class ClientCore::Impl {
public:
    std::string host;             // 服务器地址
    uint16_t port = 0;            // 服务器端口
    IClientListener* listener = nullptr; // 回调接口（UI 桥接层实现）

    Socket sock;                  // TCP 连接
    std::thread worker;           // 工作线程（连接/收发/心跳）
    std::atomic<bool> running{false};   // 线程运行标记（stop 时置 false）
    std::atomic<bool> connected{false}; // 是否已建立连接
    std::mutex sendMutex;                // 保护 pendingQueue 与发送
    std::vector<Packet> pendingQueue;    // 未连接时缓存的待发命令

    // 文件消息：上传完成后待发送的消息上下文（fileId 由服务端返回）
    struct PendingUpload {
        int64_t targetId = 0;
        int targetType = 0;
        int msgType = 0;
        std::string name;
        int64_t size = 0;
        std::string mime;
        std::string path;
    };
    std::mutex uploadMutex;
    PendingUpload pendingUpload;

    // 修改资料时若需上传头像：先传头像，成功后再发更新请求
    struct PendingProfile {
        bool waitingUpload = false;
        std::string nickname;
        std::string oldPassword;
        std::string newPassword;
    };
    std::mutex profileMutex;
    PendingProfile pendingProfile;

    // 服务器分配的用户ID（登录成功后填充）
    std::atomic<int64_t> myId{0};

    // 已请求过下载的头像 fileId（去重，避免重复下载）
    std::mutex avatarMutex;
    std::set<std::string> avatarRequests;

    bool start();                // 启动工作线程
    void stop();                 // 停止线程并关闭连接
    void run();                  // 工作线程主循环

    bool sendPacket(const Packet& pkt); // 发送一帧（未连接则缓存）
    void handlePacket(const Packet& pkt); // 分发并处理收到的帧
    void requestDownload(const std::string& fileId); // 请求下载文件
    void autoDownloadAvatar(const std::string& fileId); // 按需下载头像（去重）
    // 解析内容 "fileId|name|size|mime" 取 fileId
    bool parseFileContent(const std::string& content, std::string& fileId);
    void autoDownloadImages(const std::vector<MessageInfo>& msgs); // 自动下载图片消息
};

} // namespace im
