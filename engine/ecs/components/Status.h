#pragma once

#include "../../status/StatusContainer.h"

namespace Engine::ECS {

// Component wrapper holding a status container per entity.
struct Status {
    Engine::Status::StatusContainer container;
};

}  // namespace Engine::ECS
