#include "FontLoader.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "../core/Logger.h"

namespace Engine {

std::optional<BitmapFont> FontLoader::load(const std::string& path, TextureManager& texManager) {
    std::ifstream in(path);
    if (!in.is_open()) {
        logWarn("FontLoader: cannot open " + path);
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        logWarn("FontLoader: failed to parse " + path);
        return std::nullopt;
    }

    BitmapFont font{};
    if (!j.contains("texture") || !j["texture"].is_string()) {
        logWarn("FontLoader: missing texture field");
        return std::nullopt;
    }
    auto texPath = j["texture"].get<std::string>();
    auto tex = texManager.getOrLoadBMP(texPath);
    if (!tex) {
        logWarn("FontLoader: failed to load texture " + texPath);
        return std::nullopt;
    }
    font.texture = *tex;
    font.lineHeight = j.value("lineHeight", 16);
    font.base = j.value("base", 14);

    // If glyphs array is absent, assume monospace 16x16 grid of 8x8 glyphs starting at ASCII 32.
    if (j.contains("glyphs") && j["glyphs"].is_array() && j["glyphs"].size() == 96) {
        for (size_t i = 0; i < 96; ++i) {
            const auto& g = j["glyphs"][i];
            Glyph glyph{};
            glyph.u0 = g.value("u0", 0.0f);
            glyph.v0 = g.value("v0", 0.0f);
            glyph.u1 = g.value("u1", 0.0f);
            glyph.v1 = g.value("v1", 0.0f);
            glyph.width = g.value("w", 0);
            glyph.height = g.value("h", 0);
            glyph.xOffset = g.value("xOffset", 0);
            glyph.yOffset = g.value("yOffset", 0);
            glyph.xAdvance = g.value("xAdvance", glyph.width);
            font.glyphs[i] = glyph;
        }
    } else {
        // Monospace fallback: 8x8 cells on a 128x128 texture (16x16 grid).
        const int cols = 16;
        const int rows = 6;  // 96 chars
        const float texW = static_cast<float>(font.texture->width());
        const float texH = static_cast<float>(font.texture->height());
        const float cellW = texW / cols;
        const float cellH = texH / rows;
        for (size_t i = 0; i < 96; ++i) {
            int col = static_cast<int>(i % cols);
            int row = static_cast<int>(i / cols);
            Glyph glyph{};
            glyph.u0 = (col * cellW) / texW;
            glyph.v0 = (row * cellH) / texH;
            glyph.u1 = ((col + 1) * cellW) / texW;
            glyph.v1 = ((row + 1) * cellH) / texH;
            glyph.width = static_cast<int>(cellW);
            glyph.height = static_cast<int>(cellH);
            glyph.xAdvance = glyph.width;
            font.glyphs[i] = glyph;
        }
    }

    return font;
}

}  // namespace Engine
