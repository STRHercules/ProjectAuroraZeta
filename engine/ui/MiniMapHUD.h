#pragma once

#include "../math/Vec2.h"
#include "../render/Color.h"
#include "../render/RenderDevice.h"

#include <vector>

namespace Engine::UI {

struct MiniMapConfig {
    float sizePx{180.0f};
    float paddingPx{12.0f};
    float worldRadius{520.0f};
    float marginPx{6.0f};
    Color background{22, 24, 30, 180};
    Color border{60, 70, 90, 220};
    Color playerColor{230, 240, 255, 240};
    Color enemyColor{255, 140, 120, 230};
    Color edgeColor{255, 210, 120, 230};
    Color pickupColor{160, 230, 180, 230};
    Color goldColor{255, 215, 80, 230};
};

// Renders a simple radar-style mini-map: player at center, enemy blips clamped to radius.
class MiniMapHUD {
public:
    void setConfig(const MiniMapConfig& cfg) { config_ = cfg; }

    void draw(RenderDevice& device,
              const Vec2& playerPos,
              const std::vector<Vec2>& enemyPositions,
              const std::vector<Vec2>& pickupPositions,
              const std::vector<Vec2>& goldPositions,
              int viewportW,
              int viewportH) const;

private:
    MiniMapConfig config_{};
};

}  // namespace Engine::UI
