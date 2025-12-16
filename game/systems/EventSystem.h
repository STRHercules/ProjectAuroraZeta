// Spawns and tracks mini-events every 5 waves (escort, bounty, spawner hunts).
#pragma once

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/math/Vec2.h"
#include "../components/EventActive.h"
#include "../components/EscortObjective.h"
#include "../components/EscortTarget.h"
#include "../../engine/assets/Texture.h"
#include "../../engine/assets/TextureManager.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace Game {

struct EnemyDefinition;

class EventSystem {
public:
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void setTextureManager(Engine::TextureManager* tm) { textureManager_ = tm; }
    void update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, int wave, int& gold,
                bool inCombat, const Engine::Vec2& heroPos, int salvageReward);
    void notifyTargetKilled(Engine::ECS::Registry& registry, int eventId);
    bool isSpawnerEvent(int eventId) const;
    bool getCountdownText(std::string& labelOut, float& secondsOut) const;
    bool lastSuccess() const { return lastSuccess_; }
    bool lastFail() const { return lastFail_; }
    std::string lastEventLabel() const { return lastEventLabel_; }
    void clearOutcome() { lastSuccess_ = lastFail_ = false; }

private:
    void spawnEscort(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int wave);
    void spawnBounty(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId);
    void spawnSpireHunt(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId);
    void tickSpawners(Engine::ECS::Registry& registry, const Engine::TimeStep& step);
    Engine::TexturePtr loadEscortTexture();
    Engine::TexturePtr loadSpireTexture();
    void setCountdown(Engine::ECS::Registry& registry, float seconds);
    float countdownDefault_{3.0f};
    bool eventActive_{false};
    bool lastSuccess_{false};
    bool lastFail_{false};
    std::string lastEventLabel_;
    int lastEventWave_{0};
    int nextEventId_{1};
    int eventCycle_{0};
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
    Engine::TextureManager* textureManager_{nullptr};
    Engine::TexturePtr escortTexture_;
    Engine::TexturePtr spireTexture_;
    std::unordered_map<int, Game::EventType> eventTypes_;
    mutable float pendingCountdown_{0.0f};
    mutable std::string pendingCountdownLabel_;
};

}  // namespace Game
