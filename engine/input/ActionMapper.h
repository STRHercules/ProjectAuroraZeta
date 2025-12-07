// Maps InputState to higher-level ActionState axes.
#pragma once

#include "ActionState.h"
#include "InputBinding.h"
#include "InputState.h"

namespace Engine {

class ActionMapper {
public:
    explicit ActionMapper(InputBindings bindings = {}) : bindings_(std::move(bindings)) {}

    ActionState sample(const InputState& input) const;

private:
    float axisFromKeys(const std::vector<std::string>& positive, const std::vector<std::string>& negative,
                       const InputState& input) const;

    InputBindings bindings_;
};

}  // namespace Engine
