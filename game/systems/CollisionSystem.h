// Simple projectile-enemy collision and damage.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class CollisionSystem {
public:
    void setContactDamage(float dmg) { contactDamage_ = dmg; }
    void setXpHooks(int* xpPtr, Engine::ECS::Entity hero, float perDamageDealt, float perDamageTaken) {
        xpPtr_ = xpPtr;
        hero_ = hero;
        xpPerDamageDealt_ = perDamageDealt;
        xpPerDamageTaken_ = perDamageTaken;
    }
    void update(Engine::ECS::Registry& registry);

private:
    float contactDamage_{10.0f};
    int* xpPtr_{nullptr};
    Engine::ECS::Entity hero_{Engine::ECS::kInvalidEntity};
    float xpPerDamageDealt_{0.0f};
    float xpPerDamageTaken_{0.0f};
};

}  // namespace Game
