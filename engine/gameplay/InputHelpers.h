// Convenience helpers for interpreting input in world space.
#pragma once

#include "../render/CameraUtil.h"
#include "../input/InputState.h"

namespace Engine::Gameplay {

inline Vec2 mouseWorldPosition(const InputState& input, const Camera2D& cam, int viewportW, int viewportH) {
    return screenToWorld(Vec2{static_cast<float>(input.mouseX()), static_cast<float>(input.mouseY())}, cam,
                         static_cast<float>(viewportW), static_cast<float>(viewportH));
}

}  // namespace Engine::Gameplay
