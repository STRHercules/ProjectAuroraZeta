// Handles pickup collection by the hero.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class PickupSystem {
public:
    void update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, int& credits);
};

}  // namespace Game
