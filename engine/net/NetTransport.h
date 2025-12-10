// Background UDP transport that queues received packets for the game thread.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "NetAddress.h"
#include "NetPacket.h"
#include "UdpSocket.h"

namespace Engine::Net {

class NetTransport {
public:
    NetTransport() = default;
    ~NetTransport();

    // Bind and start receive thread.
    bool start(uint16_t listenPort);
    void stop();
    bool isRunning() const { return running_; }

    bool send(const NetAddress& to, const std::vector<uint8_t>& payload);
    bool send(const NetAddress& to, const uint8_t* data, std::size_t len);

    // Move up to maxPackets from the internal queue into out vector.
    void poll(std::vector<NetPacket>& out, std::size_t maxPackets = 32);
    std::size_t queuedPackets() const;
    uint16_t boundPort() const { return boundPort_; }

private:
    void recvLoop();

    UdpSocket socket_{};
    std::thread thread_{};
    std::atomic<bool> running_{false};
    mutable std::mutex queueMutex_{};
    std::deque<NetPacket> queue_{};
    uint16_t boundPort_{0};
};

}  // namespace Engine::Net
