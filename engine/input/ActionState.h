// High-level input actions/axes derived from raw InputState.
#pragma once

namespace Engine {

struct ActionState {
    float moveX{0.0f};
    float moveY{0.0f};
    float camX{0.0f};
    float camY{0.0f};
    int scroll{0};
    bool toggleFollow{false};
    bool primaryFire{false};
    bool restart{false};
};

}  // namespace Engine
