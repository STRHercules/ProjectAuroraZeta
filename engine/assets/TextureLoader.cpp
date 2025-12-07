#include "TextureLoader.h"

#include <SDL.h>
#include <SDL_image.h>

#include "../platform/SDLTexture.h"
#include "../platform/SDLRenderDevice.h"
#include "../core/Logger.h"
#include "../render/RenderDevice.h"

namespace Engine {

std::optional<TexturePtr> TextureLoader::loadBMP(const std::string& path, RenderDevice& device) {
    // Currently only supports SDL backend.
    auto* sdlDevice = dynamic_cast<SDLRenderDevice*>(&device);
    if (!sdlDevice) {
        logWarn("TextureLoader: unsupported render device for BMP load.");
        return std::nullopt;
    }

    SDL_Renderer* renderer = sdlDevice->rawRenderer();
    if (!renderer) {
        logWarn("TextureLoader: renderer unavailable.");
        return std::nullopt;
    }

    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    if (!surface) {
        logWarn(std::string("Failed to load BMP: ") + path + " | " + SDL_GetError());
        return std::nullopt;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!tex) {
        logWarn(std::string("Failed to create texture from ") + path + " | " + SDL_GetError());
        return std::nullopt;
    }

    return TexturePtr(std::make_shared<SDLTexture>(tex));
}

std::optional<TexturePtr> TextureLoader::loadImage(const std::string& path, RenderDevice& device) {
    auto* sdlDevice = dynamic_cast<SDLRenderDevice*>(&device);
    if (!sdlDevice) {
        logWarn("TextureLoader: unsupported render device for image load.");
        return std::nullopt;
    }
    SDL_Renderer* renderer = sdlDevice->rawRenderer();
    if (!renderer) {
        logWarn("TextureLoader: renderer unavailable.");
        return std::nullopt;
    }

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        logWarn(std::string("IMG_Load failed: ") + path + " | " + IMG_GetError());
        return loadBMP(path, device);  // fallback
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!tex) {
        logWarn(std::string("Failed to create texture from ") + path + " | " + SDL_GetError());
        return std::nullopt;
    }
    return TexturePtr(std::make_shared<SDLTexture>(tex));
}

}  // namespace Engine
