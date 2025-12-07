#include "AssetManifest.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace Engine {

std::optional<AssetManifest> AssetManifestLoader::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return std::nullopt;
    }

    AssetManifest manifest{};
    if (j.contains("heroTexture") && j["heroTexture"].is_string()) {
        manifest.heroTexture = j["heroTexture"].get<std::string>();
    }
    if (j.contains("gridTexture") && j["gridTexture"].is_string()) {
        manifest.gridTexture = j["gridTexture"].get<std::string>();
    }
    if (j.contains("fontTexture") && j["fontTexture"].is_string()) {
        manifest.fontTexture = j["fontTexture"].get<std::string>();
    }

    return manifest;
}

}  // namespace Engine
