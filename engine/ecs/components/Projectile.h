// Projectile component: simple linear projectile with damage and lifetime.
#pragma once

#include "../../math/Vec2.h"

namespace Engine::ECS {

struct Projectile {
    Vec2 velocity{};
    float damage{10.0f};
    float lifetime{2.0f};  // seconds
    float lifesteal{0.0f};
    int chain{0};  // remaining bounces
};

}  // namespace Engine::ECS
