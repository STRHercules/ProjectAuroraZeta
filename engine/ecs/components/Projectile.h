// Projectile component: simple linear projectile with damage and lifetime.
#pragma once

#include "../Entity.h"
#include "../../math/Vec2.h"
#include "../../gameplay/Combat.h"

namespace Engine::ECS {

struct Projectile {
    Vec2 velocity{};
    Engine::Gameplay::DamageEvent damage{};
    float lifetime{2.0f};  // seconds
    float lifesteal{0.0f};
    int chain{0};  // remaining bounces
    Entity owner{kInvalidEntity};
};

}  // namespace Engine::ECS
