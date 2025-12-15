#pragma once

#include "../../engine/ecs/Entity.h"

namespace Game {

// Marks a mini-unit as a Summoner spawn and keeps it leashed to its owner.
struct SummonedUnit {
    Engine::ECS::Entity owner{Engine::ECS::kInvalidEntity};
    float leashRadius{240.0f};
};

}  // namespace Game
