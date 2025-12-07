// Simple blocking texture loader (SDL-based).
#pragma once

#include <optional>
#include <string>

#include "Texture.h"

namespace Engine {

class TextureLoader {
public:
    // Loads a BMP file using the underlying renderer implementation; returns nullptr on failure.
    static std::optional<TexturePtr> loadBMP(const std::string& path, class RenderDevice& device);
    // Loads an image via SDL_image (PNG/JPG/etc.) if available; falls back to BMP loader.
    static std::optional<TexturePtr> loadImage(const std::string& path, class RenderDevice& device);
};

}  // namespace Engine
