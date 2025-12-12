#pragma once

#include "../../engine/ecs/Registry.h"

namespace Game {

struct TauntTarget {
    Engine::ECS::Entity target{Engine::ECS::kInvalidEntity};
    float timer{0.0f};
};

}  // namespace Game

