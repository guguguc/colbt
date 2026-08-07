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

void Session::start() {
    alive_.store(true);
    reader_ = std::thread([this] { readerLoop(); });
    writer_ = std::thread([this] { writerLoop(); });
}

void Session::stop() {
    if (!alive_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        closeRequested_ = true;
    }
    queueCv_.notify_all();
    sock_.close();
}

void Session::join() {
    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();
}

void Session::enqueue(const Packet& pkt) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        outQueue_.push_back(pkt);
    }
    queueCv_.notify_one();
}

void Session::readerLoop() {
    sock_.setRecvTimeout(10000);
    uint8_t hdr[12];
    auto lastActive = std::chrono::steady_clock::now();

    while (alive_.load()) {
        int rc = sock_.recvExact(hdr, 12);
        if (rc <= 0) {
            if (rc == 0) break; // 对端关闭
            // 超时：检查是否超过判定时限
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

void Session::close() {
    if (!alive_.exchange(false)) return;
    sock_.close();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        closeRequested_ = true;
    }
    queueCv_.notify_all();
}

void Session::handlePacket(const Packet& pkt) { server_->handleMessage(this, pkt); }

} // namespace im
