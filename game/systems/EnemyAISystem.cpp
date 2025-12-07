#include "EnemyAISystem.h"

#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/EnemyAttributes.h"

namespace Game {

void EnemyAISystem::update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, const Engine::TimeStep& /*step*/) {
    const auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    if (!heroTf) return;
    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity, Game::EnemyAttributes>(
        [heroTf](Engine::ECS::Entity /*e*/, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel,
                 const Game::EnemyAttributes& attr) {
            Engine::Vec2 dir{heroTf->position.x - tf.position.x, heroTf->position.y - tf.position.y};
            // Normalize dir
            float len2 = dir.x * dir.x + dir.y * dir.y;
            if (len2 > 0.0001f) {
                float inv = 1.0f / std::sqrt(len2);
                dir.x *= inv;
                dir.y *= inv;
                vel.value = {dir.x * attr.moveSpeed, dir.y * attr.moveSpeed};
            } else {
                vel.value = {0.0f, 0.0f};
            }
        });
}

}  // namespace Game
