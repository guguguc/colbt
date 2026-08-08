// socket 封装实现
// 职责：把 POSIX/Windows 两套 socket API 统一成阻塞式、线程安全的接口，
//       供 ClientCore/ServerCore 使用。Linux 用 MSG_NOSIGNAL，macOS/iOS
//       用 SO_NOSIGPIPE 防止写已关闭连接时产生 SIGPIPE 终止进程。
#include "net/socket.h"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace im {

#ifndef _WIN32
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define closesocket ::close
#endif

// 返回最近一次 socket 错误的可读描述（errno）
std::string lastSocketError() {
    char buf[256] = {0};
#ifdef _WIN32
    return "socket error code " + std::to_string(WSAGetLastError());
#elif defined(_GNU_SOURCE) && !defined(__APPLE__)
    // GNU strerror_r：返回指向错误消息的指针
    const char* msg = strerror_r(errno, buf, sizeof(buf));
    return msg ? msg : buf;
#else
    // XSI strerror_r：成功返回 0，消息写入 buf
    if (strerror_r(errno, buf, sizeof(buf)) == 0) return buf;
    return "unknown error errno " + std::to_string(errno);
#endif
}

Socket::Socket() = default;

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

Socket::~Socket() { close(); }

// 建立 TCP 连接（带超时）：
// 采用"非阻塞 connect + select 等待可写"实现可超时的连接，
// 避免默认阻塞 connect 长时间卡死（尤其目标主机不可达时）。
bool Socket::connect(const std::string& host, uint16_t port, int timeoutMs) {
    close();

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // 仅 IPv4
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return false;
    }

    int sock = -1;
    bool ok = false;
    for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
        sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;

#ifdef __APPLE__
        int set = 1;
        setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif

#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);   // 先切到非阻塞再 connect
#endif

        int rc = ::connect(sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen));
        if (rc != 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sock, &wfds);
                struct timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
                rc = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
                if (rc > 0) {
                    int soerr = 0;
                    int len = sizeof(soerr);
                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr),
                                   &len) == 0 && soerr == 0)
                        rc = 0;
                    else
                        rc = -1;
                } else
                    rc = -1;
            }
#else
            if (errno == EINPROGRESS) {
                // 连接进行中：等待可写，然后检查 SO_ERROR 确认是否成功
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sock, &wfds);
                struct timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
                rc = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
                if (rc > 0) {
                    int soerr = 0;
                    socklen_t len = sizeof(soerr);
                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &len) == 0 && soerr == 0)
                        rc = 0;
                    else
                        rc = -1;
                } else
                    rc = -1;
            }
#endif
        }

        if (rc == 0) {
            ok = true;
            break;
        }
        closesocket(sock);
        sock = -1;
    }
    freeaddrinfo(result);

    if (!ok) return false;

    // 恢复阻塞模式
#ifdef _WIN32
    u_long mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

    setNoDelay(true);   // TCP_NODELAY：小消息即时发送
    fd_ = sock;
    return true;
}

// 服务端：监听指定端口（绑定 0.0.0.0，所有网卡可接入）
bool Socket::listen(uint16_t port, int backlog) {
    close();
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(sock, backlog) != 0) {
        closesocket(sock);
        return false;
    }
    fd_ = sock;
    return true;
}

// 接受一个连接（阻塞），返回新 socket
Socket Socket::accept() {
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int c = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&peer), &len);
    Socket s;
    if (c >= 0) {
        s.fd_ = c;
#ifdef __APPLE__
        int set = 1;
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif
        s.setNoDelay(true);
    }
    return s;
}

// 立即中止收发（可唤醒同 fd 上阻塞的 recv/send）
void Socket::shutdown() {
    if (fd_ < 0) return;
#ifdef _WIN32
    ::shutdown(fd_, SD_BOTH);
#else
    ::shutdown(fd_, SHUT_RDWR);
#endif
}

void Socket::close() {
    if (fd_ >= 0) {
        // 先 shutdown 唤醒同 fd 上阻塞的 recv/send，再真正关闭
        shutdown();
        closesocket(fd_);
        fd_ = -1;
    }
}

// 阻塞发送全部数据；中途失败返回 false（调用方负责关闭连接）
bool Socket::sendAll(const void* data, size_t len) {
    if (fd_ < 0) return false;
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(fd_, p + sent, static_cast<int>(len - sent), 0);
#elif defined(MSG_NOSIGNAL)
        ssize_t n = ::send(fd_, p + sent, len - sent, MSG_NOSIGNAL);
#else
        ssize_t n = ::send(fd_, p + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// 阻塞接收恰好 len 字节；返回 1=成功 0=断开/致命错误 -1=超时
int Socket::recvExact(void* data, size_t len) {
    if (fd_ < 0) return 0;
    char* p = static_cast<char*>(data);
    size_t got = 0;
    while (got < len) {
#ifdef _WIN32
        int n = ::recv(fd_, p + got, static_cast<int>(len - got), 0);
#else
        ssize_t n = ::recv(fd_, p + got, len - got, 0);
#endif
        if (n == 0) return 0; // 对端关闭
        if (n < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) return -1; // 超时
            return 0;
#else
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1; // 超时
            return 0;
#endif
        }
        got += static_cast<size_t>(n);
    }
    return 1;
}

void Socket::setSendTimeout(int ms) {
    if (fd_ < 0) return;
#ifdef _WIN32
    int tv = ms;
    setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{ms / 1000, (ms % 1000) * 1000};
    setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::setRecvTimeout(int ms) {
    if (fd_ < 0) return;
#ifdef _WIN32
    int tv = ms;
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{ms / 1000, (ms % 1000) * 1000};
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::setNoDelay(bool on) {
    if (fd_ < 0) return;
    int flag = on ? 1 : 0;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));
}

} // namespace im
