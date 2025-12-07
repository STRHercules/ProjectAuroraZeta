// Floating combat text for damage feedback.
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct DamageNumber {
    float value{0.0f};
    float timer{0.8f};
    Engine::Vec2 velocity{0.0f, -30.0f};
};

}  // namespace Game
