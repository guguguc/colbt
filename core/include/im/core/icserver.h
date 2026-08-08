#pragma once

#include <cstdint>
#include <string>

namespace im {

// =====================================================================
// 服务器逻辑核心（纯C++，不依赖Qt）
//
// 负责：
//  - 监听 TCP 端口，accept 新连接
//  - 为每个连接维护 读线程 + 写线程（见 server/session.cpp）
//  - 解析客户端命令并分发到 ServerCore::Impl 的各 onXxx 处理器
//  - 通过 Storage 落库、向在线用户推送实时事件
// =====================================================================
class ServerCore {
public:
    ServerCore();
    ~ServerCore();

    ServerCore(const ServerCore&) = delete;
    ServerCore& operator=(const ServerCore&) = delete;

    // 启动监听并进入事件循环（阻塞直到 stop() 被调用，如 Ctrl+C）
    int run(uint16_t port, const std::string& dbPath);

    // 优雅停止（供信号处理函数调用）
    void stop();

public:
    struct Impl;
    Impl* impl_; // 实现细节藏在这里，隔离网络/SQLite 等依赖
};

} // namespace im
