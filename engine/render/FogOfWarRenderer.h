// SDL-based fog overlay renderer for flat 2D maps.
#pragma once

#include <SDL.h>

#include "../gameplay/FogOfWar.h"
#include "Camera2D.h"

namespace Engine {

void renderFog(SDL_Renderer* renderer, const Gameplay::FogOfWarLayer& fog, int tileSize,
               float originOffsetX, float originOffsetY,
               const Camera2D& camera, int viewportW, int viewportH, SDL_Texture* fogTexture);

}  // namespace Engine
