#include "NullWindow.h"

#include <string>

#include "../core/Application.h"
#include "../core/Logger.h"
#include "../render/NullRenderDevice.h"

namespace Engine {

bool NullWindow::initialize(const WindowConfig& config) {
    isOpen_ = true;
    logWarn("NullWindow active; no graphics backend yet.");
    logInfo("Requested window: " + config.title + " (" + std::to_string(config.width) + "x" +
            std::to_string(config.height) + ")");
    return true;
}

std::unique_ptr<RenderDevice> NullWindow::createRenderDevice() {
    return std::make_unique<NullRenderDevice>();
}

void NullWindow::pollEvents(Application& app, InputState& /*input*/) {
    if (shutdownRequested_) {
        return;
    }
    shutdownRequested_ = true;
    isOpen_ = false;
    app.requestQuit("NullWindow requested shutdown (no real window attached).");
}

void NullWindow::swapBuffers() {
    // Nothing to do for the null backend.
}

}  // namespace Engine
