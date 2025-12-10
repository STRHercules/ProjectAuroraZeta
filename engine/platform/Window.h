// Abstract window interface; real implementations live under /engine/platform.
#pragma once

#include <memory>
#include <string>

namespace Engine {

struct WindowConfig {
    int width{1280};
    int height{720};
    std::string title{"Project Aurora Zeta"};
    bool vsync{true};
};

class Application;

class Window {
public:
    virtual ~Window() = default;

    virtual bool initialize(const WindowConfig& config) = 0;
    virtual void pollEvents(Application& app, class InputState& input) = 0;
    virtual std::unique_ptr<class RenderDevice> createRenderDevice() = 0;
    virtual void swapBuffers() = 0;
    virtual bool isOpen() const = 0;
};

using WindowPtr = std::unique_ptr<Window>;

}  // namespace Engine
#include <memory>
