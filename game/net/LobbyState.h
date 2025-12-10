// Represents lobby participants and chat prior to match start.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Game::Net {

struct LobbyPlayer {
    uint32_t id{0};
    std::string name{"Player"};
    std::string heroId{"default"};
    bool ready{false};
    bool isHost{false};
};

struct LobbyChat {
    uint32_t fromId{0};
    std::string message;
};

struct LobbyState {
    std::vector<LobbyPlayer> players;
    std::vector<LobbyChat> chat;
    std::string lobbyName{"Lobby"};
    std::string difficultyId{"normal"};
    int maxPlayers{4};
};

}  // namespace Game::Net
