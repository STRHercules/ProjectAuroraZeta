#include "MovementSystem.h"

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/Facing.h"
#include <cmath>

namespace Game {

void MovementSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity>(
        [dt, &registry](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel) {
            tf.position += vel.value * dt;
            const float vx = vel.value.x;
            if (std::abs(vx) > 0.01f) {
                if (auto* facing = registry.get<Game::Facing>(e)) {
                    facing->dirX = vx >= 0.0f ? 1 : -1;
                }
            }
        });
}

}  // namespace Game
