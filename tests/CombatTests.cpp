// Minimal sanity checks for shields/armor/regen pipeline.
#include <cassert>
#include "../engine/gameplay/Combat.h"

using namespace Engine::Gameplay;

int main() {
    {
        UnitStats target{};
        target.maxHealth = target.currentHealth = 20.0f;
        target.healthArmor = 5.0f;
        DamageEvent hit{};
        hit.baseDamage = 3.0f;
        applyDamage(target, hit);
        // Clamped to min damage per hit.
        assert(target.currentHealth == 19.5f);
    }
    {
        UnitStats target{};
        target.maxHealth = target.currentHealth = 10.0f;
        target.maxShields = target.currentShields = 5.0f;
        target.shieldArmor = 1.0f;
        DamageEvent hit{};
        hit.baseDamage = 8.0f;
        applyDamage(target, hit);
        // 8 -1 armor =7 -> shields 5, overflow 2 to health with armor 0 => health 8
        assert(target.currentShields == 0.0f);
        assert(target.currentHealth == 8.0f);
    }
    {
        UnitStats target{};
        target.maxHealth = target.currentHealth = 10.0f;
        target.regenDelay = 0.0f;
        target.healthRegenRate = 2.0f;
        target.currentHealth = 5.0f;
        updateRegen(target, 1.5f);
        assert(target.currentHealth == 8.0f);
    }
    return 0;
}
