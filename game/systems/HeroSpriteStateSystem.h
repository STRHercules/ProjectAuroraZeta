// Selects Damage Dealer animation rows (idle/walk/pickup/attack/knockdown) based on state.
#pragma once

#include "../../engine/core/Time.h"
#include "../../engine/ecs/Registry.h"

namespace Game {

class HeroSpriteStateSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero);
};

}  // namespace Game

