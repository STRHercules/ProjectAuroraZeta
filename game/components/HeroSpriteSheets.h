// Holds per-hero sprite sheets for movement and combat.
#pragma once

#include "../../engine/assets/Texture.h"

namespace Game {

struct HeroSpriteSheets {
    Engine::TexturePtr movement{};
    Engine::TexturePtr combat{};
    int frameWidth{16};
    int frameHeight{16};
    float movementFrameDuration{0.12f};
    float combatFrameDuration{0.10f};
};

}  // namespace Game

