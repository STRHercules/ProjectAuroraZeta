// Per-entity RPG combat runtime state (CC diminishing returns + PRD streak shaping).
#pragma once

#include "../../gameplay/RPGCombat.h"

namespace Engine::ECS {

struct RPGCombatState {
    Engine::Gameplay::RPG::CCFatigueState ccFatigue{};
    Engine::Gameplay::RPG::PRDState prd{};
};

}  // namespace Engine::ECS

