// Tracks escort controller state (progress and entity linkage) for the escort event.
#pragma once

#include "../../engine/ecs/Entity.h"
#include "../../engine/math/Vec2.h"

namespace Game {

struct EscortObjective {
    Engine::ECS::Entity escort{Engine::ECS::kInvalidEntity};
    Engine::Vec2 destination{};
    Engine::Vec2 start{};
    float totalDistance{0.0f};
    float arrivalRadius{14.0f};
};

}  // namespace Game
