// Drives a SpriteAnimation across multiple rows/columns (sequence 0..totalFrames-1).
#pragma once

namespace Game {

struct MultiRowSpriteAnim {
    int columns{1};          // frames per row in the spritesheet
    int totalFrames{1};      // total frames across all rows
    float frameDuration{0.08f};
    bool loop{true};
    bool holdOnLastFrame{false};

    // Optional: linger on a specific absolute frame index (0-based).
    int holdFrame{ -1 };
    float holdExtraSeconds{0.0f};
    float holdRemaining{0.0f};

    float accumulator{0.0f};
    int current{0};          // absolute frame index 0..totalFrames-1
    bool finished{false};
};

}  // namespace Game

