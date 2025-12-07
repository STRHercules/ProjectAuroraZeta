#include "SDLWindow.h"

#include <SDL.h>

#include "../core/Application.h"
#include "../core/Logger.h"
#include "../input/InputState.h"
#include "SDLRenderDevice.h"

namespace Engine {

SDLWindow::SDLWindow() = default;

SDLWindow::~SDLWindow() {
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool SDLWindow::initialize(const WindowConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        logError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               config.width, config.height, SDL_WINDOW_SHOWN);
    if (!window_) {
        logError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    const auto rendererFlags = config.vsync ? SDL_RENDERER_PRESENTVSYNC : 0;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | rendererFlags);
    if (!renderer_) {
        logError(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        return false;
    }

    isOpen_ = true;
    logInfo("SDLWindow initialized.");
    return true;
}

std::unique_ptr<RenderDevice> SDLWindow::createRenderDevice() {
    if (!renderer_) {
        return nullptr;
    }
    return std::make_unique<SDLRenderDevice>(renderer_);
}

void SDLWindow::pollEvents(Application& app, InputState& input) {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_QUIT:
                isOpen_ = false;
                app.requestQuit("Window close requested.");
                break;
            case SDL_KEYDOWN:
                switch (evt.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        isOpen_ = false;
                        app.requestQuit("Escape pressed.");
                        break;
                    case SDLK_r:
                        input.setKeyDown(InputKey::Restart, true);
                        break;
                    case SDLK_w:
                        input.setKeyDown(InputKey::Forward, true);
                        break;
                    case SDLK_s:
                        input.setKeyDown(InputKey::Backward, true);
                        break;
                    case SDLK_a:
                        input.setKeyDown(InputKey::Left, true);
                        break;
                    case SDLK_d:
                        input.setKeyDown(InputKey::Right, true);
                        break;
                    case SDLK_UP:
                        input.setKeyDown(InputKey::CamUp, true);
                        break;
                    case SDLK_DOWN:
                        input.setKeyDown(InputKey::CamDown, true);
                        break;
                    case SDLK_LEFT:
                        input.setKeyDown(InputKey::CamLeft, true);
                        break;
                    case SDLK_RIGHT:
                        input.setKeyDown(InputKey::CamRight, true);
                        break;
                    case SDLK_c:
                        input.setKeyDown(InputKey::ToggleFollow, true);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYUP:
                switch (evt.key.keysym.sym) {
                    case SDLK_r:
                        input.setKeyDown(InputKey::Restart, false);
                        break;
                    case SDLK_w:
                        input.setKeyDown(InputKey::Forward, false);
                        break;
                    case SDLK_s:
                        input.setKeyDown(InputKey::Backward, false);
                        break;
                    case SDLK_a:
                        input.setKeyDown(InputKey::Left, false);
                        break;
                    case SDLK_d:
                        input.setKeyDown(InputKey::Right, false);
                        break;
                    case SDLK_UP:
                        input.setKeyDown(InputKey::CamUp, false);
                        break;
                    case SDLK_DOWN:
                        input.setKeyDown(InputKey::CamDown, false);
                        break;
                    case SDLK_LEFT:
                        input.setKeyDown(InputKey::CamLeft, false);
                        break;
                    case SDLK_RIGHT:
                        input.setKeyDown(InputKey::CamRight, false);
                        break;
                    case SDLK_c:
                        input.setKeyDown(InputKey::ToggleFollow, false);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_MOUSEWHEEL:
                input.addScroll(evt.wheel.y);
                break;
            case SDL_MOUSEMOTION:
                input.setMousePosition(evt.motion.x, evt.motion.y);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (evt.button.button == SDL_BUTTON_LEFT) input.setMouseButtonDown(0, true);
                if (evt.button.button == SDL_BUTTON_MIDDLE) input.setMouseButtonDown(1, true);
                if (evt.button.button == SDL_BUTTON_RIGHT) input.setMouseButtonDown(2, true);
                break;
            case SDL_MOUSEBUTTONUP:
                if (evt.button.button == SDL_BUTTON_LEFT) input.setMouseButtonDown(0, false);
                if (evt.button.button == SDL_BUTTON_MIDDLE) input.setMouseButtonDown(1, false);
                if (evt.button.button == SDL_BUTTON_RIGHT) input.setMouseButtonDown(2, false);
                break;
            default:
                break;
        }
    }
}

void SDLWindow::swapBuffers() {
    if (!renderer_) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, 12, 12, 18, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);
}

}  // namespace Engine
