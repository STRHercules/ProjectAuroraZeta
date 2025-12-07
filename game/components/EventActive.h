// Marks an active timed mini-event in the world.
#pragma once

namespace Game {

struct EventActive {
    float timer{30.0f};  // seconds remaining
    float maxTimer{30.0f};
    bool success{false};
    bool failed{false};
    bool rewardGranted{false};
    bool salvage{true};
};

}  // namespace Game
