#include "EnemySpriteStateSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/EnemyAttackSwing.h"
#include "../components/EnemyDeathAnim.h"
#include "../components/LookDirection.h"
#include "../components/Dying.h"
#include "../components/EnemyReviving.h"
#include "../components/EnemyType.h"

namespace Game {

namespace {
constexpr float kMoveEpsilon = 0.01f;

LookDir4 dirFromVec(const Engine::Vec2& v, LookDir4 fallback) {
    const float ax = std::abs(v.x);
    const float ay = std::abs(v.y);
    if (ax < 0.0001f && ay < 0.0001f) return fallback;
    if (ax >= ay) {
        return v.x >= 0.0f ? LookDir4::Right : LookDir4::Left;
    }
    // World Y+ is treated as "front" for top-down sprites.
    return v.y >= 0.0f ? LookDir4::Front : LookDir4::Back;
}

int dirIndex(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 0;
        case LookDir4::Left: return 1;
        case LookDir4::Front: return 2;
        case LookDir4::Back: return 3;
    }
    return 2;
}

void resetAnimIfRowChanged(Engine::ECS::SpriteAnimation& anim, int desiredRow) {
    if (anim.row == desiredRow) return;
    anim.row = desiredRow;
    anim.currentFrame = 0;
    anim.accumulator = 0.0f;
}
}  // namespace

void EnemySpriteStateSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);

    registry.view<Engine::ECS::SpriteAnimation, Engine::ECS::Health>([&](Engine::ECS::Entity e,
                                                                         Engine::ECS::SpriteAnimation& anim,
                                                                         Engine::ECS::Health& hp) {
        const bool isEnemy = registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e);
        if (!isEnemy) return;

        // Resolve definition (if available) to drive row selection.
        const EnemyDefinition* def = nullptr;
        if (enemyDefs_) {
            if (const auto* type = registry.get<Game::EnemyType>(e)) {
                if (type->defIndex >= 0 && type->defIndex < static_cast<int>(enemyDefs_->size())) {
                    def = &(*enemyDefs_)[static_cast<std::size_t>(type->defIndex)];
                }
            }
        }
        if (def) {
            anim.frameWidth = def->frameWidth;
            anim.frameHeight = def->frameHeight;
            anim.frameCount = std::max(1, def->frameCount);
            anim.frameDuration = def->frameDuration;
        } else {
            anim.frameCount = std::max(anim.frameCount, 4);
        }

        auto* look = registry.get<Game::LookDirection>(e);
        LookDir4 lastDir = look ? look->dir : LookDir4::Front;

        // If dead/reviving, stick to death animation row.
        if (!hp.alive() || registry.has<Game::Dying>(e) || registry.has<Game::EnemyReviving>(e)) {
            int desiredRow = 12;
            if (const auto* forced = registry.get<Game::EnemyDeathAnim>(e)) {
                desiredRow = forced->row;
            } else if (def) {
                if (def->deathMode == EnemyDeathAnimMode::DirectionalRows) {
                    desiredRow = def->deathRows[static_cast<std::size_t>(dirIndex(lastDir))];
                } else {
                    // RandomRows should be chosen at death-time; fall back to the first configured row.
                    if (!def->deathRandomRows.empty()) {
                        desiredRow = def->deathRandomRows.front();
                    } else {
                        desiredRow = def->deathRows[static_cast<std::size_t>(dirIndex(lastDir))];
                    }
                }
            } else {
                desiredRow = (lastDir == LookDir4::Left) ? 13 : 12;
            }
            resetAnimIfRowChanged(anim, desiredRow);
            anim.allowFlipX = false;
            return;
        }

        // Decrement swing timer (collision/contact resets it).
        if (auto* swing = registry.get<Game::EnemyAttackSwing>(e)) {
            swing->timer = std::max(0.0f, swing->timer - dt);
        }

        // Priorities: Swing (attacking) > Move > Idle.
        Engine::Vec2 dirVec{0.0f, 0.0f};
        bool swinging = false;
        if (const auto* swing = registry.get<Game::EnemyAttackSwing>(e)) {
            swinging = swing->timer > 0.0f;
            if (swinging) dirVec = swing->targetDir;
        }

        const auto* vel = registry.get<Engine::ECS::Velocity>(e);
        const bool moving = vel && (std::abs(vel->value.x) > kMoveEpsilon || std::abs(vel->value.y) > kMoveEpsilon);
        if (!swinging && moving) {
            dirVec = vel->value;
        }

        LookDir4 dir = dirFromVec(dirVec, lastDir);
        if (look) look->dir = dir;

        int desiredRow = 0;
        if (swinging) {
            if (def) {
                desiredRow = def->attackRows[static_cast<std::size_t>(dirIndex(dir))];
            } else {
                desiredRow = 10;
            }
        } else if (moving) {
            if (def) {
                desiredRow = def->moveRows[static_cast<std::size_t>(dirIndex(dir))];
            } else {
                desiredRow = 6;
            }
        } else {
            if (def) {
                desiredRow = def->idleRows[static_cast<std::size_t>(dirIndex(dir))];
            } else {
                desiredRow = 2;
            }
        }

        resetAnimIfRowChanged(anim, desiredRow);
        anim.allowFlipX = false;
    });
}

}  // namespace Game
