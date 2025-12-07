#include "MovementSystem.h"

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"

namespace Game {

void MovementSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity>(
        [dt](Engine::ECS::Entity /*e*/, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel) {
            tf.position += vel.value * dt;
        });
}

}  // namespace Game
