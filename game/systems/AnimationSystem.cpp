#include "AnimationSystem.h"

#include "../../engine/ecs/components/SpriteAnimation.h"

namespace Game {

void AnimationSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Engine::ECS::SpriteAnimation>([dt](Engine::ECS::Entity /*e*/, Engine::ECS::SpriteAnimation& anim) {
        if (anim.frameCount <= 1) return;
        float frameTime = anim.frameDuration > 0.0f ? anim.frameDuration : 0.01f;
        anim.accumulator += dt;
        while (anim.accumulator >= frameTime) {
            anim.accumulator -= frameTime;
            anim.currentFrame = (anim.currentFrame + 1) % anim.frameCount;
        }
    });
}

}  // namespace Game
