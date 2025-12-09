// Spawns enemies at intervals around the playfield.
#pragma once

#include <random>
#include <vector>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../EnemyDefinition.h"
#include "../../engine/gameplay/Combat.h"

namespace Game {

struct WaveSettings {
    float interval{3.0f};
    int batchSize{2};
    float enemyHp{50.0f};
    float enemyShields{0.0f};
    float enemyHealthArmor{0.0f};
    float enemyShieldArmor{0.0f};
    float enemyShieldRegen{0.0f};
    float enemyRegenDelay{0.0f};
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
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void setUpgrades(const Engine::Gameplay::UpgradeState& upgrades) { upgrades_ = upgrades; }
    void setBaseArmor(const Engine::Gameplay::BaseStats& base) { baseStats_ = base; }

private:
    const EnemyDefinition* pickEnemyDef();

    std::mt19937& rng_;
    WaveSettings settings_;
    double timer_{0.0};
    int bossWave_{20};
    float bossHpMul_{12.0f};
    float bossSpeedMul_{0.8f};
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
    Engine::Gameplay::UpgradeState upgrades_{};
    Engine::Gameplay::BaseStats baseStats_{};
};

}  // namespace Game
