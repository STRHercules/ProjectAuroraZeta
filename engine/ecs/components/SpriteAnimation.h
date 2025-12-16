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
    bool loop{true};        // when false, the animation advances once then stops
    bool holdOnLastFrame{false};  // if true, stay on the final frame when non-looping
    bool finished{false};   // set when a non-looping animation reaches its last frame
};

}  // namespace Engine::ECS
