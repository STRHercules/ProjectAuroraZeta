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
    bool combatEnabled{false};
    float attackCooldown{0.0f};
    float healCooldown{0.0f};
};

}  // namespace Game
