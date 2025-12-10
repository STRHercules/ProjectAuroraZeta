// Minimal cross-platform UDP socket wrapper (non-blocking).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "NetAddress.h"

namespace Engine::Net {

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    bool open(uint16_t port);
    void close();
    bool isOpen() const;

    bool send(const NetAddress& to, const uint8_t* data, std::size_t len);
    // Returns bytes read, 0 for no data, -1 on fatal error.
    int receive(NetAddress& from, uint8_t* buffer, std::size_t maxLen);

private:
    int fd_{-1};
    bool wsaInit_{false};
};

}  // namespace Engine::Net
