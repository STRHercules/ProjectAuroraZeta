// Implementation of the RPG combat resolver.
#include "RPGCombat.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Engine::Gameplay::RPG {

namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

float armorMitigation(float armor, float armorPen, float k) {
    float effectiveArmor = std::max(0.0f, armor - armorPen);
    return effectiveArmor / (effectiveArmor + k);
}

float applyResistClamp(float v, const ResolverConfig& cfg) {
    return std::clamp(v, cfg.resistMin, cfg.resistMax);
}

float shapedRoll(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float a = dist(rng);
    float b = dist(rng);
    return (a + b) * 0.5f;
}

float singleRoll(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

}  // namespace

float rollUnit(std::mt19937& rng, bool shaped) {
    return shaped ? shapedRoll(rng) : singleRoll(rng);
}

HitOutcome ResolveHit(const CombatantState& attacker,
                      CombatantState& defender,
                      const AttackDef& attack,
                      std::mt19937& rng,
                      const ResolverConfig& cfg,
                      CCFatigueState* ccState) {
    HitOutcome out{};
    std::ostringstream dbg;
    dbg.setf(std::ios::fixed);
    dbg.precision(3);

    // --- Hit quality (miss / glance / hit) ---
    float hitChance = attack.baseHitChance + (attacker.stats.accuracy - defender.stats.evasion) * 0.01f;
    hitChance = clamp01(hitChance);
    float qualityRoll = rollUnit(rng, cfg.rng.shaped);
    if (qualityRoll > hitChance) {
        out.quality = HitQuality::Miss;
        out.rawDamage = 0.0f;
        out.debug = "MISS (roll=" + std::to_string(qualityRoll) + " > hit=" + std::to_string(hitChance) + ")";
        return out;
    }
    float glanceChance = std::clamp(attack.baseGlanceChance - attacker.stats.accuracy * 0.0025f, 0.0f, 0.6f);
    float glanceRoll = rollUnit(rng, cfg.rng.shaped);
    bool glanced = glanceRoll < glanceChance;
    out.quality = glanced ? HitQuality::Glance : HitQuality::Hit;

    // --- Defensive reaction: Dodge/Parry ---
    float dodgeChance = std::clamp(attack.baseDodgeChance + defender.stats.evasion * 0.01f, 0.0f, cfg.dodgeCap);
    if (rollUnit(rng, cfg.rng.shaped) < dodgeChance) {
        out.dodged = true;
        out.debug = "DODGE (" + std::to_string(dodgeChance) + ")";
        return out;
    }
    float parryChance = std::clamp(attack.baseParryChance + defender.stats.tenacity * 0.005f, 0.0f, 0.6f);
    if (rollUnit(rng, cfg.rng.shaped) < parryChance) {
        out.parried = true;
        out.debug = "PARRY (" + std::to_string(parryChance) + ")";
        return out;
    }

    // --- Crit ---
    float critChance = std::clamp(attacker.stats.critChance, 0.0f, cfg.critCap);
    out.crit = rollUnit(rng, cfg.rng.shaped) < critChance;

    // --- Damage roll band ---
    float roll = attack.rollMin + (attack.rollMax - attack.rollMin) * rollUnit(rng, attack.shapedRoll && cfg.rng.shaped);
    out.rollMultiplier = roll;

    // --- Raw damage before mitigation ---
    float scale = (attack.scaling == Scaling::SpellPower) ? attacker.stats.spellPower : attacker.stats.attackPower;
    float dmg = attack.baseDamage + scale;
    if (glanced) dmg *= attack.glanceMultiplier;
    if (out.crit) dmg *= attacker.stats.critMult;
    dmg *= roll;
    out.rawDamage = dmg;

    // --- Mitigation: armor then resist ---
    float armorDR = (attack.type == DamageType::Physical || attack.type == DamageType::True)
                        ? armorMitigation(defender.stats.armor, attacker.stats.armorPen, cfg.armorConstant)
                        : 0.0f;
    float afterArmor = dmg * (1.0f - armorDR);
    float resist = applyResistClamp(defender.stats.resists.get(attack.type), cfg);
    float afterResist = afterArmor * (1.0f - resist);

    // --- Shields ---
    float shields = std::max(0.0f, defender.currentShields);
    float shieldDamage = std::min(afterResist, shields);
    float healthDamage = afterResist - shieldDamage;
    float finalHealth = std::max(0.0f, defender.currentHealth - healthDamage);

    out.mitigatedDamage = afterResist;
    out.shieldDamage = shieldDamage;
    out.healthDamage = healthDamage;

    // --- Status applications (saving throw via tenacity) ---
    for (const auto& st : attack.onHitStatuses) {
        StatusApplicationResult res{};
        res.id = st.id;
        float chance = st.baseChance - defender.stats.tenacity * 0.01f;
        chance = clamp01(chance);
        float durMul = 1.0f - defender.stats.tenacity * 0.01f;
        durMul = std::clamp(durMul, 0.25f, 1.0f);
        if (st.isCrowdControl && ccState) {
            // Simple DR: 0 -> full, 1 -> 70%, 2 -> 50%, 3 -> immune
            const float drTable[4] = {1.0f, 0.7f, 0.5f, 0.0f};
            int level = std::clamp(ccState->level, 0, 3);
            durMul *= drTable[level];
            if (ccState->remaining <= 0.0f) {
                ccState->level = 0;
            }
            if (durMul <= 0.0f) {
                res.applied = false;
                res.finalDuration = 0.0f;
                out.statuses.push_back(res);
                continue;
            }
        }
        if (rollUnit(rng, cfg.rng.shaped) < chance) {
            res.applied = true;
            res.finalDuration = st.baseDuration * durMul;
            if (st.isCrowdControl && ccState) {
                ccState->level = std::min(3, ccState->level + 1);
                ccState->remaining = std::max(ccState->remaining, st.baseDuration * 1.0f);
            }
        }
        out.statuses.push_back(res);
    }

    dbg << "hit=" << hitChance << " glance=" << (glanced ? 1 : 0)
        << " crit=" << (out.crit ? 1 : 0) << " roll=" << out.rollMultiplier
        << " armorDR=" << armorDR << " resist=" << applyResistClamp(defender.stats.resists.get(attack.type), cfg)
        << " dmg=" << out.healthDamage << " shield=" << out.shieldDamage;
    out.debug = dbg.str();
    (void)finalHealth;  // currently informational; caller owns state mutation.
    return out;
}

}  // namespace Engine::Gameplay::RPG

