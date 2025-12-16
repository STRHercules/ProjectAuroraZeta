#pragma once

#include <unordered_map>
#include <string>

#include "../../engine/status/StatusContainer.h"

namespace Game {

// Loads status specifications from data/statuses.json and provides prefab specs.
class ZetaStatusFactory {
public:
    bool load(const std::string& path);
    Engine::Status::StatusSpec make(Engine::Status::EStatusId id) const;

private:
    std::unordered_map<Engine::Status::EStatusId, Engine::Status::StatusSpec> specs_;
};

}  // namespace Game
