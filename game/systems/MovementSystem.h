// Applies velocity to transform each frame.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/math/Vec2.h"

namespace Game {

class MovementSystem {
public:
    // Configure an axis-aligned clamp; optional.
    void setBounds(const Engine::Vec2& min, const Engine::Vec2& max);
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step);

private:
    bool clampEnabled_{false};
    Engine::Vec2 boundsMin_{};
    Engine::Vec2 boundsMax_{};
};

}  // namespace Game
