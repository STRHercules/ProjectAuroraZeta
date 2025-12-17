// Simple projectile-enemy collision and damage.
#pragma once

#include <functional>
#include <string>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/gameplay/RPGCombat.h"

namespace Game {

class CollisionSystem {
public:
    void setContactDamage(float dmg) { contactDamage_ = dmg; }
    void setThornConfig(float reflectPercent, float maxReflect) {
        thornReflectPercent_ = reflectPercent;
        thornMaxReflect_ = maxReflect;
    }
    void setXpHooks(int* xpPtr, Engine::ECS::Entity hero, float perDamageDealt, float perDamageTaken) {
        xpPtr_ = xpPtr;
        hero_ = hero;
        xpPerDamageDealt_ = perDamageDealt;
        xpPerDamageTaken_ = perDamageTaken;
    }
    void setRpgCombat(bool enabled, const Engine::Gameplay::RPG::ResolverConfig& cfg) {
        useRpgCombat_ = enabled;
        rpgConfig_ = cfg;
    }
    void setCombatDebugSink(std::function<void(const std::string&)> sink) { debugSink_ = std::move(sink); }
    void update(Engine::ECS::Registry& registry);

private:
    float contactDamage_{10.0f};
    int* xpPtr_{nullptr};
    Engine::ECS::Entity hero_{Engine::ECS::kInvalidEntity};
    float xpPerDamageDealt_{0.0f};
    float xpPerDamageTaken_{0.0f};
    float thornReflectPercent_{0.0f};
    float thornMaxReflect_{0.0f};
    bool useRpgCombat_{false};
    Engine::Gameplay::RPG::ResolverConfig rpgConfig_{};
    std::mt19937 rng_{std::random_device{}()};
    std::function<void(const std::string&)> debugSink_{};
};

}  // namespace Game
