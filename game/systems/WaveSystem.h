// Spawns enemies at intervals around the playfield.
#pragma once

#include <algorithm>
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
    void setRound(int round) { currentRound_ = round; }
    void setSpawnBatchInterval(int interval) { spawnBatchInterval_ = std::max(1, interval); }
    void setBossConfig(int firstWave, int interval, float hpMul, float speedMul, float maxSize) {
        bossFirstWave_ = std::max(1, firstWave);
        bossInterval_ = std::max(1, interval);
        bossHpMul_ = hpMul;
        bossSpeedMul_ = speedMul;
        bossMaxSize_ = maxSize;
        nextBossWave_ = bossFirstWave_;
    }
    void setPlayerCount(int players) { playerCount_ = std::max(1, players); }
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void setUpgrades(const Engine::Gameplay::UpgradeState& upgrades) { upgrades_ = upgrades; }
    void setBaseArmor(const Engine::Gameplay::BaseStats& base) { baseStats_ = base; }

private:
    const EnemyDefinition* pickEnemyDef();

    std::mt19937& rng_;
    WaveSettings settings_;
    double timer_{0.0};
    int currentRound_{1};
    int spawnBatchInterval_{5};
    int roundBatchApplied_{0};
    int bossFirstWave_{20};
    int bossInterval_{20};
    int nextBossWave_{20};
    float bossMaxSize_{110.0f};
    float bossHpMul_{12.0f};
    float bossSpeedMul_{0.8f};
    int playerCount_{1};
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
    Engine::Gameplay::UpgradeState upgrades_{};
    Engine::Gameplay::BaseStats baseStats_{};
};

}  // namespace Game
