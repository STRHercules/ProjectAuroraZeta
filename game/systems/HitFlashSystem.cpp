#include "HitFlashSystem.h"

#include "../components/HitFlash.h"

namespace Game {

void HitFlashSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Game::HitFlash>([dt](Engine::ECS::Entity /*e*/, Game::HitFlash& flash) {
        flash.timer -= dt;
        if (flash.timer < 0.0f) flash.timer = 0.0f;
    });
}

}  // namespace Game
