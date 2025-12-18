#include "EnemySpecialSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Projectile.h"
#include "../../engine/ecs/components/RPGStats.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/Status.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/gameplay/RPGCombat.h"
#include "../components/EnemyAttackSwing.h"
#include "../components/EnemyAttributes.h"
#include "../components/EnemyOnHit.h"
#include "../components/EnemyType.h"
#include "../components/Facing.h"
#include "../components/FlameSkullBehavior.h"
#include "../components/LookDirection.h"
#include "../components/SlimeBehavior.h"
#include "../components/Dying.h"
#include "../components/EnemyReviving.h"

namespace Game {

namespace {
constexpr float kMoveEps = 0.01f;

inline bool normalize(Engine::Vec2& v) {
    float len2 = v.x * v.x + v.y * v.y;
    if (len2 <= 0.0001f) return false;
    float inv = 1.0f / std::sqrt(len2);
    v.x *= inv;
    v.y *= inv;
    return true;
}

inline float dist2(const Engine::Vec2& a, const Engine::Vec2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}
}  // namespace

void EnemySpecialSystem::update(Engine::ECS::Registry& registry,
                               const Engine::TimeStep& step,
                               float contactDamageBase,
                               bool /*useRpgCombat*/,
                               const Engine::Gameplay::RPG::ResolverConfig& /*rpgCfg*/) {
    const float dt = static_cast<float>(step.deltaSeconds);

    // Flame skull: ranged fireball at nearest non-cloaked hero in range.
    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Game::FlameSkullBehavior, Engine::ECS::EnemyTag>(
        [&](Engine::ECS::Entity e,
            Engine::ECS::Transform& tf,
            Engine::ECS::Health& hp,
            Game::FlameSkullBehavior& fs,
            Engine::ECS::EnemyTag&) {
            if (!hp.alive()) return;
            if (registry.has<Game::Dying>(e) || registry.has<Game::EnemyReviving>(e)) return;

            fs.timer = std::max(0.0f, fs.timer - dt);
            if (fs.timer > 0.0f) return;

            Engine::ECS::Entity bestHero = Engine::ECS::kInvalidEntity;
            Engine::Vec2 bestPos{};
            float bestD2 = std::numeric_limits<float>::max();
            const float maxR2 = fs.maxRange > 0.0f ? fs.maxRange * fs.maxRange : 0.0f;
            if (maxR2 <= 0.0f) return;

            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::HeroTag>(
                [&](Engine::ECS::Entity h, const Engine::ECS::Transform& htf, const Engine::ECS::Health& hhp, const Engine::ECS::HeroTag&) {
                    if (!hhp.alive()) return;
                    if (auto* st = registry.get<Engine::ECS::Status>(h)) {
                        if (st->container.isStealthed()) return;
                    }
                    float d2 = dist2(tf.position, htf.position);
                    if (d2 > maxR2) return;
                    if (d2 < bestD2) {
                        bestD2 = d2;
                        bestHero = h;
                        bestPos = htf.position;
                    }
                });

            if (bestHero == Engine::ECS::kInvalidEntity) return;
            const float minR2 = fs.minRange > 0.0f ? fs.minRange * fs.minRange : 0.0f;
            if (bestD2 < minR2) return;

            Engine::Vec2 dir{bestPos.x - tf.position.x, bestPos.y - tf.position.y};
            if (!normalize(dir)) return;

            auto proj = registry.create();
            registry.emplace<Engine::ECS::Transform>(proj, tf.position);
            registry.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{dir.x * fs.projectileSpeed, dir.y * fs.projectileSpeed});
            const float half = std::max(1.0f, fs.projectileHitbox * 0.5f);
            registry.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{half, half}});
            Engine::Gameplay::DamageEvent dmg{};
            dmg.type = Engine::Gameplay::DamageType::Spell;
            dmg.rpgDamageType = static_cast<int>(Engine::Gameplay::RPG::DamageType::Fire);
            dmg.baseDamage = std::max(0.0f, contactDamageBase * fs.projectileDamageMul);
            registry.emplace<Engine::ECS::Projectile>(proj,
                                                      Engine::ECS::Projectile{Engine::Vec2{dir.x * fs.projectileSpeed, dir.y * fs.projectileSpeed},
                                                                              dmg,
                                                                              fs.projectileLifetime,
                                                                              0.0f,
                                                                              0,
                                                                              e});
            registry.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});

            if (fireballVis_.texture) {
                const float w = static_cast<float>(std::max(1, fireballVis_.frameWidth));
                const float h = static_cast<float>(std::max(1, fireballVis_.frameHeight));
                registry.emplace<Engine::ECS::Renderable>(
                    proj, Engine::ECS::Renderable{Engine::Vec2{w, h}, Engine::Color{255, 255, 255, 255}, fireballVis_.texture});
                registry.emplace<Engine::ECS::SpriteAnimation>(
                    proj,
                    Engine::ECS::SpriteAnimation{fireballVis_.frameWidth,
                                                 fireballVis_.frameHeight,
                                                 std::max(1, fireballVis_.frameCount),
                                                 fireballVis_.frameDuration});
            }

            // Trigger attack animation rows via the existing swing marker.
            float swingDur = 0.30f;
            if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(e)) {
                float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                swingDur = static_cast<float>(std::max(1, anim->frameCount)) * fd;
            }
            if (auto* swing = registry.get<Game::EnemyAttackSwing>(e)) {
                swing->timer = swingDur;
                swing->targetDir = dir;
            } else {
                registry.emplace<Game::EnemyAttackSwing>(e, Game::EnemyAttackSwing{swingDur, dir});
            }

            fs.timer = std::max(0.05f, fs.cooldown);
        });

    // Slime: periodic multiplication (clone self).
    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Game::SlimeBehavior, Engine::ECS::EnemyTag>(
        [&](Engine::ECS::Entity e,
            Engine::ECS::Transform& tf,
            Engine::ECS::Health& hp,
            Game::SlimeBehavior& slime,
            Engine::ECS::EnemyTag&) {
            if (!hp.alive()) return;
            if (registry.has<Game::Dying>(e) || registry.has<Game::EnemyReviving>(e)) return;
            if (slime.remainingMultiplies <= 0) return;
            if (slime.interval <= 0.0f || slime.chance <= 0.0f) return;

            slime.timer -= dt;
            if (slime.timer > 0.0f) return;
            slime.timer = slime.interval;

            std::uniform_real_distribution<float> roll01(0.0f, 1.0f);
            if (roll01(rng_) > slime.chance) return;

            int lo = std::max(0, slime.spawnMin);
            int hi = std::max(lo, slime.spawnMax);
            if (hi <= 0) return;
            std::uniform_int_distribution<int> countDist(lo, hi);
            int count = countDist(rng_);
            count = std::clamp(count, 0, 32);  // safety cap per tick
            if (count <= 0) return;

            // Cache components we want to clone (optional ones are fetched per spawn).
            const auto* rend = registry.get<Engine::ECS::Renderable>(e);
            const auto* box = registry.get<Engine::ECS::AABB>(e);
            const auto* attr = registry.get<Game::EnemyAttributes>(e);
            const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(e);
            const auto* type = registry.get<Game::EnemyType>(e);
            const auto* onHit = registry.get<Game::EnemyOnHit>(e);
            const auto* look = registry.get<Game::LookDirection>(e);

            std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
            std::uniform_real_distribution<float> rad(14.0f, 30.0f);

            for (int i = 0; i < count; ++i) {
                float a = ang(rng_);
                float r = rad(rng_);
                Engine::Vec2 pos{tf.position.x + std::cos(a) * r, tf.position.y + std::sin(a) * r};

                auto child = registry.create();
                registry.emplace<Engine::ECS::Transform>(child, pos);
                registry.emplace<Engine::ECS::Velocity>(child, Engine::Vec2{0.0f, 0.0f});
                registry.emplace<Game::Facing>(child, Game::Facing{1});
                registry.emplace<Game::LookDirection>(child, look ? *look : Game::LookDirection{});

                if (rend) {
                    registry.emplace<Engine::ECS::Renderable>(child, *rend);
                }
                if (box) {
                    registry.emplace<Engine::ECS::AABB>(child, *box);
                }
                Engine::ECS::Health childHp = hp;
                childHp.currentHealth = childHp.maxHealth;
                childHp.currentShields = childHp.maxShields;
                registry.emplace<Engine::ECS::Health>(child, childHp);

                if (auto* rpg = registry.get<Engine::ECS::RPGStats>(e)) {
                    auto copy = *rpg;
                    copy.dirty = true;
                    registry.emplace<Engine::ECS::RPGStats>(child, copy);
                }
                registry.emplace<Engine::ECS::Status>(child, Engine::ECS::Status{});
                registry.emplace<Engine::ECS::EnemyTag>(child, Engine::ECS::EnemyTag{});
                if (attr) registry.emplace<Game::EnemyAttributes>(child, *attr);
                if (type) registry.emplace<Game::EnemyType>(child, *type);
                if (onHit) registry.emplace<Game::EnemyOnHit>(child, *onHit);
                if (anim) {
                    auto a2 = *anim;
                    a2.accumulator = 0.0f;
                    a2.currentFrame = 0;
                    a2.loop = true;
                    a2.holdOnLastFrame = false;
                    a2.finished = false;
                    registry.emplace<Engine::ECS::SpriteAnimation>(child, a2);
                }

                // Slime children can also multiply, but consume a multiply budget.
                auto childSlime = slime;
                childSlime.timer = childSlime.interval;
                childSlime.remainingMultiplies = std::max(0, childSlime.remainingMultiplies - 1);
                registry.emplace<Game::SlimeBehavior>(child, childSlime);
            }

            slime.remainingMultiplies -= 1;
        });
}

}  // namespace Game
