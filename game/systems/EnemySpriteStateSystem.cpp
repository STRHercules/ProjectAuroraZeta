#include "EnemySpriteStateSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/EnemyAttackSwing.h"
#include "../components/LookDirection.h"
#include "../components/Dying.h"

namespace Game {

namespace {
constexpr int kFramesPerAnim = 4;
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

int rowForIdle(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 0;
        case LookDir4::Left: return 1;
        case LookDir4::Front: return 2;
        case LookDir4::Back: return 3;
    }
    return 2;
}

int rowForMove(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 4;
        case LookDir4::Left: return 5;
        case LookDir4::Front: return 6;
        case LookDir4::Back: return 7;
    }
    return 6;
}

int rowForSwing(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 8;
        case LookDir4::Left: return 9;
        case LookDir4::Front: return 10;
        case LookDir4::Back: return 11;
    }
    return 10;
}

int rowForDeath(LookDir4 d) {
    // Orc sheet only provides left/right death rows.
    return (d == LookDir4::Left) ? 13 : 12;
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

        anim.frameCount = std::max(anim.frameCount, kFramesPerAnim);

        auto* look = registry.get<Game::LookDirection>(e);
        LookDir4 lastDir = look ? look->dir : LookDir4::Front;

        // If dead, stick to death animation row.
        if (!hp.alive() || registry.has<Game::Dying>(e)) {
            const int desiredRow = rowForDeath(lastDir);
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
            desiredRow = rowForSwing(dir);
        } else if (moving) {
            desiredRow = rowForMove(dir);
        } else {
            desiredRow = rowForIdle(dir);
        }

        resetAnimIfRowChanged(anim, desiredRow);
        anim.allowFlipX = false;
    });
}

}  // namespace Game
