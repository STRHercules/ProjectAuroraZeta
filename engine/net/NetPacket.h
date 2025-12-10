// Basic datagram container used by NetTransport.
#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "NetAddress.h"

namespace Engine::Net {

struct NetPacket {
    NetAddress from{};
    std::vector<uint8_t> payload{};
    std::chrono::steady_clock::time_point timestamp{};
};

}  // namespace Engine::Net
