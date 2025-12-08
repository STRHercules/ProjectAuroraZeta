// No-op renderer used by NullWindow or headless runs.
#pragma once

#include "RenderDevice.h"

namespace Engine {

class NullRenderDevice final : public RenderDevice {
public:
    void clear(const Color& /*color*/) override {}
    void drawFilledRect(const Vec2& /*topLeft*/, const Vec2& /*size*/, const Color& /*color*/) override {}
    void drawTexture(const Texture& /*texture*/, const Vec2& /*topLeft*/, const Vec2& /*size*/) override {}
    void drawTextureRegion(const Texture& /*texture*/, const Vec2& /*topLeft*/, const Vec2& /*size*/,
                           const IntRect& /*source*/, bool /*flipX*/) override {}
    void present() override {}
};

}  // namespace Engine
