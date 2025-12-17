// Full RPG combat resolver with hit-quality, crit, mitigation, shields, and status hooks.
#pragma once

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "RPGStats.h"

namespace Engine::Gameplay::RPG {

enum class HitQuality {
    Miss,
    Glance,
    Hit
};

// Scaling channel for attack definitions.
enum class Scaling {
    AttackPower,
    SpellPower
};

struct StatusRef {
    int id{-1};
    float baseChance{0.0f};   // 0..1
    float baseDuration{0.0f}; // seconds
    bool isCrowdControl{false};
};

// Attacks are defined data-first and re-used for players + AI.
struct AttackDef {
    float baseDamage{0.0f};
    DamageType type{DamageType::Physical};
    Scaling scaling{Scaling::AttackPower};
    float rollMin{0.85f};
    float rollMax{1.15f};
    float baseHitChance{0.9f};
    float baseGlanceChance{0.05f};
    float baseDodgeChance{0.0f};
    float baseParryChance{0.0f};
    float glanceMultiplier{0.5f};
    bool shapedRoll{true};  // average-of-two rolls
    std::vector<StatusRef> onHitStatuses{};
};

struct RNGConfig {
    bool shaped{true};
    bool usePRD{false};
};

// Simple bad-luck protection state for chance rolls (crit/dodge/parry).
// When enabled, we use an accumulator method: acc += p; success if roll < acc; on success acc -= 1.
struct PRDState {
    float critAcc{0.0f};
    float dodgeAcc{0.0f};
    float parryAcc{0.0f};
};

struct ResolverConfig {
    float armorConstant{110.0f};       // K in ARM/(ARM+K)
    float dodgeCap{0.70f};
    float critCap{0.80f};
    float resistMin{-0.75f};
    float resistMax{0.90f};
    bool clampRolls{true};
    RNGConfig rng{};
};

struct StatusApplicationResult {
    int id{-1};
    bool applied{false};
    float finalDuration{0.0f};
};

struct HitOutcome {
    HitQuality quality{HitQuality::Hit};
    bool dodged{false};
    bool parried{false};
    bool crit{false};
    float rollMultiplier{1.0f};
    float rawDamage{0.0f};
    float mitigatedDamage{0.0f};
    float shieldDamage{0.0f};
    float healthDamage{0.0f};
    std::vector<StatusApplicationResult> statuses{};
    std::string debug;
};

// Tracks CC diminishing returns (fatigue).
struct CCFatigueState {
    int level{0};        // 0 = none, 1 = first, 2 = second, 3 = immune window
    float remaining{0.0f};
};

struct CombatantState {
    DerivedStats stats{};
    float currentHealth{0.0f};
    float currentShields{0.0f};
};

float rollUnit(std::mt19937& rng, bool shaped);

HitOutcome ResolveHit(const CombatantState& attacker,
                      CombatantState& defender,
                      const AttackDef& attack,
                      std::mt19937& rng,
                      const ResolverConfig& cfg,
                      CCFatigueState* ccState = nullptr,
                      PRDState* attackerPrd = nullptr,
                      PRDState* defenderPrd = nullptr);

}  // namespace Engine::Gameplay::RPG
