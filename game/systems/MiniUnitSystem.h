#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class MiniUnitSystem {
public:
    void setDefaultMoveSpeed(float speed) { defaultMoveSpeed_ = speed; }
    void setStopDistance(float distance) { stopDistance_ = distance; }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);

private:
    float defaultMoveSpeed_{160.0f};
    float stopDistance_{6.0f};
};

}  // namespace Game
