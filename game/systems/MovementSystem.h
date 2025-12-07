// Applies velocity to transform each frame.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class MovementSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
};

}  // namespace Game
