#include "ActionMapper.h"

#include <algorithm>
#include <cctype>

namespace Engine {

namespace {
InputKey toInputKey(const std::string& key) {
    auto lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "w" || lower == "move_up") return InputKey::Forward;
    if (lower == "s" || lower == "move_down") return InputKey::Backward;
    if (lower == "a" || lower == "move_left") return InputKey::Left;
    if (lower == "d" || lower == "move_right") return InputKey::Right;
    if (lower == "up" || lower == "arrow_up" || lower == "cam_up") return InputKey::CamUp;
    if (lower == "down" || lower == "arrow_down" || lower == "cam_down") return InputKey::CamDown;
    if (lower == "left" || lower == "arrow_left" || lower == "cam_left") return InputKey::CamLeft;
    if (lower == "right" || lower == "arrow_right" || lower == "cam_right") return InputKey::CamRight;
    if (lower == "c" || lower == "toggle_follow") return InputKey::ToggleFollow;
    if (lower == "r" || lower == "restart") return InputKey::Restart;
    return InputKey::Count;
}
}  // namespace

float ActionMapper::axisFromKeys(const std::vector<std::string>& positive, const std::vector<std::string>& negative,
                                 const InputState& input) const {
    float value = 0.0f;
    for (const auto& key : positive) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) value += 1.0f;
    }
    for (const auto& key : negative) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) value -= 1.0f;
    }
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return value;
}

ActionState ActionMapper::sample(const InputState& input) const {
    ActionState act{};
    act.moveX = axisFromKeys(bindings_.move.positiveX, bindings_.move.negativeX, input);
    act.moveY = axisFromKeys(bindings_.move.positiveY, bindings_.move.negativeY, input);
    act.camX = axisFromKeys(bindings_.camera.positiveX, bindings_.camera.negativeX, input);
    act.camY = axisFromKeys(bindings_.camera.positiveY, bindings_.camera.negativeY, input);
    act.scroll = input.scrollDelta();

    act.toggleFollow = input.isDown(InputKey::ToggleFollow);
    for (const auto& key : bindings_.toggleFollow) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.toggleFollow = true;
            break;
        }
    }

    act.primaryFire = input.isMouseButtonDown(0);
    act.restart = input.isDown(InputKey::Restart);
    for (const auto& key : bindings_.restart) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.restart = true;
            break;
        }
    }
    return act;
}

}  // namespace Engine
