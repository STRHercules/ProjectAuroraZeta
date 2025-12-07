// Collectible pickups (e.g., currency) in the world.
#pragma once

namespace Game {

struct Pickup {
    enum class Type { Credits };
    Type type{Type::Credits};
    int amount{0};
};

}  // namespace Game
