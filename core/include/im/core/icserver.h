#pragma once

#include <cstdint>
#include <string>

namespace im {

// 服务器逻辑核心（纯C++，不依赖Qt）
class ServerCore {
public:
    ServerCore();
    ~ServerCore();

    ServerCore(const ServerCore&) = delete;
    ServerCore& operator=(const ServerCore&) = delete;

    // 启动监听，blocking（Ctrl+C 退出）
    int run(uint16_t port, const std::string& dbPath);

    // 优雅停止（供信号处理调用）
    void stop();

public:
    struct Impl;
    Impl* impl_;};

} // namespace im
