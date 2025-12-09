// Flat 2D fog of war implementation (no height/occlusion).
#pragma once

#include <cstdint>
#include <vector>

namespace Engine::Gameplay {

enum class FogState : std::uint8_t {
    Unexplored = 0,
    Fogged = 1,
    Visible = 2
};

// Basic unit descriptor used for vision queries.
struct Unit {
    float x{0.0f};           // world position in pixels (2D)
    float y{0.0f};
    float visionRadius{0.0f};  // radius in tiles
    bool isAlive{false};
    int ownerPlayerId{0};
};

inline int worldToTile(float pos, int tileSize) { return static_cast<int>(pos) / tileSize; }

class FogOfWarLayer {
public:
    FogOfWarLayer(int width, int height);

    void resetVisibility();

    // Reveal a filled 2D circle around the center tile.
    void revealCircle(int centerX, int centerY, float radiusTiles);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    FogState getState(int x, int y) const;
    bool isExplored(int x, int y) const;

    std::vector<FogState>& data() { return fogState_; }
    const std::vector<FogState>& data() const { return fogState_; }

private:
    int width_;
    int height_;
    std::vector<FogState> fogState_;
    std::vector<bool> explored_;
};

void updateFogOfWar(FogOfWarLayer& fog, const std::vector<Unit>& units, int playerId, int tileSize);

}  // namespace Engine::Gameplay
