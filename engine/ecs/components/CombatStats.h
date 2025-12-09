// Component representing unit shields/armor/health stats.
#pragma once

#include "../../gameplay/Combat.h"

namespace Engine::ECS {

struct CombatStats : Engine::Gameplay::UnitStats {
    CombatStats() = default;
    CombatStats(float maxHp, float currentHp) {
        maxHealth = maxHp;
        currentHealth = currentHp;
    }
    CombatStats(float maxHp, float currentHp, float maxShieldsIn, float currentShieldsIn = -1.0f) {
        maxHealth = maxHp;
        currentHealth = currentHp;
        maxShields = maxShieldsIn;
        currentShields = currentShieldsIn < 0.0f ? maxShieldsIn : currentShieldsIn;
    }

    bool alive() const { return !isDead(); }
};

}  // namespace Engine::ECS
