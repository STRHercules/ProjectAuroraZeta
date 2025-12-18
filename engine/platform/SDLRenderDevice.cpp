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
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(renderer_, &prev);
    if (color.a < 255) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &rect);
    SDL_SetRenderDrawBlendMode(renderer_, prev);
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

void SDLRenderDevice::drawTextureTinted(const Texture& textureBase, const Vec2& topLeft, const Vec2& size, const Color& tint) {
    const auto* tex = dynamic_cast<const SDLTexture*>(&textureBase);
    if (!tex) {
        return;
    }
    SDL_Rect dst{};
    dst.x = static_cast<int>(topLeft.x);
    dst.y = static_cast<int>(topLeft.y);
    dst.w = static_cast<int>(size.x);
    dst.h = static_cast<int>(size.y);

    SDL_Texture* raw = tex->raw();
    Uint8 prevR = 255, prevG = 255, prevB = 255, prevA = 255;
    SDL_GetTextureColorMod(raw, &prevR, &prevG, &prevB);
    SDL_GetTextureAlphaMod(raw, &prevA);
    SDL_BlendMode prevBlend = SDL_BLENDMODE_NONE;
    SDL_GetTextureBlendMode(raw, &prevBlend);

    if (tint.a < 255) {
        SDL_SetTextureBlendMode(raw, SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureColorMod(raw, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(raw, tint.a);
    SDL_RenderCopy(renderer_, raw, nullptr, &dst);

    SDL_SetTextureColorMod(raw, prevR, prevG, prevB);
    SDL_SetTextureAlphaMod(raw, prevA);
    SDL_SetTextureBlendMode(raw, prevBlend);
}

void SDLRenderDevice::drawTextureRegion(const Texture& textureBase, const Vec2& topLeft, const Vec2& size,
                                        const IntRect& source, bool flipX) {
    const auto* tex = dynamic_cast<const SDLTexture*>(&textureBase);
    if (!tex) {
        return;
    }
    SDL_Rect src{};
    src.x = source.x;
    src.y = source.y;
    src.w = source.w;
    src.h = source.h;
    SDL_Rect dst{};
    dst.x = static_cast<int>(topLeft.x);
    dst.y = static_cast<int>(topLeft.y);
    dst.w = static_cast<int>(size.x);
    dst.h = static_cast<int>(size.y);
    const SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer_, tex->raw(), &src, &dst, 0.0, nullptr, flip);
}

void SDLRenderDevice::drawTextureRegionTinted(const Texture& textureBase, const Vec2& topLeft, const Vec2& size,
                                              const IntRect& source, const Color& tint, bool flipX) {
    const auto* tex = dynamic_cast<const SDLTexture*>(&textureBase);
    if (!tex) {
        return;
    }
    SDL_Rect src{};
    src.x = source.x;
    src.y = source.y;
    src.w = source.w;
    src.h = source.h;
    SDL_Rect dst{};
    dst.x = static_cast<int>(topLeft.x);
    dst.y = static_cast<int>(topLeft.y);
    dst.w = static_cast<int>(size.x);
    dst.h = static_cast<int>(size.y);

    SDL_Texture* raw = tex->raw();
    Uint8 prevR = 255, prevG = 255, prevB = 255, prevA = 255;
    SDL_GetTextureColorMod(raw, &prevR, &prevG, &prevB);
    SDL_GetTextureAlphaMod(raw, &prevA);
    SDL_BlendMode prevBlend = SDL_BLENDMODE_NONE;
    SDL_GetTextureBlendMode(raw, &prevBlend);

    if (tint.a < 255) {
        SDL_SetTextureBlendMode(raw, SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureColorMod(raw, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(raw, tint.a);
    const SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer_, raw, &src, &dst, 0.0, nullptr, flip);

    SDL_SetTextureColorMod(raw, prevR, prevG, prevB);
    SDL_SetTextureAlphaMod(raw, prevA);
    SDL_SetTextureBlendMode(raw, prevBlend);
}

void SDLRenderDevice::present() { SDL_RenderPresent(renderer_); }

}  // namespace Engine
