#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

// Lightweight status bundle attached to enemies for wizard elemental effects.
struct StatusEffects {
    float slowTimer{0.0f};
    float slowMultiplier{1.0f};

    float burnTimer{0.0f};
    float burnDps{0.0f};

    float earthTimer{0.0f};
    float earthDps{0.0f};

    float blindTimer{0.0f};
    Engine::Vec2 blindDir{0.0f, 0.0f};
    float blindRetarget{0.0f};

    float stunTimer{0.0f};
};

}  // namespace Game
