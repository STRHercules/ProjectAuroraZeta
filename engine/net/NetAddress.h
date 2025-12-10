// Lightweight socket address wrapper used by the engine networking layer.
#pragma once

#include <cstdint>
#include <string>

namespace Engine::Net {

struct NetAddress {
    std::string ip{"127.0.0.1"};
    uint16_t port{37015};
};

}  // namespace Engine::Net
