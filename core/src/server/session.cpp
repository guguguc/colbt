// 连接会话实现：每个客户端连接对应一个 Session，拥有 读线程 + 写线程。
//   - 读线程（readerLoop）：阻塞收帧 -> 交给 server_->handleMessage 处理
//   - 写线程（writerLoop）：阻塞等待发送队列 -> 批量发送
// 关闭流程：stop()/close() 置 alive_=false 并关闭 socket，
// 唤醒阻塞的读写线程，join() 等待二者退出。
#include "server/servercore_impl.h"

#include <chrono>

#include "protocol/codec.h"

namespace im {

Session::Session(ServerCore::Impl* server, Socket&& sock)
    : server_(server), sock_(std::move(sock)) {}

Session::~Session() {
    stop();
    join();
}

// 启动读/写两个线程
void Session::start() {
    alive_.store(true);
    reader_ = std::thread([this] { readerLoop(); });
    writer_ = std::thread([this] { writerLoop(); });
}

// 请求关闭：唤醒写线程并关闭 socket
void Session::stop() {
    if (!alive_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        closeRequested_ = true;
    }
    queueCv_.notify_all();
    sock_.close();
}

// 等待读写线程退出
void Session::join() {
    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();
}

// 往发送队列尾部加入一帧（其他线程如消息推送可直接调用）
void Session::enqueue(const Packet& pkt) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        outQueue_.push_back(pkt);
    }
    queueCv_.notify_one();
}

// 读线程主循环：收 12 字节帧头 -> 收正文 -> 分发处理
void Session::readerLoop() {
    sock_.setRecvTimeout(10000);
    uint8_t hdr[12];
    auto lastActive = std::chrono::steady_clock::now();

    while (alive_.load()) {
        int rc = sock_.recvExact(hdr, 12);
        if (rc <= 0) {
            if (rc == 0) break; // 对端关闭
            // 超时：检查是否超过判定时限（心跳保活依赖于此）
            auto now = std::chrono::steady_clock::now();
            if (now - lastActive > std::chrono::seconds(kServerDeadlineSec)) break;
            continue;
        }
        uint32_t blen = 0;
        for (int i = 0; i < 4; ++i) blen |= static_cast<uint32_t>(hdr[8 + i]) << (i * 8);
        if (blen > kMaxBodyLen) break;

        Packet pkt;
        pkt.cmd = static_cast<uint16_t>(hdr[5]) | (static_cast<uint16_t>(hdr[6]) << 8);
        if (blen > 0) {
            pkt.body.resize(blen);
            if (sock_.recvExact(pkt.body.data(), blen) != 1) break;
        }
        lastActive = std::chrono::steady_clock::now();
        server_->handleMessage(this, pkt);
    }

    if (alive_.load()) close();
    server_->onSessionClosed(this);
}

// 写线程主循环：等待发送队列非空，批量编码并发送
void Session::writerLoop() {
    while (alive_.load()) {
        std::vector<Packet> batch;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return closeRequested_ || !outQueue_.empty(); });
            batch.swap(outQueue_);
            if (closeRequested_ && batch.empty()) break;
        }
        for (const auto& pkt : batch) {
            auto bytes = encodePacket(pkt);
            if (!sock_.sendAll(bytes.data(), bytes.size())) {
                close();
                break;
            }
        }
        if (!alive_.load()) break;
    }
    server_->onSessionClosed(this);
}

// 立即关闭连接（由读线程或写线程调用，幂等）
void Session::close() {
    if (!alive_.exchange(false)) return;
    sock_.close();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        closeRequested_ = true;
    }
    queueCv_.notify_all();
}

// 转发给服务器核心处理（保留该入口以便统一入口调用）
void Session::handlePacket(const Packet& pkt) { server_->handleMessage(this, pkt); }

} // namespace im
