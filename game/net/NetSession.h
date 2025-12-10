// High-level multiplayer session manager (host or client).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../engine/net/NetTransport.h"
#include "../../engine/net/NetSerializer.h"
#include "../../engine/core/Time.h"
#include "LobbyState.h"
#include "NetMessages.h"
#include "SessionConfig.h"

namespace Game::Net {

enum class SessionRole { None, Host, Client };

struct Peer {
    Engine::Net::NetAddress addr;
    uint32_t id{0};
    double lastHeard{0.0};
    bool ready{false};
    std::string heroId{"default"};
    std::string name{"Player"};
};

class NetSession {
public:
    using SnapshotProvider = std::function<std::vector<PlayerNetState>()>;
    using SnapshotConsumer = std::function<void(const SnapshotMsg&)>;

    NetSession();
    ~NetSession();

    bool host(const SessionConfig& cfg);
    bool join(const std::string& hostIp, uint16_t hostPort, const std::string& password, const std::string& name);

    void stop();
    void sendGoodbye();
    void broadcastGoodbyeHost();
    void sendHeroSelect(const std::string& heroId);
    void updateHostHero(const std::string& heroId);
    const Engine::Net::NetAddress& hostAddress() const { return hostAddr_; }

    void update(double dt);
    void beginMatch();
    void sendChat(const std::string& text);

    int playerCount() const;
    uint32_t localPlayerId() const { return localId_; }
    bool isHost() const { return role_ == SessionRole::Host; }
    bool inLobby() const { return inLobby_; }
    bool inMatch() const { return inMatch_; }
    const LobbyState& lobby() const { return lobby_; }
    const SessionConfig& config() const { return config_; }

    void setSnapshotProvider(SnapshotProvider provider) { snapshotProvider_ = std::move(provider); }
    void setSnapshotConsumer(SnapshotConsumer consumer) { snapshotConsumer_ = std::move(consumer); }

private:
    void handlePacket(const Engine::Net::NetPacket& pkt);
    void handleHello(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleWelcome(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleLobby(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleChat(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleSnapshot(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleStartMatch(const Engine::Net::NetPacket& pkt);
    void handleGoodbye(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);
    void handleHeroSelect(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r);

    void broadcastLobby();
    void broadcastSnapshot();
    void sendTo(const Engine::Net::NetAddress& to, const Engine::Net::NetWriter& w, MessageType type);
    void sendMessage(MessageType type, const Engine::Net::NetAddress& to, const std::function<void(Engine::Net::NetWriter&)>& fill);

    SessionRole role_{SessionRole::None};
    Engine::Net::NetTransport transport_{};
    SessionConfig config_{};
    LobbyState lobby_{};
    std::unordered_map<uint32_t, Peer> peers_{};
    uint32_t nextId_{2};  // host id is 1
    uint32_t localId_{0};
    bool inLobby_{false};
    bool inMatch_{false};
    double time_{0.0};
    double snapshotTimer_{0.0};
    SnapshotProvider snapshotProvider_{};
    SnapshotConsumer snapshotConsumer_{};
    Engine::Net::NetAddress hostAddr_{};
    // client retry
    double helloTimer_{0.0};
    HelloMsg pendingHello_{};
};

}  // namespace Game::Net
