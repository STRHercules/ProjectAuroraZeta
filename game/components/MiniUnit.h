#pragma once

namespace Game {

enum class MiniUnitClass {
    Light,
    Heavy,
    Medic
};

struct MiniUnit {
    MiniUnitClass cls{MiniUnitClass::Light};
    int ownerPlayerId{0};
};

}  // namespace Game
