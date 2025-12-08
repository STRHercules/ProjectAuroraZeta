// Manages rotating map hotzones that grant temporary bonuses.
#pragma once

#include <vector>
#include <random>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/math/Vec2.h"

namespace Game {

class HotzoneSystem {
public:
    enum class Bonus { XpSurge, DangerPay, WarpFlux };

    struct Zone {
        Engine::Vec2 center{};
        float radius{160.0f};
        Bonus bonus{Bonus::XpSurge};
        Engine::ECS::Entity entity{Engine::ECS::kInvalidEntity};
    };

    HotzoneSystem(float rotationInterval, float xpBonus, float creditBonus,
                  float fluxDamageBonus, float fluxRateBonus,
                  float mapRadius, float minRadius, float maxRadius, float minSeparation, float minSpawnClearance,
                  uint32_t seed = 1337u);

    void initialize(Engine::ECS::Registry& registry, int zoneCount);
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero);

    float xpMultiplier() const { return heroInsideActive_ && activeBonus_ == Bonus::XpSurge ? xpMultiplier_ : 1.0f; }
    float creditMultiplier() const { return heroInsideActive_ && activeBonus_ == Bonus::DangerPay ? creditMultiplier_ : 1.0f; }
    float damageMultiplier() const { return heroInsideActive_ && activeBonus_ == Bonus::WarpFlux ? fluxDamageMultiplier_ : 1.0f; }
    float rateMultiplier() const { return heroInsideActive_ && activeBonus_ == Bonus::WarpFlux ? fluxRateMultiplier_ : 1.0f; }
    Bonus activeBonus() const { return activeBonus_; }
    float timeRemaining() const { return static_cast<float>(rotationTimer_); }
    const Zone* activeZone() const { return zones_.empty() ? nullptr : &zones_[activeIndex_]; }
    bool heroInsideActive() const { return heroInsideActive_; }
    bool warningActive() const { return warningActive_; }

    // Called by game to see if a flux surge should spawn extra elites on activation.
    bool consumeFluxSurge() {
        bool out = fluxSurgePending_;
        fluxSurgePending_ = false;
        return out;
    }

private:
    void rotateZone(Engine::ECS::Registry& registry);
    void rerollZones(Engine::ECS::Registry& registry);
    void updateVisuals(Engine::ECS::Registry& registry);
    Engine::Vec2 samplePosition();

    std::vector<Zone> zones_;
    int activeIndex_{0};
    Bonus activeBonus_{Bonus::XpSurge};
    double rotationInterval_{60.0};
    double rotationTimer_{60.0};
    float xpMultiplier_{1.25f};
    float creditMultiplier_{1.5f};
    float fluxDamageMultiplier_{1.2f};
    float fluxRateMultiplier_{1.15f};
    bool heroInsideActive_{false};
    bool fluxSurgePending_{false};
    float mapRadius_{900.0f};
    float minRadius_{140.0f};
    float maxRadius_{220.0f};
    float minSeparation_{700.0f};
    float minSpawnClearance_{300.0f};
    double warningWindow_{3.0};
    bool warningActive_{false};
    std::mt19937 rng_;
};

}  // namespace Game
