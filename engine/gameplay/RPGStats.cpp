// Implementation of the RPG stat aggregation pipeline.
#include "RPGStats.h"

#include <algorithm>

namespace Engine::Gameplay::RPG {

namespace {

DerivedStats makeBase(const AggregationInput& in, const AggregationConstants& cfg) {
    DerivedStats out{};
    out.healthMax = in.base.baseHealth + static_cast<float>(in.attributes.END) * cfg.hpPerEND;
    out.healthRegen = 0.0f;
    out.shieldMax = in.base.baseShields;
    out.shieldRegen = 0.0f;
    out.armor = in.base.baseArmor + static_cast<float>(in.attributes.END) * cfg.armorPerEND;
    out.attackPower = in.base.baseAttackPower +
                      static_cast<float>(in.attributes.STR) * cfg.attackPerSTR +
                      static_cast<float>(in.attributes.DEX) * cfg.attackPerDEX;
    out.spellPower = in.base.baseSpellPower + static_cast<float>(in.attributes.INT) * cfg.spellPerINT;
    out.attackSpeed = in.base.baseAttackSpeed;
    out.moveSpeed = in.base.baseMoveSpeed + static_cast<float>(in.attributes.DEX) * cfg.speedPerDEX;
    out.accuracy = in.base.baseAccuracy + static_cast<float>(in.attributes.DEX) * cfg.accuracyPerDEX;
    out.evasion = in.base.baseEvasion + static_cast<float>(in.attributes.LCK) * cfg.evasionPerLCK;
    out.critChance = in.base.baseCritChance + static_cast<float>(in.attributes.LCK) * cfg.critPerLCK;
    out.critMult = in.base.baseCritMult;
    out.tenacity = in.base.baseTenacity;
    out.cooldownReduction = in.base.baseCooldownReduction;
    out.goldGainMult = 1.0f + static_cast<float>(in.attributes.LCK) * cfg.goldPerLCK;
    out.rarityScore = static_cast<float>(in.attributes.LCK) * cfg.rarityPerLCK;
    out.resists = in.base.baseResists;
    return out;
}

}  // namespace

DerivedStats aggregateDerivedStats(const AggregationInput& input, const AggregationConstants& cfg) {
    DerivedStats result = makeBase(input, cfg);

    // Apply flat contributions first.
    for (const auto& c : input.contributions) {
        result.attackPower += c.flat.attackPower;
        result.spellPower += c.flat.spellPower;
        result.attackSpeed += c.flat.attackSpeed;
        result.moveSpeed += c.flat.moveSpeed;
        result.accuracy += c.flat.accuracy;
        result.critChance += c.flat.critChance;
        result.critMult += c.flat.critMult;
        result.armorPen += c.flat.armorPen;
        result.evasion += c.flat.evasion;
        result.armor += c.flat.armor;
        result.tenacity += c.flat.tenacity;
        result.shieldMax += c.flat.shieldMax;
        result.shieldRegen += c.flat.shieldRegen;
        result.healthMax += c.flat.healthMax;
        result.healthRegen += c.flat.healthRegen;
        result.cooldownReduction += c.flat.cooldownReduction;
        result.resourceRegen += c.flat.resourceRegen;
        result.goldGainMult += c.flat.goldGainMult;
        result.rarityScore += c.flat.rarityScore;
        for (std::size_t i = 0; i < result.resists.values.size(); ++i) {
            result.resists.values[i] += c.flat.resists.values[i];
        }
    }

    // Then multiplicative modifiers.
    auto applyMult = [](float value, float mult) {
        // mult is additive multiplier (0.10 => +10%)
        return value * (1.0f + mult);
    };
    for (const auto& c : input.contributions) {
        result.attackPower = applyMult(result.attackPower, c.mult.attackPower);
        result.spellPower = applyMult(result.spellPower, c.mult.spellPower);
        result.attackSpeed = applyMult(result.attackSpeed, c.mult.attackSpeed);
        result.moveSpeed = applyMult(result.moveSpeed, c.mult.moveSpeed);
        result.accuracy = applyMult(result.accuracy, c.mult.accuracy);
        result.critChance = applyMult(result.critChance, c.mult.critChance);
        result.critMult = applyMult(result.critMult, c.mult.critMult);
        result.armorPen = applyMult(result.armorPen, c.mult.armorPen);
        result.evasion = applyMult(result.evasion, c.mult.evasion);
        result.armor = applyMult(result.armor, c.mult.armor);
        result.tenacity = applyMult(result.tenacity, c.mult.tenacity);
        result.shieldMax = applyMult(result.shieldMax, c.mult.shieldMax);
        result.shieldRegen = applyMult(result.shieldRegen, c.mult.shieldRegen);
        result.healthMax = applyMult(result.healthMax, c.mult.healthMax);
        result.healthRegen = applyMult(result.healthRegen, c.mult.healthRegen);
        result.cooldownReduction = applyMult(result.cooldownReduction, c.mult.cooldownReduction);
        result.resourceRegen = applyMult(result.resourceRegen, c.mult.resourceRegen);
        result.goldGainMult = applyMult(result.goldGainMult, c.mult.goldGainMult);
        result.rarityScore = applyMult(result.rarityScore, c.mult.rarityScore);
        for (std::size_t i = 0; i < result.resists.values.size(); ++i) {
            result.resists.values[i] = applyMult(result.resists.values[i], c.mult.resists.values[i]);
        }
    }

    // Clamp crit chance and CDR to sane ranges.
    result.critChance = std::clamp(result.critChance, 0.0f, 0.95f);
    result.cooldownReduction = std::clamp(result.cooldownReduction, -0.9f, 0.9f);
    // Resistances are clamped at resolve time; keep broad range here.
    return result;
}

}  // namespace Engine::Gameplay::RPG

