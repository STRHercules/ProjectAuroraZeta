#include "SDLTexture.h"

#include <SDL.h>

namespace Engine {

SDLTexture::SDLTexture(SDL_Texture* texture) : texture_(texture) {
    if (texture_) {
        SDL_QueryTexture(texture_, nullptr, nullptr, &width_, &height_);
    }
}

SDLTexture::~SDLTexture() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
    }
}

}  // namespace Engine
