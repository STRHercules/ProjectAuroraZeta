// Spawns enemies at intervals around the playfield.
#pragma once

#include <random>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"

namespace Game {

struct WaveSettings {
    float interval{3.0f};
    int batchSize{2};
    float enemyHp{50.0f};
    float enemySpeed{80.0f};
    float contactDamage{10.0f};
};

class WaveSystem {
public:
    explicit WaveSystem(std::mt19937& rng, WaveSettings settings);

    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero, int& wave);
    void resetTimer() { timer_ = 0.0; }
    void setTimer(double t) { timer_ = t; }

private:
    std::mt19937& rng_;
    WaveSettings settings_;
    double timer_{0.0};
};

}  // namespace Game
