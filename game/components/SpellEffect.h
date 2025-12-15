#pragma once

namespace Game {

enum class ElementType {
    Fire,
    Ice,
    Dark,
    Earth,
    Lightning,
    Wind
};

struct SpellEffect {
    ElementType element{ElementType::Fire};
    int stage{1};  // 1..3
};

}  // namespace Game
