// Temporary override for the hero's attack animation sheet (used by certain abilities).
#pragma once

#include "../../engine/assets/Texture.h"

namespace Game {

struct HeroAttackVisualOverride {
    Engine::TexturePtr texture{};
    int frameWidth{16};
    int frameHeight{16};
    float frameDuration{0.10f};
    bool allowFlipX{false};

    // Row mapping for 4-direction sheets. If mirrorLeftFromRight is true, left uses rowRight and relies on flipX.
    int rowRight{0};
    int rowLeft{1};
    int rowFront{2};
    int rowBack{3};
    bool mirrorLeftFromRight{false};
};

}  // namespace Game

