#include "RenderSystem.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>

#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/render/CameraUtil.h"
#include "../../engine/render/Color.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../components/Facing.h"
#include "../components/HitFlash.h"
#include "../components/DamageNumber.h"
#include "../components/Pickup.h"
#include "../components/PickupBob.h"
#include "../../engine/render/BitmapTextRenderer.h"
#include "../../engine/ecs/components/Tags.h"
#include "../systems/BuffSystem.h"

namespace Game {

namespace {
// Simple coordinate hash to keep tile variant selection stable without storing state.
uint32_t hashCoords(int x, int y) {
    uint32_t h = 0x811C9DC5u;
    h ^= static_cast<uint32_t>(x + 0x9E3779B9u);
    h *= 0x01000193u;
    h ^= static_cast<uint32_t>(y + 0x7F4A7C15u);
    h *= 0x01000193u;
    return h;
}
}  // namespace

void RenderSystem::draw(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW,
                        int viewportH, const Engine::Texture* gridTexture,
                        const std::vector<Engine::TexturePtr>* gridVariants,
                        const Engine::Gameplay::FogOfWarLayer* fog, int fogTileSize,
                        float fogOriginOffsetX, float fogOriginOffsetY) {
    drawGrid(camera, viewportW, viewportH, gridTexture, gridVariants, fog, fogTileSize, fogOriginOffsetX,
             fogOriginOffsetY);

    const Engine::Vec2 center{static_cast<float>(viewportW) * 0.5f, static_cast<float>(viewportH) * 0.5f};
    registry.view<Engine::ECS::Transform, Engine::ECS::Renderable>(
        [this, center, &camera, viewportW, viewportH, &registry, fog, fogTileSize, fogOriginOffsetX,
         fogOriginOffsetY](Engine::ECS::Entity e, const Engine::ECS::Transform& tf,
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

            // Fog culling: hide enemies when tile not visible to player.
            if (fog && fogTileSize > 0 &&
                (registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e))) {
                const int tileX = static_cast<int>(
                    std::floor((tf.position.x + fogOriginOffsetX) / static_cast<float>(fogTileSize)));
                const int tileY = static_cast<int>(
                    std::floor((tf.position.y + fogOriginOffsetY) / static_cast<float>(fogTileSize)));
                if (fog->getState(tileX, tileY) != Engine::Gameplay::FogState::Visible) {
                    return;
                }
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

            bool flipX = false;
            if (const auto* facing = registry.get<Game::Facing>(e)) {
                flipX = facing->dirX < 0;
            } else if (const auto* vel = registry.get<Engine::ECS::Velocity>(e)) {
                flipX = vel->value.x < -0.01f;
            }

            if (rend.texture) {
                if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(e);
                    anim && anim->frameCount > 1) {
                    Engine::IntRect src{anim->frameWidth * anim->currentFrame, 0, anim->frameWidth,
                                        anim->frameHeight};
                    device_.drawTextureRegion(*rend.texture, screenPos, scaledSize, src, flipX);
                } else {
                    Engine::IntRect src{0, 0, rend.texture->width(), rend.texture->height()};
                    device_.drawTextureRegion(*rend.texture, screenPos, scaledSize, src, flipX);
                }
            } else {
                device_.drawFilledRect(screenPos, scaledSize, color);
            }

            // Floating health/shield bars for enemies.
            if (registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e)) {
                if (const auto* hp = registry.get<Engine::ECS::Health>(e)) {
                    const float barW = std::clamp(rend.size.x * camera.zoom, 18.0f, 60.0f);
                    const float barH = 3.0f;
                    const float left = screenPos.x + (scaledSize.x - barW) * 0.5f;
                    const float shieldY = screenPos.y - 8.0f;
                    const float healthY = screenPos.y - 3.5f;
                    Engine::Gameplay::BuffState buff{};
                    if (const auto* armorBuff = registry.get<Game::ArmorBuff>(e)) {
                        buff = armorBuff->state;
                    }

                    // Shield bar (top)
                    if (hp->maxShields > 0.0f) {
                        float sFrac = std::clamp(hp->currentShields / hp->maxShields, 0.0f, 1.0f);
                        device_.drawFilledRect(Engine::Vec2{left, shieldY}, Engine::Vec2{barW, barH},
                                               Engine::Color{30, 40, 60, 170});
                        if (sFrac > 0.0f) {
                            device_.drawFilledRect(Engine::Vec2{left, shieldY},
                                                   Engine::Vec2{barW * sFrac, barH},
                                                   Engine::Color{90, 150, 240, 230});
                        }
                        float armorLen = std::min(barW, (hp->shieldArmor + buff.shieldArmorBonus) * 4.0f);
                        if (armorLen > 0.5f) {
                            device_.drawFilledRect(Engine::Vec2{left, shieldY - 1.0f},
                                                   Engine::Vec2{armorLen, 1.0f},
                                                   Engine::Color{40, 50, 60, 220});
                        }
                    }

                    // Health bar (bottom)
                    if (hp->maxHealth > 0.0f) {
                        float hFrac = std::clamp(hp->currentHealth / hp->maxHealth, 0.0f, 1.0f);
                        device_.drawFilledRect(Engine::Vec2{left, healthY}, Engine::Vec2{barW, barH},
                                               Engine::Color{60, 30, 30, 170});
                        if (hFrac > 0.0f) {
                            device_.drawFilledRect(Engine::Vec2{left, healthY},
                                                   Engine::Vec2{barW * hFrac, barH},
                                                   Engine::Color{200, 80, 80, 230});
                        }
                        float armorLen = std::min(barW, (hp->healthArmor + buff.healthArmorBonus) * 4.0f);
                        if (armorLen > 0.5f) {
                            device_.drawFilledRect(Engine::Vec2{left, healthY - 1.0f},
                                                   Engine::Vec2{armorLen, 1.0f},
                                                   Engine::Color{50, 40, 30, 220});
                        }
                    }
                }
            }
        });
}

void RenderSystem::drawGrid(const Engine::Camera2D& camera, int viewportW, int viewportH,
                            const Engine::Texture* gridTexture,
                            const std::vector<Engine::TexturePtr>* gridVariants,
                            const Engine::Gameplay::FogOfWarLayer* fog, int fogTileSize,
                            float /*fogOriginOffsetX*/, float /*fogOriginOffsetY*/) {
    const float viewWidth = viewportW / camera.zoom;
    const float viewHeight = viewportH / camera.zoom;
    const float left = camera.position.x - viewWidth * 0.5f;
    const float right = camera.position.x + viewWidth * 0.5f;
    const float top = camera.position.y - viewHeight * 0.5f;
    const float bottom = camera.position.y + viewHeight * 0.5f;

    if (gridVariants && !gridVariants->empty()) {
        const auto& first = *(*gridVariants)[0];
        const float tileW = static_cast<float>(first.width());
        const float tileH = static_cast<float>(first.height());
        const float firstX = std::floor(left / tileW) * tileW;
        const float firstY = std::floor(top / tileH) * tileH;
        const std::size_t variantCount = gridVariants->size();
        for (float y = firstY; y < bottom; y += tileH) {
            for (float x = firstX; x < right; x += tileW) {
                int tileX = static_cast<int>(std::floor(x / tileW));
                int tileY = static_cast<int>(std::floor(y / tileH));
                uint32_t h = hashCoords(tileX, tileY);
                const auto& texPtr = (*gridVariants)[h % variantCount];
                if (!texPtr) continue;
                Engine::Vec2 screen = Engine::worldToScreen(Engine::Vec2{x, y}, camera,
                                                            static_cast<float>(viewportW),
                                                            static_cast<float>(viewportH));
                screen.x = std::round(screen.x);
                screen.y = std::round(screen.y);
                Engine::Vec2 size{tileW * camera.zoom + 1.0f, tileH * camera.zoom + 1.0f};
                device_.drawTexture(*texPtr, screen, size);
            }
        }
        return;
    }

    if (gridTexture && gridTexture->width() > 0 && gridTexture->height() > 0) {
        const float tileW = static_cast<float>(gridTexture->width());
        const float tileH = static_cast<float>(gridTexture->height());
        const float firstX = std::floor(left / tileW) * tileW;
        const float firstY = std::floor(top / tileH) * tileH;
        for (float y = firstY; y < bottom; y += tileH) {
            for (float x = firstX; x < right; x += tileW) {
                Engine::Vec2 screen = Engine::worldToScreen(Engine::Vec2{x, y}, camera, static_cast<float>(viewportW),
                                                            static_cast<float>(viewportH));
                screen.x = std::round(screen.x);
                screen.y = std::round(screen.y);
                Engine::Vec2 size{tileW * camera.zoom + 1.0f, tileH * camera.zoom + 1.0f};
                device_.drawTexture(*gridTexture, screen, size);
            }
        }
        return;
    }

    // No debug grid when textures are missing; keep background clean.
}

}  // namespace Game
