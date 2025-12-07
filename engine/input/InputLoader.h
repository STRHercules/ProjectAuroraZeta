// Loads input bindings from JSON file.
#pragma once

#include <optional>
#include <string>

#include "InputBinding.h"

namespace Engine {

class InputLoader {
public:
    static std::optional<InputBindings> loadFromFile(const std::string& path);
};

}  // namespace Engine
