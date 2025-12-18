// Selects directional enemy animation rows (idle/move/swing/death) based on state.
#pragma once

#include <vector>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../EnemyDefinition.h"

namespace Game {

class EnemySpriteStateSystem {
public:
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);

private:
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
};

}  // namespace Game
