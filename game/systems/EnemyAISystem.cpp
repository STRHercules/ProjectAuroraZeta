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

namespace Game {

void EnemyAISystem::update(Engine::ECS::Registry& registry, Engine::ECS::Entity hero, const Engine::TimeStep& step) {
    const auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    if (!heroTf) return;
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

        // Tier 3: hero.
        outTf = heroTf;
    };

    registry.view<Engine::ECS::Transform, Engine::ECS::Velocity, Engine::ECS::Health, Game::EnemyAttributes>(
        [&registry, heroTf, &step, &pickNearestTarget, &pickNearestEscort](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Velocity& vel,
                            Engine::ECS::Health& hp, const Game::EnemyAttributes& attr) {
            if (!hp.alive()) {
                vel.value = {0.0f, 0.0f};
                return;
            }
            // If taunted, chase the taunting mini-unit instead of the hero/other targets.
            const Engine::ECS::Transform* targetTf = nullptr;
            if (auto* taunt = registry.get<Game::TauntTarget>(e)) {
                if (taunt->timer > 0.0f) {
                    taunt->timer -= static_cast<float>(step.deltaSeconds);
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
            Engine::Vec2 dir{targetTf->position.x - tf.position.x, targetTf->position.y - tf.position.y};
            // Normalize dir
            float len2 = dir.x * dir.x + dir.y * dir.y;
            if (len2 > 0.0001f) {
                float inv = 1.0f / std::sqrt(len2);
                dir.x *= inv;
                dir.y *= inv;
                vel.value = {dir.x * attr.moveSpeed, dir.y * attr.moveSpeed};
            } else {
                vel.value = {0.0f, 0.0f};
            }
        });
}

}  // namespace Game
