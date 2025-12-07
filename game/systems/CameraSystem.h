// Handles camera follow/pan/zoom behavior.
#pragma once

#include "../../engine/core/Time.h"
#include "../../engine/ecs/Registry.h"
#include "../../engine/input/ActionState.h"
#include "../../engine/input/InputState.h"
#include "../../engine/render/Camera2D.h"

namespace Game {

class CameraSystem {
public:
    void update(Engine::Camera2D& camera, const Engine::ECS::Registry& registry, Engine::ECS::Entity hero,
                const Engine::ActionState& actions, const Engine::InputState& input, const Engine::TimeStep& step,
                int viewportW, int viewportH);

    bool followEnabled() const { return followHero_; }
    void resetFollow(bool enable = true) {
        followHero_ = enable;
        previousToggle_ = false;
    }

private:
    bool followHero_{true};
    bool previousToggle_{false};
};

}  // namespace Game
