// Flame skull ranged attack runtime state.
#pragma once

namespace Game {

struct FlameSkullBehavior {
    float cooldown{1.8f};
    float timer{0.0f};
    float minRange{0.0f};
    float maxRange{0.0f};
    float projectileSpeed{260.0f};
    float projectileHitbox{10.0f};
    float projectileLifetime{1.6f};
    float projectileDamageMul{1.0f};
};

}  // namespace Game

