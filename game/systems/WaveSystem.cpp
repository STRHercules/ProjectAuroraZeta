#include "WaveSystem.h"

#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/math/Vec2.h"
#include "../components/EnemyAttributes.h"

namespace Game {

WaveSystem::WaveSystem(std::mt19937& rng, WaveSettings settings) : rng_(rng), settings_(settings) {}

void WaveSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero,
                        int& wave) {
    timer_ -= step.deltaSeconds;
    if (timer_ > 0.0) return;
    timer_ += settings_.interval;
    wave++;
    // Simple scaling: every wave increase HP and batch.
    settings_.enemyHp *= 1.08f;
    settings_.enemySpeed *= 1.01f;
    if (wave % 2 == 0 && settings_.batchSize < 12) settings_.batchSize += 1;

    // Spawn enemies in a ring around hero.
    auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    Engine::Vec2 center = heroTf ? heroTf->position : Engine::Vec2{0.0f, 0.0f};

    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radiusDist(240.0f, 320.0f);

    for (int i = 0; i < settings_.batchSize; ++i) {
        float ang = angleDist(rng_);
        float rad = radiusDist(rng_);
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};

        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{18.0f, 18.0f}, Engine::Color{200, 80, 80, 255}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{9.0f, 9.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{settings_.enemyHp, settings_.enemyHp});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{settings_.enemySpeed});
    }
}

}  // namespace Game
