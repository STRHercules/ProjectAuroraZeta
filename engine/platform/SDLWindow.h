// SDL2-backed window implementation.
#pragma once

#include <SDL.h>

#include "Window.h"

namespace Engine {

class SDLWindow final : public Window {
public:
    SDLWindow();
    ~SDLWindow() override;

    bool initialize(const WindowConfig& config) override;
    std::unique_ptr<class RenderDevice> createRenderDevice() override;
    void pollEvents(Application& app, class InputState& input) override;
    void swapBuffers() override;
    bool isOpen() const override { return isOpen_; }

private:
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    bool isOpen_{false};
};

}  // namespace Engine
