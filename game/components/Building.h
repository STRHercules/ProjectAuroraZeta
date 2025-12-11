#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

enum class BuildingType {
    Turret,
    Barracks,
    Bunker,
    House
};

struct Building {
    BuildingType type{BuildingType::Turret};
    int ownerPlayerId{0};
    bool powered{true};
    float spawnTimer{0.0f};
};

}  // namespace Game
