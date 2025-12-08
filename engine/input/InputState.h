// Per-frame input snapshot (keyboard + mouse wheel).
#pragma once

#include <array>

namespace Engine {

enum class InputKey {
    Forward = 0,
    Backward,
    Left,
    Right,
    Restart,
    CamUp,
    CamDown,
    CamLeft,
    CamRight,
    ToggleFollow,
    ToggleShop,
    Pause,
    Dash,
    Ability1,
    Ability2,
    Ability3,
    Ultimate,
    Reload,
    MenuBack,
    Count
};

class InputState {
public:
    void setKeyDown(InputKey key, bool down) { keys_[static_cast<int>(key)] = down; }
    bool isDown(InputKey key) const { return keys_[static_cast<int>(key)]; }

    void addScroll(int delta) { scrollDelta_ += delta; }
    int scrollDelta() const { return scrollDelta_; }

    void setMousePosition(int x, int y) {
        mouseX_ = x;
        mouseY_ = y;
    }
    int mouseX() const { return mouseX_; }
    int mouseY() const { return mouseY_; }

    void setMouseButtonDown(int index, bool down) {
        if (index >= 0 && index < static_cast<int>(mouseButtons_.size())) {
            mouseButtons_[index] = down;
        }
    }
    bool isMouseButtonDown(int index) const {
        if (index >= 0 && index < static_cast<int>(mouseButtons_.size())) {
            return mouseButtons_[index];
        }
        return false;
    }

    void nextFrame() { scrollDelta_ = 0; }

private:
    std::array<bool, static_cast<int>(InputKey::Count)> keys_{};
    int scrollDelta_{0};
    int mouseX_{0};
    int mouseY_{0};
    std::array<bool, 3> mouseButtons_{};  // 0: left, 1: middle, 2: right
};

}  // namespace Engine
