// Position component (engine-agnostic).
#pragma once

#include "../../math/Vec2.h"

namespace Engine::ECS {

struct Transform {
    Vec2 position{};
};

}  // namespace Engine::ECS
