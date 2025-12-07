// Spawns and tracks simple mini-events (salvage crates) every 5 waves.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/math/Vec2.h"
#include "../components/EventActive.h"

namespace Game {

class EventSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, int wave, int& credits,
                bool inCombat, const Engine::Vec2& heroPos);
    bool lastSuccess() const { return lastSuccess_; }
    bool lastFail() const { return lastFail_; }
    void clearOutcome() { lastSuccess_ = lastFail_ = false; }

private:
    void spawnSalvage(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos);
    bool eventActive_{false};
    bool lastSuccess_{false};
    bool lastFail_{false};
};

}  // namespace Game
