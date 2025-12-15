#pragma once

namespace Game {

// Attach to a projectile to apply radial damage on impact.
struct AreaDamage {
    float radius{80.0f};
    float damageMultiplier{1.0f};
};

}  // namespace Game
