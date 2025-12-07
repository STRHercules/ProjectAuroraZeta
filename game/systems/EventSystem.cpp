#include "EventSystem.h"

#include <algorithm>

#include "../components/EventActive.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/math/Vec2.h"
#include <random>

namespace Game {

void EventSystem::spawnSalvage(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos) {
    // Spawn toward a random edge from hero to encourage movement.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    float ang = angleDist(rng);
    float dist = 420.0f;
    Engine::Vec2 pos{heroPos.x + std::cos(ang) * dist, heroPos.y + std::sin(ang) * dist};
    auto e = registry.create();
    registry.emplace<Engine::ECS::Transform>(e, pos);
    registry.emplace<Engine::ECS::Renderable>(e,
        Engine::ECS::Renderable{Engine::Vec2{28.0f, 28.0f}, Engine::Color{120, 200, 255, 255}});
    registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{14.0f, 14.0f}});
    registry.emplace<Game::EventActive>(e, Game::EventActive{30.0f, 30.0f, false, false, false, true});
    registry.emplace<Engine::ECS::PickupTag>(e, Engine::ECS::PickupTag{});
}

void EventSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, int wave, int& credits,
                         bool inCombat, const Engine::Vec2& heroPos) {
    lastSuccess_ = false;
    lastFail_ = false;
    // Trigger salvage every 5th wave start.
    if (wave > 0 && wave % 5 == 0 && !eventActive_) {
        spawnSalvage(registry, heroPos);
        eventActive_ = true;
    }

    // Tick active events.
    registry.view<Game::EventActive, Engine::ECS::Transform, Engine::ECS::Renderable, Engine::ECS::PickupTag>(
        [&](Engine::ECS::Entity e, Game::EventActive& ev, Engine::ECS::Transform&, Engine::ECS::Renderable& rend,
            Engine::ECS::PickupTag&) {
            ev.timer -= static_cast<float>(step.deltaSeconds);
            // Success if player touches (pickup system handles destroy) or timer hits 0 in intermission.
            if (ev.timer <= 0.0f && !ev.failed) {
                ev.success = true;
            }
            if (ev.success) {
                rend.color = Engine::Color{120, 255, 160, 255};
                if (!ev.rewardGranted) {
                    credits += 20;
                    ev.rewardGranted = true;
                }
                registry.destroy(e);
                eventActive_ = false;
                lastSuccess_ = true;
            } else if (ev.timer <= 0.0f && inCombat) {
                ev.failed = true;
                rend.color = Engine::Color{255, 80, 80, 255};
                registry.destroy(e);
                eventActive_ = false;
                lastFail_ = true;
            } else {
                // Pulse color to draw attention.
                float t = std::clamp(ev.timer / 30.0f, 0.0f, 1.0f);
                rend.color = Engine::Color{static_cast<uint8_t>(120 + 80 * (1 - t)), 200, 255, 255};
            }
        });
}

}  // namespace Game
