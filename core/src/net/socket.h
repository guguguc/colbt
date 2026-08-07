#pragma once

#include <cstdint>
#include <string>

namespace im {

// 阻塞式 socket 封装（线程安全由调用方保证）
class Socket {
public:
    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool connect(const std::string& host, uint16_t port, int timeoutMs = 5000);

    // 服务端：监听端口，成功返回 true
    bool listen(uint16_t port, int backlog = 64);
    // 接受连接（阻塞）；返回新 socket
    Socket accept();

    // 立即中止收发（可唤醒阻塞中的 recv/send，通常由 close 内部调用）
    void shutdown();
    void close();

    bool isValid() const { return fd_ >= 0; }
    int fd() const { return fd_; }

    // 阻塞发送全部数据；失败返回 false
    bool sendAll(const void* data, size_t len);
    // 阻塞接收 len 字节；返回 1=成功 0=断开/致命错误 -1=超时
    int recvExact(void* data, size_t len);

    void setSendTimeout(int ms);
    void setRecvTimeout(int ms);
    void setNoDelay(bool on);

private:
    int fd_ = -1;
};

std::string lastSocketError();

} // namespace im
