#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Projectile.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/gameplay/Combat.h"
#include "BuffSystem.h"
#include "../components/HitFlash.h"
#include "../components/DamageNumber.h"
#include "../components/Invulnerable.h"
#include "../components/OffensiveType.h"
#include "../components/MiniUnit.h"
#include "../components/Building.h"

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
                        const float preHealth = health.currentHealth;
                        const float preShields = health.currentShields;

                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(targetEnt)) {
                            buff = armorBuff->state;
                        }
                        Engine::Gameplay::applyDamage(health, proj.damage, buff);

                        const float postHealth = health.currentHealth;
                        const float postShields = health.currentShields;
                        float dealt = (preHealth + preShields) - (postHealth + postShields);
                        if (dealt > 0.0f && dealt < Engine::Gameplay::MIN_DAMAGE_PER_HIT) {
                            dealt = Engine::Gameplay::MIN_DAMAGE_PER_HIT;
                        }
                        deadProjectiles.push_back(projEnt);
                        // Lifesteal to hero if applicable.
                        if (proj.lifesteal > 0.0f) {
                            registry.view<Engine::ECS::Health, Engine::ECS::HeroTag>(
                                [&](Engine::ECS::Entity, Engine::ECS::Health& heroHp, Engine::ECS::HeroTag&) {
                                    float heal = dealt * proj.lifesteal;
                                    heroHp.currentHealth = std::min(heroHp.maxHealth, heroHp.currentHealth + heal);
                                });
                        }
                        // Grant XP for damage dealt.
                        if (xpPtr_ && dealt > 0.0f) {
                            *xpPtr_ += static_cast<int>(std::round(dealt * xpPerDamageDealt_));
                        }
                        // Chain bounce.
                        if (proj.chain > 0) {
                            Engine::ECS::Entity best = Engine::ECS::kInvalidEntity;
                            Engine::Vec2 bestPos{};
                            float bestDist2 = 90000.0f;  // 300 radius squared
                            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                                [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, const Engine::ECS::EnemyTag&) {
                                    if (e == targetEnt || !hp.alive()) return;
                                    float dx = tf.position.x - tgtTf.position.x;
                                    float dy = tf.position.y - tgtTf.position.y;
                                    float d2 = dx * dx + dy * dy;
                                    if (d2 < bestDist2) {
                                        bestDist2 = d2;
                                        bestPos = tf.position;
                                        best = e;
                                    }
                                });
                            if (best != Engine::ECS::kInvalidEntity) {
                                Engine::Vec2 dir{bestPos.x - tgtTf.position.x, bestPos.y - tgtTf.position.y};
                                float len2 = dir.x * dir.x + dir.y * dir.y;
                                float speed = std::sqrt(proj.velocity.x * proj.velocity.x + proj.velocity.y * proj.velocity.y);
                                if (len2 > 0.0001f && speed > 0.0f) {
                                    float inv = 1.0f / std::sqrt(len2);
                                    dir.x *= inv;
                                    dir.y *= inv;
                                    auto np = registry.create();
                                    registry.emplace<Engine::ECS::Transform>(np, tgtTf.position);
                                    registry.emplace<Engine::ECS::Velocity>(np, Engine::Vec2{0.0f, 0.0f});
                                    registry.emplace<Engine::ECS::AABB>(np, projBox);
                                    registry.emplace<Engine::ECS::Renderable>(np,
                                        Engine::ECS::Renderable{Engine::Vec2{projBox.halfExtents.x * 2.0f, projBox.halfExtents.y * 2.0f},
                                                                Engine::Color{255, 230, 140, 220}});
                                    Engine::Gameplay::DamageEvent bounceDmg = proj.damage;
                                    bounceDmg.baseDamage *= 0.8f;
                                    registry.emplace<Engine::ECS::Projectile>(np,
                                        Engine::ECS::Projectile{Engine::Vec2{dir.x * speed, dir.y * speed},
                                                                bounceDmg,
                                                                proj.lifetime,
                                                                proj.lifesteal, proj.chain - 1});
                                    registry.emplace<Engine::ECS::ProjectileTag>(np, Engine::ECS::ProjectileTag{});
                                }
                            }
                        }
                        // Kick a brief hit flash for feedback on the enemy.
                        if (auto* flash = registry.get<Game::HitFlash>(targetEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(targetEnt, Game::HitFlash{0.12f});
                        }
                        // Spawn a floating damage number.
                        auto dn = registry.create();
                        registry.emplace<Engine::ECS::Transform>(dn, tgtTf.position);
                        registry.emplace<Game::DamageNumber>(dn, Game::DamageNumber{dealt, 0.8f, {0.0f, -30.0f}});
                    }
                });
        });

    // Enemy contact damages hero.
    registry.view<Engine::ECS::Transform, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity enemyEnt, Engine::ECS::Transform& enemyTf, Engine::ECS::EnemyTag& /*tag*/,
            Engine::ECS::AABB& enemyBox) {
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Engine::ECS::HeroTag>(
                [&](Engine::ECS::Entity heroEnt, Engine::ECS::Transform& heroTf, Engine::ECS::Health& heroHp,
                    Engine::ECS::AABB& heroBox, Engine::ECS::HeroTag&) {
                    if (!heroHp.alive()) return;
                    if (auto* inv = registry.get<Game::Invulnerable>(heroEnt)) {
                        if (inv->timer > 0.0f) return;
                    }
                    if (aabbOverlap(enemyTf, enemyBox, heroTf, heroBox)) {
                        const float preHealth = heroHp.currentHealth;
                        const float preShields = heroHp.currentShields;
                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(heroEnt)) {
                            buff = armorBuff->state;
                        }
                        Engine::Gameplay::applyDamage(heroHp, contact, buff);
                        float dealt = (preHealth + preShields) - (heroHp.currentHealth + heroHp.currentShields);
                        if (dealt > 0.0f) {
                            if (auto* flash = registry.get<Game::HitFlash>(heroEnt)) {
                                flash->timer = 0.12f;
                            } else {
                                registry.emplace<Game::HitFlash>(heroEnt, Game::HitFlash{0.12f});
                            }
                            if (xpPtr_ && heroEnt == hero_) {
                                *xpPtr_ += static_cast<int>(std::round(dealt * xpPerDamageTaken_));
                            }
                            bool thorny = false;
                            if (auto* ot = registry.get<Game::OffensiveTypeTag>(heroEnt)) {
                                thorny = ot->type == Game::OffensiveType::ThornTank;
                            }
                            if (thorny) {
                                if (auto* enemyHp = registry.get<Engine::ECS::Health>(enemyEnt)) {
                                    float reflect = std::min(thornMaxReflect_, dealt * thornReflectPercent_);
                                    if (reflect > 0.0f) {
                                        Engine::Gameplay::DamageEvent reflectDmg{};
                                        reflectDmg.baseDamage = reflect;
                                        reflectDmg.type = Engine::Gameplay::DamageType::Normal;
                                        Engine::Gameplay::BuffState enemyBuff{};
                                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(enemyEnt)) {
                                            enemyBuff = armorBuff->state;
                                        }
                                        Engine::Gameplay::applyDamage(*enemyHp, reflectDmg, enemyBuff);
                                    }
                                }
                            }
                        }
                    }
                });
        });

    // Enemy contact damages mini units and turrets.
    registry.view<Engine::ECS::Transform, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity enemyEnt, Engine::ECS::Transform& enemyTf, Engine::ECS::EnemyTag& /*tag*/,
            Engine::ECS::AABB& enemyBox) {
            // Mini units.
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Game::MiniUnit>(
                [&](Engine::ECS::Entity miniEnt, Engine::ECS::Transform& miniTf, Engine::ECS::Health& miniHp,
                    Engine::ECS::AABB& miniBox, Game::MiniUnit&) {
                    if (!miniHp.alive()) return;
                    if (aabbOverlap(enemyTf, enemyBox, miniTf, miniBox)) {
                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        contact.type = Engine::Gameplay::DamageType::Normal;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(miniEnt)) {
                            buff = armorBuff->state;
                        }
                        Engine::Gameplay::applyDamage(miniHp, contact, buff);
                        if (auto* flash = registry.get<Game::HitFlash>(miniEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(miniEnt, Game::HitFlash{0.12f});
                        }
                    }
                });
            // Turrets only.
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Game::Building>(
                [&](Engine::ECS::Entity bEnt, Engine::ECS::Transform& bTf, Engine::ECS::Health& bHp,
                    Engine::ECS::AABB& bBox, Game::Building& b) {
                    if (b.type != Game::BuildingType::Turret) return;
                    if (!bHp.alive()) return;
                    if (aabbOverlap(enemyTf, enemyBox, bTf, bBox)) {
                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        contact.type = Engine::Gameplay::DamageType::Normal;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(bEnt)) {
                            buff = armorBuff->state;
                        }
                        Engine::Gameplay::applyDamage(bHp, contact, buff);
                        if (auto* flash = registry.get<Game::HitFlash>(bEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(bEnt, Game::HitFlash{0.12f});
                        }
                    }
                });
        });

    for (auto e : deadProjectiles) {
        registry.destroy(e);
    }
}

}  // namespace Game
