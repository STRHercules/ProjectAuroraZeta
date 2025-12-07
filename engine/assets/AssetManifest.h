// Data-driven asset manifest for core textures.
#pragma once

#include <optional>
#include <string>

namespace Engine {

struct AssetManifest {
    std::string heroTexture;
    std::string gridTexture;
    std::string fontTexture;
};

class AssetManifestLoader {
public:
    static std::optional<AssetManifest> load(const std::string& path);
};

}  // namespace Engine
