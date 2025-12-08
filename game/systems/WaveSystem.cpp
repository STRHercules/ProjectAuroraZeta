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
#include "../components/PickupBob.h"
#include "../components/BountyTag.h"

namespace Game {

WaveSystem::WaveSystem(std::mt19937& rng, WaveSettings settings) : rng_(rng), settings_(settings) {}

bool WaveSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero,
                        int& wave) {
    timer_ -= step.deltaSeconds;
    if (timer_ > 0.0) return false;
    timer_ += settings_.interval + settings_.grace;
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
        bool fast = (i % 3 == 0);  // 1-in-3 spawns are runners
        float size = fast ? settings_.enemySize * 0.8f : settings_.enemySize;
        float hpVal = fast ? settings_.enemyHp * 0.6f : settings_.enemyHp;
        float speedVal = fast ? settings_.enemySpeed * 1.6f : settings_.enemySpeed;
        Engine::Color col = fast ? Engine::Color{180, 200, 255, 255} : Engine::Color{200, 80, 80, 255};
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{size, size}, col});
        const float hb = (fast ? settings_.enemyHitbox * 0.7f : settings_.enemyHitbox) * 0.5f;
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
    }

    // Every 5th wave spawn a bounty elite near hero.
    if (wave % 5 == 0) {
        float ang = angleDist(rng_);
        float rad = radiusDist(rng_) * 0.6f;
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{24.0f, 24.0f}, Engine::Color{255, 170, 60, 255}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{12.0f, 12.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{settings_.enemyHp * 3.0f, settings_.enemyHp * 3.0f});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{settings_.enemySpeed * 1.1f});
        registry.emplace<Game::PickupBob>(e, Game::PickupBob{pos, 0.0f, 2.0f, 2.5f});
        registry.emplace<Game::BountyTag>(e, Game::BountyTag{});
    }

    // Boss spawn on milestone.
    if (wave == bossWave_) {
        float ang = angleDist(rng_);
        float rad = 260.0f;
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{34.0f, 34.0f}, Engine::Color{200, 80, 200, 255}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{17.0f, 17.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{settings_.enemyHp * bossHpMul_, settings_.enemyHp * bossHpMul_});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Engine::ECS::BossTag>(e, Engine::ECS::BossTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{settings_.enemySpeed * bossSpeedMul_});
    }

    return true;
}

}  // namespace Game
