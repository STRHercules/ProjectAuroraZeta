// Simple runtime shop item definition.
#pragma once

#include <string>

namespace Game {

struct ShopItem {
    std::string name;
    std::string desc;
    int cost{0};
    enum class Type { Damage, Health, Speed } type{Type::Damage};
    float value{0.0f};
};

}  // namespace Game
