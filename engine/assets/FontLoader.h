// Loads bitmap font metadata and texture.
#pragma once

#include <optional>
#include <string>

#include "../render/BitmapFont.h"
#include "TextureManager.h"

namespace Engine {

class FontLoader {
public:
    // Expects JSON with fields: texture, lineHeight, base, glyphs (optional).
    static std::optional<BitmapFont> load(const std::string& path, TextureManager& texManager);
};

}  // namespace Engine
