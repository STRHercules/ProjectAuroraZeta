// Simple sprite animation descriptor (frame-based, single row).
#pragma once

namespace Engine::ECS {

struct SpriteAnimation {
    int frameWidth{16};
    int frameHeight{16};
    int frameCount{1};
    float frameDuration{0.16f};
    float accumulator{0.0f};
    int currentFrame{0};
    int row{0};           // 0-based row index in the spritesheet
    bool allowFlipX{true};  // if false, renderer ignores Facing-based mirroring
};

}  // namespace Engine::ECS
