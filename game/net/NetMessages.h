// Message definitions for multiplayer traffic.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../engine/net/NetSerializer.h"
#include "LobbyState.h"

namespace Game::Net {

enum class MessageType : uint8_t {
    Hello = 1,
    Welcome = 2,
    LobbyState = 3,
    Chat = 4,
    Snapshot = 5,
    Input = 6,
    StartMatch = 7,
    Ping = 8,
    Pong = 9,
    Goodbye = 10,
    HeroSelect = 11
};

struct HelloMsg {
    uint32_t passwordHash{0};
    std::string playerName{"Player"};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(passwordHash);
        w.writeString(playerName);
    }
    bool deserialize(Engine::Net::NetReader& r) {
        return r.readU32(passwordHash) && r.readString(playerName);
    }
};

struct WelcomeMsg {
    uint32_t assignedId{0};
    std::string lobbyName;
    std::string difficultyId;
    int maxPlayers{4};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(assignedId);
        w.writeString(lobbyName);
        w.writeString(difficultyId);
        w.writeI32(maxPlayers);
    }
    bool deserialize(Engine::Net::NetReader& r) {
        return r.readU32(assignedId) && r.readString(lobbyName) && r.readString(difficultyId) &&
               r.readI32(maxPlayers);
    }
};

struct ChatMsg {
    uint32_t fromId{0};
    std::string text;

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(fromId);
        w.writeString(text);
    }
    bool deserialize(Engine::Net::NetReader& r) { return r.readU32(fromId) && r.readString(text); }
};

struct PlayerNetState {
    uint32_t id{0};
    std::string heroId{"default"};
    float x{0.0f};
    float y{0.0f};
    float hp{0.0f};
    float shields{0.0f};
    int level{1};
    bool alive{true};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(id);
        w.writeString(heroId);
        w.writeF32(x);
        w.writeF32(y);
        w.writeF32(hp);
        w.writeF32(shields);
        w.writeI32(level);
        w.writeU8(alive ? 1 : 0);
    }
    bool deserialize(Engine::Net::NetReader& r) {
        uint8_t aliveFlag{};
        return r.readU32(id) && r.readString(heroId) && r.readF32(x) && r.readF32(y) && r.readF32(hp) &&
               r.readF32(shields) && r.readI32(level) && r.readU8(aliveFlag) && ((alive = aliveFlag != 0), true);
    }
};

struct SnapshotMsg {
    uint32_t tick{0};
    std::vector<PlayerNetState> players;
    int wave{1};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(tick);
        w.writeI32(wave);
        w.writeU16(static_cast<uint16_t>(players.size()));
        for (const auto& p : players) {
            p.serialize(w);
        }
    }
    bool deserialize(Engine::Net::NetReader& r) {
        uint16_t count{};
        if (!r.readU32(tick) || !r.readI32(wave) || !r.readU16(count)) return false;
        players.clear();
        players.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            PlayerNetState s{};
            if (!s.deserialize(r)) return false;
            players.push_back(s);
        }
        return true;
    }
};

}  // namespace Game::Net
