#include "EventSystem.h"

#include <algorithm>
#include <random>
#include <vector>

#include "../components/EventActive.h"
#include "../components/EventMarker.h"
#include "../components/Spawner.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/math/Vec2.h"
#include "../components/EnemyAttributes.h"
#include "../components/BountyTag.h"
#include "../components/Facing.h"
#include "../components/LookDirection.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../EnemyDefinition.h"

namespace Game {

namespace {
const EnemyDefinition* pickEventEnemyDef(const std::vector<EnemyDefinition>* defs) {
    if (!defs || defs->empty()) return nullptr;
    std::vector<const EnemyDefinition*> textured;
    textured.reserve(defs->size());
    for (const auto& d : *defs) {
        if (d.texture) textured.push_back(&d);
    }
    if (textured.empty()) return &(*defs)[0];
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> pick(0, textured.size() - 1);
    return textured[pick(rng)];
}
}  // namespace

void EventSystem::spawnSalvage(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos) {
    // Spawn toward a random edge from hero to encourage movement.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    float ang = angleDist(rng);
    float dist = 420.0f;
    Engine::Vec2 pos{heroPos.x + std::cos(ang) * dist, heroPos.y + std::sin(ang) * dist};
    auto e = registry.create();
    registry.emplace<Engine::ECS::Transform>(e, pos);
    registry.emplace<Engine::ECS::Renderable>(e,
        Engine::ECS::Renderable{Engine::Vec2{28.0f, 28.0f}, Engine::Color{120, 200, 255, 255}});
    registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{14.0f, 14.0f}});
    registry.emplace<Game::EventActive>(e, Game::EventActive{nextEventId_, Game::EventType::Salvage, 30.0f, 30.0f,
                                                             false, false, false, true, 0});
    registry.emplace<Engine::ECS::PickupTag>(e, Engine::ECS::PickupTag{});
}

void EventSystem::spawnBounty(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radiusDist(260.0f, 320.0f);
    const int count = 3;
    for (int i = 0; i < count; ++i) {
        float ang = angleDist(rng);
        float rad = radiusDist(rng);
        Engine::Vec2 pos{heroPos.x + std::cos(ang) * rad, heroPos.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Game::Facing>(e, Game::Facing{1});
        registry.emplace<Game::LookDirection>(e, Game::LookDirection{});
        const EnemyDefinition* def = pickEventEnemyDef(enemyDefs_);
        Engine::TexturePtr tex = def ? def->texture : Engine::TexturePtr{};
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{22.0f, 22.0f}, Engine::Color{255, 160, 110, 255}, tex});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{11.0f, 11.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{160.0f, 160.0f});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{110.0f});
        registry.emplace<Game::BountyTag>(e, Game::BountyTag{});
        registry.emplace<Game::EventMarker>(e, Game::EventMarker{eventId});
        if (tex) {
            const int fw = def ? def->frameWidth : 16;
            const int fh = def ? def->frameHeight : 16;
            const float fd = def ? def->frameDuration : 0.14f;
            registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{fw, fh, 4, fd});
        }
    }
    // Controller entity tracks timer and kill requirement.
    auto controller = registry.create();
    registry.emplace<Game::EventActive>(controller,
        Game::EventActive{eventId, Game::EventType::Bounty, 35.0f, 35.0f, false, false, false, false, count});
}

void EventSystem::spawnSpireHunt(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radiusDist(340.0f, 480.0f);
    const int count = 3;
    for (int i = 0; i < count; ++i) {
        float ang = angleDist(rng);
        float rad = radiusDist(rng);
        Engine::Vec2 pos{heroPos.x + std::cos(ang) * rad, heroPos.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{26.0f, 26.0f}, Engine::Color{255, 110, 170, 255}});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{13.0f, 13.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{240.0f, 240.0f});
        registry.emplace<Game::EventMarker>(e, Game::EventMarker{eventId});
        registry.emplace<Game::Spawner>(e, Game::Spawner{1.0f, 3.0f, eventId});
    }
    auto controller = registry.create();
    registry.emplace<Game::EventActive>(controller,
        Game::EventActive{eventId, Game::EventType::SpawnerHunt, 45.0f, 45.0f, false, false, false, false, count});
}

void EventSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, int wave, int& gold,
                         bool inCombat, const Engine::Vec2& heroPos, int salvageReward) {
    lastSuccess_ = false;
    lastFail_ = false;
    std::vector<int> completedEventIds;
    // Trigger every 5th wave once.
    if (wave > 0 && wave % 5 == 0 && wave != lastEventWave_ && !eventActive_) {
        int eventId = nextEventId_++;
        lastEventWave_ = wave;
        int choice = eventCycle_ % 3;
        if (choice == 0) {
            spawnSalvage(registry, heroPos);
            lastEventLabel_ = "Salvage Run";
        } else if (choice == 1) {
            spawnBounty(registry, heroPos, eventId);
            lastEventLabel_ = "Bounty Hunt";
        } else {
            spawnSpireHunt(registry, heroPos, eventId);
            lastEventLabel_ = "Assassinate Spawners";
        }
        eventCycle_++;
    }

    // Spawner ticking (extra adds pressure).
    tickSpawners(registry, step);

    // Tick active events.
    std::vector<Engine::ECS::Entity> toDestroy;
    registry.view<Game::EventActive>([&](Engine::ECS::Entity e, Game::EventActive& ev) {
        ev.timer -= static_cast<float>(step.deltaSeconds);
        if (ev.type == Game::EventType::Salvage) {
            if (auto* rend = registry.get<Engine::ECS::Renderable>(e)) {
                if (ev.timer <= 0.0f && !ev.failed) {
                    ev.success = true;  // timer elapsed (intermission) counts as success
                }
                if (ev.success) {
                    rend->color = Engine::Color{120, 255, 160, 255};
                    if (!ev.rewardGranted) {
                        gold += salvageReward;
                        ev.rewardGranted = true;
                    }
                    toDestroy.push_back(e);
                    lastSuccess_ = true;
                    completedEventIds.push_back(ev.id);
                } else if (ev.timer <= 0.0f && inCombat) {
                    ev.failed = true;
                    rend->color = Engine::Color{255, 80, 80, 255};
                    toDestroy.push_back(e);
                    lastFail_ = true;
                    completedEventIds.push_back(ev.id);
                } else {
                    // Pulse color to draw attention.
                    float t = std::clamp(ev.timer / ev.maxTimer, 0.0f, 1.0f);
                    rend->color = Engine::Color{static_cast<uint8_t>(120 + 80 * (1 - t)), 200, 255, 255};
                }
            } else {
                // If salvage object was picked up/destroyed early, treat as success.
                ev.success = true;
                toDestroy.push_back(e);
                lastSuccess_ = true;
            }
        } else if (ev.type == Game::EventType::Bounty) {
            if (ev.requiredKills <= 0) {
                ev.success = true;
                toDestroy.push_back(e);
                lastSuccess_ = true;
                completedEventIds.push_back(ev.id);
            } else if (ev.timer <= 0.0f && inCombat) {
                ev.failed = true;
                toDestroy.push_back(e);
                lastFail_ = true;
                completedEventIds.push_back(ev.id);
            }
        } else {  // Spawner hunt
            if (ev.requiredKills <= 0) {
                ev.success = true;
                toDestroy.push_back(e);
                lastSuccess_ = true;
                completedEventIds.push_back(ev.id);
            } else if (ev.timer <= 0.0f && inCombat) {
                ev.failed = true;
                toDestroy.push_back(e);
                lastFail_ = true;
                completedEventIds.push_back(ev.id);
            }
        }
    });

    for (auto e : toDestroy) {
        registry.destroy(e);
    }
    // Clean up spawners related to completed/failed events to avoid endless spawns.
    if (!completedEventIds.empty()) {
        std::vector<Engine::ECS::Entity> spawnersToDestroy;
        registry.view<Game::Spawner>([&](Engine::ECS::Entity e, Game::Spawner& sp) {
            for (int id : completedEventIds) {
                if (sp.eventId == id) {
                    spawnersToDestroy.push_back(e);
                    break;
                }
            }
        });
        for (auto e : spawnersToDestroy) {
            registry.destroy(e);
        }
    }
    // Update active flag (true if any EventActive remain).
    eventActive_ = false;
    registry.view<Game::EventActive>([&](Engine::ECS::Entity, Game::EventActive&) { eventActive_ = true; });
}

void EventSystem::notifyTargetKilled(Engine::ECS::Registry& registry, int eventId) {
    registry.view<Game::EventActive>([&](Engine::ECS::Entity, Game::EventActive& ev) {
        if (ev.id == eventId && ev.requiredKills > 0) {
            ev.requiredKills -= 1;
        }
    });
}

void EventSystem::tickSpawners(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
    std::uniform_real_distribution<float> rad(90.0f, 140.0f);
    registry.view<Game::Spawner, Engine::ECS::Transform>([&](Engine::ECS::Entity, Game::Spawner& sp, Engine::ECS::Transform& tf) {
        sp.spawnTimer -= static_cast<float>(step.deltaSeconds);
        if (sp.spawnTimer <= 0.0f) {
            sp.spawnTimer = sp.interval;
            float a = ang(rng);
            float r = rad(rng);
            Engine::Vec2 pos{tf.position.x + std::cos(a) * r, tf.position.y + std::sin(a) * r};
            auto m = registry.create();
            registry.emplace<Engine::ECS::Transform>(m, pos);
            registry.emplace<Engine::ECS::Velocity>(m, Engine::Vec2{0.0f, 0.0f});
            registry.emplace<Game::Facing>(m, Game::Facing{1});
            registry.emplace<Game::LookDirection>(m, Game::LookDirection{});
            const EnemyDefinition* def = pickEventEnemyDef(enemyDefs_);
            Engine::TexturePtr tex = def ? def->texture : Engine::TexturePtr{};
            registry.emplace<Engine::ECS::Renderable>(m,
                Engine::ECS::Renderable{Engine::Vec2{16.0f, 16.0f}, Engine::Color{255, 150, 180, 255}, tex});
            registry.emplace<Engine::ECS::AABB>(m, Engine::ECS::AABB{Engine::Vec2{8.0f, 8.0f}});
            registry.emplace<Engine::ECS::Health>(m, Engine::ECS::Health{70.0f, 70.0f});
            registry.emplace<Engine::ECS::EnemyTag>(m, Engine::ECS::EnemyTag{});
            registry.emplace<Game::EnemyAttributes>(m, Game::EnemyAttributes{140.0f});
            registry.emplace<Game::EventMarker>(m, Game::EventMarker{sp.eventId});
            if (tex) {
                const int fw = def ? def->frameWidth : 16;
                const int fh = def ? def->frameHeight : 16;
                const float fd = def ? def->frameDuration : 0.14f;
                registry.emplace<Engine::ECS::SpriteAnimation>(m, Engine::ECS::SpriteAnimation{fw, fh, 4, fd});
            }
        }
    });
}

}  // namespace Game
