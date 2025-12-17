// Very simple enemy AI: move toward hero.
#pragma once

#include <functional>
#include <random>
#include <string>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/gameplay/RPGCombat.h"

namespace Game {

class EnemyAISystem {
public:
    void setRpgCombat(bool enabled, const Engine::Gameplay::RPG::ResolverConfig& cfg, std::mt19937* rng) {
        useRpgCombat_ = enabled;
        rpgConfig_ = cfg;
        rng_ = rng;
    }
    void setCombatDebugSink(std::function<void(const std::string&)> sink) { debugSink_ = std::move(sink); }
    void update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, const Engine::TimeStep& step);

private:
    bool useRpgCombat_{false};
    Engine::Gameplay::RPG::ResolverConfig rpgConfig_{};
    std::mt19937* rng_{nullptr};
    std::function<void(const std::string&)> debugSink_{};
};

}  // namespace Game
