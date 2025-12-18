// Marks an enemy that is temporarily down while a revive animation plays.
#pragma once

namespace Game {

struct EnemyReviving {
    float timer{0.0f};  // seconds remaining
};

}  // namespace Game

