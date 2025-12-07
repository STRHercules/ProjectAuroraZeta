// Core application loop orchestrator.
#pragma once

#include <memory>
#include <string>

#include "ApplicationListener.h"
#include "Time.h"
#include "../input/InputState.h"
#include "../platform/Window.h"
#include "../render/RenderDevice.h"

namespace Engine {

class Application {
public:
    Application(ApplicationListener& listener, WindowPtr window, WindowConfig config = {});
    ~Application();

    bool initialize();
    void run();
    void requestQuit(const std::string& reason);

    Window& window() { return *window_; }
    const Window& window() const { return *window_; }
    RenderDevice& renderer() { return *renderDevice_; }
    const RenderDevice& renderer() const { return *renderDevice_; }
    const WindowConfig& config() const { return config_; }

private:
    ApplicationListener& listener_;
    WindowPtr window_;
    WindowConfig config_;
    bool running_{false};
    TimeStep timeStep_{};
    InputState input_{};
    RenderDevicePtr renderDevice_;
};

}  // namespace Engine
