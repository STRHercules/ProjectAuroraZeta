// Minimal bitmap-font text rendering interface.
#pragma once

#include <string>

#include "Color.h"
#include "../math/Vec2.h"
#include "../assets/Texture.h"

namespace Engine {

class TextRenderer {
public:
    virtual ~TextRenderer() = default;
    virtual void drawText(const std::string& text, const Vec2& topLeft, float scale, const Color& color) = 0;
};

}  // namespace Engine
