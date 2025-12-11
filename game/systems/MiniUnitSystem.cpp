#include "MiniUnitSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/MiniUnit.h"
#include "../components/MiniUnitCommand.h"

namespace Game {

void MiniUnitSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    (void)step;
    const float arriveDist2 = stopDistance_ * stopDistance_;
    registry.view<Game::MiniUnit, Engine::ECS::Transform, Engine::ECS::Velocity, Game::MiniUnitCommand>(
        [&](Engine::ECS::Entity, Game::MiniUnit&, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel,
            Game::MiniUnitCommand& cmd) {
            if (!cmd.hasOrder) {
                // Let movement system decay toward zero if no order.
                return;
            }
            Engine::Vec2 delta{cmd.target.x - tf.position.x, cmd.target.y - tf.position.y};
            float dist2 = delta.x * delta.x + delta.y * delta.y;
            if (dist2 <= arriveDist2) {
                cmd.hasOrder = false;
                vel.value = {0.0f, 0.0f};
                return;
            }
            float invLen = 1.0f / std::sqrt(std::max(dist2, 0.0001f));
            vel.value = {delta.x * invLen * defaultMoveSpeed_, delta.y * invLen * defaultMoveSpeed_};
        });
}

}  // namespace Game
