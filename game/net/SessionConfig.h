// Configuration for hosting or joining multiplayer sessions.
#pragma once

#include <cstdint>
#include <string>

namespace Game::Net {

struct SessionConfig {
    std::string lobbyName{"Zeta Lobby"};
    std::string password{};
    std::string difficultyId{"normal"};
    uint16_t port{37015};
    int maxPlayers{4};
    std::string playerName{"Host"};
};

inline uint32_t passwordHash(const std::string& pw) {
    // Lightweight non-cryptographic hash to avoid shipping raw passwords over the wire.
    std::hash<std::string> h;
    return static_cast<uint32_t>(h(pw));
}

}  // namespace Game::Net
