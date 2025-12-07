// Helpers to convert between screen and world space for Camera2D.
#pragma once

#include "Camera2D.h"
#include "../math/Vec2.h"

namespace Engine {

inline Vec2 screenToWorld(const Vec2& screen, const Camera2D& cam, float viewportWidth, float viewportHeight) {
    Vec2 center{viewportWidth * 0.5f, viewportHeight * 0.5f};
    Vec2 world{};
    world.x = (screen.x - center.x) / cam.zoom + cam.position.x;
    world.y = (screen.y - center.y) / cam.zoom + cam.position.y;
    return world;
}

inline Vec2 worldToScreen(const Vec2& world, const Camera2D& cam, float viewportWidth, float viewportHeight) {
    Vec2 center{viewportWidth * 0.5f, viewportHeight * 0.5f};
    Vec2 screen{};
    screen.x = (world.x - cam.position.x) * cam.zoom + center.x;
    screen.y = (world.y - cam.position.y) * cam.zoom + center.y;
    return screen;
}

}  // namespace Engine
