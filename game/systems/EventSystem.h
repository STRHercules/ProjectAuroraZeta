// Spawns and tracks simple mini-events (salvage crates) every 5 waves.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/math/Vec2.h"
#include "../components/EventActive.h"
#include <string>
#include <vector>

namespace Game {

struct EnemyDefinition;

class EventSystem {
public:
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, int wave, int& gold,
                bool inCombat, const Engine::Vec2& heroPos, int salvageReward);
    void notifyTargetKilled(Engine::ECS::Registry& registry, int eventId);
    bool lastSuccess() const { return lastSuccess_; }
    bool lastFail() const { return lastFail_; }
    std::string lastEventLabel() const { return lastEventLabel_; }
    void clearOutcome() { lastSuccess_ = lastFail_ = false; }

private:
    void spawnSalvage(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos);
    void spawnBounty(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId);
    void spawnSpireHunt(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId);
    void tickSpawners(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
    bool eventActive_{false};
    bool lastSuccess_{false};
    bool lastFail_{false};
    std::string lastEventLabel_;
    int lastEventWave_{0};
    int nextEventId_{1};
    int eventCycle_{0};
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
};

}  // namespace Game
