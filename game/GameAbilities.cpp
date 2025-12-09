// Ability execution and upgrade helpers.
#include "Game.h"

#include "../engine/ecs/components/Transform.h"
#include "../engine/ecs/components/Velocity.h"
#include "../engine/ecs/components/Renderable.h"
#include "../engine/ecs/components/AABB.h"
#include "../engine/ecs/components/Health.h"
#include "../engine/ecs/components/Projectile.h"
#include "../engine/ecs/components/Tags.h"
#include "../engine/gameplay/Combat.h"
#include "systems/BuffSystem.h"
#include "../engine/math/Vec2.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace Game {

void GameRoot::upgradeFocusedAbility() {
    if (abilities_.empty()) return;
    int idx = std::clamp(abilityFocus_, 0, static_cast<int>(abilities_.size() - 1));
    auto& slot = abilities_[idx];
    auto& st = abilityStates_[idx];
    if (slot.level >= slot.maxLevel) return;
    int cost = slot.upgradeCost + (slot.level - 1) * (slot.upgradeCost / 2);
    if (copper_ < cost) {
        shopNoFundsTimer_ = 0.6;
        return;
    }
    copper_ -= cost;
    slot.level += 1;
    st.level = slot.level;
    // Scale power for known types
    if (slot.type == "scatter" || slot.type == "nova" || slot.type == "ultimate") {
        slot.powerScale *= 1.2f;
    } else if (slot.type == "rage") {
        slot.powerScale *= 1.1f;
        slot.cooldownMax = std::max(4.0f, slot.cooldownMax * 0.92f);
    }
}

void GameRoot::applyPowerupPickup(Pickup::Powerup type) {
    switch (type) {
        case Pickup::Powerup::Heal: {
            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                hp->currentHealth = hp->maxHealth;
                hp->currentShields = hp->maxShields;
                hp->regenDelay = 0.0f;
            }
            break;
        }
        case Pickup::Powerup::Kaboom: {
            registry_.view<Engine::ECS::Health, Engine::ECS::EnemyTag>(
                [](Engine::ECS::Entity, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                    hp.currentHealth = 0.0f;
                    hp.currentShields = 0.0f;
                });
            break;
        }
        case Pickup::Powerup::Recharge: {
            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                hp->currentShields = hp->maxShields;
                hp->regenDelay = 0.0f;
            }
            energy_ = energyMax_;
            break;
        }
        case Pickup::Powerup::Frenzy: {
            frenzyTimer_ = 30.0f;
            frenzyRateBuff_ = 1.25f;
            break;
        }
        case Pickup::Powerup::Immortal: {
            immortalTimer_ = 30.0f;
            // Component refresh handled in main update loop.
            break;
        }
        default:
            break;
    }
}

void GameRoot::executeAbility(int index) {
    if (index < 0 || index >= static_cast<int>(abilityStates_.size())) return;
    auto& slot = abilities_[index];
    auto& st = abilityStates_[index];
    if (slot.cooldown > 0.0f) return;
    if (slot.energyCost > 0.0f && energy_ < slot.energyCost) {
        energyWarningTimer_ = 0.6;
        return;
    }

    // Helper to pull hero position
    const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
    if (!heroTf) return;
    float zoneDmgMul = hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f;

    auto setCooldown = [&](float cd) {
        float adjusted = cd * abilityCooldownMul_;
        slot.cooldown = adjusted;
        slot.cooldownMax = std::max(slot.cooldownMax, adjusted);
        st.cooldown = adjusted;
    };
    auto spendEnergy = [&]() {
        if (slot.energyCost > 0.0f) {
            float cost = slot.energyCost * abilityVitalCostMul_;
            energy_ = std::max(0.0f, energy_ - cost);
        }
    };

    auto spawnProjectile = [&](const Engine::Vec2& dir, float speed, Engine::Gameplay::DamageEvent dmgEvent, float sizeMul = 1.0f) {
        auto p = registry_.create();
        registry_.emplace<Engine::ECS::Transform>(p, heroTf->position);
        registry_.emplace<Engine::ECS::Velocity>(p, Engine::Vec2{0.0f, 0.0f});
        float sz = projectileSize_ * sizeMul;
        float hb = projectileHitboxSize_ * sizeMul * 0.5f;
        registry_.emplace<Engine::ECS::AABB>(p, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
        registry_.emplace<Engine::ECS::Renderable>(p,
            Engine::ECS::Renderable{Engine::Vec2{sz, sz}, Engine::Color{255, 220, 140, 255}});
        registry_.emplace<Engine::ECS::Projectile>(p, Engine::ECS::Projectile{dir * speed, dmgEvent, projectileLifetime_, lifestealPercent_, chainBounces_});
        registry_.emplace<Engine::ECS::ProjectileTag>(p, Engine::ECS::ProjectileTag{});
    };

    if (slot.type == "scatter") {
        // Cone blast of pellets
        spendEnergy();
        std::uniform_real_distribution<float> spread(-0.35f, 0.35f);
        std::mt19937& rng = rng_;
        for (int i = 0; i < 6 + slot.level; ++i) {
            float ang = std::atan2(mouseWorld_.y - heroTf->position.y, mouseWorld_.x - heroTf->position.x);
            ang += spread(rng);
            Engine::Vec2 dir{std::cos(ang), std::sin(ang)};
            Engine::Gameplay::DamageEvent dmgEvent{};
            dmgEvent.baseDamage = projectileDamage_ * 0.7f * slot.powerScale * zoneDmgMul;
            dmgEvent.type = Engine::Gameplay::DamageType::Normal;
            // Cone spray: bonus vs Armored to differentiate.
            dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Armored] = 2.0f;
            spawnProjectile(dir, projectileSpeed_ * 0.8f, dmgEvent, 0.9f);
        }
        setCooldown(std::max(0.5f, slot.cooldownMax));
    } else if (slot.type == "rage") {
        spendEnergy();
        rageDamageBuff_ = 1.2f * slot.powerScale;
        rageRateBuff_ = 1.15f * slot.powerScale;
        rageTimer_ = 5.0f + slot.level * 0.6f;
        // Apply a brief defensive buff.
        Engine::Gameplay::BuffState buff{};
        buff.healthArmorBonus = 1.0f;
        buff.shieldArmorBonus = 1.0f;
        buff.damageTakenMultiplier = 0.9f;
        if (registry_.has<Game::ArmorBuff>(hero_)) {
            registry_.remove<Game::ArmorBuff>(hero_);
        }
        registry_.emplace<Game::ArmorBuff>(hero_, Game::ArmorBuff{buff, rageTimer_});
        setCooldown(std::max(3.0f, slot.cooldownMax));
    } else if (slot.type == "nova") {
        spendEnergy();
        int count = 12 + slot.level * 2;
        float speed = projectileSpeed_ * 0.7f;
        float dmg = projectileDamage_ * 1.6f * slot.powerScale * zoneDmgMul;
        for (int i = 0; i < count; ++i) {
            float ang = (6.2831853f * i) / static_cast<float>(count);
            Engine::Vec2 dir{std::cos(ang), std::sin(ang)};
            Engine::Gameplay::DamageEvent dmgEvent{};
            dmgEvent.baseDamage = dmg;
            dmgEvent.type = Engine::Gameplay::DamageType::Normal;
            spawnProjectile(dir, speed, dmgEvent, 1.2f);
        }
        setCooldown(std::max(2.0f, slot.cooldownMax));
    } else if (slot.type == "ultimate") {
        spendEnergy();
        int waves = 3;
        for (int w = 0; w < waves; ++w) {
            int count = 20 + slot.level * 2;
            float speed = projectileSpeed_ * (0.8f + 0.1f * w);
                float dmg = projectileDamage_ * (2.0f + 0.2f * w) * slot.powerScale * zoneDmgMul;
                for (int i = 0; i < count; ++i) {
                    float ang = (6.2831853f * i) / static_cast<float>(count);
                    Engine::Vec2 dir{std::cos(ang), std::sin(ang)};
                    Engine::Gameplay::DamageEvent dmgEvent{};
                    dmgEvent.baseDamage = dmg;
                    dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                    spawnProjectile(dir, speed, dmgEvent, 1.3f);
                }
        }
        setCooldown(std::max(8.0f, slot.cooldownMax));
    } else {
        // Generic placeholder
        spendEnergy();
        setCooldown(slot.cooldownMax);
    }
}

}  // namespace Game
