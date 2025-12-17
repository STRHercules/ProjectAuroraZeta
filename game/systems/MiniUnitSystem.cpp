#include "MiniUnitSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Status.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/gameplay/Combat.h"
#include "../components/MiniUnit.h"
#include "../components/MiniUnitCommand.h"
#include "../components/MiniUnitStats.h"
#include "../components/TauntTarget.h"
#include "../components/SummonedUnit.h"
#include "RpgDamage.h"

namespace Game {

void MiniUnitSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float arriveDist2 = stopDistance_ * stopDistance_;
    const float dt = static_cast<float>(step.deltaSeconds);

    auto findNearestEnemy = [&](const Engine::Vec2& from, Engine::ECS::Entity& outEnemy,
                               Engine::Vec2& outEnemyPos, float& outDist2) {
        outEnemy = Engine::ECS::kInvalidEntity;
        outDist2 = std::numeric_limits<float>::max();
        registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tfE, const Engine::ECS::Health& hpE, Engine::ECS::EnemyTag&) {
                if (!hpE.alive()) return;
                float dx = tfE.position.x - from.x;
                float dy = tfE.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < outDist2) {
                    outDist2 = d2;
                    outEnemy = e;
                    outEnemyPos = tfE.position;
                }
            });
    };

    auto findHealTarget = [&](Engine::ECS::Entity self, const Engine::Vec2& from,
                              Engine::ECS::Entity& outFriend, Engine::Vec2& outPos, float& outDist2) {
        outFriend = Engine::ECS::kInvalidEntity;
        outDist2 = std::numeric_limits<float>::max();
        registry.view<Game::MiniUnit, Engine::ECS::Transform, Engine::ECS::Health>(
            [&](Engine::ECS::Entity e, const Game::MiniUnit&, const Engine::ECS::Transform& tfF, const Engine::ECS::Health& hpF) {
                if (e == self) return;
                if (!hpF.alive()) return;
                float missing = (hpF.maxHealth + hpF.maxShields) - (hpF.currentHealth + hpF.currentShields);
                if (missing <= 0.5f) return;
                float dx = tfF.position.x - from.x;
                float dy = tfF.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < outDist2) {
                    outDist2 = d2;
                    outFriend = e;
                    outPos = tfF.position;
                }
            });
    };

    registry.view<Game::MiniUnit, Game::MiniUnitStats, Engine::ECS::Transform, Engine::ECS::Velocity, Game::MiniUnitCommand>(
        [&](Engine::ECS::Entity e, Game::MiniUnit& mu, Game::MiniUnitStats& stats, Engine::ECS::Transform& tf,
            Engine::ECS::Velocity& vel, Game::MiniUnitCommand& cmd) {
            // Summoned units stay leashed to their owner.
            if (auto* leash = registry.get<Game::SummonedUnit>(e)) {
                Engine::Vec2 ownerPos{};
                bool hasOwner = false;
                if (leash->owner != Engine::ECS::kInvalidEntity) {
                    if (auto* otf = registry.get<Engine::ECS::Transform>(leash->owner)) {
                        ownerPos = otf->position;
                        hasOwner = true;
                    }
                }
                if (hasOwner) {
                    Engine::Vec2 delta{ownerPos.x - tf.position.x, ownerPos.y - tf.position.y};
                    float dist2 = delta.x * delta.x + delta.y * delta.y;
                    float leashR2 = leash->leashRadius * leash->leashRadius;
                    if (dist2 > leashR2) {
                        float invLen = 1.0f / std::sqrt(std::max(dist2, 0.0001f));
                        Engine::Vec2 desired{delta.x * invLen * stats.moveSpeed, delta.y * invLen * stats.moveSpeed};
                        float accel = 12.0f;
                        float t = std::clamp(accel * dt, 0.0f, 1.0f);
                        vel.value = {vel.value.x + (desired.x - vel.value.x) * t,
                                     vel.value.y + (desired.y - vel.value.y) * t};
                        // While returning, suppress combat AI for a moment.
                        mu.attackCooldown = std::max(0.0f, mu.attackCooldown - dt);
                        return;
                    }
                }
            }
            // 1) Explicit move orders take priority.
            if (cmd.hasOrder) {
                Engine::Vec2 delta{cmd.target.x - tf.position.x, cmd.target.y - tf.position.y};
                float dist2 = delta.x * delta.x + delta.y * delta.y;
                if (dist2 <= arriveDist2) {
                    cmd.hasOrder = false;
                    vel.value = {0.0f, 0.0f};
                } else {
                    float invLen = 1.0f / std::sqrt(std::max(dist2, 0.0001f));
                    float speed = stats.moveSpeed > 0.0f ? stats.moveSpeed : defaultMoveSpeed_;
                    Engine::Vec2 desired{delta.x * invLen * speed, delta.y * invLen * speed};
                    float accel = 12.0f;
                    float t = std::clamp(accel * dt, 0.0f, 1.0f);
                    vel.value = {vel.value.x + (desired.x - vel.value.x) * t,
                                 vel.value.y + (desired.y - vel.value.y) * t};
                }
                return;
            }

            // 2) If combat AI disabled, idle.
            if (!mu.combatEnabled) {
                return;
            }

            // 3) AI per class.
            if (mu.cls == Game::MiniUnitClass::Medic) {
                Engine::ECS::Entity friendEnt;
                Engine::Vec2 friendPos{};
                float dist2{};
                findHealTarget(e, tf.position, friendEnt, friendPos, dist2);
                if (friendEnt == Engine::ECS::kInvalidEntity) {
                    vel.value = {0.0f, 0.0f};
                    return;
                }
                float healRange2 = stats.healRange * stats.healRange;
                Engine::Vec2 delta{friendPos.x - tf.position.x, friendPos.y - tf.position.y};
                float d2 = delta.x * delta.x + delta.y * delta.y;
                if (d2 > healRange2) {
                    float invLen = 1.0f / std::sqrt(std::max(d2, 0.0001f));
                    Engine::Vec2 desired{delta.x * invLen * stats.moveSpeed, delta.y * invLen * stats.moveSpeed};
                    float accel = 10.0f;
                    float t = std::clamp(accel * dt, 0.0f, 1.0f);
                    vel.value = {vel.value.x + (desired.x - vel.value.x) * t,
                                 vel.value.y + (desired.y - vel.value.y) * t};
                } else {
                    vel.value = {0.0f, 0.0f};
                    if (auto* hpF = registry.get<Engine::ECS::Health>(friendEnt)) {
                        hpF->currentHealth = std::min(hpF->maxHealth, hpF->currentHealth + stats.healPerSecond * globalHealMul_ * dt);
                        // Shields heal very slowly (medic focus on health).
                        if (hpF->maxShields > 0.0f) {
                            hpF->currentShields = std::min(hpF->maxShields, hpF->currentShields + stats.healPerSecond * globalHealMul_ * 0.25f * dt);
                        }
                    }
                }
                return;
            }

            Engine::ECS::Entity enemyEnt;
            Engine::Vec2 enemyPos{};
            float enemyDist2{};
            findNearestEnemy(tf.position, enemyEnt, enemyPos, enemyDist2);
            if (enemyEnt == Engine::ECS::kInvalidEntity) {
                vel.value = {0.0f, 0.0f};
                return;
            }

            float dist = std::sqrt(std::max(enemyDist2, 0.0001f));
            Engine::Vec2 toEnemy{enemyPos.x - tf.position.x, enemyPos.y - tf.position.y};
            float invLen = 1.0f / dist;
            Engine::Vec2 dir{toEnemy.x * invLen, toEnemy.y * invLen};

            const float attackRange = stats.attackRange;
            const float preferred = stats.preferredDistance;

            // Light kites to preferred distance.
            if (mu.cls == Game::MiniUnitClass::Light && preferred > 0.0f) {
                Engine::Vec2 desired{};
                if (dist > preferred + 10.0f) {
                    desired = {dir.x * stats.moveSpeed, dir.y * stats.moveSpeed};
                } else if (dist < preferred - 10.0f) {
                    desired = {-dir.x * stats.moveSpeed, -dir.y * stats.moveSpeed};
                }
                float accel = 10.0f;
                float t = std::clamp(accel * dt, 0.0f, 1.0f);
                vel.value = {vel.value.x + (desired.x - vel.value.x) * t,
                             vel.value.y + (desired.y - vel.value.y) * t};
            } else {
                // Heavy (or light without preferred): close to attack range.
                Engine::Vec2 desired{};
                if (dist > attackRange * 0.95f) {
                    desired = {dir.x * stats.moveSpeed, dir.y * stats.moveSpeed};
                }
                float accel = 10.0f;
                float t = std::clamp(accel * dt, 0.0f, 1.0f);
                vel.value = {vel.value.x + (desired.x - vel.value.x) * t,
                             vel.value.y + (desired.y - vel.value.y) * t};
            }

            // Attack if in range.
            if (attackRange > 0.0f && dist <= attackRange) {
                mu.attackCooldown -= dt;
                if (mu.attackCooldown <= 0.0f) {
                    if (auto* enemyHp = registry.get<Engine::ECS::Health>(enemyEnt)) {
                        if (enemyHp->alive()) {
                            Engine::Gameplay::DamageEvent dmg{};
                            dmg.baseDamage = stats.damage * globalDamageMul_;
                            dmg.type = Engine::Gameplay::DamageType::Normal;
                            Engine::Gameplay::BuffState buff{};
                            if (auto* st = registry.get<Engine::ECS::Status>(enemyEnt)) {
                                if (st->container.isStasis()) {
                                    // do nothing
                                } else {
                                    float armorDelta = st->container.armorDeltaTotal();
                                    buff.healthArmorBonus += armorDelta;
                                    buff.shieldArmorBonus += armorDelta;
                                    buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                                }
                            }
                            if (useRpgCombat_ && rng_) {
                                (void)Game::RpgDamage::apply(registry, e, enemyEnt, *enemyHp, dmg, buff, true, rpgConfig_, *rng_,
                                                             "mini", debugSink_);
                            } else {
                                Engine::Gameplay::applyDamage(*enemyHp, dmg, buff);
                            }
                        }
                    }
                    mu.attackCooldown = std::max(0.1f, stats.attackRate * globalAttackRateMul_);
                }
            } else {
                mu.attackCooldown = std::max(0.0f, mu.attackCooldown - dt);
            }

            // Heavy taunt: mark nearby enemies to chase this heavy.
            if (mu.cls == Game::MiniUnitClass::Heavy && stats.tauntRadius > 0.0f) {
                float tauntR2 = stats.tauntRadius * stats.tauntRadius;
                registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                    [&](Engine::ECS::Entity en, const Engine::ECS::Transform& tfe, const Engine::ECS::Health& hpe, Engine::ECS::EnemyTag&) {
                        if (!hpe.alive()) return;
                        float dx = tfe.position.x - tf.position.x;
                        float dy = tfe.position.y - tf.position.y;
                        float d2 = dx * dx + dy * dy;
                        if (d2 <= tauntR2) {
                            if (auto* taunt = registry.get<Game::TauntTarget>(en)) {
                                taunt->target = e;
                                taunt->timer = 0.6f;
                            } else {
                                registry.emplace<Game::TauntTarget>(en, Game::TauntTarget{e, 0.6f});
                            }
                        }
                    });
            }
        });
}

}  // namespace Game
