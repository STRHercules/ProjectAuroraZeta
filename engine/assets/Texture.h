// Texture handle placeholder (SDL implementation stores SDL_Texture*).
#pragma once

#include <memory>
#include <string>

namespace Engine {

class Texture {
public:
    virtual ~Texture() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

using TexturePtr = std::shared_ptr<Texture>;

}  // namespace Engine
