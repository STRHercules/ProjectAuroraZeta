// Data-driven asset manifest for core textures.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Engine {

struct AssetManifest {
    std::string heroTexture;
    std::string gridTexture;
    std::vector<std::string> gridTextures;
    std::string fontTexture;
};

class AssetManifestLoader {
public:
    static std::optional<AssetManifest> load(const std::string& path);
};

}  // namespace Engine
