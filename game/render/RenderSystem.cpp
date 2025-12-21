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
#include "../components/EscortTarget.h"
#include "../components/BountyTag.h"
#include "../components/Ghost.h"
#include "../components/HeroTint.h"
#include "../../engine/render/BitmapTextRenderer.h"
#include "../../engine/ecs/components/Tags.h"
#include "../systems/BuffSystem.h"
#include "../components/MiniUnit.h"
#include "../../engine/ecs/components/Status.h"

namespace Game {

namespace {
// Simple coordinate hash to keep tile variant selection stable without storing state.
uint32_t hashCoords(int x, int y) {
    // Mix inspired by splitmix32-style avalanching. Designed to avoid stripe artifacts when variantCount is small.
    uint32_t h = 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h ^= static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

constexpr float kEnemyVisualScale = 2.0f;
constexpr float kHeroVisualScale = 2.0f;
}  // namespace

void RenderSystem::draw(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW,
                        int viewportH, const Engine::Texture* gridTexture,
                        const std::vector<Engine::TexturePtr>* gridVariants,
                        const Engine::Gameplay::FogOfWarLayer* fog, int fogTileSize,
                        float fogOriginOffsetX, float fogOriginOffsetY,
                        bool disableEnemyFogCulling) {
    drawGridPass(camera, viewportW, viewportH, gridTexture, gridVariants, fog, fogTileSize, fogOriginOffsetX,
                 fogOriginOffsetY);
    drawEntitiesPass(registry, camera, viewportW, viewportH, fog, fogTileSize, fogOriginOffsetX, fogOriginOffsetY,
                     disableEnemyFogCulling);
}

void RenderSystem::drawEntitiesPass(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW,
                                   int viewportH, const Engine::Gameplay::FogOfWarLayer* fog, int fogTileSize,
                                   float fogOriginOffsetX, float fogOriginOffsetY, bool disableEnemyFogCulling) {
    const Engine::Vec2 center{static_cast<float>(viewportW) * 0.5f, static_cast<float>(viewportH) * 0.5f};

    struct DrawItem {
        Engine::ECS::Entity e{Engine::ECS::kInvalidEntity};
        float sortKey{0.0f};  // "feet" y in world space
    };
    std::vector<DrawItem> items;
    items.reserve(2048);
    registry.view<Engine::ECS::Transform, Engine::ECS::Renderable>(
        [&items, &registry](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Renderable& rend) {
            const bool isEnemy = registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e);
            const bool isHero = registry.has<Engine::ECS::HeroTag>(e);
            const float visualScale = isEnemy ? kEnemyVisualScale : (isHero ? kHeroVisualScale : 1.0f);
            const float feetY = tf.position.y + (rend.size.y * 0.5f * visualScale);
            items.push_back(DrawItem{e, feetY});
        });

    std::stable_sort(items.begin(), items.end(),
                     [](const DrawItem& a, const DrawItem& b) { return a.sortKey < b.sortKey; });

    for (const auto& it : items) {
        const Engine::ECS::Entity e = it.e;
        const auto* tfPtr = registry.get<Engine::ECS::Transform>(e);
        const auto* rendPtr = registry.get<Engine::ECS::Renderable>(e);
        if (!tfPtr || !rendPtr) continue;
        const Engine::ECS::Transform& tf = *tfPtr;
        const Engine::ECS::Renderable& rend = *rendPtr;
            Engine::Vec2 screenPos =
                Engine::worldToScreen(tf.position, camera, static_cast<float>(viewportW), static_cast<float>(viewportH));
            const bool isEnemy = registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e);
            const bool isHero = registry.has<Engine::ECS::HeroTag>(e);

            // Fog culling:
            // - Enemies/bosses: only draw when Visible.
            // - World props/pickups/etc: draw when Visible OR Fogged (explored), but not when Unexplored.
            Engine::Gameplay::FogState fogState = Engine::Gameplay::FogState::Visible;
            if (!disableEnemyFogCulling && fog && fogTileSize > 0 && !isHero) {
                const int tileX = static_cast<int>(
                    std::floor((tf.position.x + fogOriginOffsetX) / static_cast<float>(fogTileSize)));
                const int tileY = static_cast<int>(
                    std::floor((tf.position.y + fogOriginOffsetY) / static_cast<float>(fogTileSize)));
                fogState = fog->getState(tileX, tileY);
                if (isEnemy) {
                    if (fogState != Engine::Gameplay::FogState::Visible) continue;
                } else {
                    if (fogState == Engine::Gameplay::FogState::Unexplored) continue;
                }
            }
            const float visualScale = isEnemy ? kEnemyVisualScale : (isHero ? kHeroVisualScale : 1.0f);
            screenPos.x -= rend.size.x * 0.5f * camera.zoom * visualScale;
            screenPos.y -= rend.size.y * 0.5f * camera.zoom * visualScale;
            Engine::Vec2 scaledSize{rend.size.x * camera.zoom * visualScale, rend.size.y * camera.zoom * visualScale};
            const float cullMargin = 64.0f;
            if (screenPos.x > center.x * 2.0f + cullMargin || screenPos.y > center.y * 2.0f + cullMargin ||
                screenPos.x + scaledSize.x < -cullMargin || screenPos.y + scaledSize.y < -cullMargin) {
                continue;  // simple frustum cull
            }

            Engine::Color color = rend.color;
            const auto* status = registry.get<Engine::ECS::Status>(e);
            const bool cloaked = status && status->container.isStealthed();
            if (cloaked && isEnemy) {
                continue;  // fully hidden to player
            }
            // Keep pickup sprites untinted; they should render at full texture color.
            if (registry.has<Game::Pickup>(e)) {
                color = Engine::Color{255, 255, 255, color.a};
            }
            // Tint bosses differently.
            if (registry.has<Engine::ECS::BossTag>(e)) {
                color = Engine::Color{color.r, static_cast<uint8_t>(std::min(255, color.g / 2)), color.b, color.a};
            }
            if (registry.has<Game::BountyTag>(e)) {
                color = Engine::Color{255, static_cast<uint8_t>(std::min(255, color.g + 40)), 120, color.a};
            }
            if (const auto* ghost = registry.get<Game::Ghost>(e)) {
                if (ghost->active) {
                    color.a = static_cast<uint8_t>(std::min<int>(color.a, 120));
                }
            }
            if (const auto* flash = registry.get<Game::HitFlash>(e)) {
                if (flash->timer > 0.0f) {
                    float t = std::clamp(flash->timer / 0.12f, 0.0f, 1.0f);
                    const bool isEnemy = registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e);
                    if (isEnemy) {
                        // Flash enemies red on hit.
                        auto add = static_cast<uint8_t>(std::min(255.0f, color.r + 160.0f * t));
                        auto sub = static_cast<uint8_t>(std::min(255.0f, 120.0f * t));
                        color = {add,
                                 static_cast<uint8_t>(color.g > sub ? (color.g - sub) : 0),
                                 static_cast<uint8_t>(color.b > sub ? (color.b - sub) : 0),
                                 color.a};
                    } else {
                        // Keep neutral "brighten" flash for non-enemies.
                        auto boost = static_cast<uint8_t>(std::min(255.0f, color.r + 120.0f * t));
                        color = {boost, boost, std::min<uint8_t>(255, static_cast<uint8_t>(color.b + 40)), color.a};
                    }
                }
            }
            if (cloaked && isHero) {
                color.a = static_cast<uint8_t>(std::min<int>(color.a, 90));
            }

            bool flipX = false;
            if (const auto* facing = registry.get<Game::Facing>(e)) {
                flipX = facing->dirX < 0;
            } else if (const auto* vel = registry.get<Engine::ECS::Velocity>(e)) {
                flipX = vel->value.x < -0.01f;
            }

            if (rend.texture) {
                // Do not tint playable characters; keep sprites visually "true" and use alpha-only effects (cloak/ghost).
                Engine::Color tint = color;
                if (isHero) {
                    if (const auto* ht = registry.get<Game::HeroTint>(e)) {
                        tint.r = ht->color.r;
                        tint.g = ht->color.g;
                        tint.b = ht->color.b;
                    } else {
                        tint.r = 255;
                        tint.g = 255;
                        tint.b = 255;
                    }
                }
                if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(e);
                    anim && anim->frameCount > 1) {
                    Engine::IntRect src{anim->frameWidth * anim->currentFrame,
                                        anim->frameHeight * anim->row,
                                        anim->frameWidth,
                                        anim->frameHeight};
                    device_.drawTextureRegionTinted(*rend.texture, screenPos, scaledSize, src, tint, flipX && anim->allowFlipX);
                } else {
                    Engine::IntRect src{0, 0, rend.texture->width(), rend.texture->height()};
                    device_.drawTextureRegionTinted(*rend.texture, screenPos, scaledSize, src, tint, flipX);
                }
            } else {
                device_.drawFilledRect(screenPos, scaledSize, color);
            }

            // Floating health/shield bars for enemies, mini units, and escort target.
            if (registry.has<Engine::ECS::EnemyTag>(e) || registry.has<Engine::ECS::BossTag>(e) ||
                registry.has<Game::MiniUnit>(e) || registry.has<Game::EscortTarget>(e)) {
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

                // Marker over bounty targets.
                if (registry.has<Game::BountyTag>(e)) {
                    Engine::Vec2 markPos{screenPos.x + scaledSize.x * 0.5f - 3.0f, screenPos.y - 12.0f};
                    device_.drawFilledRect(markPos, Engine::Vec2{6.0f, 6.0f}, Engine::Color{255, 180, 80, 240});
                }
            }
    }
}

void RenderSystem::drawGridPass(const Engine::Camera2D& camera, int viewportW, int viewportH,
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
        // Use the smallest tile dimensions as the base grid size so larger textures (e.g. 48x48 dirt patches)
        // can be drawn at their native size without forcing the whole floor grid to that size.
        int baseW = 0;
        int baseH = 0;
        for (const auto& t : *gridVariants) {
            if (!t) continue;
            const int w = t->width();
            const int h = t->height();
            if (w <= 0 || h <= 0) continue;
            if (baseW == 0 || w < baseW) baseW = w;
            if (baseH == 0 || h < baseH) baseH = h;
        }
        if (baseW <= 0 || baseH <= 0) {
            return;
        }

        struct OverlayVariant {
            Engine::TexturePtr tex;
            int spanX{1};
            int spanY{1};
        };

        std::vector<Engine::TexturePtr> baseVariants;
        std::vector<OverlayVariant> overlayVariants;
        baseVariants.reserve(gridVariants->size());
        overlayVariants.reserve(8);
        for (const auto& t : *gridVariants) {
            if (!t) continue;
            if (t->width() == baseW && t->height() == baseH) {
                baseVariants.push_back(t);
            } else {
                OverlayVariant ov{};
                ov.tex = t;
                ov.spanX = std::max(1, t->width() / baseW);
                ov.spanY = std::max(1, t->height() / baseH);
                overlayVariants.push_back(ov);
            }
        }
        if (baseVariants.empty()) {
            // If all variants are oversized, fall back to drawing them as if they were base tiles.
            for (const auto& t : *gridVariants) {
                if (t) baseVariants.push_back(t);
            }
        }

        const int startTileX = static_cast<int>(std::floor(left / static_cast<float>(baseW))) - 2;
        const int endTileX = static_cast<int>(std::ceil(right / static_cast<float>(baseW))) + 2;
        const int startTileY = static_cast<int>(std::floor(top / static_cast<float>(baseH))) - 2;
        const int endTileY = static_cast<int>(std::ceil(bottom / static_cast<float>(baseH))) + 2;

        const float baseWf = static_cast<float>(baseW);
        const float baseHf = static_cast<float>(baseH);
        const float tileScreenW = baseWf * camera.zoom;
        const float tileScreenH = baseHf * camera.zoom;

        // Overlay placement is anchored to the base grid and occurs on a subset of anchor tiles to avoid
        // repeating the same large texture on adjacent cells.
        constexpr uint32_t kOverlayChance256 = 5;  // ~2% chance at anchor tiles (rarer to reduce clumping)

        // Pass 1: base floor tiles.
        for (int ty = startTileY; ty < endTileY; ++ty) {
            for (int tx = startTileX; tx < endTileX; ++tx) {
                const Engine::Vec2 world{tx * baseWf, ty * baseHf};
                Engine::Vec2 screen = Engine::worldToScreen(world, camera,
                                                            static_cast<float>(viewportW),
                                                            static_cast<float>(viewportH));
                screen.x = std::round(screen.x);
                screen.y = std::round(screen.y);

                const uint32_t h = hashCoords(tx, ty);
                const auto& baseTex = baseVariants[h % baseVariants.size()];
                if (baseTex) {
                    device_.drawTexture(*baseTex, screen, Engine::Vec2{tileScreenW + 1.0f, tileScreenH + 1.0f});
                }
            }
        }

        // Pass 2: larger overlay tiles (e.g. 48x48 dirt patch) at native size, drawn after the base so they aren't
        // overwritten by subsequent base-cell draws.
        if (!overlayVariants.empty()) {
            for (int ty = startTileY; ty < endTileY; ++ty) {
                for (int tx = startTileX; tx < endTileX; ++tx) {
                    const uint32_t h = hashCoords(tx, ty);
                    const OverlayVariant& ov = overlayVariants[(h >> 8) % overlayVariants.size()];
                    if (!ov.tex) continue;
                    if ((tx % ov.spanX) != 0 || (ty % ov.spanY) != 0) continue;
                    const uint32_t ho = hashCoords(tx / ov.spanX, ty / ov.spanY) ^ 0xD17A4F29u;
                    if ((ho & 0xFFu) >= kOverlayChance256) continue;

                    const Engine::Vec2 world{tx * baseWf, ty * baseHf};
                    Engine::Vec2 screen = Engine::worldToScreen(world, camera,
                                                                static_cast<float>(viewportW),
                                                                static_cast<float>(viewportH));
                    screen.x = std::round(screen.x);
                    screen.y = std::round(screen.y);
                    const Engine::Vec2 size{static_cast<float>(ov.tex->width()) * camera.zoom + 1.0f,
                                            static_cast<float>(ov.tex->height()) * camera.zoom + 1.0f};
                    device_.drawTexture(*ov.tex, screen, size);
                }
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
