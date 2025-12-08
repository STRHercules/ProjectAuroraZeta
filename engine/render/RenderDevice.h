// Minimal immediate-mode 2D render device.
#pragma once

#include <memory>

#include "../math/Vec2.h"
#include "Color.h"

namespace Engine {

struct IntRect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    virtual void clear(const Color& color) = 0;
    virtual void drawFilledRect(const Vec2& topLeft, const Vec2& size, const Color& color) = 0;
    virtual void drawTexture(const class Texture& texture, const Vec2& topLeft, const Vec2& size) = 0;
    virtual void drawTextureRegion(const class Texture& texture, const Vec2& topLeft, const Vec2& size,
                                   const IntRect& source, bool flipX = false) = 0;
    virtual void present() = 0;
};

using RenderDevicePtr = std::unique_ptr<RenderDevice>;

}  // namespace Engine
