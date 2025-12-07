// Simple idle bob for pickups.
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct PickupBob {
    Engine::Vec2 basePos{};
    float phase{0.0f};
    float amplitude{4.0f};
    float speed{3.0f};
    float pulsePhase{0.0f};
};

}  // namespace Game
