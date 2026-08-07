#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "im/core/icclient.h"
#include "im/core/protocol.h"
#include "im/core/types.h"
#include "net/socket.h"

namespace im {

class ClientCore::Impl {
public:
    std::string host;
    uint16_t port = 0;
    IClientListener* listener = nullptr;

    Socket sock;
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};
    std::mutex sendMutex;
    std::vector<Packet> pendingQueue; // 未连接时缓存的待发命令

    // 上传完成后待发送的消息上下文（fileId 由服务端返回）
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

    bool start();
    void stop();
    void run();

    bool sendPacket(const Packet& pkt);
    void handlePacket(const Packet& pkt);
    void requestDownload(const std::string& fileId);
    // 解析内容 "fileId|name|size|mime"
    bool parseFileContent(const std::string& content, std::string& fileId);
    void autoDownloadImages(const std::vector<MessageInfo>& msgs);
};

} // namespace im
