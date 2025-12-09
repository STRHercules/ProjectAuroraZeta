// Game-layer render system that draws ECS renderables.
#pragma once

#include <vector>

#include "../../engine/ecs/Registry.h"
#include "../../engine/render/Camera2D.h"
#include "../../engine/render/RenderDevice.h"
#include "../../engine/gameplay/FogOfWar.h"
#include "../../engine/assets/Texture.h"
#include "../components/Facing.h"

namespace Game {

class RenderSystem {
public:
    explicit RenderSystem(Engine::RenderDevice& device) : device_(device) {}

    void draw(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW, int viewportH,
              const Engine::Texture* gridTexture,
              const std::vector<Engine::TexturePtr>* gridVariants = nullptr,
              const Engine::Gameplay::FogOfWarLayer* fog = nullptr, int fogTileSize = 32,
              float fogOriginOffsetX = 0.0f, float fogOriginOffsetY = 0.0f);

private:
    void drawGrid(const Engine::Camera2D& camera, int viewportW, int viewportH, const Engine::Texture* gridTexture,
                  const std::vector<Engine::TexturePtr>* gridVariants, const Engine::Gameplay::FogOfWarLayer* fog,
                  int fogTileSize, float fogOriginOffsetX, float fogOriginOffsetY);

    Engine::RenderDevice& device_;
};

}  // namespace Game
