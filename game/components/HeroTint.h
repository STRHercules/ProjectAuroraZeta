#pragma once

#include "../../engine/render/Color.h"

namespace Game {

// Optional per-hero tint override used for temporary ability visuals (e.g., Rage).
// RenderSystem preserves hero alpha effects (cloak/ghost) and applies this RGB tint when present.
struct HeroTint {
    Engine::Color color{255, 255, 255, 255};
};

}  // namespace Game

