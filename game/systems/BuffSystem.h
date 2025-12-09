// Handles timed defensive buffs on entities.
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/gameplay/Combat.h"

namespace Game {

struct ArmorBuff {
    Engine::Gameplay::BuffState state{};
    float duration{0.0f};  // seconds remaining
};

class BuffSystem {
public:
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
        const float dt = static_cast<float>(step.deltaSeconds);
        std::vector<Engine::ECS::Entity> toRemove;
        registry.view<ArmorBuff>([dt, &toRemove](Engine::ECS::Entity e, ArmorBuff& buff) {
            buff.duration -= dt;
            if (buff.duration <= 0.0f) {
                toRemove.push_back(e);
            }
        });
        for (auto e : toRemove) {
            registry.remove<ArmorBuff>(e);
        }
    }
};

}  // namespace Game
