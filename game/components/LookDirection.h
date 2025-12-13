// Tracks the last known 4-way look direction for directional sprites.
#pragma once

namespace Game {

enum class LookDir4 : int {
    Right = 0,
    Left = 1,
    Front = 2,
    Back = 3,
};

struct LookDirection {
    LookDir4 dir{LookDir4::Front};
};

}  // namespace Game

