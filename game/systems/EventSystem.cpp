#include "EventSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <vector>

#include "../components/EventActive.h"
#include "../components/EventMarker.h"
#include "../components/Spawner.h"
#include "../components/EscortObjective.h"
#include "../components/EscortTarget.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/core/Logger.h"
#include "../../engine/math/Vec2.h"
#include "../components/EnemyAttributes.h"
#include "../components/BountyTag.h"
#include "../components/Facing.h"
#include "../components/LookDirection.h"
#include "../components/EscortPreMove.h"
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

constexpr float kMoveEps = 0.01f;

LookDir4 dirFromVec(const Engine::Vec2& v, LookDir4 fallback) {
    const float ax = std::abs(v.x);
    const float ay = std::abs(v.y);
    if (ax < 0.0001f && ay < 0.0001f) return fallback;
    if (ax >= ay) {
        return v.x >= 0.0f ? LookDir4::Right : LookDir4::Left;
    }
    return v.y >= 0.0f ? LookDir4::Front : LookDir4::Back;
}

int rowIdle(LookDir4 d) {
    switch (d) {
        case LookDir4::Left: return 0;
        case LookDir4::Right: return 1;
        case LookDir4::Front: return 2;
        case LookDir4::Back: return 3;
    }
    return 2;
}

int rowWalk(LookDir4 d) {
    switch (d) {
        case LookDir4::Left: return 4;
        case LookDir4::Right: return 5;
        case LookDir4::Front: return 6;
        case LookDir4::Back: return 7;
    }
    return 6;
}
}  // namespace

Engine::TexturePtr EventSystem::loadEscortTexture() {
    if (escortTexture_) return escortTexture_;
    if (!textureManager_) return {};
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    if (const char* home = std::getenv("HOME")) {
        candidates.emplace_back(fs::path(home) / "assets" / "Sprites" / "Characters" / "Escort.png");
    }
    candidates.emplace_back(fs::path("assets") / "Sprites" / "Characters" / "Escort.png");
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            auto tex = textureManager_->getOrLoadBMP(p.string());
            if (tex) {
                escortTexture_ = *tex;
                return escortTexture_;
            }
        }
    }
    Engine::logWarn("EventSystem: escort texture not found; using fallback color block.");
    return {};
}

Engine::TexturePtr EventSystem::loadSpireTexture() {
    if (spireTexture_) return spireTexture_;
    if (!textureManager_) return {};
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    if (const char* home = std::getenv("HOME")) {
        candidates.emplace_back(fs::path(home) / "assets" / "Sprites" / "Buildings" / "spawner.png");
    }
    candidates.emplace_back(fs::path("assets") / "Sprites" / "Buildings" / "spawner.png");
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            auto tex = textureManager_->getOrLoadBMP(p.string());
            if (tex) {
                spireTexture_ = *tex;
                return spireTexture_;
            }
        }
    }
    Engine::logWarn("EventSystem: spawner texture not found; using fallback color block.");
    return {};
}

void EventSystem::spawnEscort(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int wave) {
    // Spawn near hero and task the NPC to travel 500–1200 units.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> spawnAng(0.0f, 6.28318f);
    std::uniform_real_distribution<float> spawnRad(90.0f, 160.0f);
    std::uniform_real_distribution<float> travelDist(500.0f, 1200.0f);
    std::uniform_real_distribution<float> travelAng(0.0f, 6.28318f);

    float angSpawn = spawnAng(rng);
    float radSpawn = spawnRad(rng);
    Engine::Vec2 spawnPos{heroPos.x + std::cos(angSpawn) * radSpawn, heroPos.y + std::sin(angSpawn) * radSpawn};

    float distTarget = travelDist(rng);
    float angTravel = travelAng(rng);
    Engine::Vec2 dest{spawnPos.x + std::cos(angTravel) * distTarget, spawnPos.y + std::sin(angTravel) * distTarget};

    Engine::TexturePtr tex = loadEscortTexture();
    float size = 26.0f * 1.5f;  // 50% larger render scale
    auto escortEnt = registry.create();
    registry.emplace<Engine::ECS::Transform>(escortEnt, spawnPos);
    registry.emplace<Engine::ECS::Velocity>(escortEnt, Engine::Vec2{0.0f, 0.0f});
    registry.emplace<Game::Facing>(escortEnt, Game::Facing{1});
    registry.emplace<Game::LookDirection>(escortEnt, Game::LookDirection{Game::LookDir4::Front});
    registry.emplace<Engine::ECS::Renderable>(escortEnt,
        Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{180, 230, 255, 255}, tex});
    registry.emplace<Engine::ECS::AABB>(escortEnt, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
    // Scale escort durability with wave so it keeps pace with enemy power curve.
    int waveIdx = std::max(1, wave);
    float hpMul = std::pow(1.08f, static_cast<float>(waveIdx - 1));   // mirrors enemy hp growth
    float armorMul = 1.0f + 0.015f * static_cast<float>(waveIdx - 1); // steady armor gain
    float regenMul = 1.0f + 0.01f * static_cast<float>(waveIdx - 1);  // modest regen scaling
    float baseHp = 1200.0f * hpMul;
    float baseShields = 600.0f * hpMul;
    float baseHealthArmor = 3.5f * armorMul;
    float baseShieldArmor = 2.5f * armorMul;
    float baseShieldRegen = std::min(40.0f, 10.0f * regenMul);
    registry.emplace<Engine::ECS::Health>(escortEnt, Engine::ECS::Health{baseHp, baseHp});
    if (auto* hp = registry.get<Engine::ECS::Health>(escortEnt)) {
        hp->maxShields = baseShields;
        hp->currentShields = baseShields;
        hp->healthArmor = baseHealthArmor;
        hp->shieldArmor = baseShieldArmor;
        hp->regenDelay = 1.6f;
        hp->shieldRegenRate = baseShieldRegen;
    }
    registry.emplace<Game::EscortTarget>(escortEnt, Game::EscortTarget{nextEventId_, dest, spawnPos, 65.0f});
    // Escort is protected and non-aggro until countdown ends.
    registry.emplace<Game::EscortPreMove>(escortEnt, Game::EscortPreMove{});
    if (tex) {
        const int frameW = 16;
        const int frameH = 16;
        const int frames = std::max(1, tex->width() / frameW);
        registry.emplace<Engine::ECS::SpriteAnimation>(escortEnt, Engine::ECS::SpriteAnimation{frameW, frameH, frames, 0.12f});
    }

    // Controller keeps timer/progress and shares a transform for HUD arrows.
    auto controller = registry.create();
    registry.emplace<Engine::ECS::Transform>(controller, spawnPos);
    Game::EventActive ev{nextEventId_, Game::EventType::Escort, 55.0f, 55.0f, false, false, false, true, 0};
    ev.countdown = 4.0f;
    registry.emplace<Game::EventActive>(controller, ev);
    registry.emplace<Game::EscortObjective>(controller,
        Game::EscortObjective{escortEnt, dest, spawnPos, distTarget, 16.0f});
    eventTypes_[ev.id] = ev.type;
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
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{360.0f, 360.0f, 140.0f, 140.0f});
        if (auto* hp = registry.get<Engine::ECS::Health>(e)) {
            hp->healthArmor = 2.0f;
            hp->shieldArmor = 1.5f;
            hp->regenDelay = 1.5f;
            hp->shieldRegenRate = 8.0f;
        }
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
    Game::EventActive ev{};
    ev.id = eventId;
    ev.type = Game::EventType::Bounty;
    ev.timer = ev.maxTimer = 35.0f;
    ev.requiredKills = count;
    ev.countdown = countdownDefault_;
    ev.countdown = countdownDefault_;
    registry.emplace<Game::EventActive>(controller, ev);
    eventTypes_[ev.id] = ev.type;
}

void EventSystem::spawnSpireHunt(Engine::ECS::Registry& registry, const Engine::Vec2& heroPos, int eventId) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radiusDist(340.0f, 480.0f);
    const int count = 3;
    Engine::TexturePtr texSpire = loadSpireTexture();
    std::uniform_real_distribution<float> lifetimeDist(120.0f, 240.0f);
    for (int i = 0; i < count; ++i) {
        float ang = angleDist(rng);
        float rad = radiusDist(rng);
        Engine::Vec2 pos{heroPos.x + std::cos(ang) * rad, heroPos.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Renderable>(e,
            Engine::ECS::Renderable{Engine::Vec2{26.0f, 26.0f}, Engine::Color{255, 110, 170, 255}, texSpire});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{13.0f, 13.0f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{240.0f, 240.0f});
        registry.emplace<Game::EventMarker>(e, Game::EventMarker{eventId});
        Game::Spawner sp{};
        sp.spawnTimer = 1.0f;
        sp.interval = 3.0f;
        sp.eventId = eventId;
        sp.lifetime = lifetimeDist(rng);
        registry.emplace<Game::Spawner>(e, sp);
    }
    auto controller = registry.create();
    Game::EventActive ev{};
    ev.id = eventId;
    ev.type = Game::EventType::SpawnerHunt;
    ev.timer = ev.maxTimer = 45.0f;
    ev.requiredKills = count;
    ev.countdown = countdownDefault_;
    registry.emplace<Game::EventActive>(controller, ev);
    eventTypes_[ev.id] = ev.type;
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
            spawnEscort(registry, heroPos, wave);
            lastEventLabel_ = "Escort Duty";
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
    pendingCountdown_ = 0.0f;
    pendingCountdownLabel_.clear();
    registry.view<Game::EventActive>([&](Engine::ECS::Entity e, Game::EventActive& ev) {
        if (ev.countdown > 0.0f) {
            ev.countdown -= static_cast<float>(step.deltaSeconds);
            // Track earliest countdown to display.
            if (pendingCountdown_ <= 0.0f || ev.countdown < pendingCountdown_) {
                pendingCountdown_ = ev.countdown;
                switch (ev.type) {
                    case Game::EventType::Escort: pendingCountdownLabel_ = "Escort Duty"; break;
                    case Game::EventType::Bounty: pendingCountdownLabel_ = "Bounty Hunt"; break;
                    case Game::EventType::SpawnerHunt: pendingCountdownLabel_ = "Assassinate Spawners"; break;
                }
            }
            return;
        }
        ev.timer -= static_cast<float>(step.deltaSeconds);
        if (ev.type == Game::EventType::Escort) {
            auto* escortObj = registry.get<Game::EscortObjective>(e);
            Engine::ECS::Entity escortEnt = escortObj ? escortObj->escort : Engine::ECS::kInvalidEntity;
            auto* escortHp = (escortEnt != Engine::ECS::kInvalidEntity) ? registry.get<Engine::ECS::Health>(escortEnt) : nullptr;
            auto* escortTf = (escortEnt != Engine::ECS::kInvalidEntity) ? registry.get<Engine::ECS::Transform>(escortEnt) : nullptr;
            auto* escortVel = (escortEnt != Engine::ECS::kInvalidEntity) ? registry.get<Engine::ECS::Velocity>(escortEnt) : nullptr;
            auto* escortTag = (escortEnt != Engine::ECS::kInvalidEntity) ? registry.get<Game::EscortTarget>(escortEnt) : nullptr;

            if (auto* ctrlTf = registry.get<Engine::ECS::Transform>(e)) {
                if (escortTf) ctrlTf->position = escortTf->position;
            }

            bool escortDead = !escortObj || escortEnt == Engine::ECS::kInvalidEntity || !escortHp || !escortHp->alive() || !escortTf;
            if (escortDead) {
                ev.failed = true;
                toDestroy.push_back(e);
                if (escortEnt != Engine::ECS::kInvalidEntity) {
                    toDestroy.push_back(escortEnt);
                }
                lastFail_ = true;
                completedEventIds.push_back(ev.id);
            } else {
                Engine::Vec2 delta{escortObj->destination.x - escortTf->position.x, escortObj->destination.y - escortTf->position.y};
                float dist2 = delta.x * delta.x + delta.y * delta.y;
                if (dist2 <= escortObj->arrivalRadius * escortObj->arrivalRadius) {
                    ev.success = true;
                } else if (escortVel && ev.countdown <= 0.0f) {
                    float invLen = 1.0f / std::sqrt(std::max(dist2, 0.0001f));
                    float speed = escortTag ? escortTag->speed : 95.0f;
                    escortVel->value = {delta.x * invLen * speed, delta.y * invLen * speed};
                } else if (escortVel && ev.countdown > 0.0f) {
                    escortVel->value = {0.0f, 0.0f};
                }

                // Drive escort animation row (idle vs walk) to match movement.
                if (escortVel) {
                    Engine::Vec2 v = escortVel->value;
                    auto* anim = registry.get<Engine::ECS::SpriteAnimation>(escortEnt);
                    auto* look = registry.get<Game::LookDirection>(escortEnt);
                    LookDir4 dir = look ? look->dir : LookDir4::Front;
                    if (std::abs(v.x) > kMoveEps || std::abs(v.y) > kMoveEps) {
                        dir = dirFromVec(v, dir);
                        if (look) look->dir = dir;
                        if (anim && anim->row != rowWalk(dir)) {
                            anim->row = rowWalk(dir);
                            anim->currentFrame = 0;
                            anim->accumulator = 0.0f;
                        }
                    } else {
                        if (anim && anim->row != rowIdle(dir)) {
                            anim->row = rowIdle(dir);
                            anim->currentFrame = 0;
                            anim->accumulator = 0.0f;
                        }
                    }
                }

                if (ev.success) {
                    if (escortVel) escortVel->value = {0.0f, 0.0f};
                    if (!ev.rewardGranted) {
                        gold += salvageReward;
                        ev.rewardGranted = true;
                    }
                    toDestroy.push_back(e);
                    toDestroy.push_back(escortEnt);
                    lastSuccess_ = true;
                    completedEventIds.push_back(ev.id);
                } else if (ev.timer <= 0.0f && inCombat) {
                    ev.failed = true;
                    if (escortVel) escortVel->value = {0.0f, 0.0f};
                    toDestroy.push_back(e);
                    toDestroy.push_back(escortEnt);
                    lastFail_ = true;
                    completedEventIds.push_back(ev.id);
                }
                // Once countdown ends, allow damage/aggro.
                if (ev.countdown <= 0.0f && registry.has<Game::EscortPreMove>(escortEnt)) {
                    registry.remove<Game::EscortPreMove>(escortEnt);
                }
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

bool EventSystem::isSpawnerEvent(int eventId) const {
    auto it = eventTypes_.find(eventId);
    return it != eventTypes_.end() && it->second == Game::EventType::SpawnerHunt;
}

bool EventSystem::getCountdownText(std::string& labelOut, float& secondsOut) const {
    if (pendingCountdown_ > 0.0f && !pendingCountdownLabel_.empty()) {
        labelOut = pendingCountdownLabel_;
        secondsOut = pendingCountdown_;
        return true;
    }
    return false;
}

void EventSystem::tickSpawners(Engine::ECS::Registry& registry, const Engine::TimeStep& step) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
    std::uniform_real_distribution<float> rad(90.0f, 140.0f);
    std::vector<Engine::ECS::Entity> expired;
    registry.view<Game::Spawner, Engine::ECS::Transform>([&](Engine::ECS::Entity e, Game::Spawner& sp, Engine::ECS::Transform& tf) {
        sp.spawnTimer -= static_cast<float>(step.deltaSeconds);
        if (sp.lifetime > 0.0f) {
            sp.lifetime -= static_cast<float>(step.deltaSeconds);
            if (sp.lifetime <= 0.0f) {
                expired.push_back(e);
                return;
            }
        }
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
    if (!expired.empty()) {
        for (auto e : expired) {
            if (auto* sp = registry.get<Game::Spawner>(e)) {
                notifyTargetKilled(registry, sp->eventId);
            }
            registry.destroy(e);
        }
    }
}

}  // namespace Game
