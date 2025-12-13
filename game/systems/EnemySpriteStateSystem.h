// Selects directional enemy animation rows (idle/move/swing/death) based on state.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class EnemySpriteStateSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
};

}  // namespace Game
