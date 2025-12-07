// Simple bitmap font metadata.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "../assets/Texture.h"

namespace Engine {

struct Glyph {
    float u0, v0, u1, v1;
    int width;
    int height;
    int xOffset;
    int yOffset;
    int xAdvance;
};

struct BitmapFont {
    TexturePtr texture;
    std::array<Glyph, 96> glyphs;  // ASCII 32-127
    int lineHeight{0};
    int base{0};
};

}  // namespace Engine
