// Per-frame input snapshot (keyboard + mouse wheel).
#pragma once

#include <array>
#include <string>

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
    InventoryCycle,
    CharacterScreen,
    Pause,
    Dash,
    Interact,
    UseItem,
    Ability1,
    Ability2,
    Ability3,
    Ultimate,
    Reload,
    MenuBack,
    BuildMenu,
    Count
};

class InputState {
public:
    void setKeyDown(InputKey key, bool down) { keys_[static_cast<int>(key)] = down; }
    bool isDown(InputKey key) const { return keys_[static_cast<int>(key)]; }

    void addScroll(int delta) { scrollDelta_ += delta; }
    int scrollDelta() const { return scrollDelta_; }

    // Text input helpers (cleared each frame via nextFrame).
    void addText(const char* text) { textInput_ += text; }
    const std::string& textInput() const { return textInput_; }
    bool backspacePressed() const { return backspaceEdge_; }
    bool enterPressed() const { return enterEdge_; }
    void markBackspace() { backspaceEdge_ = true; }
    void markEnter() { enterEdge_ = true; }

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

    void nextFrame() {
        scrollDelta_ = 0;
        textInput_.clear();
        backspaceEdge_ = false;
        enterEdge_ = false;
    }

private:
    std::array<bool, static_cast<int>(InputKey::Count)> keys_{};
    int scrollDelta_{0};
    std::string textInput_;
    bool backspaceEdge_{false};
    bool enterEdge_{false};
    int mouseX_{0};
    int mouseY_{0};
    std::array<bool, 3> mouseButtons_{};  // 0: left, 1: middle, 2: right
};

}  // namespace Engine
