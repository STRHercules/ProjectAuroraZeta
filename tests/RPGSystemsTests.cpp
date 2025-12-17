// Unit tests for the new RPG combat + stat systems.
#include <cassert>
#include <random>

#include "../engine/gameplay/RPGStats.h"
#include "../engine/gameplay/RPGCombat.h"

using namespace Engine::Gameplay::RPG;

int main() {
    // Armor curve correctness
    {
        CombatantState atk{};
        atk.stats.attackPower = 50.0f;
        CombatantState def{};
        def.stats.armor = 100.0f;
        def.stats.resists.set(DamageType::Physical, 0.0f);
        def.currentHealth = 100.0f;
        AttackDef a{};
        a.baseDamage = 0.0f;  // use attack power only
        a.rollMin = a.rollMax = 1.0f;
        a.baseHitChance = 1.0f;
        a.baseGlanceChance = 0.0f;
        a.shapedRoll = false;
        ResolverConfig cfg{};
        cfg.rng.shaped = false;
        std::mt19937 rng(42);
        auto out = ResolveHit(atk, def, a, rng, cfg);
        float dr = def.stats.armor / (def.stats.armor + cfg.armorConstant);
        float expected = atk.stats.attackPower * (1.0f - dr);
        assert(std::abs(out.healthDamage - expected) < 0.001f);
    }

    // Resistance clamp behaviour (upper and lower)
    {
        CombatantState atk{};
        atk.stats.attackPower = 40.0f;
        CombatantState def{};
        def.stats.armor = 0.0f;
        def.currentHealth = 100.0f;
        def.stats.resists.set(DamageType::Fire, 1.2f);   // should clamp to 0.90
        def.stats.resists.set(DamageType::Frost, -0.9f); // should clamp to -0.75
        AttackDef fire{};
        fire.type = DamageType::Fire;
        fire.baseDamage = 0.0f;
        fire.rollMin = fire.rollMax = 1.0f;
        fire.baseHitChance = 1.0f;
        fire.baseGlanceChance = 0.0f;
        fire.shapedRoll = false;
        ResolverConfig cfg{};
        cfg.rng.shaped = false;
        std::mt19937 rng(7);
        auto outFire = ResolveHit(atk, def, fire, rng, cfg);
        float expectedFire = atk.stats.attackPower * (1.0f - cfg.resistMax);
        assert(std::abs(outFire.healthDamage - expectedFire) < 0.001f);

        AttackDef frost = fire;
        frost.type = DamageType::Frost;
        auto outFrost = ResolveHit(atk, def, frost, rng, cfg);
        float expectedFrost = atk.stats.attackPower * (1.0f - cfg.resistMin);
        assert(std::abs(outFrost.healthDamage - expectedFrost) < 0.001f);
    }

    // CC diminishing returns
    {
        CombatantState atk{};
        atk.stats.attackPower = 1.0f;
        CombatantState def{};
        def.currentHealth = 10.0f;
        def.stats.resists.set(DamageType::Physical, 0.0f);
        AttackDef a{};
        a.baseDamage = 0.0f;
        a.rollMin = a.rollMax = 1.0f;
        a.baseHitChance = 1.0f;
        a.baseGlanceChance = 0.0f;
        a.shapedRoll = false;
        StatusRef cc{};
        cc.id = 1;
        cc.baseChance = 1.0f;
        cc.baseDuration = 2.0f;
        cc.isCrowdControl = true;
        a.onHitStatuses.push_back(cc);
        ResolverConfig cfg{};
        cfg.rng.shaped = false;
        std::mt19937 rng(11);
        CCFatigueState fatigue{};
        auto first = ResolveHit(atk, def, a, rng, cfg, &fatigue);
        assert(first.statuses.front().applied);
        assert(std::abs(first.statuses.front().finalDuration - 2.0f) < 0.01f);
        auto second = ResolveHit(atk, def, a, rng, cfg, &fatigue);
        assert(second.statuses.front().applied);
        assert(second.statuses.front().finalDuration < 2.0f && second.statuses.front().finalDuration > 1.2f);
        auto third = ResolveHit(atk, def, a, rng, cfg, &fatigue);
        assert(third.statuses.front().applied);
        assert(third.statuses.front().finalDuration <= 1.1f);
        auto fourth = ResolveHit(atk, def, a, rng, cfg, &fatigue);
        assert(!fourth.statuses.front().applied);
    }

    // Equipment/stat aggregation pipeline
    {
        AggregationInput input{};
        input.attributes = Attributes{5, 3, 0, 4, 2};
        input.base.baseHealth = 100.0f;
        input.base.baseAttackPower = 20.0f;
        StatContribution gear{};
        gear.flat.attackPower = 10.0f;
        gear.flat.armor = 5.0f;
        gear.mult.attackPower = 0.20f;  // +20%
        gear.mult.healthMax = 0.10f;    // +10%
        input.contributions.push_back(gear);
        auto stats = aggregateDerivedStats(input);
        float expectedAP = 20.0f + 5 * 2.0f + 3 * 0.5f + 10.0f; // base + STR + DEX + flat
        expectedAP *= 1.20f; // multiplier
        assert(std::abs(stats.attackPower - expectedAP) < 0.01f);
        float expectedHP = (100.0f + 4 * 8.0f); // hpPerEND default 8
        expectedHP *= 1.10f;
        assert(std::abs(stats.healthMax - expectedHP) < 0.1f);
        assert(stats.armor >= 5.0f); // flat armor applied
    }

    return 0;
}

