// Marks an active timed mini-event in the world.
#pragma once

namespace Game {

enum class EventType { Salvage, Bounty, SpawnerHunt };

struct EventActive {
    int id{0};
    EventType type{EventType::Salvage};
    float timer{30.0f};   // seconds remaining
    float maxTimer{30.0f};
    bool success{false};
    bool failed{false};
    bool rewardGranted{false};
    bool salvage{true};
    int requiredKills{0};  // used by bounty hunts
};

}  // namespace Game
