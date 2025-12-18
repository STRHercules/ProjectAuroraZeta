// Optional per-enemy contact-damage tuning and on-hit effects.
#pragma once

namespace Game {

struct EnemyOnHit {
    float damageMultiplier{1.0f};

    // Damage-over-time effects applied to the target on successful contact hits.
    float bleedChance{0.0f};
    float bleedDuration{0.0f};
    float bleedDpsMul{0.0f};   // DPS = baseContactDamage * mul

    float poisonChance{0.0f};
    float poisonDuration{0.0f};
    float poisonDpsMul{0.0f};  // DPS = baseContactDamage * mul

    float fearChance{0.0f};
    float fearDuration{0.0f};
};

}  // namespace Game

