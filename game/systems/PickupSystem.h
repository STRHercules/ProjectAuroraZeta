// Handles pickup collection by the hero.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include <functional>
#include "../components/Pickup.h"

namespace Game {

class PickupSystem {
public:
    using CollectFn = std::function<void(const Game::Pickup&)>;

    void update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, CollectFn onCollect);
};

}  // namespace Game
