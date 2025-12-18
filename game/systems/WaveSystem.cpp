#include "WaveSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/RPGStats.h"
#include "../../engine/ecs/components/Status.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/math/Vec2.h"
#include "../../engine/gameplay/Combat.h"
#include "../components/EnemyAttributes.h"
#include "../components/EnemyOnHit.h"
#include "../components/EnemyRevive.h"
#include "../components/EnemyType.h"
#include "../components/FlameSkullBehavior.h"
#include "../components/PickupBob.h"
#include "../components/BountyTag.h"
#include "../components/Facing.h"
#include "../components/LookDirection.h"
#include "../components/SlimeBehavior.h"

namespace Game {

WaveSystem::WaveSystem(std::mt19937& rng, WaveSettings settings) : rng_(rng), settings_(settings) {}

const EnemyDefinition* WaveSystem::pickEnemyDef() {
    if (!enemyDefs_ || enemyDefs_->empty()) return nullptr;
    std::vector<const EnemyDefinition*> textured;
    textured.reserve(enemyDefs_->size());
    for (const auto& def : *enemyDefs_) {
        if (def.texture) textured.push_back(&def);
    }
    if (!textured.empty()) {
        std::uniform_int_distribution<std::size_t> dist(0, textured.size() - 1);
        return textured[dist(rng_)];
    }
    std::uniform_int_distribution<std::size_t> dist(0, enemyDefs_->size() - 1);
    return &((*enemyDefs_)[dist(rng_)]);
}

bool WaveSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero,
                        int& wave) {
    timer_ -= step.deltaSeconds;
    if (timer_ > 0.0) return false;
    // Round-based spawn scaling: every N rounds add 1 batch size (once per threshold).
    int level = std::max(0, currentRound_ / spawnBatchInterval_);
    if (level > roundBatchApplied_) {
        int diff = level - roundBatchApplied_;
        settings_.batchSize = std::min(16, settings_.batchSize + diff);
        roundBatchApplied_ = level;
    }
    timer_ += settings_.interval + settings_.grace;
    wave++;
    // Simple scaling: every wave increase HP and batch.
    settings_.enemyHp *= 1.08f;
    settings_.enemyShields *= 1.08f;
    settings_.enemySpeed *= 1.01f;
    if (wave % 2 == 0 && settings_.batchSize < 12) settings_.batchSize += 1;

    // Spawn enemies in a ring around hero.
    auto* heroTf = registry.get<Engine::ECS::Transform>(hero);
    Engine::Vec2 center = heroTf ? heroTf->position : Engine::Vec2{0.0f, 0.0f};

    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> radiusDist(240.0f, 320.0f);
    std::uniform_real_distribution<float> packAng(0.0f, 6.28318f);
    std::uniform_real_distribution<float> packRad(10.0f, 28.0f);
    std::uniform_int_distribution<int> packFallback(1, 1);
    auto pickVariantTexture = [&](const EnemyDefinition* def) -> Engine::TexturePtr {
        if (!def) return {};
        if (!def->variantTextures.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, def->variantTextures.size() - 1);
            return def->variantTextures[pick(rng_)];
        }
        return def->texture;
    };

    int batchCount = std::clamp(settings_.batchSize * std::max(1, playerCount_), 1, 128);
    for (int i = 0; i < batchCount; ++i) {
        float ang = angleDist(rng_);
        float rad = radiusDist(rng_);
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};

        const EnemyDefinition* def = pickEnemyDef();
        int groupMin = def ? std::max(1, def->spawnGroupMin) : 1;
        int groupMax = def ? std::max(groupMin, def->spawnGroupMax) : 1;
        std::uniform_int_distribution<int> packCount(groupMin, groupMax);
        const int groupCount = (groupMax > 1) ? packCount(rng_) : packFallback(rng_);

        auto spawnOne = [&](const Engine::Vec2& spawnPos) {
            auto e = registry.create();
            registry.emplace<Engine::ECS::Transform>(e, spawnPos);
            registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
            registry.emplace<Game::Facing>(e, Game::Facing{1});
            registry.emplace<Game::LookDirection>(e, Game::LookDirection{});

            float hpVal = settings_.enemyHp;
            float shieldsVal = settings_.enemyShields;
            float speedVal = settings_.enemySpeed;
            float sizeMul = 1.0f;
            float dmgMul = 1.0f;
            Engine::TexturePtr tex{};
            int defIndex = -1;
            if (def) {
                hpVal *= def->hpMultiplier;
                shieldsVal *= def->shieldMultiplier;
                speedVal *= def->speedMultiplier;
                sizeMul = def->sizeMultiplier;
                dmgMul = def->damageMultiplier;
                tex = pickVariantTexture(def);
                if (enemyDefs_ && !enemyDefs_->empty()) {
                    defIndex = static_cast<int>(def - enemyDefs_->data());
                }
            }
            if (speedVal < 5.0f) speedVal = settings_.enemySpeed * 0.6f;

            // If no texture available, fall back to any textured enemy or leave empty.
            if (!tex && enemyDefs_) {
                for (const auto& d : *enemyDefs_) {
                    if (d.texture) { tex = d.texture; break; }
                }
            }
            float size = settings_.enemySize * sizeMul;
            registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                                  Engine::Color{255, 255, 255, 255},
                                                                                  tex});
            const float hb = settings_.enemyHitbox * 0.5f * sizeMul;
            registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
            registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
            if (auto* hp = registry.get<Engine::ECS::Health>(e)) {
                Engine::Gameplay::applyUpgradesToUnit(*hp, baseStats_, upgrades_, false);
                hp->tags = {Engine::Gameplay::Tag::Biological};
                hp->maxShields = std::max(0.0f, shieldsVal);
                hp->currentShields = hp->maxShields;
                hp->healthArmor = settings_.enemyHealthArmor;
                hp->shieldArmor = settings_.enemyShieldArmor;
                hp->shieldRegenRate = settings_.enemyShieldRegen * (def ? def->shieldRegenMultiplier : 1.0f);
                hp->regenDelay = settings_.enemyRegenDelay * (def ? def->regenDelayMultiplier : 1.0f);
                if (def && def->healthRegenFracPerSecond > 0.0f) {
                    hp->healthRegenRate = std::max(0.0f, hp->maxHealth * def->healthRegenFracPerSecond);
                }
            }
            registry.emplace<Engine::ECS::RPGStats>(e, Engine::ECS::RPGStats{});
            if (auto* rpg = registry.get<Engine::ECS::RPGStats>(e)) {
                rpg->baseFromHealth = true;
                // Seed a minimal RPG snapshot so enemies participate in shared combat rules (ACC/EVA/crit scaling).
                // Keep this conservative and wave-scaled so legacy tuning doesn't explode.
                const int lvl = std::max(1, wave + 1);
                rpg->attributes.STR = lvl / 12;
                rpg->attributes.DEX = lvl / 14;
                rpg->attributes.INT = lvl / 18;
                rpg->attributes.END = lvl / 12;
                rpg->attributes.LCK = lvl / 20;
                rpg->base.baseAttackPower = std::max(1.0f, settings_.contactDamage * dmgMul);
                rpg->base.baseSpellPower = rpg->base.baseAttackPower;
                rpg->base.baseAccuracy = 1.0f + static_cast<float>(lvl) * 0.05f;
                rpg->base.baseCritChance = 0.02f;
                rpg->base.baseEvasion = static_cast<float>(lvl) * 0.08f;
                rpg->dirty = true;
            }
            registry.emplace<Engine::ECS::Status>(e, Engine::ECS::Status{});
            registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
            registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
            if (defIndex >= 0) {
                registry.emplace<Game::EnemyType>(e, Game::EnemyType{defIndex});
            }
            if (def && (def->onHitBleedChance > 0.0f || def->onHitPoisonChance > 0.0f || def->onHitFearChance > 0.0f || def->damageMultiplier != 1.0f)) {
                registry.emplace<Game::EnemyOnHit>(e, Game::EnemyOnHit{dmgMul,
                                                                      def->onHitBleedChance, def->onHitBleedDuration, def->onHitBleedDpsMul,
                                                                      def->onHitPoisonChance, def->onHitPoisonDuration, def->onHitPoisonDpsMul,
                                                                      def->onHitFearChance, def->onHitFearDuration});
            }
            if (def && def->reviveChance > 0.0f && def->reviveMaxCount > 0) {
                registry.emplace<Game::EnemyRevive>(
                    e, Game::EnemyRevive{def->reviveChance, def->reviveHealthFraction, def->reviveDelay, def->reviveMaxCount});
            }
            if (def && def->slimeMultiplyChance > 0.0f && def->slimeMultiplyInterval > 0.0f && def->slimeMultiplyMaxCount > 0) {
                registry.emplace<Game::SlimeBehavior>(e, Game::SlimeBehavior{def->slimeMultiplyInterval,
                                                                             def->slimeMultiplyInterval,
                                                                             def->slimeMultiplyChance,
                                                                             def->slimeMultiplyMin,
                                                                             def->slimeMultiplyMax,
                                                                             def->slimeMultiplyMaxCount});
            }
            if (def && def->fireballEnabled) {
                registry.emplace<Game::FlameSkullBehavior>(
                    e, Game::FlameSkullBehavior{def->fireballCooldown,
                                                def->fireballCooldown,
                                                def->fireballMinRange,
                                                def->fireballMaxRange,
                                                def->fireballSpeed,
                                                def->fireballHitbox,
                                                def->fireballLifetime,
                                                def->fireballDamageMul});
            }
            if (tex && def) {
                registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{def->frameWidth,
                                                                                               def->frameHeight,
                                                                                               std::max(1, def->frameCount),
                                                                                               def->frameDuration});
            }
            registry.emplace<Game::PickupBob>(e, Game::PickupBob{spawnPos, 0.0f, 2.0f, 2.5f});
        };

        for (int g = 0; g < groupCount; ++g) {
            Engine::Vec2 spawnPos = pos;
            if (g > 0) {
                float a = packAng(rng_);
                float r = packRad(rng_);
                spawnPos = Engine::Vec2{pos.x + std::cos(a) * r, pos.y + std::sin(a) * r};
            }
            spawnOne(spawnPos);
        }
    }

    // Every 5th wave spawn a bounty elite near hero.
    if (wave % 5 == 0) {
        float ang = angleDist(rng_);
        float rad = radiusDist(rng_) * 0.6f;
        Engine::Vec2 pos{center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad};
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, pos);
        registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
        registry.emplace<Game::Facing>(e, Game::Facing{1});
        registry.emplace<Game::LookDirection>(e, Game::LookDirection{});
        const EnemyDefinition* def = pickEnemyDef();
        float sizeMul = def ? def->sizeMultiplier : 1.2f;
        float size = 24.0f * sizeMul;
        float hpVal = settings_.enemyHp * 3.0f * (def ? def->hpMultiplier : 1.0f);
        float shieldsVal = settings_.enemyShields * (def ? def->shieldMultiplier : 1.0f);
        float speedVal = settings_.enemySpeed * 1.1f * (def ? def->speedMultiplier : 1.0f);
        float dmgMul = def ? def->damageMultiplier : 1.0f;
        if (speedVal < 8.0f) speedVal = settings_.enemySpeed * 0.8f;
        Engine::TexturePtr tex = def ? pickVariantTexture(def) : Engine::TexturePtr{};
        int defIndex = -1;
        if (def && enemyDefs_ && !enemyDefs_->empty()) {
            defIndex = static_cast<int>(def - enemyDefs_->data());
        }
        if (!tex && enemyDefs_) {
            for (const auto& d : *enemyDefs_) {
                if (d.texture) { tex = d.texture; break; }
            }
        }
        registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                              Engine::Color{255, 170, 60, 255},
                                                                              tex});
        registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
        registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
        if (auto* hp = registry.get<Engine::ECS::Health>(e)) {
            Engine::Gameplay::applyUpgradesToUnit(*hp, baseStats_, upgrades_, false);
            hp->tags = {Engine::Gameplay::Tag::Biological, Engine::Gameplay::Tag::Armored};
            hp->maxShields = std::max(0.0f, shieldsVal);
            hp->currentShields = hp->maxShields;
            hp->healthArmor = settings_.enemyHealthArmor + 1.0f;  // elites slightly tougher
            hp->shieldArmor = settings_.enemyShieldArmor;
            hp->shieldRegenRate = settings_.enemyShieldRegen * (def ? def->shieldRegenMultiplier : 1.0f);
            hp->regenDelay = settings_.enemyRegenDelay * (def ? def->regenDelayMultiplier : 1.0f);
            if (def && def->healthRegenFracPerSecond > 0.0f) {
                hp->healthRegenRate = std::max(0.0f, hp->maxHealth * def->healthRegenFracPerSecond);
            }
        }
        registry.emplace<Engine::ECS::RPGStats>(e, Engine::ECS::RPGStats{});
        if (auto* rpg = registry.get<Engine::ECS::RPGStats>(e)) {
            rpg->baseFromHealth = true;
            const int lvl = std::max(1, wave + 1);
            rpg->attributes.STR = lvl / 10 + 1;
            rpg->attributes.DEX = lvl / 12 + 1;
            rpg->attributes.INT = lvl / 16;
            rpg->attributes.END = lvl / 10 + 1;
            rpg->attributes.LCK = lvl / 18;
            rpg->base.baseAttackPower = std::max(1.0f, settings_.contactDamage * 1.25f * dmgMul);
            rpg->base.baseSpellPower = rpg->base.baseAttackPower;
            rpg->base.baseAccuracy = 2.0f + static_cast<float>(lvl) * 0.06f;
            rpg->base.baseCritChance = 0.03f;
            rpg->base.baseEvasion = static_cast<float>(lvl) * 0.10f;
            rpg->dirty = true;
        }
        registry.emplace<Engine::ECS::Status>(e, Engine::ECS::Status{});
        registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
        registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
        if (defIndex >= 0) registry.emplace<Game::EnemyType>(e, Game::EnemyType{defIndex});
        if (def && (def->onHitBleedChance > 0.0f || def->onHitPoisonChance > 0.0f || def->onHitFearChance > 0.0f || def->damageMultiplier != 1.0f)) {
            registry.emplace<Game::EnemyOnHit>(e, Game::EnemyOnHit{dmgMul,
                                                                  def->onHitBleedChance, def->onHitBleedDuration, def->onHitBleedDpsMul,
                                                                  def->onHitPoisonChance, def->onHitPoisonDuration, def->onHitPoisonDpsMul,
                                                                  def->onHitFearChance, def->onHitFearDuration});
        }
        if (def && def->reviveChance > 0.0f && def->reviveMaxCount > 0) {
            registry.emplace<Game::EnemyRevive>(
                e, Game::EnemyRevive{def->reviveChance, def->reviveHealthFraction, def->reviveDelay, def->reviveMaxCount});
        }
        if (def && def->slimeMultiplyChance > 0.0f && def->slimeMultiplyInterval > 0.0f && def->slimeMultiplyMaxCount > 0) {
            registry.emplace<Game::SlimeBehavior>(e, Game::SlimeBehavior{def->slimeMultiplyInterval,
                                                                         def->slimeMultiplyInterval,
                                                                         def->slimeMultiplyChance,
                                                                         def->slimeMultiplyMin,
                                                                         def->slimeMultiplyMax,
                                                                         def->slimeMultiplyMaxCount});
        }
        if (def && def->fireballEnabled) {
            registry.emplace<Game::FlameSkullBehavior>(
                e, Game::FlameSkullBehavior{def->fireballCooldown,
                                            def->fireballCooldown,
                                            def->fireballMinRange,
                                            def->fireballMaxRange,
                                            def->fireballSpeed,
                                            def->fireballHitbox,
                                            def->fireballLifetime,
                                            def->fireballDamageMul});
        }
        if (tex) {
            const int fw = def ? def->frameWidth : 16;
            const int fh = def ? def->frameHeight : 16;
            const int fc = def ? std::max(1, def->frameCount) : 4;
            const float fd = def ? def->frameDuration : 0.14f;
            registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{fw, fh, fc, fd});
        }
        registry.emplace<Game::PickupBob>(e, Game::PickupBob{pos, 0.0f, 2.0f, 2.5f});
        registry.emplace<Game::BountyTag>(e, Game::BountyTag{});
    }

    // Boss spawn on milestone (repeats every bossInterval_ after first).
    if (wave == nextBossWave_) {
        float ang = angleDist(rng_);
        float rad = 260.0f;
        int bosses = std::max(1, playerCount_);
        for (int i = 0; i < bosses; ++i) {
            float offsetAng = ang + static_cast<float>(i) * (6.28318f / static_cast<float>(bosses));
            Engine::Vec2 pos{center.x + std::cos(offsetAng) * rad, center.y + std::sin(offsetAng) * rad};
            auto e = registry.create();
            registry.emplace<Engine::ECS::Transform>(e, pos);
            registry.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
            registry.emplace<Game::Facing>(e, Game::Facing{1});
            registry.emplace<Game::LookDirection>(e, Game::LookDirection{});
            const EnemyDefinition* def = pickEnemyDef();
            float sizeMul = def ? def->sizeMultiplier * 1.6f : 2.0f;
            float size = 34.0f * sizeMul;
            // Cap boss visual scale to avoid runaway growth on high waves/content packs.
            if (size > bossMaxSize_) {
                const float clampMul = bossMaxSize_ / size;
                size *= clampMul;
                sizeMul *= clampMul;
            }
            float hpVal = settings_.enemyHp * bossHpMul_ * (def ? def->hpMultiplier : 1.0f);
            float shieldsVal = settings_.enemyShields * (def ? def->shieldMultiplier : 1.0f);
            float speedVal = settings_.enemySpeed * bossSpeedMul_ * (def ? def->speedMultiplier : 1.0f);
            float dmgMul = def ? def->damageMultiplier : 1.0f;
            if (speedVal < 10.0f) speedVal = std::max(speedVal, settings_.enemySpeed * 0.6f);
            Engine::TexturePtr tex = def ? pickVariantTexture(def) : Engine::TexturePtr{};
            int defIndex = -1;
            if (def && enemyDefs_ && !enemyDefs_->empty()) {
                defIndex = static_cast<int>(def - enemyDefs_->data());
            }
            if (!tex && enemyDefs_) {
                for (const auto& d : *enemyDefs_) {
                    if (d.texture) { tex = d.texture; break; }
                }
            }
            registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                                  Engine::Color{200, 80, 200, 255},
                                                                                  tex});
            registry.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
            registry.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hpVal, hpVal});
            if (auto* hp = registry.get<Engine::ECS::Health>(e)) {
                Engine::Gameplay::applyUpgradesToUnit(*hp, baseStats_, upgrades_, false);
                hp->tags = {Engine::Gameplay::Tag::Biological, Engine::Gameplay::Tag::Armored};
                hp->healthArmor = settings_.enemyHealthArmor + 1.5f;
                hp->shieldArmor = settings_.enemyShieldArmor + 1.0f;
                hp->maxShields = std::max(0.0f, shieldsVal * 1.5f);
                hp->currentShields = hp->maxShields;
                hp->shieldRegenRate = settings_.enemyShieldRegen * 1.1f * (def ? def->shieldRegenMultiplier : 1.0f);
                hp->regenDelay = settings_.enemyRegenDelay * (def ? def->regenDelayMultiplier : 1.0f);
                if (def && def->healthRegenFracPerSecond > 0.0f) {
                    hp->healthRegenRate = std::max(0.0f, hp->maxHealth * def->healthRegenFracPerSecond);
                }
            }
            registry.emplace<Engine::ECS::RPGStats>(e, Engine::ECS::RPGStats{});
            if (auto* rpg = registry.get<Engine::ECS::RPGStats>(e)) {
                rpg->baseFromHealth = true;
                const int lvl = std::max(1, wave + 1);
                rpg->attributes.STR = lvl / 8 + 2;
                rpg->attributes.DEX = lvl / 10 + 2;
                rpg->attributes.INT = lvl / 14 + 1;
                rpg->attributes.END = lvl / 8 + 2;
                rpg->attributes.LCK = lvl / 16;
                rpg->base.baseAttackPower = std::max(1.0f, settings_.contactDamage * 1.6f * dmgMul);
                rpg->base.baseSpellPower = rpg->base.baseAttackPower;
                rpg->base.baseAccuracy = 3.0f + static_cast<float>(lvl) * 0.08f;
                rpg->base.baseCritChance = 0.04f;
                rpg->base.baseEvasion = static_cast<float>(lvl) * 0.12f;
                rpg->dirty = true;
            }
            registry.emplace<Engine::ECS::Status>(e, Engine::ECS::Status{});
            registry.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
            registry.emplace<Engine::ECS::BossTag>(e, Engine::ECS::BossTag{});
            registry.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{speedVal});
            if (defIndex >= 0) registry.emplace<Game::EnemyType>(e, Game::EnemyType{defIndex});
            if (def && (def->onHitBleedChance > 0.0f || def->onHitPoisonChance > 0.0f || def->onHitFearChance > 0.0f || def->damageMultiplier != 1.0f)) {
                registry.emplace<Game::EnemyOnHit>(e, Game::EnemyOnHit{dmgMul,
                                                                      def->onHitBleedChance, def->onHitBleedDuration, def->onHitBleedDpsMul,
                                                                      def->onHitPoisonChance, def->onHitPoisonDuration, def->onHitPoisonDpsMul,
                                                                      def->onHitFearChance, def->onHitFearDuration});
            }
            if (def && def->reviveChance > 0.0f && def->reviveMaxCount > 0) {
                registry.emplace<Game::EnemyRevive>(
                    e, Game::EnemyRevive{def->reviveChance, def->reviveHealthFraction, def->reviveDelay, def->reviveMaxCount});
            }
            if (def && def->slimeMultiplyChance > 0.0f && def->slimeMultiplyInterval > 0.0f && def->slimeMultiplyMaxCount > 0) {
                registry.emplace<Game::SlimeBehavior>(e, Game::SlimeBehavior{def->slimeMultiplyInterval,
                                                                             def->slimeMultiplyInterval,
                                                                             def->slimeMultiplyChance,
                                                                             def->slimeMultiplyMin,
                                                                             def->slimeMultiplyMax,
                                                                             def->slimeMultiplyMaxCount});
            }
            if (def && def->fireballEnabled) {
                registry.emplace<Game::FlameSkullBehavior>(
                    e, Game::FlameSkullBehavior{def->fireballCooldown,
                                                def->fireballCooldown,
                                                def->fireballMinRange,
                                                def->fireballMaxRange,
                                                def->fireballSpeed,
                                                def->fireballHitbox,
                                                def->fireballLifetime,
                                                def->fireballDamageMul});
            }
            if (tex) {
                const int fw = def ? def->frameWidth : 16;
                const int fh = def ? def->frameHeight : 16;
                const int fc = def ? std::max(1, def->frameCount) : 4;
                const float fd = def ? def->frameDuration : 0.14f;
                registry.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{fw, fh, fc, fd});
            }
        }
        nextBossWave_ += bossInterval_;
    }

    return true;
}

}  // namespace Game
