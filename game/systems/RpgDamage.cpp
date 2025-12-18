// Helpers for applying damage through either legacy Combat.h or the new RPG resolver.
#include "RpgDamage.h"

#include <algorithm>
#include <sstream>

#include "../../engine/ecs/components/RPGStats.h"
#include "../status/ZetaStatusFactory.h"

namespace Game::RpgDamage {

namespace {

constexpr bool isValidRpgDamageTypeInt(int v) {
    return v >= 0 && v < static_cast<int>(Engine::Gameplay::RPG::DamageType::Count);
}

Engine::Gameplay::RPG::DamageType toRpgDamage(Engine::Gameplay::DamageType t) {
    switch (t) {
        case Engine::Gameplay::DamageType::Normal: return Engine::Gameplay::RPG::DamageType::Physical;
        case Engine::Gameplay::DamageType::Spell: return Engine::Gameplay::RPG::DamageType::Arcane;
        case Engine::Gameplay::DamageType::True: return Engine::Gameplay::RPG::DamageType::True;
        default: return Engine::Gameplay::RPG::DamageType::Physical;
    }
}

Engine::Gameplay::RPG::DerivedStats computeDerivedFromHealth(const Engine::ECS::Health& hp) {
    Engine::Gameplay::RPG::DerivedStats out{};
    out.healthMax = hp.maxHealth;
    out.shieldMax = hp.maxShields;
    out.armor = hp.healthArmor;
    out.critChance = 0.0f;
    out.critMult = 1.5f;
    out.tenacity = 0.0f;
    return out;
}

Game::ZetaStatusFactory& statusFactory() {
    static Game::ZetaStatusFactory factory{};
    static bool loaded = false;
    if (!loaded) {
        (void)factory.load("data/statuses.json");
        loaded = true;
    }
    return factory;
}

void refreshRpgDerivedIfNeeded(Engine::ECS::Registry& registry, Engine::ECS::Entity ent) {
    auto* rpg = registry.get<Engine::ECS::RPGStats>(ent);
    if (!rpg) return;
    if (rpg->baseFromHealth) {
        if (const auto* hp = registry.get<Engine::ECS::Health>(ent)) {
            rpg->base.baseHealth = hp->maxHealth;
            rpg->base.baseShields = hp->maxShields;
            rpg->base.baseArmor = hp->healthArmor;
        }
    }
    if (!rpg->dirty) return;
    Engine::Gameplay::RPG::AggregationInput input{};
    input.attributes = rpg->attributes;
    input.base = rpg->base;
    input.contributions = {rpg->modifiers};
    rpg->derived = Engine::Gameplay::RPG::aggregateDerivedStats(input);
    rpg->dirty = false;
}

Engine::ECS::RPGCombatState* ensureCombatState(Engine::ECS::Registry& registry, Engine::ECS::Entity ent) {
    if (ent == Engine::ECS::kInvalidEntity) return nullptr;
    if (auto* st = registry.get<Engine::ECS::RPGCombatState>(ent)) return st;
    return &registry.emplace<Engine::ECS::RPGCombatState>(ent, Engine::ECS::RPGCombatState{});
}

}  // namespace

float apply(Engine::ECS::Registry& registry,
            Engine::ECS::Entity attacker,
            Engine::ECS::Entity defender,
            Engine::ECS::Health& defenderHp,
            const Engine::Gameplay::DamageEvent& dmg,
            const Engine::Gameplay::BuffState& buff,
            bool useRpgCombat,
            const Engine::Gameplay::RPG::ResolverConfig& rpgCfg,
            std::mt19937& rng,
            std::string_view label,
            const DebugSink& debugSink,
            const OutcomeSink& outcomeSink) {
    if (!useRpgCombat) {
        const float pre = defenderHp.currentHealth + defenderHp.currentShields;
        Engine::Gameplay::applyDamage(defenderHp, dmg, buff);
        const float post = defenderHp.currentHealth + defenderHp.currentShields;
        return std::max(0.0f, pre - post);
    }

    Engine::Gameplay::RPG::CombatantState atk{};
    float powerRatio = 1.0f;
    bool isSpell = (dmg.type == Engine::Gameplay::DamageType::Spell);
    if (attacker != Engine::ECS::kInvalidEntity) {
        refreshRpgDerivedIfNeeded(registry, attacker);
        if (auto* rpg = registry.get<Engine::ECS::RPGStats>(attacker)) {
            atk.stats = rpg->derived;
            const float basePower = std::max(1.0f, isSpell ? rpg->base.baseSpellPower : rpg->base.baseAttackPower);
            const float currentPower = std::max(0.0f, isSpell ? rpg->derived.spellPower : rpg->derived.attackPower);
            powerRatio = currentPower / basePower;
        } else if (auto* hp = registry.get<Engine::ECS::Health>(attacker)) {
            (void)hp;
        }
    }
    // Fall back to legacy behavior (DamageEvent drives all scaling) if we don't have an attacker RPG snapshot.
    if (attacker == Engine::ECS::kInvalidEntity || (atk.stats.attackPower == 0.0f && atk.stats.spellPower == 0.0f)) {
        atk.stats.attackPower = dmg.baseDamage;
        atk.stats.spellPower = dmg.baseDamage;
        powerRatio = 1.0f;
    }

    Engine::Gameplay::RPG::CombatantState def{};
    def.currentHealth = defenderHp.currentHealth;
    def.currentShields = defenderHp.currentShields;
    if (defender != Engine::ECS::kInvalidEntity) {
        refreshRpgDerivedIfNeeded(registry, defender);
        if (auto* rpg = registry.get<Engine::ECS::RPGStats>(defender)) {
            def.stats = rpg->derived;
        } else {
            def.stats = computeDerivedFromHealth(defenderHp);
        }
    } else {
        def.stats = computeDerivedFromHealth(defenderHp);
    }
    // Apply legacy buff armor deltas as a final adjustment.
    def.stats.armor += buff.healthArmorBonus;
    def.stats.healthMax = std::max(def.stats.healthMax, defenderHp.maxHealth);
    def.stats.shieldMax = std::max(def.stats.shieldMax, defenderHp.maxShields);

    Engine::Gameplay::RPG::AttackDef attack{};
    attack.baseDamage = 0.0f;
    attack.scaling = isSpell ? Engine::Gameplay::RPG::Scaling::SpellPower : Engine::Gameplay::RPG::Scaling::AttackPower;
    if (isValidRpgDamageTypeInt(dmg.rpgDamageType)) {
        attack.type = static_cast<Engine::Gameplay::RPG::DamageType>(dmg.rpgDamageType);
    } else {
        attack.type = toRpgDamage(dmg.type);
    }
    attack.rollMin = 0.85f;
    attack.rollMax = 1.15f;
    attack.baseHitChance = 1.0f;
    attack.baseGlanceChance = 0.0f;
    attack.shapedRoll = true;

    // Carry RPG on-hit status intents from DamageEvent into the resolver.
    for (const auto& s : dmg.rpgOnHitStatuses) {
        if (s.id < 0) continue;
        Engine::Gameplay::RPG::StatusRef ref{};
        ref.id = s.id;
        ref.baseChance = s.baseChance;
        ref.baseDuration = s.baseDuration;
        ref.isCrowdControl = s.isCrowdControl;
        attack.onHitStatuses.push_back(ref);
    }

    // Re-interpret DamageEvent.baseDamage as the baseline "power" for the hit, then apply RPG power as a ratio.
    if (isSpell) {
        atk.stats.spellPower = dmg.baseDamage * powerRatio;
        atk.stats.attackPower = 0.0f;
    } else {
        atk.stats.attackPower = dmg.baseDamage * powerRatio;
        atk.stats.spellPower = 0.0f;
    }

    Engine::ECS::RPGCombatState* atkState = ensureCombatState(registry, attacker);
    Engine::ECS::RPGCombatState* defState = ensureCombatState(registry, defender);
    auto outcome = Engine::Gameplay::RPG::ResolveHit(atk, def, attack, rng, rpgCfg,
                                                     defState ? &defState->ccFatigue : nullptr,
                                                     atkState ? &atkState->prd : nullptr,
                                                     defState ? &defState->prd : nullptr);
    if (outcomeSink) {
        outcomeSink(outcome);
    }
    const float shieldDmg = std::min(defenderHp.currentShields, outcome.shieldDamage * buff.damageTakenMultiplier);
    const float healthDmg = std::min(defenderHp.currentHealth, outcome.healthDamage * buff.damageTakenMultiplier);
    defenderHp.currentShields = std::max(0.0f, defenderHp.currentShields - shieldDmg);
    defenderHp.currentHealth = std::max(0.0f, defenderHp.currentHealth - healthDmg);
    if (defenderHp.currentHealth <= 0.0f) defenderHp.currentHealth = 0.0f;

    // Apply RPG resolver statuses into the engine status system.
    if (defender != Engine::ECS::kInvalidEntity && !outcome.statuses.empty()) {
        if (!registry.has<Engine::ECS::Status>(defender)) {
            registry.emplace<Engine::ECS::Status>(defender, Engine::ECS::Status{});
        }
        if (auto* st = registry.get<Engine::ECS::Status>(defender)) {
            for (const auto& s : outcome.statuses) {
                if (!s.applied) continue;
                if (s.id < 0) continue;
                auto id = static_cast<Engine::Status::EStatusId>(s.id);
                Engine::Status::StatusSpec spec = statusFactory().make(id);
                spec.duration = std::max(0.0f, s.finalDuration);
                if (spec.duration <= 0.0001f) continue;
                (void)st->container.apply(spec, attacker);
            }
        }
    }

    if (debugSink) {
        std::ostringstream line;
        if (!label.empty()) {
            line << "[" << label << "] ";
        }
        line.setf(std::ios::fixed);
        line.precision(2);
        line << (isSpell ? "Spell" : "Phys") << " base=" << dmg.baseDamage << " ratio=" << powerRatio
             << " -> hp=" << healthDmg << " sh=" << shieldDmg << " | " << outcome.debug;
        debugSink(line.str());
    }

    return shieldDmg + healthDmg;
}

}  // namespace Game::RpgDamage
