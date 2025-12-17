// Helpers for applying damage through either legacy Combat.h or the new RPG resolver.
#pragma once

#include <functional>
#include <random>
#include <string>
#include <string_view>

#include "../../engine/ecs/Registry.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/gameplay/Combat.h"
#include "../../engine/gameplay/RPGCombat.h"

namespace Game::RpgDamage {

using DebugSink = std::function<void(const std::string&)>;

// Applies damage to `defenderHp` (which must correspond to `defender`) and returns total damage dealt (shield+health).
// When `useRpgCombat` is enabled, uses Engine::Gameplay::RPG::ResolveHit() with per-entity RPG snapshots.
float apply(Engine::ECS::Registry& registry,
            Engine::ECS::Entity attacker,
            Engine::ECS::Entity defender,
            Engine::ECS::Health& defenderHp,
            const Engine::Gameplay::DamageEvent& dmg,
            const Engine::Gameplay::BuffState& buff,
            bool useRpgCombat,
            const Engine::Gameplay::RPG::ResolverConfig& rpgCfg,
            std::mt19937& rng,
            std::string_view label = {},
            const DebugSink& debugSink = {});

}  // namespace Game::RpgDamage

