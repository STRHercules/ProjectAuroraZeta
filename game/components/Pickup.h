// Collectible pickups (e.g., currency) in the world.
#pragma once

#include "../meta/ItemDefs.h"

namespace Game {

struct Pickup {
    enum class Kind { Copper, Gold, Powerup, Item, Revive };
    enum class Powerup { None, Heal, Kaboom, Recharge, Frenzy, Immortal, Freeze };

    Kind kind{Kind::Copper};
    int amount{0};  // used for currency kinds
    Powerup powerup{Powerup::None};
    ItemDefinition item{};  // used when kind == Item
};

}  // namespace Game
