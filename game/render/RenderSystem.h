// Game-layer render system that draws ECS renderables.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/render/Camera2D.h"
#include "../../engine/render/RenderDevice.h"

namespace Game {

class RenderSystem {
public:
    explicit RenderSystem(Engine::RenderDevice& device) : device_(device) {}

    void draw(const Engine::ECS::Registry& registry, const Engine::Camera2D& camera, int viewportW, int viewportH,
              const Engine::Texture* gridTexture);

private:
    void drawGrid(const Engine::Camera2D& camera, int viewportW, int viewportH, const Engine::Texture* gridTexture);

    Engine::RenderDevice& device_;
};

}  // namespace Game
