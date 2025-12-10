# Shields, Armor & Health System 

This document defines a StarCraft II–style **Shields + Armor + Health** damage system for a C++ game. It is written as implementation notes you can hand directly to an AI coding assistant (Codex, etc.) to generate or maintain code.

---

## 1. Core Concepts

Each unit has up to three defensive layers:

1. **Shields** (optional, Protoss-style)
2. **Health / Life**
3. **Armor** (separate values for shields and health)

Damage always flows like this:

> Damage event → apply bonus vs tags → apply to Shields (minus shield armor) → leftover applies to Health (minus health armor).

There is a **minimum damage per hit** so armor can’t reduce damage to zero.

Different damage types may or may not use armor (e.g., spells that ignore armor).

---

## 2. Data Structures

### 2.1 Tags

Tags give units classifications that attacks can care about (`Armored`, `Light`, `Biological`, etc.).

```cpp
enum class Tag {
    Light,
    Armored,
    Biological,
    Mechanical,
    // Add as needed
};
```

### 2.2 Damage Types

Damage types control whether armor is applied, and how.

```cpp
enum class DamageType {
    Normal,  // uses armor
    Spell,   // usually ignores armor
    True     // ignores armor and modifiers
};
```

### 2.3 UnitStats

This struct holds all defensive stats needed for the system.

```cpp
#include <vector>

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

    bool isDead() const {
        return currentHealth <= 0.0f;
    }
};
```

### 2.4 DamageEvent

Every attack / hit is represented as a `DamageEvent`.

```cpp
#include <unordered_map>

struct DamageEvent {
    float baseDamage = 0.0f;
    DamageType type = DamageType::Normal;

    // Bonus damage against specific tags
    // e.g. { Tag::Armored -> +10.0f }
    std::unordered_map<Tag, float> bonusVsTag;
};
```

Example setups:

```cpp
// Marine
DamageEvent marineShot() {
    DamageEvent dmg;
    dmg.baseDamage = 6.0f;
    dmg.type = DamageType::Normal;
    return dmg;
}

// Marauder (bonus vs Armored)
DamageEvent marauderShot() {
    DamageEvent dmg;
    dmg.baseDamage = 10.0f;
    dmg.type = DamageType::Normal;
    dmg.bonusVsTag[Tag::Armored] = 10.0f;
    return dmg;
}

// Spell that ignores armor
DamageEvent psionicStormTick() {
    DamageEvent dmg;
    dmg.baseDamage = 8.0f; // per tick
    dmg.type = DamageType::Spell; // no armor
    return dmg;
}
```

---

## 3. Damage Resolution Pipeline

### 3.1 Helper: Does this damage use armor?

```cpp
inline bool damageUsesArmor(DamageType type) {
    switch (type) {
        case DamageType::Normal: return true;
        case DamageType::Spell:  return false;
        case DamageType::True:   return false;
        default:                 return true;
    }
}
```

### 3.2 Helper: Compute effective damage vs target tags

```cpp
float computeEffectiveDamage(const DamageEvent& dmg, const UnitStats& target) {
    float result = dmg.baseDamage;

    for (Tag targetTag : target.tags) {
        auto it = dmg.bonusVsTag.find(targetTag);
        if (it != dmg.bonusVsTag.end()) {
            result += it->second;
        }
    }

    return result;
}
```

### 3.3 Main `applyDamage` function

This function applies a single hit to a unit, including armor and shields.

Key rules:

- Damage hits **shields first**.
- Shields use **shieldArmor** if the damage type uses armor.
- Any damage that gets through shields spills into **health**, which uses **healthArmor**.
- Per-hit minimum damage ensures armor can’t reduce a hit below `MIN_DAMAGE_PER_HIT`.

```cpp
constexpr float MIN_DAMAGE_PER_HIT = 0.5f;

void onUnitDamaged(UnitStats& unit) {
    unit.timeSinceLastHit = 0.0f;
}

void applyDamage(UnitStats& target, const DamageEvent& dmg) {
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
```

That is the C++ core that mirrors the SC2-style logic in a clean, reusable way.

---

## 4.1 Data hooks used in Project Aurora Zeta

- `data/gameplay.json` now exposes shield/armor/regen knobs for both `enemy` and `hero` blocks: `shields`, `healthArmor`, `shieldArmor`, `shieldRegen`, `healthRegen` (hero only), and `regenDelay`.
- Wave spawns and hero init read these values and populate `Engine::ECS::Health` (alias of `CombatStats`), so tuning is fully data-driven.
- Archetype presets scale shields alongside health; elites/bosses add small armor bonuses to demonstrate the pipeline.

---

## 4. Regeneration System

Each frame / tick you update regen for each unit.

### 4.1 Update function

```cpp
void updateRegen(UnitStats& unit, float deltaTime) {
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
```

### 4.2 Class presets (example)

You can build helper functions or data tables to define “class templates”:

```cpp
void makeDamageDealer(UnitStats& u, float maxHealth, float maxShields) {
    u.maxHealth       = maxHealth;
    u.currentHealth   = maxHealth;
    u.maxShields      = maxShields;
    u.currentShields  = maxShields;
    u.healthArmor     = 0.0f;
    u.shieldArmor     = 0.0f;
    u.shieldRegenRate = 4.0f;     // tune for feel
    u.healthRegenRate = 0.0f;
    u.regenDelay      = 8.0f;     // seconds
}

void makeSpecial(UnitStats& u, float maxHealth) {
    u.maxHealth       = maxHealth;
    u.currentHealth   = maxHealth;
    u.maxShields      = 0.0f;
    u.currentShields  = 0.0f;
    u.healthArmor     = 0.0f;
    u.shieldArmor     = 0.0f;
    u.shieldRegenRate = 0.0f;
    u.healthRegenRate = 0.0f;
    u.regenDelay      = 0.0f;
}

void makeHealer(UnitStats& u, float maxHealth) {
    u.maxHealth       = maxHealth;
    u.currentHealth   = maxHealth;
    u.maxShields      = 0.0f;
    u.currentShields  = 0.0f;
    u.healthArmor     = 0.0f;
    u.shieldArmor     = 0.0f;
    u.shieldRegenRate = 0.0f;
    u.healthRegenRate = 1.5f;     // regen/sec, tune as desired
    u.regenDelay      = 0.5f;     // optional small delay
}
```

You can tune numbers to match your own game feel (faster/slower regen, longer/shorter delay, etc.).

---

## 5. Upgrades and Buffs

The system is designed so upgrades and temporary buffs are just **modifiers** to armor and regen.

### 5.1 Global upgrade state

```cpp
struct UpgradeState {
    int groundArmorLevel   = 0;
    int airArmorLevel      = 0;
    int shieldArmorLevel   = 0;
};
```

### 5.2 Applying upgrades to units

In your unit-definition code (or when upgrades change), recompute armor values:

```cpp
struct BaseStats {
    float baseHealthArmor = 0.0f;
    float baseShieldArmor = 0.0f;
    // ... other fields like baseHealth, baseShields
};

void applyUpgradesToUnit(UnitStats& unit,
                         const BaseStats& base,
                         const UpgradeState& upgrades,
                         bool isAirUnit)
{
    unit.healthArmor = base.baseHealthArmor +
        (isAirUnit ? upgrades.airArmorLevel : upgrades.groundArmorLevel);

    unit.shieldArmor = base.baseShieldArmor + upgrades.shieldArmorLevel;
}
```

This lets you do SC2-style upgrades where:

- Ground armor upgrades increase only healthArmor for ground units.
- Shield upgrades increase shieldArmor for all shielded units.

### 5.3 Temporary buffs (optional)

For abilities like “+2 armor for 10 seconds,” you can layer a buff struct on top:

```cpp
struct BuffState {
    float healthArmorBonus = 0.0f;
    float shieldArmorBonus = 0.0f;
    float damageTakenMultiplier = 1.0f; // e.g. 0.75f for -25% damage taken
};
```

Then extend `applyDamage` to use:

- `effectiveHealthArmor = target.healthArmor + buff.healthArmorBonus`
- `effectiveShieldArmor = target.shieldArmor + buff.shieldArmorBonus`
- Multiply final damage by `damageTakenMultiplier` if needed.

---

## 6. Integration Steps (Checklist)

This is the direct “do this in code” summary:

1. **Add `UnitStats` to your units**  
   - Either as a component (in ECS) or as a member field in your unit class.

2. **Build unit templates**  
   - Define base stats per unit type (health, shields, armor, regen).  
   - Use helpers like `makeProtossUnit`, `makeTerranUnit`, `makeZergUnit` to keep things consistent.

3. **Hook damage into your hit system**  
   - Whenever a projectile/melee hit is confirmed:
     - Construct a `DamageEvent`.
     - Call `applyDamage(targetStats, dmg)`.  
   - Make sure to call `onUnitDamaged` (already inside `applyDamage`) to reset regen timing.

4. **Update regen each frame**  
   - In your main update loop, for each unit:
     - `updateRegen(unitStats, deltaTime);`

5. **Wire up upgrades**  
   - Maintain an `UpgradeState` per player/faction.  
   - When upgrades complete, recompute armor for affected units via `applyUpgradesToUnit`.

6. **Add tags and bonus damage**  
   - Give units tags (`Armored`, `Light`, etc.).  
   - For attacks, fill in `bonusVsTag` on `DamageEvent`.  
   - The core system will automatically add bonuses vs targets that have those tags.

7. **Tune and iterate**  
   - Adjust armor values, regen rates, regen delays, and min-damage to get the desired TTK and “feel” for your game.

---

## 7. Example: Simple C++ Usage Flow

```cpp
void simulateHit(UnitStats& attacker, UnitStats& defender) {
    // Build damage event for attacker (example: marine)
    DamageEvent shot = marineShot();

    // Apply damage
    applyDamage(defender, shot);

    if (defender.isDead()) {
        // Handle death (remove unit, spawn effects, etc.)
    }
}

void updateAllUnits(std::vector<UnitStats>& units, float deltaTime) {
    for (auto& unit : units) {
        updateRegen(unit, deltaTime);
    }
}
```
