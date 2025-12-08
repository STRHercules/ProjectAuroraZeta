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
    bool toggleShop{false};
    bool pause{false};
    bool interact{false};
    bool useItem{false};
    bool dash{false};
    bool ability1{false};
    bool ability2{false};
    bool ability3{false};
    bool ultimate{false};
    bool reload{false};
    bool menuBack{false};
};

}  // namespace Engine
