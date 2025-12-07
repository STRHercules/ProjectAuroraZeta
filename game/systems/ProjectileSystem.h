// Updates projectile movement and lifetime.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class ProjectileSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
};

}  // namespace Game
