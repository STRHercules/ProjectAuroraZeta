// Ticks hit flash timers for feedback.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class HitFlashSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
};

}  // namespace Game
