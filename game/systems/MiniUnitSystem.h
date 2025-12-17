#pragma once

#include <functional>
#include <random>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/gameplay/RPGCombat.h"

namespace Game {

class MiniUnitSystem {
public:
    void setDefaultMoveSpeed(float speed) { defaultMoveSpeed_ = speed; }
    void setStopDistance(float distance) { stopDistance_ = distance; }
    void setGlobalDamageMul(float m) { globalDamageMul_ = m; }
    void setGlobalAttackRateMul(float m) { globalAttackRateMul_ = m; }
    void setGlobalHealMul(float m) { globalHealMul_ = m; }
    void setRpgCombat(bool enabled, const Engine::Gameplay::RPG::ResolverConfig& cfg, std::mt19937* rng) {
        useRpgCombat_ = enabled;
        rpgConfig_ = cfg;
        rng_ = rng;
    }
    void setCombatDebugSink(std::function<void(const std::string&)> sink) { debugSink_ = std::move(sink); }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);

private:
    float defaultMoveSpeed_{160.0f};
    float stopDistance_{6.0f};
    float globalDamageMul_{1.0f};
    float globalAttackRateMul_{1.0f};
    float globalHealMul_{1.0f};
    bool useRpgCombat_{false};
    Engine::Gameplay::RPG::ResolverConfig rpgConfig_{};
    std::mt19937* rng_{nullptr};
    std::function<void(const std::string&)> debugSink_{};
};

}  // namespace Game
