// SDL2 implementation of RenderDevice.
#pragma once

#include <SDL.h>

#include "../render/RenderDevice.h"

namespace Engine {

class SDLRenderDevice final : public RenderDevice {
public:
    explicit SDLRenderDevice(SDL_Renderer* renderer);

    void clear(const Color& color) override;
    void drawFilledRect(const Vec2& topLeft, const Vec2& size, const Color& color) override;
    void drawTexture(const Texture& texture, const Vec2& topLeft, const Vec2& size) override;
    void drawTextureRegion(const Texture& texture, const Vec2& topLeft, const Vec2& size,
                           const IntRect& source, bool flipX = false) override;
    void present() override;

    SDL_Renderer* rawRenderer() const { return renderer_; }

    // Helper used by loaders; safe only while renderer_ is valid.
    SDL_Renderer* rendererHandle() const { return renderer_; }

private:
    SDL_Renderer* renderer_{nullptr};  // owned by SDLWindow
};

}  // namespace Engine
