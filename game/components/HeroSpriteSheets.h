// Holds per-hero sprite sheets for movement and combat.
#pragma once

#include "../../engine/assets/Texture.h"

namespace Game {

struct HeroSpriteSheets {
    Engine::TexturePtr movement{};
    Engine::TexturePtr combat{};
    int movementFrameWidth{16};
    int movementFrameHeight{16};
    int combatFrameWidth{16};
    int combatFrameHeight{16};
    float baseRenderSize{16.0f};  // logical on-screen size for movement frame width
    float movementFrameDuration{0.12f};
    float combatFrameDuration{0.10f};
    bool swapVertical{false};  // swap front/back rows (used for bear form assets)
    bool lockFrontBackIdle{false}; // if true, idle uses front/back rows only (no left/right)
    bool hasPickupRows{true};
    // Row indices (0-based) for flexible sheets.
    int rowIdleRight{0};
    int rowIdleLeft{1};
    int rowIdleFront{2};
    int rowIdleBack{3};
    int rowWalkRight{4};
    int rowWalkLeft{5};
    int rowWalkFront{6};
    int rowWalkBack{7};
    int rowPickupRight{16};
    int rowPickupLeft{17};
    int rowPickupFront{18};
    int rowPickupBack{19};
    int rowKnockdownRight{24};
    int rowKnockdownLeft{25};
};

}  // namespace Game
