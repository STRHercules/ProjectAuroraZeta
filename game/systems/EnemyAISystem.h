// Very simple enemy AI: move toward hero.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class EnemyAISystem {
public:
    void update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, const Engine::TimeStep& step);
};

}  // namespace Game
