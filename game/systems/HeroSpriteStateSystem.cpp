#include "HeroSpriteStateSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../components/HeroAttackAnim.h"
#include "../components/HeroPickupAnim.h"
#include "../components/HeroSpriteSheets.h"
#include "../components/LookDirection.h"

namespace Game {

namespace {
constexpr float kMoveEpsilon = 0.01f;

LookDir4 swapLeftRight(LookDir4 d) {
    switch (d) {
        case LookDir4::Left: return LookDir4::Right;
        case LookDir4::Right: return LookDir4::Left;
        default: return d;
    }
}

LookDir4 dirFromVec(const Engine::Vec2& v, LookDir4 fallback) {
    const float ax = std::abs(v.x);
    const float ay = std::abs(v.y);
    if (ax < 0.0001f && ay < 0.0001f) return fallback;
    if (ax >= ay) {
        return v.x >= 0.0f ? LookDir4::Right : LookDir4::Left;
    }
    // Y+ is treated as "down" for the character sheets.
    return v.y >= 0.0f ? LookDir4::Front : LookDir4::Back;
}

int rowIdle(LookDir4 d) {
    d = swapLeftRight(d);
    // Movement sheet rows are 1-based in the asset reference.
    switch (d) {
        case LookDir4::Left: return 0;   // 1: idle left
        case LookDir4::Right: return 1;  // 2: idle right
        case LookDir4::Front: return 2;  // 3: idle down
        case LookDir4::Back: return 3;   // 4: idle up
    }
    return 2;
}

int rowWalk(LookDir4 d) {
    d = swapLeftRight(d);
    switch (d) {
        case LookDir4::Left: return 4;   // 5: walk left
        case LookDir4::Right: return 5;  // 6: walk right
        case LookDir4::Front: return 6;  // 7: walk down
        case LookDir4::Back: return 7;   // 8: walk up
    }
    return 6;
}

int rowPickup(LookDir4 d) {
    d = swapLeftRight(d);
    switch (d) {
        case LookDir4::Left: return 16;   // 17: pickup left
        case LookDir4::Right: return 17;  // 18: pickup right
        case LookDir4::Front: return 18;  // 19: pickup down
        case LookDir4::Back: return 19;   // 20: pickup up
    }
    return 18;
}

int rowKnockdown(LookDir4 d) {
    d = swapLeftRight(d);
    switch (d) {
        case LookDir4::Left: return 24;   // 25: knockdown left
        case LookDir4::Right: return 25;  // 26: knockdown right
        case LookDir4::Front: return 26;  // 27: knockdown down
        case LookDir4::Back: return 27;   // 28: knockdown up
    }
    return 26;
}

int rowMeleeAttack(LookDir4 d, int variant) {
    d = swapLeftRight(d);
    const int v = (variant % 2 == 0) ? 0 : 1;
    switch (d) {
        case LookDir4::Left: return 0 + v;   // left 1/2
        case LookDir4::Right: return 2 + v;  // right 1/2
        case LookDir4::Front: return 4 + v;  // down 1/2
        case LookDir4::Back: return 6 + v;   // up 1/2
    }
    return 4 + v;
}

int framesInSheet(const Engine::TexturePtr& tex, int frameWidth) {
    if (!tex || frameWidth <= 0) return 1;
    int frames = tex->width() / frameWidth;
    return std::max(1, frames);
}

void resetAnimIfChanged(Engine::ECS::SpriteAnimation& anim, int row, int frameCount, float frameDuration) {
    const bool changed = anim.row != row || anim.frameCount != frameCount ||
                         std::abs(anim.frameDuration - frameDuration) > 0.0001f;
    if (!changed) return;
    anim.row = row;
    anim.frameCount = frameCount;
    anim.frameDuration = frameDuration;
    anim.currentFrame = 0;
    anim.accumulator = 0.0f;
}
}  // namespace

void HeroSpriteStateSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero) {
    if (hero == Engine::ECS::kInvalidEntity) return;
    auto* rend = registry.get<Engine::ECS::Renderable>(hero);
    auto* anim = registry.get<Engine::ECS::SpriteAnimation>(hero);
    auto* sheets = registry.get<Game::HeroSpriteSheets>(hero);
    auto* hp = registry.get<Engine::ECS::Health>(hero);
    if (!rend || !anim || !sheets || !hp) return;

    auto* look = registry.get<Game::LookDirection>(hero);
    LookDir4 lastDir = look ? look->dir : LookDir4::Front;

    const float dt = static_cast<float>(step.deltaSeconds);
    if (auto* atk = registry.get<Game::HeroAttackAnim>(hero)) {
        atk->timer = std::max(0.0f, atk->timer - dt);
    }
    if (auto* pick = registry.get<Game::HeroPickupAnim>(hero)) {
        pick->timer = std::max(0.0f, pick->timer - dt);
    }

    // Determine current facing direction from highest-priority driving vector.
    Engine::Vec2 dirVec{0.0f, 0.0f};
    bool attacking = false;
    int attackVariant = 0;
    if (const auto* atk = registry.get<Game::HeroAttackAnim>(hero)) {
        attacking = atk->timer > 0.0f;
        if (attacking) {
            dirVec = atk->targetDir;
            attackVariant = atk->variant;
        }
    }
    const auto* vel = registry.get<Engine::ECS::Velocity>(hero);
    const bool moving = vel && (std::abs(vel->value.x) > kMoveEpsilon || std::abs(vel->value.y) > kMoveEpsilon);
    if (!attacking && moving) {
        dirVec = vel->value;
    }

    LookDir4 dir = dirFromVec(dirVec, lastDir);
    if (look) look->dir = dir;

    // Priorities: Knockdown (dead) > Attack > Pickup > Walk > Idle.
    if (!hp->alive()) {
        rend->texture = sheets->movement;
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->movement, sheets->frameWidth);
        resetAnimIfChanged(*anim, rowKnockdown(dir), frames, sheets->movementFrameDuration);
        return;
    }

    if (attacking && sheets->combat) {
        rend->texture = sheets->combat;
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->combat, sheets->frameWidth);
        resetAnimIfChanged(*anim, rowMeleeAttack(dir, attackVariant), frames, sheets->combatFrameDuration);
        return;
    }

    bool pickingUp = false;
    if (const auto* pick = registry.get<Game::HeroPickupAnim>(hero)) {
        pickingUp = pick->timer > 0.0f;
    }
    if (pickingUp && sheets->movement) {
        rend->texture = sheets->movement;
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->movement, sheets->frameWidth);
        resetAnimIfChanged(*anim, rowPickup(dir), frames, sheets->movementFrameDuration);
        return;
    }

    rend->texture = sheets->movement;
    anim->allowFlipX = false;
    const int frames = framesInSheet(sheets->movement, sheets->frameWidth);
    if (moving) {
        resetAnimIfChanged(*anim, rowWalk(dir), frames, sheets->movementFrameDuration);
    } else {
        resetAnimIfChanged(*anim, rowIdle(dir), frames, sheets->movementFrameDuration);
    }
}

}  // namespace Game
