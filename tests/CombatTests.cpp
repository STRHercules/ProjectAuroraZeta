// Minimal sanity checks for combat and fog of war logic.
#include <cassert>
#include "../engine/gameplay/Combat.h"
#include "../engine/gameplay/FogOfWar.h"

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
    {
        FogOfWarLayer fog(4, 4);
        fog.revealCircle(1, 1, 1.0f);
        assert(fog.getState(1, 1) == FogState::Visible);
        assert(fog.isExplored(1, 1));
        fog.resetVisibility();
        assert(fog.getState(1, 1) == FogState::Fogged);
    }
    {
        FogOfWarLayer fog(6, 6);
        std::vector<Unit> units{
            Unit{8.0f, 8.0f, 1.5f, true, 0},
            Unit{20.0f, 20.0f, 2.0f, true, 1},  // different owner, ignored
        };
        updateFogOfWar(fog, units, 0, 4);
        // First unit at world (8,8) -> tile (2,2) with radius 1.5 should reveal center.
        assert(fog.getState(2, 2) == FogState::Visible);
        // Tile (0,0) remains unexplored.
        assert(fog.getState(0, 0) == FogState::Unexplored);
    }
    return 0;
}
