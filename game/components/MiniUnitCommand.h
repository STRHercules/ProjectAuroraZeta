#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct MiniUnitCommand {
    bool hasOrder{false};
    Engine::Vec2 target{};
};

}  // namespace Game
