// Placeholder window implementation that immediately requests shutdown.
#pragma once

#include "Window.h"

namespace Engine {

class NullWindow final : public Window {
public:
    bool initialize(const WindowConfig& config) override;
    std::unique_ptr<class RenderDevice> createRenderDevice() override;
    void pollEvents(Application& app, class InputState& input) override;
    void swapBuffers() override;
    bool isOpen() const override { return isOpen_; }

private:
    bool isOpen_{false};
    bool shutdownRequested_{false};
};

}  // namespace Engine
