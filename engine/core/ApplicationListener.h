// Engine-level application lifecycle interface (engine-agnostic).
#pragma once

namespace Engine {

class Application;
struct TimeStep;
class InputState;

class ApplicationListener {
public:
    virtual ~ApplicationListener() = default;

    virtual bool onInitialize(Application& app) = 0;
    virtual void onUpdate(const TimeStep& step, const InputState& input) = 0;
    virtual void onShutdown() = 0;
};

}  // namespace Engine
