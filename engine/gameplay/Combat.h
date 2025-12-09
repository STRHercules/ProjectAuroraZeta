// Core combat/shields/armor logic shared across engine and game layers.
#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Engine::Gameplay {

enum class Tag {
    Light,
    Armored,
    Biological,
    Mechanical,
    // Extend as needed by game layer.
};

enum class DamageType {
    Normal,  // uses armor
    Spell,   // usually ignores armor
    True     // ignores armor and modifiers
};

struct UnitStats {
    // Health
    float maxHealth = 0.0f;
    float currentHealth = 0.0f;

    // Shields (0 if unit has no shields)
    float maxShields = 0.0f;
    float currentShields = 0.0f;

    // Flat armor values
    float healthArmor = 0.0f;  // for damage that hits health
    float shieldArmor = 0.0f;  // for damage that hits shields

    // Regeneration
    float shieldRegenRate = 0.0f;   // shields/sec
    float healthRegenRate = 0.0f;   // health/sec
    float regenDelay = 0.0f;        // seconds after last hit before regen starts
    float timeSinceLastHit = 0.0f;  // internal timer, updated every frame

    // Classification tags (Armored, Light, etc.)
    std::vector<Tag> tags;

    bool isDead() const { return currentHealth <= 0.0f; }
    bool alive() const { return !isDead(); }
};

struct DamageEvent {
    float baseDamage = 0.0f;
    DamageType type = DamageType::Normal;

    // Bonus damage against specific tags
    // e.g. { Tag::Armored -> +10.0f }
    std::unordered_map<Tag, float> bonusVsTag;
};

constexpr float MIN_DAMAGE_PER_HIT = 0.5f;

inline bool damageUsesArmor(DamageType type) {
    switch (type) {
        case DamageType::Normal: return true;
        case DamageType::Spell:  return false;
        case DamageType::True:   return false;
        default:                 return true;
    }
}

inline float computeEffectiveDamage(const DamageEvent& dmg, const UnitStats& target) {
    float result = dmg.baseDamage;

    for (Tag targetTag : target.tags) {
        auto it = dmg.bonusVsTag.find(targetTag);
        if (it != dmg.bonusVsTag.end()) {
            result += it->second;
        }
    }

    return result;
}

inline void onUnitDamaged(UnitStats& unit) {
    unit.timeSinceLastHit = 0.0f;
}

inline void applyDamage(UnitStats& target, const DamageEvent& dmg) {
    if (target.isDead()) {
        return;
    }

    float damage = computeEffectiveDamage(dmg, target);
    if (damage <= 0.0f) {
        return;
    }

    const bool usesArmor = damageUsesArmor(dmg.type);

    // --- Step 1: Apply damage to shields ---
    if (target.currentShields > 0.0f) {
        float shieldDamage = damage;

        if (usesArmor) {
            shieldDamage -= target.shieldArmor;
            if (shieldDamage < MIN_DAMAGE_PER_HIT) {
                shieldDamage = MIN_DAMAGE_PER_HIT;
            }
        }

        // Clamp to available shields
        const float actualShieldDamage = std::min(shieldDamage, target.currentShields);
        target.currentShields -= actualShieldDamage;

        // Reduce remaining damage by what we actually dealt to shields
        damage -= actualShieldDamage;
        if (damage < 0.0f) {
            damage = 0.0f;
        }
    }

    // --- Step 2: Apply leftover damage to health ---
    if (damage > 0.0f && target.currentHealth > 0.0f) {
        float healthDamage = damage;

        if (usesArmor) {
            healthDamage -= target.healthArmor;
            if (healthDamage < MIN_DAMAGE_PER_HIT) {
                healthDamage = MIN_DAMAGE_PER_HIT;
            }
        }

        const float actualHealthDamage = std::min(healthDamage, target.currentHealth);
        target.currentHealth -= actualHealthDamage;
    }

    // Reset regen timer
    onUnitDamaged(target);
}

inline void updateRegen(UnitStats& unit, float deltaTime) {
    if (unit.isDead()) {
        return;
    }

    // Update timer since last damage
    unit.timeSinceLastHit += deltaTime;

    // Not ready to regen yet
    if (unit.timeSinceLastHit < unit.regenDelay) {
        return;
    }

    // Shields regen
    if (unit.maxShields > 0.0f && unit.shieldRegenRate > 0.0f) {
        unit.currentShields += unit.shieldRegenRate * deltaTime;
        if (unit.currentShields > unit.maxShields) {
            unit.currentShields = unit.maxShields;
        }
    }

    // Health regen (Zerg-style or special units)
    if (unit.healthRegenRate > 0.0f) {
        unit.currentHealth += unit.healthRegenRate * deltaTime;
        if (unit.currentHealth > unit.maxHealth) {
            unit.currentHealth = unit.maxHealth;
        }
    }
}

struct UpgradeState {
    int groundArmorLevel   = 0;
    int airArmorLevel      = 0;
    int shieldArmorLevel   = 0;
};

struct BaseStats {
    float baseHealthArmor = 0.0f;
    float baseShieldArmor = 0.0f;
};

inline void applyUpgradesToUnit(UnitStats& unit,
                         const BaseStats& base,
                         const UpgradeState& upgrades,
                         bool isAirUnit)
{
    unit.healthArmor = base.baseHealthArmor +
        (isAirUnit ? static_cast<float>(upgrades.airArmorLevel) : static_cast<float>(upgrades.groundArmorLevel));

    unit.shieldArmor = base.baseShieldArmor + static_cast<float>(upgrades.shieldArmorLevel);
}

struct BuffState {
    float healthArmorBonus = 0.0f;
    float shieldArmorBonus = 0.0f;
    float damageTakenMultiplier = 1.0f; // e.g. 0.75f for -25% damage taken
};

inline void applyDamage(UnitStats& target, const DamageEvent& dmg, const BuffState& buff) {
    if (target.isDead()) {
        return;
    }
    DamageEvent effective = dmg;
    float damage = computeEffectiveDamage(effective, target);
    if (damage <= 0.0f) {
        return;
    }
    const bool usesArmor = damageUsesArmor(effective.type);
    // Shields
    if (target.currentShields > 0.0f) {
        float shieldDamage = damage;
        if (usesArmor) {
            shieldDamage -= (target.shieldArmor + buff.shieldArmorBonus);
            if (shieldDamage < MIN_DAMAGE_PER_HIT) {
                shieldDamage = MIN_DAMAGE_PER_HIT;
            }
        }
        const float actualShieldDamage = std::min(shieldDamage, target.currentShields);
        target.currentShields -= actualShieldDamage;
        damage -= actualShieldDamage;
        if (damage < 0.0f) damage = 0.0f;
    }
    // Health
    if (damage > 0.0f && target.currentHealth > 0.0f) {
        float healthDamage = damage;
        if (usesArmor) {
            healthDamage -= (target.healthArmor + buff.healthArmorBonus);
            if (healthDamage < MIN_DAMAGE_PER_HIT) {
                healthDamage = MIN_DAMAGE_PER_HIT;
            }
        }
        healthDamage *= buff.damageTakenMultiplier;
        const float actualHealthDamage = std::min(healthDamage, target.currentHealth);
        target.currentHealth -= actualHealthDamage;
    }
    onUnitDamaged(target);
}

}  // namespace Engine::Gameplay
