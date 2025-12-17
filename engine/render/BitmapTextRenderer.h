// Bitmap font renderer using RenderDevice.
#pragma once

#include "TextRenderer.h"
#include "BitmapFont.h"
#include "RenderDevice.h"

namespace Engine {

class BitmapTextRenderer final : public TextRenderer {
public:
    BitmapTextRenderer(RenderDevice& device, BitmapFont font)
        : device_(device), font_(std::move(font)) {}

    void drawText(const std::string& text, const Vec2& topLeft, float scale, const Color& color) override;
    Vec2 measureText(const std::string& text, float scale) const;

private:
    RenderDevice& device_;
    BitmapFont font_;
};

}  // namespace Engine
