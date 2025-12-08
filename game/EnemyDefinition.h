// Data structure describing an enemy archetype sourced from spritesheet assets.
#pragma once

#include <string>

#include "../engine/assets/Texture.h"

namespace Game {

struct EnemyDefinition {
    std::string id;
    Engine::TexturePtr texture;
    float sizeMultiplier{1.0f};
    float hpMultiplier{1.0f};
    float speedMultiplier{1.0f};
    int frameWidth{16};
    int frameHeight{16};
    float frameDuration{0.16f};
};

}  // namespace Game
