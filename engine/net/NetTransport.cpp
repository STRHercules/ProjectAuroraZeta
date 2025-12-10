#include "NetTransport.h"

#include <chrono>

namespace Engine::Net {

NetTransport::~NetTransport() { stop(); }

bool NetTransport::start(uint16_t listenPort) {
    stop();
    if (!socket_.open(listenPort)) {
        return false;
    }
    boundPort_ = listenPort;
    running_ = true;
    thread_ = std::thread(&NetTransport::recvLoop, this);
    return true;
}

void NetTransport::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    socket_.close();
    boundPort_ = 0;
    std::scoped_lock lk(queueMutex_);
    queue_.clear();
}

bool NetTransport::send(const NetAddress& to, const std::vector<uint8_t>& payload) {
    return send(to, payload.data(), payload.size());
}

bool NetTransport::send(const NetAddress& to, const uint8_t* data, std::size_t len) {
    if (!running_) return false;
    return socket_.send(to, data, len);
}

void NetTransport::poll(std::vector<NetPacket>& out, std::size_t maxPackets) {
    std::scoped_lock lk(queueMutex_);
    std::size_t count = std::min(maxPackets, queue_.size());
    out.reserve(out.size() + count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(std::move(queue_.front()));
        queue_.pop_front();
    }
}

std::size_t NetTransport::queuedPackets() const {
    std::scoped_lock lk(queueMutex_);
    return queue_.size();
}

void NetTransport::recvLoop() {
    constexpr std::size_t kMaxPacket = 1400;
    std::vector<uint8_t> buffer(kMaxPacket);
    while (running_) {
        NetAddress from{};
        int read = socket_.receive(from, buffer.data(), buffer.size());
        if (read > 0) {
            NetPacket pkt;
            pkt.from = from;
            pkt.timestamp = std::chrono::steady_clock::now();
            pkt.payload.assign(buffer.begin(), buffer.begin() + read);
            {
                std::scoped_lock lk(queueMutex_);
                queue_.push_back(std::move(pkt));
                if (queue_.size() > 256) {
                    queue_.pop_front();  // drop oldest to avoid unbounded growth
                }
            }
        } else {
            // Sleep briefly to avoid busy spin when no data.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

}  // namespace Engine::Net
