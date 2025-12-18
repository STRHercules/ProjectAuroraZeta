// Enemy revive settings and remaining revive count (mummy/skelly).
#pragma once

namespace Game {

struct EnemyRevive {
    float chance{0.0f};               // 0..1
    float healthFraction{0.35f};      // fraction of maxHealth restored
    float delay{0.65f};               // seconds to stay "down"
    int remaining{0};                 // remaining revives
};

}  // namespace Game

