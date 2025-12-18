// Slime-specific runtime state (multiplication).
#pragma once

namespace Game {

struct SlimeBehavior {
    float interval{0.0f};     // seconds between checks
    float timer{0.0f};        // countdown
    float chance{0.0f};       // 0..1 chance per interval
    int spawnMin{0};
    int spawnMax{0};
    int remainingMultiplies{0};
};

}  // namespace Game

