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
    std::vector<std::string> characterScreen;
    std::vector<std::string> talentTree;
    std::vector<std::string> pause;
    std::vector<std::string> dash;
    std::vector<std::string> swapWeapon;
    std::vector<std::string> interact;
    std::vector<std::string> useItem;
    std::vector<std::string> hotbar1;
    std::vector<std::string> hotbar2;
    std::vector<std::string> ability1;
    std::vector<std::string> ability2;
    std::vector<std::string> ability3;
    std::vector<std::string> ultimate;
    std::vector<std::string> reload;
    std::vector<std::string> menuBack;
    std::vector<std::string> buildMenu;
};

}  // namespace Engine
