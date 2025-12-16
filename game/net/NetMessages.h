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
    int wave{0};
    uint8_t lookDir{2};   // 0=Right,1=Left,2=Front,3=Back
    uint8_t attacking{0}; // legacy flag: 1 if currently in an attack animation window
    float attackTimer{0.0f}; // seconds of attack anim remaining (authoritative)

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(id);
        w.writeString(heroId);
        w.writeF32(x);
        w.writeF32(y);
        w.writeF32(hp);
        w.writeF32(shields);
        w.writeI32(level);
        w.writeU8(alive ? 1 : 0);
        w.writeI32(wave);
        w.writeU8(lookDir);
        w.writeU8(attacking);
        w.writeF32(attackTimer);
    }
    bool deserialize(Engine::Net::NetReader& r) {
        uint8_t aliveFlag{};
        return r.readU32(id) && r.readString(heroId) && r.readF32(x) && r.readF32(y) && r.readF32(hp) &&
               r.readF32(shields) && r.readI32(level) && r.readU8(aliveFlag) && r.readI32(wave) &&
               r.readU8(lookDir) && r.readU8(attacking) && r.readF32(attackTimer) && ((alive = aliveFlag != 0), true);
    }
};

struct EnemyNetState {
    uint32_t id{0};
    float x{0.0f};
    float y{0.0f};
    float hp{0.0f};
    float shields{0.0f};
    bool alive{true};
    std::string defId{};
    int frameWidth{16};
    int frameHeight{16};
    float frameDuration{0.16f};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(id);
        w.writeF32(x);
        w.writeF32(y);
        w.writeF32(hp);
        w.writeF32(shields);
        w.writeU8(alive ? 1 : 0);
        w.writeString(defId);
        w.writeI32(frameWidth);
        w.writeI32(frameHeight);
        w.writeF32(frameDuration);
    }
    bool deserialize(Engine::Net::NetReader& r) {
        uint8_t aliveFlag{};
        return r.readU32(id) && r.readF32(x) && r.readF32(y) && r.readF32(hp) && r.readF32(shields) &&
               r.readU8(aliveFlag) && r.readString(defId) && r.readI32(frameWidth) && r.readI32(frameHeight) &&
               r.readF32(frameDuration) && ((alive = aliveFlag != 0), true);
    }
};

struct SnapshotMsg {
    uint32_t tick{0};
    std::vector<PlayerNetState> players;
    std::vector<EnemyNetState> enemies;
    int wave{1};

    void serialize(Engine::Net::NetWriter& w) const {
        w.writeU32(tick);
        w.writeI32(wave);
        w.writeU16(static_cast<uint16_t>(players.size()));
        for (const auto& p : players) {
            p.serialize(w);
        }
        w.writeU16(static_cast<uint16_t>(enemies.size()));
        for (const auto& e : enemies) {
            e.serialize(w);
        }
    }
    bool deserialize(Engine::Net::NetReader& r) {
        uint16_t count{};
        uint16_t enemyCount{};
        if (!r.readU32(tick) || !r.readI32(wave) || !r.readU16(count)) return false;
        players.clear();
        players.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            PlayerNetState s{};
            if (!s.deserialize(r)) return false;
            players.push_back(s);
        }
        if (!r.readU16(enemyCount)) return false;
        enemies.clear();
        enemies.reserve(enemyCount);
        for (uint16_t i = 0; i < enemyCount; ++i) {
            EnemyNetState e{};
            if (!e.deserialize(r)) return false;
            enemies.push_back(e);
        }
        return true;
    }
};

}  // namespace Game::Net
