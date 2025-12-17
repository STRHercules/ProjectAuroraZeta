#include "EnemyAISystem.h"

#include <cmath>
#include <limits>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/EnemyAttributes.h"
#include "../components/TauntTarget.h"
#include "../../engine/ecs/components/Tags.h"
#include "../components/MiniUnit.h"
#include "../components/Building.h"
#include "../components/BountyTag.h"
#include "../components/EscortTarget.h"
#include "../components/EscortPreMove.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Status.h"
#include "../components/StatusEffects.h"
#include "RpgDamage.h"

namespace Game {

void EnemyAISystem::update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, const Engine::TimeStep& step) {
    const auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    if (!heroTf) return;
    const float dt = static_cast<float>(step.deltaSeconds);
    bool heroCloaked = false;
    if (auto* stHero = registry.get<Engine::ECS::Status>(hero)) {
        heroCloaked = stHero->container.isStealthed();
    }
    auto pickNearestEscort = [&](const Engine::Vec2& from, const Engine::ECS::Transform*& outTf) {
        outTf = nullptr;
        float bestD2 = std::numeric_limits<float>::max();
        registry.view<Engine::ECS::Transform, Engine::ECS::Health, Game::EscortTarget>(
            [&](Engine::ECS::Entity escortEnt, const Engine::ECS::Transform& tfE, const Engine::ECS::Health& hpE, const Game::EscortTarget&) {
                if (!hpE.alive()) return;
                if (registry.has<Game::EscortPreMove>(escortEnt)) return;  // ignore until countdown done
                float dx = tfE.position.x - from.x;
                float dy = tfE.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    outTf = &tfE;
                }
            });
    };

    auto pickNearestTarget = [&](const Engine::Vec2& from, const Engine::ECS::Transform*& outTf) {
        outTf = nullptr;
        float bestD2 = std::numeric_limits<float>::max();

        // Tier 1: nearest turret (if any alive).
        bool foundTurret = false;
        registry.view<Engine::ECS::Transform, Engine::ECS::Health, Game::Building>(
            [&](Engine::ECS::Entity, const Engine::ECS::Transform& tfB, const Engine::ECS::Health& hpB, const Game::Building& b) {
                if (b.type != Game::BuildingType::Turret) return;
                if (!hpB.alive()) return;
                float dx = tfB.position.x - from.x;
                float dy = tfB.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (!foundTurret || d2 < bestD2) {
                    foundTurret = true;
                    bestD2 = d2;
                    outTf = &tfB;
                }
            });
        if (foundTurret) return;

        // Tier 2: nearest mini unit (if any alive).
        bool foundMini = false;
        bestD2 = std::numeric_limits<float>::max();
        registry.view<Engine::ECS::Transform, Engine::ECS::Health, Game::MiniUnit>(
            [&](Engine::ECS::Entity, const Engine::ECS::Transform& tfM, const Engine::ECS::Health& hpM, const Game::MiniUnit&) {
                if (!hpM.alive()) return;
                float dx = tfM.position.x - from.x;
                float dy = tfM.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (!foundMini || d2 < bestD2) {
                    foundMini = true;
                    bestD2 = d2;
                    outTf = &tfM;
                }
            });
        if (foundMini) return;

        // Tier 3: nearest hero (includes local and remote players).
        bool foundHero = false;
        bestD2 = std::numeric_limits<float>::max();
        registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::HeroTag>(
            [&](Engine::ECS::Entity hEnt, const Engine::ECS::Transform& tfH, const Engine::ECS::Health& hpH, const Engine::ECS::HeroTag&) {
                if (!hpH.alive()) return;
                if (auto* st = registry.get<Engine::ECS::Status>(hEnt)) {
                    if (st->container.isStealthed()) return;
                }
                float dx = tfH.position.x - from.x;
                float dy = tfH.position.y - from.y;
                float d2 = dx * dx + dy * dy;
                if (!foundHero || d2 < bestD2) {
                    foundHero = true;
                    bestD2 = d2;
                    outTf = &tfH;
                }
            });
        if (foundHero) return;

        // Tier 3: hero.
        outTf = heroCloaked ? nullptr : heroTf;
    };

    const bool useRpg = useRpgCombat_;
    const auto rpgCfg = rpgConfig_;
    std::mt19937* rpgRng = rng_;
    auto debugSink = debugSink_;
    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity, Engine::ECS::Health, Game::EnemyAttributes>(
        [&registry, heroTf, dt, hero, useRpg, rpgCfg, rpgRng, debugSink, &pickNearestTarget, &pickNearestEscort](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel,
                            Engine::ECS::Health& hp, const Game::EnemyAttributes& attr) {
            if (!hp.alive()) {
                vel.value = {0.0f, 0.0f};
                return;
            }
            float speedMul = 1.0f;
            bool stunned = false;
            Engine::Vec2 blindDir{0.0f, 0.0f};
            bool blinded = false;
            if (auto* st = registry.get<Engine::ECS::Status>(e)) {
                if (st->container.isStasis()) {
                    vel.value = {0.0f, 0.0f};
                    return;
                }
                speedMul *= st->container.moveSpeedMultiplier();
                if (st->container.isFeared()) {
                    Engine::Vec2 away{tf.position.x - heroTf->position.x, tf.position.y - heroTf->position.y};
                    float len2 = away.x * away.x + away.y * away.y;
                    if (len2 > 0.0001f) {
                        float inv = 1.0f / std::sqrt(len2);
                        away.x *= inv;
                        away.y *= inv;
                    } else {
                        away = {0.0f, 1.0f};
                    }
                    vel.value = {away.x * attr.moveSpeed * speedMul, away.y * attr.moveSpeed * speedMul};
                    return;
                }
                if (st->container.has(Engine::Status::EStatusId::Blindness)) {
                    blinded = true;
                    float seed = tf.position.x * 0.31f + tf.position.y * 0.17f + dt * 3.1f;
                    blindDir = {std::cos(seed), std::sin(seed)};
                }
            }
            if (auto* se = registry.get<Game::StatusEffects>(e)) {
                if (se->burnTimer > 0.0f && se->burnDps > 0.0f) {
                    Engine::Gameplay::DamageEvent burn{};
                    burn.type = Engine::Gameplay::DamageType::Spell;
                    burn.rpgDamageType = static_cast<int>(Engine::Gameplay::RPG::DamageType::Fire);
                    burn.baseDamage = se->burnDps * dt;
                    if (useRpg && rpgRng) {
                        (void)Game::RpgDamage::apply(registry, hero, e, hp, burn, {}, useRpg, rpgCfg, *rpgRng, "dot_burn", debugSink);
                    } else {
                        Engine::Gameplay::applyDamage(hp, burn, {});
                    }
                    se->burnTimer -= dt;
                    if (se->burnTimer <= 0.0f) se->burnDps = 0.0f;
                }
                if (se->earthTimer > 0.0f && se->earthDps > 0.0f) {
                    Engine::Gameplay::DamageEvent thorn{};
                    thorn.type = Engine::Gameplay::DamageType::Spell;
                    thorn.rpgDamageType = static_cast<int>(Engine::Gameplay::RPG::DamageType::Poison);
                    thorn.baseDamage = se->earthDps * dt;
                    if (useRpg && rpgRng) {
                        (void)Game::RpgDamage::apply(registry, hero, e, hp, thorn, {}, useRpg, rpgCfg, *rpgRng, "dot_earth", debugSink);
                    } else {
                        Engine::Gameplay::applyDamage(hp, thorn, {});
                    }
                    se->earthTimer -= dt;
                    if (se->earthTimer <= 0.0f) se->earthDps = 0.0f;
                }
                if (se->slowTimer > 0.0f) {
                    se->slowTimer -= dt;
                    speedMul = std::min(speedMul, se->slowMultiplier);
                    if (se->slowTimer <= 0.0f) {
                        se->slowMultiplier = 1.0f;
                        se->slowTimer = 0.0f;
                    }
                }
                if (se->stunTimer > 0.0f) {
                    se->stunTimer -= dt;
                    stunned = true;
                }
                if (se->blindTimer > 0.0f) {
                    se->blindTimer -= dt;
                    se->blindRetarget -= dt;
                    if (se->blindRetarget <= 0.0f) {
                        float seed = tf.position.x + tf.position.y;
                        float angle = std::fmod(seed * 0.23f + se->blindTimer * 1.3f, 6.28318f);
                        se->blindDir = {std::cos(angle), std::sin(angle)};
                        se->blindRetarget = 0.6f;
                    }
                    blindDir = se->blindDir;
                    blinded = true;
                }
            }
            if (stunned) {
                vel.value = {0.0f, 0.0f};
                return;
            }
            // If taunted, chase the taunting mini-unit instead of the hero/other targets.
            const Engine::ECS::Transform* targetTf = nullptr;
                if (auto* taunt = registry.get<Game::TauntTarget>(e)) {
                    if (taunt->timer > 0.0f) {
                        taunt->timer -= dt;
                        if (taunt->timer < 0.0f) taunt->timer = 0.0f;
                    }
                if (taunt->timer > 0.0f) {
                    if (taunt->target != Engine::ECS::kInvalidEntity) {
                        if (auto* miniTf = registry.get<Engine::ECS::Transform>(taunt->target)) {
                            if (auto* miniHp = registry.get<Engine::ECS::Health>(taunt->target)) {
                                if (miniHp->alive()) {
                                    targetTf = miniTf;
                                }
                            }
                        }
                    }
                }
            }
            if (!targetTf) {
                // Bosses and bounty elites should always pressure the hero; regular enemies prefer turrets/minis first.
                pickNearestEscort(tf.position, targetTf);
            }
            if (!targetTf) {
                // Bosses and bounty elites should always pressure the hero; regular enemies prefer turrets/minis first.
                const bool isBoss = registry.has<Engine::ECS::BossTag>(e);
                const bool isBounty = registry.has<Game::BountyTag>(e);
                if (isBoss || isBounty) {
                    targetTf = heroTf;
                } else {
                    pickNearestTarget(tf.position, targetTf);
                }
            }
            if (!targetTf) return;
            Engine::Vec2 dir{};
            if (blinded) {
                dir = blindDir;
            } else {
                dir = {targetTf->position.x - tf.position.x, targetTf->position.y - tf.position.y};
                float len2 = dir.x * dir.x + dir.y * dir.y;
                if (len2 > 0.0001f) {
                    float inv = 1.0f / std::sqrt(len2);
                    dir.x *= inv;
                    dir.y *= inv;
                }
            }
            float speed = attr.moveSpeed * speedMul;
            vel.value = {dir.x * speed, dir.y * speed};
        });
}

}  // namespace Game
