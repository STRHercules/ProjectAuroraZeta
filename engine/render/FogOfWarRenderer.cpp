#include "FogOfWarRenderer.h"

#include <algorithm>

#include "CameraUtil.h"

namespace Engine {

void renderFog(SDL_Renderer* renderer, const Gameplay::FogOfWarLayer& fog, int tileSize,
               float originOffsetX, float originOffsetY,
               const Camera2D& camera, int viewportW, int viewportH, SDL_Texture* fogTexture) {
    if (!renderer || !fogTexture || tileSize <= 0) {
        return;
    }

    const float viewW = viewportW / camera.zoom;
    const float viewH = viewportH / camera.zoom;
    const float camLeft = camera.position.x - viewW * 0.5f;
    const float camTop = camera.position.y - viewH * 0.5f;

    const int startTileX = static_cast<int>(std::floor((camLeft + originOffsetX) / tileSize)) - 1;
    const int startTileY = static_cast<int>(std::floor((camTop + originOffsetY) / tileSize)) - 1;
    const int endTileX = static_cast<int>(std::ceil((camLeft + viewW + originOffsetX) / tileSize)) + 2;
    const int endTileY = static_cast<int>(std::ceil((camTop + viewH + originOffsetY) / tileSize)) + 2;

    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(renderer, &prev);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const float tileScreenW = tileSize * camera.zoom;
    const float tileScreenH = tileSize * camera.zoom;

    for (int ty = startTileY; ty < endTileY; ++ty) {
        int runStart = startTileX;
        Gameplay::FogState runState = Gameplay::FogState::Visible;
        for (int tx = startTileX; tx <= endTileX; ++tx) {
            Gameplay::FogState state = (tx < endTileX) ? fog.getState(tx, ty) : Gameplay::FogState::Visible;
            if (state == runState) {
                continue;
            }
            // Flush previous run if it was non-visible.
            if (runState != Gameplay::FogState::Visible) {
                // World position of run start
                Vec2 world{runStart * static_cast<float>(tileSize) - originOffsetX,
                           ty * static_cast<float>(tileSize) - originOffsetY};
                Vec2 screen = worldToScreen(world, camera, static_cast<float>(viewportW), static_cast<float>(viewportH));
                float runWidth = static_cast<float>(tx - runStart) * tileScreenW;
                SDL_FRect rect{screen.x, screen.y, runWidth, tileScreenH};
                const Uint8 alpha = (runState == Gameplay::FogState::Fogged) ? 140 : 255;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
                SDL_RenderFillRectF(renderer, &rect);
            }
            runStart = tx;
            runState = state;
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, prev);
}

}  // namespace Engine
