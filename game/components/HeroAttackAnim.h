// Marks the hero as actively attacking for animation selection.
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct HeroAttackAnim {
    float timer{0.0f};              // seconds remaining
    Engine::Vec2 targetDir{1.0f, 0.0f};  // direction from hero to target when attack triggered
    int variant{0};                 // 0 = attack 1, 1 = attack 2
};

struct HeroAttackCycle {
    int nextVariant{0};
};

}  // namespace Game

