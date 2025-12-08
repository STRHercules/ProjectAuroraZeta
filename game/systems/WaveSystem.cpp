#include "WaveSystem.h"

#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/math/Vec2.h"
#include "../components/EnemyAttributes.h"
#include "../components/PickupBob.h"
#include "../components/BountyTag.h"
#include "../components/Facing.h"

namespace Game {

WaveSystem::WaveSystem(std::mt19937& rng, WaveSettings settings) : rng_(rng), settings_(settings) {}

const EnemyDefinition* WaveSystem::pickEnemyDef() {
    if (!enemyDefs_ || enemyDefs_->empty()) return nullptr;
    std::uniform_int_distribution<std::size_t> dist(0, enemyDefs_->size() - 1);
    return &((*enemyDefs_)[dist(rng_)]);
}

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
        registry.emplace<Game::Facing>(e, Game::Facing{1});
        const EnemyDefinition* def = pickEnemyDef();
        float hpVal = settings_.enemyHp;
        float speedVal = settings_.enemySpeed;
        float sizeMul = 1.0f;
        if (def) {
            hpVal *= def->hpMultiplier;
            speedVal *= def->speedMultiplier;
            sizeMul = def->sizeMultiplier;
        }
        float size = settings_.enemySize * sizeMul;
        registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                              Engine::Color{255, 255, 255, 255},
                                                                              def ? def->texture : Engine::TexturePtr{}});
        const float hb = settings_.enemyHitbox * 0.5f * sizeMul;
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
        if (def && def->texture) {
            registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{def->frameWidth,
                                                                                           def->frameHeight,
                                                                                           4,
                                                                                           def->frameDuration});
        }
    }

    // Every 5th wave spawn a bounty elite near hero.
    if (wave % 5 == 0) {
        float ang = angleDist(rng_);
        float rad = radiusDist(rng_) * 0.6f;
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Game::Facing>(e, Game::Facing{1});
        const EnemyDefinition* def = pickEnemyDef();
        float sizeMul = def ? def->sizeMultiplier : 1.2f;
        float size = 24.0f * sizeMul;
        float hpVal = settings_.enemyHp * 3.0f * (def ? def->hpMultiplier : 1.0f);
        float speedVal = settings_.enemySpeed * 1.1f * (def ? def->speedMultiplier : 1.0f);
        registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                              Engine::Color{255, 170, 60, 255},
                                                                              def ? def->texture : Engine::TexturePtr{}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
        if (def && def->texture) {
            registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{def->frameWidth,
                                                                                           def->frameHeight,
                                                                                           4,
                                                                                           def->frameDuration});
        }
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
        registry.emplace<Game::Facing>(e, Game::Facing{1});
        const EnemyDefinition* def = pickEnemyDef();
        float sizeMul = def ? def->sizeMultiplier * 1.6f : 2.0f;
        float size = 34.0f * sizeMul;
        float hpVal = settings_.enemyHp * bossHpMul_ * (def ? def->hpMultiplier : 1.0f);
        float speedVal = settings_.enemySpeed * bossSpeedMul_ * (def ? def->speedMultiplier : 1.0f);
        registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                              Engine::Color{200, 80, 200, 255},
                                                                              def ? def->texture : Engine::TexturePtr{}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Engine::ECS::BossTag>(e, Engine::ECS::BossTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
        if (def && def->texture) {
            registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{def->frameWidth,
                                                                                           def->frameHeight,
                                                                                           4,
                                                                                           def->frameDuration});
        }
    }

    return true;
}

}  // namespace Game
