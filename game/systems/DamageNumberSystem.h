// Updates floating damage numbers.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class DamageNumberSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
};

}  // namespace Game
