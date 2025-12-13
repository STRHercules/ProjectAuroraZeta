// Marks an enemy as actively attacking (swinging) for animation selection.
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct EnemyAttackSwing {
    float timer{0.0f};              // seconds remaining
    Engine::Vec2 targetDir{1.0f, 0.0f};  // direction from enemy to target when swing triggered
};

}  // namespace Game

