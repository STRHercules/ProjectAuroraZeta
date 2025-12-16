#include "MiniMapHUD.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI {

void MiniMapHUD::draw(RenderDevice& device,
                      const Vec2& playerPos,
                      const std::vector<Vec2>& enemyPositions,
                      const std::vector<Vec2>& pickupPositions,
                      const std::vector<Vec2>& goldPositions,
                      int viewportW,
                      int viewportH) const {
    const float size = config_.sizePx;
    const float x = static_cast<float>(viewportW) - config_.paddingPx - size;
    const float y = config_.paddingPx;
    const Vec2 center{x + size * 0.5f, y + size * 0.5f};

    // Background panel
    device.drawFilledRect(Vec2{x, y}, Vec2{size, size}, config_.background);
    // Border (simple inset frame).
    const float t = 2.0f;
    device.drawFilledRect(Vec2{x, y}, Vec2{size, t}, config_.border);
    device.drawFilledRect(Vec2{x, y + size - t}, Vec2{size, t}, config_.border);
    device.drawFilledRect(Vec2{x, y}, Vec2{t, size}, config_.border);
    device.drawFilledRect(Vec2{x + size - t, y}, Vec2{t, size}, config_.border);

    // Player dot
    const float dot = 6.0f;
    device.drawFilledRect(Vec2{center.x - dot * 0.5f, center.y - dot * 0.5f}, Vec2{dot, dot}, config_.playerColor);

    const float radius = std::max(1.0f, config_.worldRadius);
    const float mapHalf = size * 0.5f - config_.marginPx;
    auto drawBlips = [&](const std::vector<Vec2>& positions, const Color& normal, const Color& clampedColor, float size) {
        for (const Vec2& pos : positions) {
            Vec2 delta{pos.x - playerPos.x, pos.y - playerPos.y};
            float len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            bool clamped = false;
            if (len > radius && len > 0.0001f) {
                float inv = radius / len;
                delta.x *= inv;
                delta.y *= inv;
                len = radius;
                clamped = true;
            }
            Vec2 u{delta.x / radius, delta.y / radius};
            float px = center.x + u.x * mapHalf;
            float py = center.y + u.y * mapHalf;
            Color c = clamped ? clampedColor : normal;
            device.drawFilledRect(Vec2{px - size * 0.5f, py - size * 0.5f}, Vec2{size, size}, c);
        }
    };

    drawBlips(enemyPositions, config_.enemyColor, config_.edgeColor, 4.0f);
    drawBlips(pickupPositions, config_.pickupColor, config_.pickupColor, 4.0f);
    drawBlips(goldPositions, config_.goldColor, config_.goldColor, 5.0f);
}

}  // namespace Engine::UI
