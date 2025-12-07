// Simple 2D camera with position and zoom.
#pragma once

#include "../math/Vec2.h"

namespace Engine {

struct Camera2D {
    Vec2 position{0.0f, 0.0f};
    float zoom{1.0f};
};

}  // namespace Engine
