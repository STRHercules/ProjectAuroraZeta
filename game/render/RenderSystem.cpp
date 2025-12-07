#include "RenderSystem.h"

#include <cmath>
#include <algorithm>

#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/render/CameraUtil.h"
#include "../../engine/render/Color.h"
#include "../components/HitFlash.h"
#include "../components/DamageNumber.h"
#include "../components/Pickup.h"
#include "../components/PickupBob.h"
#include "../../engine/render/BitmapTextRenderer.h"
#include "../../engine/ecs/components/Tags.h"

namespace Game {

void RenderSystem::draw(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW,
                        int viewportH, const Engine::Texture* gridTexture) {
    drawGrid(camera, viewportW, viewportH, gridTexture);

    const Engine::Vec2 center{static_cast<float>(viewportW) * 0.5f, static_cast<float>(viewportH) * 0.5f};
    registry.view<Engine::ECS::Transform, Engine::ECS::Renderable>(
        [this, center, &camera, viewportW, viewportH, &registry](Engine::ECS::Entity e,
                                                                 const Engine::ECS::Transform& tf,
                                                                 const Engine::ECS::Renderable& rend) {
            Engine::Vec2 screenPos =
                Engine::worldToScreen(tf.position, camera, static_cast<float>(viewportW), static_cast<float>(viewportH));
            screenPos.x -= rend.size.x * 0.5f * camera.zoom;
            screenPos.y -= rend.size.y * 0.5f * camera.zoom;
            Engine::Vec2 scaledSize{rend.size.x * camera.zoom, rend.size.y * camera.zoom};
            const float cullMargin = 64.0f;
            if (screenPos.x > center.x * 2.0f + cullMargin || screenPos.y > center.y * 2.0f + cullMargin ||
                screenPos.x + scaledSize.x < -cullMargin || screenPos.y + scaledSize.y < -cullMargin) {
                return;  // simple frustum cull
            }
            Engine::Color color = rend.color;
            // Pulse color for pickups.
            if (registry.has<Game::Pickup>(e)) {
                if (const auto* bob = registry.get<Game::PickupBob>(e)) {
                    float t = 0.5f + 0.5f * std::sin(bob->pulsePhase * 3.14159f * 2.0f);
                    auto add = static_cast<uint8_t>(30 + 80 * t);
                    color = {static_cast<uint8_t>(std::min(255, color.r + add)),
                             static_cast<uint8_t>(std::min(255, color.g + add)),
                             color.b, color.a};
                }
            }
            // Tint bosses differently.
            if (registry.has<Engine::ECS::BossTag>(e)) {
                color = Engine::Color{color.r, static_cast<uint8_t>(std::min(255, color.g / 2)), color.b, color.a};
            }
            if (const auto* flash = registry.get<Game::HitFlash>(e)) {
                if (flash->timer > 0.0f) {
                    float t = std::clamp(flash->timer / 0.12f, 0.0f, 1.0f);
                    auto boost = static_cast<uint8_t>(std::min(255.0f, color.r + 120.0f * t));
                    color = {boost, boost, std::min<uint8_t>(255, static_cast<uint8_t>(color.b + 40)), color.a};
                }
            }

            if (rend.texture) {
                device_.drawTexture(*rend.texture, screenPos, scaledSize);
            } else {
                device_.drawFilledRect(screenPos, scaledSize, color);
            }
        });
}

void RenderSystem::drawGrid(const Engine::Camera2D& camera, int viewportW, int viewportH,
                            const Engine::Texture* gridTexture) {
    const float viewWidth = viewportW / camera.zoom;
    const float viewHeight = viewportH / camera.zoom;
    const float left = camera.position.x - viewWidth * 0.5f;
    const float right = camera.position.x + viewWidth * 0.5f;
    const float top = camera.position.y - viewHeight * 0.5f;
    const float bottom = camera.position.y + viewHeight * 0.5f;

    if (gridTexture && gridTexture->width() > 0 && gridTexture->height() > 0) {
        const float tileW = static_cast<float>(gridTexture->width());
        const float tileH = static_cast<float>(gridTexture->height());
        const float firstX = std::floor(left / tileW) * tileW;
        const float firstY = std::floor(top / tileH) * tileH;
        for (float y = firstY; y < bottom; y += tileH) {
            for (float x = firstX; x < right; x += tileW) {
                Engine::Vec2 screen = Engine::worldToScreen(Engine::Vec2{x, y}, camera, static_cast<float>(viewportW),
                                                            static_cast<float>(viewportH));
                Engine::Vec2 size{tileW * camera.zoom, tileH * camera.zoom};
                device_.drawTexture(*gridTexture, screen, size);
            }
        }
        return;
    }

    const float spacing = 64.0f;
    const float firstVertical = std::floor(left / spacing) * spacing;
    const float firstHorizontal = std::floor(top / spacing) * spacing;

    Engine::Color gridColor{30, 30, 38, 120};
    for (float x = firstVertical; x <= right; x += spacing) {
        float screenX = (x - camera.position.x) * camera.zoom + viewportW * 0.5f;
        device_.drawFilledRect(Engine::Vec2{screenX, 0.0f}, Engine::Vec2{1.0f, static_cast<float>(viewportH)},
                               gridColor);
    }

    for (float y = firstHorizontal; y <= bottom; y += spacing) {
        float screenY = (y - camera.position.y) * camera.zoom + viewportH * 0.5f;
        device_.drawFilledRect(Engine::Vec2{0.0f, screenY}, Engine::Vec2{static_cast<float>(viewportW), 1.0f},
                               gridColor);
    }
}

}  // namespace Game
