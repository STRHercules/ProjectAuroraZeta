// Health component for damageable entities.
#pragma once

namespace Engine::ECS {

struct Health {
    float current{100.0f};
    float max{100.0f};
    bool alive() const { return current > 0.0f; }
};

}  // namespace Engine::ECS
