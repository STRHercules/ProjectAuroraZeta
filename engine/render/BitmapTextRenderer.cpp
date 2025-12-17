#include "BitmapTextRenderer.h"

#include <cctype>

namespace Engine {

void BitmapTextRenderer::drawText(const std::string& text, const Vec2& topLeft, float scale, const Color& color) {
    if (!font_.texture) return;
    Vec2 pen = topLeft;
    for (char c : text) {
        if (c == '\n') {
            pen.x = topLeft.x;
            pen.y += font_.lineHeight * scale;
            continue;
        }
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc >= 128) continue;
        const Glyph& g = font_.glyphs[uc - 32];
        Vec2 size{g.width * scale, g.height * scale};
        Vec2 pos{pen.x + g.xOffset * scale, pen.y + g.yOffset * scale};
        // Textures are drawn as-is; tint via colored quad underlay.
        if (color.a > 0 && (color.r != 255 || color.g != 255 || color.b != 255)) {
            device_.drawFilledRect(pos, size, color);
        }
        device_.drawTexture(*font_.texture, pos, size);
        pen.x += g.xAdvance * scale;
    }
}

Vec2 BitmapTextRenderer::measureText(const std::string& text, float scale) const {
    if (text.empty()) return Vec2{0.0f, 0.0f};
    float maxW = 0.0f;
    float lineW = 0.0f;
    int lines = 1;
    for (char c : text) {
        if (c == '\n') {
            maxW = std::max(maxW, lineW);
            lineW = 0.0f;
            lines += 1;
            continue;
        }
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc >= 128) continue;
        const Glyph& g = font_.glyphs[uc - 32];
        lineW += static_cast<float>(g.xAdvance) * scale;
    }
    maxW = std::max(maxW, lineW);
    const float h = static_cast<float>(std::max(1, font_.lineHeight)) * scale * static_cast<float>(lines);
    return Vec2{maxW, h};
}

}  // namespace Engine
