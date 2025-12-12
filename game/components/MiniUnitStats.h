#pragma once

namespace Game {

struct MiniUnitStats {
    float moveSpeed{160.0f};
    float attackRange{160.0f};
    float attackRate{1.0f};        // seconds between attacks
    float damage{0.0f};
    float healPerSecond{0.0f};
    float preferredDistance{120.0f}; // light kiting distance
    float tauntRadius{160.0f};       // heavy taunt radius
    float healRange{90.0f};
};

}  // namespace Game

