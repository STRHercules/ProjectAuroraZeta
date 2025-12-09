#include "MovementSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/Facing.h"

namespace Game {

void MovementSystem::setBounds(const Engine::Vec2& min, const Engine::Vec2& max) {
    boundsMin_ = min;
    boundsMax_ = max;
    clampEnabled_ = true;
}

void MovementSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity>(
        [dt, this, &registry](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel) {
            tf.position += vel.value * dt;
            const float vx = vel.value.x;
            if (std::abs(vx) > 0.01f) {
                if (auto* facing = registry.get<Game::Facing>(e)) {
                    facing->dirX = vx >= 0.0f ? 1 : -1;
                }
            }

            // Keep the hero within the configured bounds to avoid leaving the fog/grid.
            if (clampEnabled_ && registry.has<Engine::ECS::HeroTag>(e)) {
                Engine::Vec2 min = boundsMin_;
                Engine::Vec2 max = boundsMax_;
                if (const auto* box = registry.get<Engine::ECS::AABB>(e)) {
                    min.x += box->halfExtents.x;
                    min.y += box->halfExtents.y;
                    max.x -= box->halfExtents.x;
                    max.y -= box->halfExtents.y;
                }
                tf.position.x = std::clamp(tf.position.x, min.x, max.x);
                tf.position.y = std::clamp(tf.position.y, min.y, max.y);
            }
        });
}

}  // namespace Game
