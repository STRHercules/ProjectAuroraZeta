#pragma once

namespace Game {

enum class OffensiveType {
    Ranged,
    Melee,
    Plasma,
    ThornTank,
    Builder
};

struct OffensiveTypeTag {
    OffensiveType type{OffensiveType::Ranged};
};

}  // namespace Game
