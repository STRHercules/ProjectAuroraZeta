// Simple projectile-enemy collision and damage.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class CollisionSystem {
public:
    void setContactDamage(float dmg) { contactDamage_ = dmg; }
    void update(Engine::ECS::Registry& registry);

private:
    float contactDamage_{10.0f};
};

}  // namespace Game
