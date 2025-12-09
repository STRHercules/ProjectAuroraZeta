#include "CameraSystem.h"

#include <algorithm>

#include "../../engine/core/Logger.h"
#include "../../engine/ecs/components/Transform.h"

namespace Game {

void CameraSystem::update(Engine::Camera2D& camera, const Engine::ECS::Registry& registry, Engine::ECS::Entity hero,
                          const Engine::ActionState& actions, const Engine::InputState& input,
                          const Engine::TimeStep& step, int viewportW, int viewportH) {
    // Toggle follow on rising edge.
    if (actions.toggleFollow && !previousToggle_) {
        followHero_ = !followHero_;
        Engine::logInfo(std::string("Camera follow ") + (followHero_ ? "ON" : "OFF"));
    }
    previousToggle_ = actions.toggleFollow;

    // Compute camera target.
    Engine::Vec2 target = camera.position;
    if (followHero_ && hero != Engine::ECS::kInvalidEntity) {
        if (const auto* tf = registry.get<Engine::ECS::Transform>(hero)) {
            target = tf->position;
        }
    } else {
        const float camSpeed = 300.0f;
        Engine::Vec2 camMove{};
        camMove.x += actions.camX * camSpeed * static_cast<float>(step.deltaSeconds);
        camMove.y += actions.camY * camSpeed * static_cast<float>(step.deltaSeconds);

        // Edge panning with mouse near screen borders.
        const int edge = 20;
        // Faster edge panning for RTS feel.
        const float edgeSpeed = 900.0f * static_cast<float>(step.deltaSeconds);
        if (input.mouseX() < edge) camMove.x -= edgeSpeed;
        if (input.mouseX() > viewportW - edge) camMove.x += edgeSpeed;
        if (input.mouseY() < edge) camMove.y -= edgeSpeed;
        if (input.mouseY() > viewportH - edge) camMove.y += edgeSpeed;

        target = camera.position + camMove;
    }

    // Smoothly move toward target.
    const float smooth = 10.0f;
    const float factor = std::min(1.0f, static_cast<float>(step.deltaSeconds) * smooth);
    camera.position.x += (target.x - camera.position.x) * factor;
    camera.position.y += (target.y - camera.position.y) * factor;

    // Zoom from scroll.
    camera.zoom += static_cast<float>(actions.scroll) * 0.05f;
    camera.zoom = std::clamp(camera.zoom, 0.3f, 3.0f);
}

}  // namespace Game
