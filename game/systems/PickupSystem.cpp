#include "PickupSystem.h"

#include <cmath>

#include "../components/Pickup.h"
#include "../components/PickupBob.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Tags.h"

namespace {
bool aabbOverlap(const Engine::ECS::Transform& ta, const Engine::ECS::AABB& aa, const Engine::ECS::Transform& tb,
                 const Engine::ECS::AABB& ab) {
    return std::abs(ta.position.x - tb.position.x) <= (aa.halfExtents.x + ab.halfExtents.x) &&
           std::abs(ta.position.y - tb.position.y) <= (aa.halfExtents.y + ab.halfExtents.y);
}
}  // namespace

namespace Game {

void PickupSystem::update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, int& credits) {
    const auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    const auto* heroBox = registry.get<Engine::ECS::AABB>(hero);
    if (!heroTf || !heroBox) return;

    // Animate idle bob.
    const float dt = 1.0f / 60.0f;  // approximate; not timing critical
    registry.view<Engine::ECS::Transform, Game::PickupBob>(
        [dt](Engine::ECS::Entity /*e*/, Engine::ECS::Transform& tf, Game::PickupBob& bob) {
            bob.phase += bob.speed * dt;
            bob.pulsePhase += 2.5f * dt;
            float offset = std::sin(bob.phase) * bob.amplitude;
            tf.position = {bob.basePos.x, bob.basePos.y + offset};
        });

    std::vector<Engine::ECS::Entity> collected;
    registry.view<Engine::ECS::Transform, Engine::ECS::AABB, Game::Pickup, Engine::ECS::PickupTag>(
        [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::AABB& box, Game::Pickup& pickup,
            Engine::ECS::PickupTag&) {
            if (aabbOverlap(*heroTf, *heroBox, tf, box)) {
                if (pickup.type == Game::Pickup::Type::Credits) {
                    credits += pickup.amount;
                }
                collected.push_back(e);
            }
        });

    for (auto e : collected) {
        registry.destroy(e);
    }
}

}  // namespace Game
