#include "SDLRenderDevice.h"

#include <SDL.h>

#include "../platform/SDLTexture.h"

namespace Engine {

SDLRenderDevice::SDLRenderDevice(SDL_Renderer* renderer) : renderer_(renderer) {}

void SDLRenderDevice::clear(const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void SDLRenderDevice::drawFilledRect(const Vec2& topLeft, const Vec2& size, const Color& color) {
    SDL_Rect rect{};
    rect.x = static_cast<int>(topLeft.x);
    rect.y = static_cast<int>(topLeft.y);
    rect.w = static_cast<int>(size.x);
    rect.h = static_cast<int>(size.y);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &rect);
}

void SDLRenderDevice::drawTexture(const Texture& textureBase, const Vec2& topLeft, const Vec2& size) {
    const auto* tex = dynamic_cast<const SDLTexture*>(&textureBase);
    if (!tex) {
        return;
    }
    SDL_Rect dst{};
    dst.x = static_cast<int>(topLeft.x);
    dst.y = static_cast<int>(topLeft.y);
    dst.w = static_cast<int>(size.x);
    dst.h = static_cast<int>(size.y);
    SDL_RenderCopy(renderer_, tex->raw(), nullptr, &dst);
}

void SDLRenderDevice::present() { SDL_RenderPresent(renderer_); }

}  // namespace Engine
