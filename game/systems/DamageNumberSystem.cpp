#include "DamageNumberSystem.h"

#include "../components/DamageNumber.h"
#include "../../engine/ecs/components/Transform.h"

namespace Game {

void DamageNumberSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    std::vector<Engine::ECS::Entity> expired;
    registry.view<Engine::ECS::Transform, Game::DamageNumber>(
        [dt, &expired](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Game::DamageNumber& dn) {
            tf.position += dn.velocity * dt;
            dn.timer -= dt;
            if (dn.timer <= 0.0f) expired.push_back(e);
        });
    for (auto e : expired) registry.destroy(e);
}

}  // namespace Game
