#include "Application.h"

#include <chrono>
#include <thread>

#include "Logger.h"

namespace Engine {

Application::Application(ApplicationListener& listener, WindowPtr window, WindowConfig config)
    : listener_(listener), window_(std::move(window)), config_(std::move(config)) {}

Application::~Application() { listener_.onShutdown(); }

bool Application::initialize() {
    if (!window_) {
        logError("Application requires a Window instance.");
        return false;
    }

    if (!window_->initialize(config_)) {
        logError("Failed to initialize window.");
        return false;
    }

    renderDevice_ = window_->createRenderDevice();
    if (!renderDevice_) {
        logError("Failed to create render device.");
        return false;
    }

    running_ = listener_.onInitialize(*this);
    return running_;
}

void Application::run() {
    using clock = std::chrono::steady_clock;
    constexpr double targetDelta = 1.0 / 60.0;  // 60 FPS fixed step

    auto last = clock::now();
    while (running_ && window_->isOpen()) {
        auto now = clock::now();
        std::chrono::duration<double> dt = now - last;
        last = now;

        timeStep_.deltaSeconds = dt.count();
        timeStep_.elapsedSeconds += timeStep_.deltaSeconds;

        window_->pollEvents(*this, input_);
        listener_.onUpdate(timeStep_, input_);
        input_.nextFrame();
        window_->swapBuffers();
        renderDevice_->present();

        // Simple frame pacing for the placeholder backend.
        if (timeStep_.deltaSeconds < targetDelta) {
            auto sleepDuration = std::chrono::duration<double>(targetDelta - timeStep_.deltaSeconds);
            std::this_thread::sleep_for(sleepDuration);
        }
    }

    logInfo("Application loop exited.");
}

void Application::requestQuit(const std::string& reason) {
    if (!running_) {
        return;
    }
    running_ = false;
    logInfo("Shutdown requested: " + reason);
}

}  // namespace Engine
