// Axis-aligned bounding box for simple collision.
#pragma once

#include "../../math/Vec2.h"

namespace Engine::ECS {

struct AABB {
    Vec2 halfExtents{8.0f, 8.0f};  // half-size
};

}  // namespace Engine::ECS
