// SDL implementation of Texture.
#pragma once

#include <SDL.h>

#include "../assets/Texture.h"

namespace Engine {

class SDLTexture final : public Texture {
public:
    explicit SDLTexture(SDL_Texture* texture);
    ~SDLTexture() override;

    int width() const override { return width_; }
    int height() const override { return height_; }
    SDL_Texture* raw() const { return texture_; }

private:
    SDL_Texture* texture_{nullptr};
    int width_{0};
    int height_{0};
};

}  // namespace Engine
