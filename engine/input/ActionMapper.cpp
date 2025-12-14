#include "ActionMapper.h"

#include <algorithm>
#include <cctype>

namespace Engine {

namespace {
InputKey toInputKey(const std::string& key) {
    auto lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.rfind("key:", 0) == 0) {
        auto k = lower.substr(4);
        if (k == "tab") return InputKey::InventoryCycle;
        if (k == "w") return InputKey::Ability2;
        if (k == "e") return InputKey::Interact;
        if (k == "q") return InputKey::UseItem;
        if (k == "r") return InputKey::Ultimate;
        if (k == "f1") return InputKey::Reload;
        if (k == "f") return InputKey::Reload;
        if (k == "m") return InputKey::MenuBack;
        if (k == "1") return InputKey::Ability1;
        if (k == "2") return InputKey::Ability2;
        if (k == "3") return InputKey::Ability3;
        if (k == "4") return InputKey::Ultimate;
    }
    if (lower == "w" || lower == "move_up") return InputKey::Forward;
    if (lower == "s" || lower == "move_down") return InputKey::Backward;
    if (lower == "a" || lower == "move_left") return InputKey::Left;
    if (lower == "d" || lower == "move_right") return InputKey::Right;
    if (lower == "backspace" || lower == "bksp") return InputKey::Restart;
    if (lower == "up" || lower == "arrow_up" || lower == "cam_up") return InputKey::CamUp;
    if (lower == "down" || lower == "arrow_down" || lower == "cam_down") return InputKey::CamDown;
    if (lower == "left" || lower == "arrow_left" || lower == "cam_left") return InputKey::CamLeft;
    if (lower == "right" || lower == "arrow_right" || lower == "cam_right") return InputKey::CamRight;
    if (lower == "c" || lower == "toggle_follow") return InputKey::ToggleFollow;
    if (lower == "tab" || lower == "inventory" || lower == "inventory_cycle") return InputKey::InventoryCycle;
    if (lower == "r" || lower == "restart") return InputKey::Restart;
    if (lower == "b" || lower == "toggle_shop" || lower == "shop") return InputKey::ToggleShop;
    if (lower == "i" || lower == "character" || lower == "character_screen") return InputKey::CharacterScreen;
    if (lower == "v" || lower == "build" || lower == "build_menu") return InputKey::BuildMenu;
    if (lower == "escape" || lower == "pause" || lower == "esc") return InputKey::Pause;
    if (lower == "space" || lower == "dash" || lower == "shift") return InputKey::Dash;
    if (lower == "e" || lower == "interact") return InputKey::Interact;
    if (lower == "q" || lower == "use_item" || lower == "useitem") return InputKey::UseItem;
    if (lower == "ability1") return InputKey::Ability1;
    if (lower == "ability2") return InputKey::Ability2;
    if (lower == "ability3") return InputKey::Ability3;
    if (lower == "ultimate" || lower == "ult") return InputKey::Ultimate;
    if (lower == "reload") return InputKey::Reload;
    if (lower == "menu_back" || lower == "menu" || lower == "m") return InputKey::MenuBack;
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

    act.primaryFire = false;  // Disable left-click manual firing (StarCraft-style auto/command-only attacks).
    act.restart = input.isDown(InputKey::Restart);
    for (const auto& key : bindings_.restart) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.restart = true;
            break;
        }
    }
    act.toggleShop = input.isDown(InputKey::ToggleShop);
    for (const auto& key : bindings_.toggleShop) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.toggleShop = true;
            break;
        }
    }
    act.characterScreen = input.isDown(InputKey::CharacterScreen);
    for (const auto& key : bindings_.characterScreen) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.characterScreen = true;
            break;
        }
    }
    act.interact = input.isDown(InputKey::Interact);
    for (const auto& key : bindings_.interact) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.interact = true;
            break;
        }
    }
    act.useItem = input.isDown(InputKey::UseItem);
    for (const auto& key : bindings_.useItem) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.useItem = true;
            break;
        }
    }
    act.pause = input.isDown(InputKey::Pause);
    for (const auto& key : bindings_.pause) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.pause = true;
            break;
        }
    }
    act.dash = input.isDown(InputKey::Dash);
    for (const auto& key : bindings_.dash) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.dash = true;
            break;
        }
    }
    act.ability1 = input.isDown(InputKey::Ability1);
    for (const auto& key : bindings_.ability1) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.ability1 = true; break; }
    }
    act.ability2 = input.isDown(InputKey::Ability2);
    for (const auto& key : bindings_.ability2) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.ability2 = true; break; }
    }
    act.ability3 = input.isDown(InputKey::Ability3);
    for (const auto& key : bindings_.ability3) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.ability3 = true; break; }
    }
    act.ultimate = input.isDown(InputKey::Ultimate);
    for (const auto& key : bindings_.ultimate) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.ultimate = true; break; }
    }
    act.reload = input.isDown(InputKey::Reload);
    for (const auto& key : bindings_.reload) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.reload = true; break; }
    }
    act.menuBack = input.isDown(InputKey::MenuBack);
    for (const auto& key : bindings_.menuBack) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) { act.menuBack = true; break; }
    }
    act.buildMenu = input.isDown(InputKey::BuildMenu);
    for (const auto& key : bindings_.buildMenu) {
        InputKey mapped = toInputKey(key);
        if (mapped != InputKey::Count && input.isDown(mapped)) {
            act.buildMenu = true;
            break;
        }
    }
    return act;
}

}  // namespace Engine
