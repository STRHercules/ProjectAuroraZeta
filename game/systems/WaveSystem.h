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
    float grace{1.5f};  // downtime after each spawn batch
    float enemySize{18.0f};
    float enemyHitbox{18.0f};
};

class WaveSystem {
public:
    explicit WaveSystem(std::mt19937& rng, WaveSettings settings);

    // Returns true if a new wave was triggered this frame.
    bool update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero, int& wave);
    void resetTimer() { timer_ = 0.0; }
    void setTimer(double t) { timer_ = t; }
    double timeToNext() const { return timer_; }
    void setBossConfig(int bossWave, float hpMul, float speedMul) {
        bossWave_ = bossWave;
        bossHpMul_ = hpMul;
        bossSpeedMul_ = speedMul;
    }

private:
    std::mt19937& rng_;
    WaveSettings settings_;
    double timer_{0.0};
    int bossWave_{20};
    float bossHpMul_{12.0f};
    float bossSpeedMul_{0.8f};
};

}  // namespace Game
