// Lightweight RPG stat model shared by engine and game layers.
// Provides attribute/derived stat structures and a data-driven aggregation pipeline.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Gameplay::RPG {

// Damage channels used by the new RPG combat model.
enum class DamageType : std::uint8_t {
    Physical = 0,
    Fire,
    Frost,
    Shock,
    Poison,
    Arcane,
    True,  // bypasses most mitigation; still shaped for logging
    Count
};

// Core attribute set (shared by heroes and enemies).
struct Attributes {
    int STR{0};
    int DEX{0};
    int INT{0};
    int END{0};
    int LCK{0};
};

// Simple resistance container indexed by DamageType.
struct Resistances {
    std::array<float, static_cast<std::size_t>(DamageType::Count)> values{};

    float get(DamageType type) const {
        return values[static_cast<std::size_t>(type)];
    }
    void set(DamageType type, float v) {
        values[static_cast<std::size_t>(type)] = v;
    }
};

// Pools for mana-like resources. Pools are optional (enabled=false skips checks).
struct ResourcePool {
    float max{0.0f};
    float current{0.0f};
    float regen{0.0f};
    float onHitGain{0.0f};
    float onKillGain{0.0f};
    bool enabled{false};
};

struct Resources {
    ResourcePool mana{};
    ResourcePool stamina{};
    ResourcePool energy{};
    ResourcePool ammo{};
    ResourcePool heat{};
};

// Derived stats snapshot used for combat resolution.
struct DerivedStats {
    // Offense
    float attackPower{0.0f};
    float spellPower{0.0f};
    float attackSpeed{1.0f};
    float moveSpeed{1.0f};
    float accuracy{0.0f};      // hit quality helper
    float critChance{0.0f};    // 0..1
    float critMult{1.5f};
    float armorPen{0.0f};

    // Defense
    float evasion{0.0f};       // contributes to dodge/parry
    float armor{0.0f};         // physical mitigation
    float tenacity{0.0f};      // status resistance
    float shieldMax{0.0f};
    float shieldRegen{0.0f};
    float healthMax{0.0f};
    float healthRegen{0.0f};
    Resistances resists{};

    // Utility/economy
    float cooldownReduction{0.0f};  // 0..1 (fractional reduction)
    float resourceRegen{0.0f};
    float goldGainMult{1.0f};
    float rarityScore{0.0f};
};

// Tunable aggregation constants (kept data-driven but with sane defaults).
struct AggregationConstants {
    float hpPerEND{8.0f};
    float armorPerEND{0.2f};
    float attackPerSTR{2.0f};
    float attackPerDEX{0.5f};
    float spellPerINT{2.5f};
    float speedPerDEX{0.015f};
    float accuracyPerDEX{0.6f};
    float evasionPerLCK{0.5f};
    float critPerLCK{0.01f};      // +1% per LCK by default
    float rarityPerLCK{0.15f};
    float goldPerLCK{0.01f};
};

// Base stat template for an entity (prior to items/talents/status).
struct BaseStatTemplate {
    float baseHealth{100.0f};
    float baseShields{0.0f};
    float baseArmor{0.0f};
    float baseAttackPower{10.0f};
    float baseSpellPower{8.0f};
    float baseAttackSpeed{1.0f};
    float baseMoveSpeed{4.0f};
    float baseAccuracy{0.0f};
    float baseCritChance{0.05f};
    float baseCritMult{1.5f};
    float baseEvasion{0.0f};
    float baseTenacity{0.0f};
    float baseCooldownReduction{0.0f};
    Resistances baseResists{};
};

// Flat/multiplicative adjustments (from gear, talents, buffs).
struct StatContribution {
    DerivedStats flat{};
    DerivedStats mult{};  // interpreted as (1 + mult.x)
};

// Input bundle for aggregation.
struct AggregationInput {
    Attributes attributes{};
    BaseStatTemplate base{};
    std::vector<StatContribution> contributions{};
};

// Builds a DerivedStats snapshot given attributes and contributions.
DerivedStats aggregateDerivedStats(const AggregationInput& input, const AggregationConstants& cfg = {});

}  // namespace Engine::Gameplay::RPG

