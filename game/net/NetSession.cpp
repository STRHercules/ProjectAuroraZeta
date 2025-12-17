#include "NetSession.h"

#include <algorithm>
#include <iostream>
#include <random>

#include "../../engine/core/Logger.h"

namespace Game::Net {

namespace {
void prependType(Engine::Net::NetWriter& w, MessageType type) { w.writeU8(static_cast<uint8_t>(type)); }

bool readType(Engine::Net::NetReader& r, MessageType& out) {
    uint8_t t{};
    if (!r.readU8(t)) return false;
    out = static_cast<MessageType>(t);
    return true;
}
}  // namespace

NetSession::NetSession() = default;
NetSession::~NetSession() { stop(); }

void NetSession::beginMatch() {
    inMatch_ = true;
    if (role_ == SessionRole::Host) {
        if (matchSeed_ == 0) {
            uint64_t hi = static_cast<uint64_t>(std::random_device{}());
            uint64_t lo = static_cast<uint64_t>(std::random_device{}());
            matchSeed_ = (hi << 32) ^ lo;
        }
        StartMatchMsg msg{};
        msg.seed = matchSeed_;
        for (const auto& [id, peer] : peers_) {
            (void)id;
            sendMessage(MessageType::StartMatch, peer.addr, [&](Engine::Net::NetWriter& w) { msg.serialize(w); });
        }
    } else if (role_ == SessionRole::Client) {
        if (snapshotProvider_) {
            SnapshotMsg snap = snapshotProvider_();
            snap.tick = static_cast<uint32_t>(time_ * 60.0);
            sendMessage(MessageType::Snapshot, hostAddr_, [&](Engine::Net::NetWriter& w) { snap.serialize(w); });
        }
    }
}

bool NetSession::host(const SessionConfig& cfg) {
    stop();
    if (!transport_.start(cfg.port)) {
        Engine::logError("NetSession: failed to bind port " + std::to_string(cfg.port));
        return false;
    }
    role_ = SessionRole::Host;
    config_ = cfg;
    lobby_.lobbyName = cfg.lobbyName;
    lobby_.difficultyId = cfg.difficultyId;
    lobby_.maxPlayers = cfg.maxPlayers;
    lobby_.players.clear();
    lobby_.players.push_back(LobbyPlayer{1u, cfg.playerName.empty() ? "Host" : cfg.playerName, "default", true, true});
    config_.playerName = cfg.playerName;
    localId_ = 1;
    inLobby_ = true;
    Engine::logInfo("Hosting lobby on port " + std::to_string(cfg.port));
    return true;
}

bool NetSession::join(const std::string& hostIp, uint16_t hostPort, const std::string& password,
                      const std::string& name) {
    stop();
    if (!transport_.start(0)) {  // ephemeral
        Engine::logError("NetSession: failed to open client socket");
        return false;
    }
    role_ = SessionRole::Client;
    hostAddr_.ip = hostIp;
    hostAddr_.port = hostPort;
    localId_ = 0;
    inLobby_ = true;
    inMatch_ = false;
    // Send hello immediately.
    pendingHello_.passwordHash = passwordHash(password);
    pendingHello_.playerName = name.empty() ? "Player" : name;
    helloTimer_ = 0.0;
    sendMessage(MessageType::Hello, hostAddr_, [&](Engine::Net::NetWriter& w) { pendingHello_.serialize(w); });
    Engine::logInfo("Sent hello to " + hostIp + ":" + std::to_string(hostPort));
    return true;
}

void NetSession::stop() {
    if (role_ == SessionRole::Host) {
        broadcastGoodbyeHost();
    }
    if (role_ == SessionRole::Client) {
        sendGoodbye();
    }
    transport_.stop();
    peers_.clear();
    lobby_ = {};
    role_ = SessionRole::None;
    localId_ = 0;
    inLobby_ = false;
    inMatch_ = false;
    matchSeed_ = 0;
    time_ = 0.0;
    snapshotTimer_ = 0.0;
    helloTimer_ = 0.0;
}

void NetSession::sendGoodbye() {
    if (role_ != SessionRole::Client) return;
    sendMessage(MessageType::Goodbye, hostAddr_, [&](Engine::Net::NetWriter& w) { w.writeU32(localId_); });
}

void NetSession::broadcastGoodbyeHost() {
    if (role_ != SessionRole::Host) return;
    Engine::Net::NetWriter w;
    prependType(w, MessageType::Goodbye);
    w.writeU32(1u);  // host id
    for (const auto& [id, peer] : peers_) {
        (void)id;
        transport_.send(peer.addr, w.buffer());
    }
}

void NetSession::sendHeroSelect(const std::string& heroId) {
    if (role_ != SessionRole::Client) return;
    sendMessage(MessageType::HeroSelect, hostAddr_, [&](Engine::Net::NetWriter& w) { w.writeString(heroId); });
}

void NetSession::updateHostHero(const std::string& heroId) {
    if (role_ != SessionRole::Host) return;
    for (auto& p : lobby_.players) {
        if (p.id == 1u) {
            p.heroId = heroId;
            break;
        }
    }
    broadcastLobby();
}

void NetSession::update(double dt) {
    if (role_ == SessionRole::None) return;
    time_ += dt;
    std::vector<Engine::Net::NetPacket> packets;
    transport_.poll(packets, 64);
    for (const auto& pkt : packets) {
        handlePacket(pkt);
    }

    if (role_ == SessionRole::Host && inMatch_ && snapshotProvider_) {
        snapshotTimer_ += dt;
        if (snapshotTimer_ >= 0.05) {  // 20Hz snapshots
            snapshotTimer_ = 0.0;
            broadcastSnapshot();
        }
    } else if (role_ == SessionRole::Client && inMatch_ && snapshotProvider_) {
        snapshotTimer_ += dt;
        if (snapshotTimer_ >= 0.05) {
            snapshotTimer_ = 0.0;
            SnapshotMsg snap = snapshotProvider_();
            snap.tick = static_cast<uint32_t>(time_ * 60.0);
            sendMessage(MessageType::Snapshot, hostAddr_, [&](Engine::Net::NetWriter& w) { snap.serialize(w); });
        }
    }

    // Client retry hello until welcome arrives.
    if (role_ == SessionRole::Client && inLobby_ && localId_ == 0) {
        helloTimer_ += dt;
        if (helloTimer_ >= 1.0) {
            helloTimer_ = 0.0;
            sendMessage(MessageType::Hello, hostAddr_, [&](Engine::Net::NetWriter& w) { pendingHello_.serialize(w); });
            Engine::logInfo("Retry hello to " + hostAddr_.ip + ":" + std::to_string(hostAddr_.port));
        }
    }
}

void NetSession::sendChat(const std::string& text) {
    if (!inLobby_) return;
    ChatMsg msg;
    msg.fromId = localId_;
    msg.text = text;
    if (role_ == SessionRole::Host) {
        lobby_.chat.push_back(LobbyChat{msg.fromId, msg.text});
        broadcastLobby();
    } else {
        sendMessage(MessageType::Chat, hostAddr_, [&](Engine::Net::NetWriter& w) { msg.serialize(w); });
    }
}

int NetSession::playerCount() const {
    if (role_ == SessionRole::None) return 1;
    if (role_ == SessionRole::Host) {
        return static_cast<int>(lobby_.players.size());
    }
    // Client: use lobby cache
    return static_cast<int>(lobby_.players.size());
}

void NetSession::handlePacket(const Engine::Net::NetPacket& pkt) {
    Engine::Net::NetReader reader(pkt.payload);
    MessageType type{};
    if (!readType(reader, type)) return;
    switch (type) {
        case MessageType::Hello: handleHello(pkt, reader); break;
        case MessageType::Welcome: handleWelcome(pkt, reader); break;
        case MessageType::LobbyState: handleLobby(pkt, reader); break;
        case MessageType::Chat: handleChat(pkt, reader); break;
        case MessageType::Snapshot: handleSnapshot(pkt, reader); break;
        case MessageType::StartMatch: handleStartMatch(pkt, reader); break;
        case MessageType::Goodbye: handleGoodbye(pkt, reader); break;
        case MessageType::HeroSelect: handleHeroSelect(pkt, reader); break;
        default: break;
    }
}

void NetSession::handleHello(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    if (role_ != SessionRole::Host) return;
    HelloMsg hello{};
    if (!hello.deserialize(r)) return;
    if (static_cast<int>(lobby_.players.size()) >= config_.maxPlayers) {
        Engine::logWarn("Rejecting connection; lobby full.");
        return;
    }
    if (passwordHash(config_.password) != hello.passwordHash) {
        Engine::logWarn("Rejecting connection; bad password.");
        return;
    }
    uint32_t assigned = nextId_++;
    Peer peer;
    peer.addr = pkt.from;
    peer.id = assigned;
    peer.lastHeard = time_;
    peer.name = hello.playerName;
    peers_[assigned] = peer;
    lobby_.players.push_back(LobbyPlayer{assigned, hello.playerName, "default", false, false});

    WelcomeMsg welcome;
    welcome.assignedId = assigned;
    welcome.lobbyName = config_.lobbyName;
    welcome.difficultyId = config_.difficultyId;
    welcome.maxPlayers = config_.maxPlayers;
    sendMessage(MessageType::Welcome, pkt.from, [&](Engine::Net::NetWriter& w) { welcome.serialize(w); });
    broadcastLobby();
    Engine::logInfo("Accepted peer " + hello.playerName + " id=" + std::to_string(assigned));
}

void NetSession::handleWelcome(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    if (role_ != SessionRole::Client) return;
    WelcomeMsg welcome{};
    if (!welcome.deserialize(r)) return;
    localId_ = welcome.assignedId;
    lobby_.lobbyName = welcome.lobbyName;
    lobby_.difficultyId = welcome.difficultyId;
    lobby_.maxPlayers = welcome.maxPlayers;
    Engine::logInfo("Joined lobby as id " + std::to_string(localId_));
}

void NetSession::handleLobby(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    (void)pkt;
    LobbyState newState{};
    uint16_t playerCount{};
    uint16_t chatCount{};
    if (!r.readString(newState.lobbyName)) return;
    if (!r.readString(newState.difficultyId)) return;
    if (!r.readI32(newState.maxPlayers)) return;
    if (!r.readU16(playerCount)) return;
    for (uint16_t i = 0; i < playerCount; ++i) {
        LobbyPlayer p{};
        if (!r.readU32(p.id)) return;
        if (!r.readString(p.name)) return;
        if (!r.readString(p.heroId)) return;
        uint8_t ready{};
        uint8_t host{};
        if (!r.readU8(ready) || !r.readU8(host)) return;
        p.ready = ready != 0;
        p.isHost = host != 0;
        newState.players.push_back(p);
    }
    if (!r.readU16(chatCount)) return;
    for (uint16_t i = 0; i < chatCount; ++i) {
        LobbyChat c{};
        if (!r.readU32(c.fromId) || !r.readString(c.message)) return;
        newState.chat.push_back(c);
    }
    lobby_ = std::move(newState);
}

void NetSession::handleChat(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    if (role_ != SessionRole::Host) return;
    ChatMsg msg{};
    if (!msg.deserialize(r)) return;
    lobby_.chat.push_back(LobbyChat{msg.fromId, msg.text});
    broadcastLobby();
}

void NetSession::handleSnapshot(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    (void)pkt;
    if (!snapshotConsumer_) return;
    SnapshotMsg snap{};
    if (!snap.deserialize(r)) return;
    snapshotConsumer_(snap);
    inMatch_ = true;
}

void NetSession::handleStartMatch(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    (void)pkt;
    StartMatchMsg msg{};
    if (msg.deserialize(r)) {
        matchSeed_ = msg.seed;
    }
    inMatch_ = true;
}

void NetSession::handleGoodbye(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    (void)pkt;
    uint32_t id{};
    if (!r.readU32(id)) return;
    // Remove peer and mark lobby update.
    peers_.erase(id);
    lobby_.players.erase(std::remove_if(lobby_.players.begin(), lobby_.players.end(),
                                        [&](const LobbyPlayer& p) { return p.id == id; }),
                         lobby_.players.end());
    if (role_ == SessionRole::Host) {
        broadcastLobby();
    }
}

void NetSession::handleHeroSelect(const Engine::Net::NetPacket& pkt, Engine::Net::NetReader& r) {
    if (role_ != SessionRole::Host) return;
    std::string heroId;
    if (!r.readString(heroId)) return;
    uint32_t sender = 0;
    for (const auto& [id, peer] : peers_) {
        if (peer.addr.ip == pkt.from.ip && peer.addr.port == pkt.from.port) {
            sender = id;
            break;
        }
    }
    if (sender == 0) return;
    for (auto& p : lobby_.players) {
        if (p.id == sender) {
            p.heroId = heroId;
            break;
        }
    }
    broadcastLobby();
}

void NetSession::broadcastLobby() {
    if (role_ != SessionRole::Host) return;
    Engine::Net::NetWriter w;
    prependType(w, MessageType::LobbyState);
    w.writeString(lobby_.lobbyName);
    w.writeString(lobby_.difficultyId);
    w.writeI32(lobby_.maxPlayers);
    w.writeU16(static_cast<uint16_t>(lobby_.players.size()));
    for (const auto& p : lobby_.players) {
        w.writeU32(p.id);
        w.writeString(p.name);
        w.writeString(p.heroId);
        w.writeU8(p.ready ? 1 : 0);
        w.writeU8(p.isHost ? 1 : 0);
    }
    w.writeU16(static_cast<uint16_t>(lobby_.chat.size()));
    for (const auto& c : lobby_.chat) {
        w.writeU32(c.fromId);
        w.writeString(c.message);
    }
    for (const auto& [id, peer] : peers_) {
        (void)id;
        transport_.send(peer.addr, w.buffer());
    }
}

void NetSession::broadcastSnapshot() {
    if (!snapshotProvider_) return;
    SnapshotMsg snap = snapshotProvider_();
    snap.tick = static_cast<uint32_t>(time_ * 60.0);
    Engine::Net::NetWriter w;
    prependType(w, MessageType::Snapshot);
    snap.serialize(w);
    for (const auto& [id, peer] : peers_) {
        (void)id;
        transport_.send(peer.addr, w.buffer());
    }
}

void NetSession::sendTo(const Engine::Net::NetAddress& to, const Engine::Net::NetWriter& w, MessageType type) {
    Engine::Net::NetWriter buffer = w;
    Engine::Net::NetWriter wrapper;
    prependType(wrapper, type);
    auto& dst = const_cast<std::vector<uint8_t>&>(wrapper.buffer());
    dst.insert(dst.end(), buffer.buffer().begin(), buffer.buffer().end());
    transport_.send(to, wrapper.buffer());
}

void NetSession::sendMessage(MessageType type, const Engine::Net::NetAddress& to,
                             const std::function<void(Engine::Net::NetWriter&)>& fill) {
    Engine::Net::NetWriter w;
    prependType(w, type);
    fill(w);
    transport_.send(to, w.buffer());
}

}  // namespace Game::Net
