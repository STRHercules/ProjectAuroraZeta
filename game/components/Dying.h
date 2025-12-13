// Indicates an entity is dead but kept for a short death animation before despawn.
#pragma once

namespace Game {

struct Dying {
    float timer{0.0f};  // seconds remaining until destroy
};

}  // namespace Game

