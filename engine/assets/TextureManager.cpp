#include "TextureManager.h"

#include "TextureLoader.h"
#include "../core/Logger.h"

namespace Engine {

std::optional<TexturePtr> TextureManager::getOrLoadBMP(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        return it->second;
    }
    auto loaded = TextureLoader::loadImage(path, device_);
    if (!loaded) {
        logWarn("TextureManager: failed to load " + path);
        return std::nullopt;
    }
    cache_[path] = *loaded;
    return *loaded;
}

}  // namespace Engine
