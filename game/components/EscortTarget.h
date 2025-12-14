// Marks the escort NPC that enemies should prioritize during an escort event.
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct EscortTarget {
    int eventId{0};
    Engine::Vec2 destination{};
    Engine::Vec2 start{};
    float speed{0.0f};
};

}  // namespace Game
