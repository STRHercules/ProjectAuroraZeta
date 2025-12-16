#include "AnimationSystem.h"

#include "../../engine/ecs/components/SpriteAnimation.h"

namespace Game {

void AnimationSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    registry.view<Engine::ECS::SpriteAnimation>([dt](Engine::ECS::Entity /*e*/, Engine::ECS::SpriteAnimation& anim) {
        if (anim.frameCount <= 1) return;
        if (anim.finished) return;
        float frameTime = anim.frameDuration > 0.0f ? anim.frameDuration : 0.01f;
        anim.accumulator += dt;
        while (anim.accumulator >= frameTime) {
            anim.accumulator -= frameTime;
            if (anim.loop) {
                anim.currentFrame = (anim.currentFrame + 1) % anim.frameCount;
            } else {
                if (anim.currentFrame + 1 >= anim.frameCount) {
                    anim.currentFrame = anim.holdOnLastFrame ? std::max(0, anim.frameCount - 1) : anim.frameCount - 1;
                    anim.finished = true;
                    anim.accumulator = 0.0f;
                    break;
                } else {
                    anim.currentFrame += 1;
                }
            }
        }
    });
}

}  // namespace Game
