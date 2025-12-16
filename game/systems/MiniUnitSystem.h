#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

class MiniUnitSystem {
public:
    void setDefaultMoveSpeed(float speed) { defaultMoveSpeed_ = speed; }
    void setStopDistance(float distance) { stopDistance_ = distance; }
    void setGlobalDamageMul(float m) { globalDamageMul_ = m; }
    void setGlobalAttackRateMul(float m) { globalAttackRateMul_ = m; }
    void setGlobalHealMul(float m) { globalHealMul_ = m; }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);

private:
    float defaultMoveSpeed_{160.0f};
    float stopDistance_{6.0f};
    float globalDamageMul_{1.0f};
    float globalAttackRateMul_{1.0f};
    float globalHealMul_{1.0f};
};

}  // namespace Game
