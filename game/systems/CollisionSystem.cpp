#include "CollisionSystem.h"

#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Projectile.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Tags.h"
#include "../components/HitFlash.h"
#include "../components/DamageNumber.h"
#include "../components/Invulnerable.h"

namespace {
bool aabbOverlap(const Engine::ECS::Transform& ta, const Engine::ECS::AABB& aa, const Engine::ECS::Transform& tb,
                 const Engine::ECS::AABB& ab) {
    return std::abs(ta.position.x - tb.position.x) <= (aa.halfExtents.x + ab.halfExtents.x) &&
           std::abs(ta.position.y - tb.position.y) <= (aa.halfExtents.y + ab.halfExtents.y);
}
}  // namespace

namespace Game {

void CollisionSystem::update(Engine::ECS::Registry& registry) {
    std::vector<Engine::ECS::Entity> deadProjectiles;

    // Projectile hits enemy.
    registry.view<Engine::ECS::Transform, Engine::ECS::Projectile, Engine::ECS::AABB, Engine::ECS::ProjectileTag>(
        [&](Engine::ECS::Entity projEnt, Engine::ECS::Transform& projTf, Engine::ECS::Projectile& proj,
            Engine::ECS::AABB& projBox, Engine::ECS::ProjectileTag&) {
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity targetEnt, Engine::ECS::Transform& tgtTf, Engine::ECS::Health& health,
                    Engine::ECS::AABB& tgtBox, Engine::ECS::EnemyTag&) {
                    if (!health.alive()) return;
                    if (aabbOverlap(projTf, projBox, tgtTf, tgtBox)) {
                        health.current -= proj.damage;
                        deadProjectiles.push_back(projEnt);
                        if (health.current < 0.0f) health.current = 0.0f;
                        // Kick a brief hit flash for feedback on the enemy.
                        if (auto* flash = registry.get<Game::HitFlash>(targetEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(targetEnt, Game::HitFlash{0.12f});
                        }
                        // Spawn a floating damage number.
                        auto dn = registry.create();
                        registry.emplace<Engine::ECS::Transform>(dn, tgtTf.position);
                        registry.emplace<Game::DamageNumber>(dn, Game::DamageNumber{proj.damage, 0.8f, {0.0f, -30.0f}});
                    }
                });
        });

    // Enemy contact damages hero.
    registry.view<Engine::ECS::Transform, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity /*enemyEnt*/, Engine::ECS::Transform& enemyTf, Engine::ECS::EnemyTag& /*tag*/,
            Engine::ECS::AABB& enemyBox) {
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Engine::ECS::HeroTag>(
                [&](Engine::ECS::Entity heroEnt, Engine::ECS::Transform& heroTf, Engine::ECS::Health& heroHp,
                    Engine::ECS::AABB& heroBox, Engine::ECS::HeroTag&) {
                    if (!heroHp.alive()) return;
                    if (auto* inv = registry.get<Game::Invulnerable>(heroEnt)) {
                        if (inv->timer > 0.0f) return;
                    }
                    if (aabbOverlap(enemyTf, enemyBox, heroTf, heroBox)) {
                        heroHp.current -= contactDamage_;
                        if (heroHp.current < 0.0f) heroHp.current = 0.0f;
                    }
                });
        });

    for (auto e : deadProjectiles) {
        registry.destroy(e);
    }
}

}  // namespace Game
