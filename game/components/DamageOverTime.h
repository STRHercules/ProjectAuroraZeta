// Simple DoT carrier for entities (currently used for enemy on-hit bleed/poison).
#pragma once

namespace Game {

struct DamageOverTime {
    float bleedTimer{0.0f};
    float bleedDps{0.0f};
    float poisonTimer{0.0f};
    float poisonDps{0.0f};
};

}  // namespace Game

