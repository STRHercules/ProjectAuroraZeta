#include "ProjectileSystem.h"

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Projectile.h"

namespace Game {

void ProjectileSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    std::vector<Engine::ECS::Entity> toDestroy;
    registry.view<Engine::ECS::Transform, Engine::ECS::Projectile>(
        [dt, &toDestroy](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Projectile& proj) {
            tf.position += proj.velocity * dt;
            proj.lifetime -= dt;
            if (proj.lifetime <= 0.0f) {
                toDestroy.push_back(e);
            }
        });
    for (auto e : toDestroy) {
        registry.destroy(e);
    }
}

}  // namespace Game
