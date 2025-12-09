#include "FogOfWar.h"

#include <algorithm>
#include <cmath>

namespace Engine::Gameplay {

namespace {
inline int index(int x, int y, int width) { return y * width + x; }
}

FogOfWarLayer::FogOfWarLayer(int width, int height)
    : width_(width), height_(height), fogState_(static_cast<std::size_t>(width * height), FogState::Unexplored),
      explored_(static_cast<std::size_t>(width * height), false) {}

void FogOfWarLayer::resetVisibility() {
    const std::size_t total = fogState_.size();
    for (std::size_t i = 0; i < total; ++i) {
        if (fogState_[i] == FogState::Visible) {
            fogState_[i] = explored_[i] ? FogState::Fogged : FogState::Unexplored;
        }
    }
}

void FogOfWarLayer::revealCircle(int centerX, int centerY, float radiusTiles) {
    if (radiusTiles <= 0.0f) {
        if (centerX >= 0 && centerY >= 0 && centerX < width_ && centerY < height_) {
            const int idx = index(centerX, centerY, width_);
            fogState_[idx] = FogState::Visible;
            explored_[idx] = true;
        }
        return;
    }

    const int r = static_cast<int>(std::ceil(radiusTiles));
    const float radiusSq = radiusTiles * radiusTiles;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const int tx = centerX + dx;
            const int ty = centerY + dy;
            if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) {
                continue;
            }
            const float distSq = static_cast<float>(dx * dx + dy * dy);
            if (distSq <= radiusSq) {
                const int idx = index(tx, ty, width_);
                fogState_[idx] = FogState::Visible;
                explored_[idx] = true;
            }
        }
    }
}

FogState FogOfWarLayer::getState(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return FogState::Unexplored;
    }
    return fogState_[index(x, y, width_)];
}

bool FogOfWarLayer::isExplored(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return false;
    }
    return explored_[index(x, y, width_)];
}

void updateFogOfWar(FogOfWarLayer& fog, const std::vector<Unit>& units, int playerId, int tileSize) {
    fog.resetVisibility();

    for (const Unit& u : units) {
        if (!u.isAlive) continue;
        if (u.ownerPlayerId != playerId) continue;

        const int tileX = worldToTile(u.x, tileSize);
        const int tileY = worldToTile(u.y, tileSize);
        fog.revealCircle(tileX, tileY, u.visionRadius);
    }
}

}  // namespace Engine::Gameplay
