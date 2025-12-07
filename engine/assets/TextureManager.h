// Simple texture cache keyed by path.
#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "Texture.h"

namespace Engine {

class TextureManager {
public:
    explicit TextureManager(class RenderDevice& device) : device_(device) {}

    // Loads or returns cached BMP texture. Returns nullopt on failure.
    std::optional<TexturePtr> getOrLoadBMP(const std::string& path);

private:
    class RenderDevice& device_;
    std::unordered_map<std::string, TexturePtr> cache_;
};

}  // namespace Engine
