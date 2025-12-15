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
#include "../components/SecondaryWeapon.h"

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
    // Y+ is treated as "down" for the character sheets.
    return v.y >= 0.0f ? LookDir4::Front : LookDir4::Back;
}

int rowMeleeAttack(LookDir4 d, int variant) {
    const int v = (variant % 2 == 0) ? 0 : 1;
    switch (d) {
        case LookDir4::Right: return 0 + v;  // attack right / backhand
        case LookDir4::Left: return 2 + v;   // attack left / backhand
        case LookDir4::Front: return 4 + v;  // attack front / backhand
        case LookDir4::Back: return 6 + v;   // attack back / backhand
    }
    return 4 + v;
}

int rowSecondaryAttack(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 12;  // row 13
        case LookDir4::Left: return 13;   // row 14
        case LookDir4::Front: return 14;  // row 15
        case LookDir4::Back: return 15;   // row 16
    }
    return 14;
}

int rowSecondaryIdle(LookDir4 d) {
    switch (d) {
        case LookDir4::Right: return 16;  // row 17
        case LookDir4::Left: return 17;   // row 18
        case LookDir4::Front: return 18;  // row 19
        case LookDir4::Back: return 19;   // row 20
    }
    return 18;
}

int framesInSheet(const Engine::TexturePtr& tex, int frameWidth) {
    if (!tex || frameWidth <= 0) return 1;
    int frames = tex->width() / frameWidth;
    return std::max(1, frames);
}

int rowsInSheet(const Engine::TexturePtr& tex, int frameHeight) {
    if (!tex || frameHeight <= 0) return 1;
    int rows = tex->height() / frameHeight;
    return std::max(1, rows);
}

int clampRowToSheet(const Engine::TexturePtr& tex, int frameHeight, int row) {
    int rows = rowsInSheet(tex, frameHeight);
    return std::clamp(row, 0, std::max(0, rows - 1));
}

void applyRenderSize(Game::HeroSpriteSheets* sheets, Engine::ECS::Renderable* rend, bool combat) {
    if (!sheets || !rend) return;
    float base = sheets->baseRenderSize > 0.0f ? sheets->baseRenderSize : rend->size.x;
    float refW = std::max(1, sheets->movementFrameWidth);
    float srcW = combat ? std::max(1, sheets->combatFrameWidth) : std::max(1, sheets->movementFrameWidth);
    float srcH = combat ? std::max(1, sheets->combatFrameHeight) : std::max(1, sheets->movementFrameHeight);
    float scale = base / static_cast<float>(refW);
    rend->size = {static_cast<float>(srcW) * scale, static_cast<float>(srcH) * scale};
}

void resetAnimIfChanged(Engine::ECS::SpriteAnimation& anim,
                        int row,
                        int frameCount,
                        float frameDuration,
                        int frameWidth,
                        int frameHeight) {
    const bool changed = anim.row != row || anim.frameCount != frameCount ||
                         anim.frameWidth != frameWidth || anim.frameHeight != frameHeight ||
                         std::abs(anim.frameDuration - frameDuration) > 0.0001f;
    if (!changed) return;
    anim.frameWidth = frameWidth;
    anim.frameHeight = frameHeight;
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
    const bool secondaryEquipped = registry.has<Game::SecondaryWeapon>(hero) &&
                                    registry.get<Game::SecondaryWeapon>(hero)->active;
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
    if (sheets->swapVertical) {
        if (dir == LookDir4::Front) dir = LookDir4::Back;
        else if (dir == LookDir4::Back) dir = LookDir4::Front;
    }
    if (look) look->dir = dir;

    // Priorities: Knockdown (dead) > Attack > Pickup > Walk > Idle.
    if (!hp->alive()) {
        rend->texture = sheets->movement;
        applyRenderSize(sheets, rend, false);
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->movement, sheets->movementFrameWidth);
        auto rowKnockdown = [&](LookDir4 d) {
            switch (d) {
                case LookDir4::Right: return sheets->rowKnockdownRight;
                case LookDir4::Left: return sheets->rowKnockdownLeft;
                case LookDir4::Front: return sheets->rowKnockdownRight;  // fallback to right/left rows
                case LookDir4::Back: return sheets->rowKnockdownLeft;
            }
            return sheets->rowKnockdownRight;
        };
        int row = clampRowToSheet(sheets->movement, sheets->movementFrameHeight, rowKnockdown(dir));
        resetAnimIfChanged(*anim, row, frames, sheets->movementFrameDuration,
                           sheets->movementFrameWidth, sheets->movementFrameHeight);
        return;
    }

    if (attacking && sheets->combat) {
        rend->texture = sheets->combat;
        applyRenderSize(sheets, rend, true);
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->combat, sheets->combatFrameWidth);
        int row = secondaryEquipped ? rowSecondaryAttack(dir) : rowMeleeAttack(dir, attackVariant);
        row = clampRowToSheet(sheets->combat, sheets->combatFrameHeight, row);
        resetAnimIfChanged(*anim, row, frames, sheets->combatFrameDuration,
                           sheets->combatFrameWidth, sheets->combatFrameHeight);
        return;
    }

    bool pickingUp = false;
    if (const auto* pick = registry.get<Game::HeroPickupAnim>(hero)) {
        pickingUp = pick->timer > 0.0f;
    }
    if (pickingUp && sheets->movement && sheets->hasPickupRows) {
        rend->texture = sheets->movement;
        applyRenderSize(sheets, rend, false);
        anim->allowFlipX = false;
        const int frames = framesInSheet(sheets->movement, sheets->movementFrameWidth);
        auto rowPickup = [&](LookDir4 d) {
            switch (d) {
                case LookDir4::Right: return sheets->rowPickupRight;
                case LookDir4::Left: return sheets->rowPickupLeft;
                case LookDir4::Front: return sheets->rowPickupFront;
                case LookDir4::Back: return sheets->rowPickupBack;
            }
            return sheets->rowPickupFront;
        };
        int row = clampRowToSheet(sheets->movement, sheets->movementFrameHeight, rowPickup(dir));
        resetAnimIfChanged(*anim, row, frames, sheets->movementFrameDuration,
                           sheets->movementFrameWidth, sheets->movementFrameHeight);
        return;
    }

    rend->texture = sheets->movement;
    applyRenderSize(sheets, rend, false);
    anim->allowFlipX = false;
    const int frames = framesInSheet(sheets->movement, sheets->movementFrameWidth);
    if (moving) {
        auto rowWalk = [&](LookDir4 d) {
            switch (d) {
                case LookDir4::Right: return sheets->rowWalkRight;
                case LookDir4::Left: return sheets->rowWalkLeft;
                case LookDir4::Front: return sheets->rowWalkFront;
                case LookDir4::Back: return sheets->rowWalkBack;
            }
            return sheets->rowWalkFront;
        };
        int row = clampRowToSheet(sheets->movement, sheets->movementFrameHeight, rowWalk(dir));
        resetAnimIfChanged(*anim, row, frames, sheets->movementFrameDuration,
                           sheets->movementFrameWidth, sheets->movementFrameHeight);
    } else if (secondaryEquipped && sheets->combat) {
        rend->texture = sheets->combat;
        applyRenderSize(sheets, rend, true);
        int row = clampRowToSheet(sheets->combat, sheets->combatFrameHeight, rowSecondaryIdle(dir));
        int framesCombat = framesInSheet(sheets->combat, sheets->combatFrameWidth);
        resetAnimIfChanged(*anim, row, framesCombat, sheets->combatFrameDuration,
                           sheets->combatFrameWidth, sheets->combatFrameHeight);
    } else {
        auto rowIdle = [&](LookDir4 d) {
            switch (d) {
                case LookDir4::Right: return sheets->rowIdleRight;
                case LookDir4::Left: return sheets->rowIdleLeft;
                case LookDir4::Front: return sheets->rowIdleFront;
                case LookDir4::Back: return sheets->rowIdleBack;
            }
            return sheets->rowIdleFront;
        };
        int baseRow = rowIdle(dir);
        if (sheets->lockFrontBackIdle) {
            // Force front/back rows only.
            if (dir == LookDir4::Left || dir == LookDir4::Right) {
                baseRow = rowIdle(LookDir4::Front);
            }
        }
        int row = clampRowToSheet(sheets->movement, sheets->movementFrameHeight, baseRow);
        resetAnimIfChanged(*anim, row, frames, sheets->movementFrameDuration,
                           sheets->movementFrameWidth, sheets->movementFrameHeight);
    }
}

}  // namespace Game
