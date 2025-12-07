// Renderable rectangle component (placeholder for sprite rendering).
#pragma once

#include "../../math/Vec2.h"
#include "../../render/Color.h"
#include "../../assets/Texture.h"

namespace Engine::ECS {

struct Renderable {
    Vec2 size{16.0f, 16.0f};
    Color color{200, 200, 200, 255};
    Engine::TexturePtr texture{};
};

}  // namespace Engine::ECS
