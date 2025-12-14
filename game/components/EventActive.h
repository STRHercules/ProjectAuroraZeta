// Marks an active timed mini-event in the world.
#pragma once

namespace Game {

enum class EventType { Escort, Bounty, SpawnerHunt };

struct EventActive {
    int id{0};
    EventType type{EventType::Escort};
    float timer{30.0f};   // seconds remaining (active phase)
    float maxTimer{30.0f};
    float countdown{0.0f};  // pre-start countdown
    bool success{false};
    bool failed{false};
    bool rewardGranted{false};
    bool escort{true};
    int requiredKills{0};  // used by bounty hunts
};

}  // namespace Game
