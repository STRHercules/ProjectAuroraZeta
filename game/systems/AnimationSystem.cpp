#include "AnimationSystem.h"

#include <algorithm>

#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../components/MultiRowSpriteAnim.h"

namespace Game {

void AnimationSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    const float dt = static_cast<float>(step.deltaSeconds);
    // Multi-row animations: drive row/column selection explicitly (sequence 0..totalFrames-1).
    registry.view<Engine::ECS::SpriteAnimation, Game::MultiRowSpriteAnim>(
        [dt](Engine::ECS::Entity /*e*/, Engine::ECS::SpriteAnimation& anim, Game::MultiRowSpriteAnim& mr) {
            if (mr.totalFrames <= 1 || mr.columns <= 0) return;
            if (mr.finished) return;

            const float frameTime = mr.frameDuration > 0.0f ? mr.frameDuration : 0.01f;
            mr.accumulator += dt;
            while (mr.accumulator >= frameTime) {
                mr.accumulator -= frameTime;

                // Optional linger on a specific frame.
                if (mr.holdFrame >= 0 && mr.current == mr.holdFrame && mr.holdExtraSeconds > 0.0f) {
                    if (mr.holdRemaining <= 0.0f) {
                        mr.holdRemaining = mr.holdExtraSeconds;
                    }
                    mr.holdRemaining -= frameTime;
                    if (mr.holdRemaining > 0.0f) {
                        break;
                    }
                    mr.holdRemaining = 0.0f;
                }

                if (mr.loop) {
                    mr.current = (mr.current + 1) % mr.totalFrames;
                } else {
                    if (mr.current + 1 >= mr.totalFrames) {
                        mr.current = mr.holdOnLastFrame ? std::max(0, mr.totalFrames - 1) : mr.totalFrames - 1;
                        mr.finished = true;
                        mr.accumulator = 0.0f;
                        break;
                    }
                    mr.current += 1;
                }
            }

            // Project absolute frame index into (row, col) for the renderer.
            const int absFrame = std::clamp(mr.current, 0, std::max(0, mr.totalFrames - 1));
            anim.row = absFrame / mr.columns;
            anim.currentFrame = absFrame % mr.columns;
            anim.frameCount = std::max(1, mr.columns);
            anim.frameDuration = mr.frameDuration;
            anim.loop = mr.loop;
            anim.holdOnLastFrame = mr.holdOnLastFrame;
            anim.finished = mr.finished;
        });

    // Default single-row animations (skip those managed by MultiRowSpriteAnim).
    registry.view<Engine::ECS::SpriteAnimation>([dt, &registry](Engine::ECS::Entity e, Engine::ECS::SpriteAnimation& anim) {
        if (registry.has<Game::MultiRowSpriteAnim>(e)) return;
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
