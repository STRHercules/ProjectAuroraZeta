// Optional tag on projectiles to drive weapon-specific SFX.
#pragma once

namespace Game {

enum class WeaponSfx {
    None = 0,
    Bow = 1
};

struct WeaponSfxTag {
    WeaponSfx weapon{WeaponSfx::None};
};

}  // namespace Game

