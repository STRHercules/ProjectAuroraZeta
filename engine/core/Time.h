// Time structures used by the main loop.
#pragma once

namespace Engine {

struct TimeStep {
    double deltaSeconds{0.0};
    double elapsedSeconds{0.0};
};

}  // namespace Engine
