// Data-driven input binding definitions.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

struct AxisBinding {
    std::vector<std::string> positiveX;
    std::vector<std::string> negativeX;
    std::vector<std::string> positiveY;
    std::vector<std::string> negativeY;
};

struct InputBindings {
    AxisBinding move;
    AxisBinding camera;
    std::vector<std::string> toggleFollow;
    std::vector<std::string> restart;
    std::vector<std::string> toggleShop;
    std::vector<std::string> pause;
};

}  // namespace Engine
