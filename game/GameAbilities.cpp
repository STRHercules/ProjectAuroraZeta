// Ability execution and upgrade helpers.
#include "Game.h"

#include "../engine/ecs/components/Transform.h"
#include "../engine/ecs/components/Velocity.h"
#include "../engine/ecs/components/Renderable.h"
#include "../engine/ecs/components/AABB.h"
#include "../engine/ecs/components/Health.h"
#include "../engine/ecs/components/Status.h"
#include "../engine/ecs/components/Projectile.h"
#include "../engine/ecs/components/Tags.h"
#include "../engine/ecs/components/SpriteAnimation.h"
#include "../engine/gameplay/Combat.h"
#include "systems/RpgDamage.h"
#include "systems/BuffSystem.h"
#include "components/SummonedUnit.h"
#include "components/SpellEffect.h"
#include "components/AreaDamage.h"
#include "components/StatusEffects.h"
#include "../engine/math/Vec2.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <filesystem>

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
        case Pickup::Powerup::Freeze: {
            freezeTimer_ = std::max(freezeTimer_, 5.0);  // brief crowd-control window
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
    if (const auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        if (!hp->alive()) return;
    }
    if (const auto* status = registry_.get<Engine::ECS::Status>(hero_)) {
        if (status->container.blocksCasting()) {
            return;
        }
    }
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
        bool wiz = activeArchetype_.id == "wizard";
        float speedLocal = wiz ? speed * 0.35f : speed;
        float sizeLocal = wiz ? sizeMul * 2.8f : sizeMul;
        auto p = registry_.create();
        registry_.emplace<Engine::ECS::Transform>(p, heroTf->position);
        registry_.emplace<Engine::ECS::Velocity>(p, dir * speedLocal);
        float hb = projectileHitboxSize_ * sizeLocal * 0.5f;
        registry_.emplace<Engine::ECS::AABB>(p, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
        const SpriteSheetDesc* vis = (usingSecondaryWeapon_ && archetypeSupportsSecondaryWeapon(activeArchetype_))
                                         ? &projectileVisualArrow_
                                         : nullptr;
        applyProjectileVisual(p, sizeLocal, Engine::Color{255, 220, 140, 255}, false, vis);
        registry_.emplace<Engine::ECS::Projectile>(p, Engine::ECS::Projectile{dir * speedLocal, dmgEvent, projectileLifetime_, lifestealPercent_, chainBounces_, hero_});
        registry_.emplace<Engine::ECS::ProjectileTag>(p, Engine::ECS::ProjectileTag{});
        return p;
    };
    auto wizardStage = [&]() {
        int lvl = !abilityStates_.empty() ? abilityStates_[0].level : 1;
        return std::clamp((lvl + 1) / 2, 1, 3);
    };
    auto applyWizardVisual = [&](Engine::ECS::Entity ent, Game::ElementType el, int stage, float sizeMul = 1.0f) {
        if (activeArchetype_.id != "wizard") return;
        if (!wizardElementTex_) return;
        int rowBase = 0;
        switch (el) {
            case Game::ElementType::Lightning: rowBase = 0; break;
            case Game::ElementType::Dark: rowBase = 3; break;
            case Game::ElementType::Fire: rowBase = 6; break;
            case Game::ElementType::Ice: rowBase = 9; break;
            case Game::ElementType::Wind: rowBase = 13; break;
            case Game::ElementType::Earth: rowBase = 17; break;
            default: rowBase = 0; break;
        }
        int row = rowBase + (stage - 1);
        int maxRows = std::max(1, wizardElementTex_->height() / 8);
        row = std::clamp(row, 0, maxRows - 1);
        float size = 8.0f * 2.8f * sizeMul;
        if (registry_.has<Engine::ECS::Renderable>(ent)) registry_.remove<Engine::ECS::Renderable>(ent);
        registry_.emplace<Engine::ECS::Renderable>(ent,
            Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{255, 255, 255, 255}, wizardElementTex_});
        auto anim = Engine::ECS::SpriteAnimation{8, 8, std::max(1, wizardElementColumns_), wizardElementFrameDuration_};
        anim.row = row;
        if (registry_.has<Engine::ECS::SpriteAnimation>(ent)) registry_.remove<Engine::ECS::SpriteAnimation>(ent);
        registry_.emplace<Engine::ECS::SpriteAnimation>(ent, anim);
    };
    auto spawnSummoned = [&](const std::string& key) {
        auto it = miniUnitDefs_.find(key);
        if (it == miniUnitDefs_.end()) return false;
        std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
        std::uniform_real_distribution<float> rad(18.0f, 48.0f);
        float a = ang(rng_);
        float r = rad(rng_);
        Engine::Vec2 spawnPos{heroTf->position.x + std::cos(a) * r, heroTf->position.y + std::sin(a) * r};
        auto ent = spawnMiniUnit(it->second, spawnPos);
        if (ent != Engine::ECS::kInvalidEntity) {
            if (registry_.has<Game::SummonedUnit>(ent)) {
                auto* su = registry_.get<Game::SummonedUnit>(ent);
                su->owner = hero_;
                su->leashRadius = 320.0f;
            } else {
                registry_.emplace<Game::SummonedUnit>(ent, Game::SummonedUnit{hero_, 320.0f});
            }
            if (auto* mu = registry_.get<Game::MiniUnit>(ent)) {
                mu->combatEnabled = true;
            }
        }
        return ent != Engine::ECS::kInvalidEntity;
    };
    auto applyDruidForm = [&](DruidForm form) {
        druidForm_ = form;
        heroMaxHp_ = heroMaxHpPreset_;
        heroShield_ = heroShieldPreset_;
        heroMoveSpeed_ = heroMoveSpeedPreset_;
        projectileDamage_ = projectileDamagePreset_;
        heroHealthArmor_ = heroBaseStatsScaled_.baseHealthArmor;
        heroShieldArmor_ = heroBaseStatsScaled_.baseShieldArmor;
        Game::OffensiveType offense = activeArchetype_.offensiveType;
        if (form == DruidForm::Bear) {
            heroMaxHp_ *= 1.6f;
            heroShield_ *= 1.1f;
            heroShield_ += 100.0f;  // bear form gains a hefty shield buffer
            heroHealthArmor_ += 2.0f;
            heroShieldArmor_ += 1.5f;
            heroMoveSpeed_ *= 0.82f;
            projectileDamage_ *= 0.9f;
            offense = Game::OffensiveType::Melee;
        } else if (form == DruidForm::Wolf) {
            heroMaxHp_ *= 1.1f;
            heroMoveSpeed_ *= 1.22f;
            projectileDamage_ *= 1.25f;
            heroHealthArmor_ += 0.5f;
            offense = Game::OffensiveType::Melee;
        }
        heroBaseOffense_ = offense;
        refreshHeroOffenseTag();
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            float healthRatio = hp->maxHealth > 0.0f ? hp->currentHealth / hp->maxHealth : 1.0f;
            float shieldRatio = hp->maxShields > 0.0f ? hp->currentShields / hp->maxShields : 1.0f;
            hp->maxHealth = heroMaxHp_;
            hp->maxShields = heroShield_;
            hp->currentHealth = std::min(heroMaxHp_, heroMaxHp_ * std::max(0.25f, healthRatio));
            hp->currentShields = std::min(heroShield_, heroShield_ * std::max(0.0f, shieldRatio));
            hp->healthArmor = heroHealthArmor_;
            hp->shieldArmor = heroShieldArmor_;
        }
        auto setSheets = [&](const std::string& movePath, const std::string& combatPath, int rows,
                             int moveOverride, int combatOverride, float baseSize, bool swapVertical) {
            Engine::TexturePtr move = loadTextureOptional(movePath);
            Engine::TexturePtr combat = loadTextureOptional(combatPath);
            if (!move || !combat) return;
            int movementColumns = 4;
            int moveFrameW = moveOverride > 0 ? moveOverride : std::max(1, move->width() / movementColumns);
            int moveFrameH = moveOverride > 0 ? moveOverride
                                              : ((rows > 0 && move->height() % rows == 0) ? move->height() / rows : move->height());
            int moveFrames = std::max(1, move->width() / moveFrameW);
            int combatColumns = 4;
            int combatFrameW = combatOverride > 0 ? combatOverride : std::max(1, combat->width() / combatColumns);
            int combatFrameH = combatOverride > 0 ? combatOverride : combatFrameW;
            if (combatOverride == 0 && combat->height() % 20 == 0) combatFrameH = combat->height() / 20;
            float moveFrameDur = 0.12f;
            float combatFrameDur = 0.10f;
            if (registry_.has<Game::HeroSpriteSheets>(hero_)) {
                registry_.remove<Game::HeroSpriteSheets>(hero_);
            }
            registry_.emplace<Game::HeroSpriteSheets>(hero_,
                Game::HeroSpriteSheets{move, combat, moveFrameW, moveFrameH, combatFrameW, combatFrameH, baseSize, moveFrameDur, combatFrameDur, swapVertical});
            if (auto* rend = registry_.get<Engine::ECS::Renderable>(hero_)) {
                rend->texture = move;
            }
            if (registry_.has<Engine::ECS::SpriteAnimation>(hero_)) {
                registry_.remove<Engine::ECS::SpriteAnimation>(hero_);
            }
            registry_.emplace<Engine::ECS::SpriteAnimation>(
                hero_, Engine::ECS::SpriteAnimation{moveFrameW, moveFrameH, moveFrames, moveFrameDur});
        };
        if (form == DruidForm::Bear) {
            setSheets("assets/Sprites/Units/Bear/Bear.png", "assets/Sprites/Units/Bear/Bear_Attack.png", 14, 24, 32, 24.0f, false);
            if (auto* hs = registry_.get<Game::HeroSpriteSheets>(hero_)) {
                hs->lockFrontBackIdle = true;
                hs->hasPickupRows = false;
                hs->rowWalkFront = 7;  // legend: walk front row8 (index7)
                hs->rowWalkBack = 6;   // legend: walk back row7 (index6)
                hs->rowKnockdownRight = 12; // row13 (index12)
                hs->rowKnockdownLeft = 13;  // row14 (index13)
            }
        } else if (form == DruidForm::Wolf) {
            setSheets("assets/Sprites/Units/Wolf/Wolf.png", "assets/Sprites/Units/Wolf/Wolf_Attack.png", 14, 16, 24, 16.0f, false);
            if (auto* hs = registry_.get<Game::HeroSpriteSheets>(hero_)) {
                hs->lockFrontBackIdle = false;
                hs->hasPickupRows = false;
                hs->rowKnockdownRight = 12;
                hs->rowKnockdownLeft = 13;
            }
        } else {
            auto files = heroSpriteFilesFor(activeArchetype_);
            std::filesystem::path base = std::filesystem::path("assets") / "Sprites" / "Characters" / files.folder;
            setSheets((base / files.movementFile).string(), (base / files.combatFile).string(), 31, 0, 0, heroSize_, false);
            if (auto* hs = registry_.get<Game::HeroSpriteSheets>(hero_)) {
                hs->lockFrontBackIdle = false;
                hs->hasPickupRows = true;
                hs->rowWalkFront = 6;
                hs->rowWalkBack = 7;
                hs->rowKnockdownRight = 24;
                hs->rowKnockdownLeft = 25;
            }
        }
    };

    // Builder-specific ability hooks: upgrade mini-units and trigger overdrive.
    if (activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
        if (slot.type == "builder_up_light" || slot.type == "builder_up_heavy" || slot.type == "builder_up_medic") {
            spendEnergy();
            setCooldown(slot.cooldownMax);
            const float mul = 1.05f;
            if (slot.type == "builder_up_light") applyMiniUnitBuff(Game::MiniUnitClass::Light, mul);
            if (slot.type == "builder_up_heavy") applyMiniUnitBuff(Game::MiniUnitClass::Heavy, mul);
            if (slot.type == "builder_up_medic") applyMiniUnitBuff(Game::MiniUnitClass::Medic, mul);
            slot.level += 1;
            st.level = slot.level;
            return;
        }
        if (slot.type == "builder_rage") {
            spendEnergy();
            setCooldown(slot.cooldownMax);
            const float duration = 30.0f;
            const float dmgMul = 1.35f;
            const float atkRateMul = 0.65f;  // faster attacks
            const float hpMul = 1.25f;
            const float healMul = 1.35f;
            activateMiniUnitRage(duration, dmgMul, atkRateMul, hpMul, healMul);
            return;
        }
    }

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
    } else if (slot.type == "summon_beatle") {
        spendEnergy();
        bool ok = spawnSummoned("beatle");
        if (ok) setCooldown(std::max(1.5f, slot.cooldownMax));
    } else if (slot.type == "summon_snake") {
        spendEnergy();
        bool ok = spawnSummoned("snake");
        if (ok) setCooldown(std::max(2.0f, slot.cooldownMax));
    } else if (slot.type == "summon_rally") {
        spendEnergy();
        registry_.view<Game::SummonedUnit, Game::MiniUnitCommand>(
            [&](Engine::ECS::Entity, Game::SummonedUnit& su, Game::MiniUnitCommand& cmd) {
                if (su.owner != hero_) return;
                cmd.hasOrder = true;
                cmd.target = heroTf->position;
            });
        setCooldown(std::max(2.0f, slot.cooldownMax));
    } else if (slot.type == "summon_horde") {
        spendEnergy();
        int beetles = 2 + slot.level / 2;
        int snakes = 1 + slot.level / 3;
        int spawned = 0;
        for (int i = 0; i < beetles; ++i) if (spawnSummoned("beatle")) ++spawned;
        for (int i = 0; i < snakes; ++i) if (spawnSummoned("snake")) ++spawned;
        if (spawned > 0) setCooldown(std::max(6.0f, slot.cooldownMax));
    } else if (slot.type == "druid_select_bear") {
        if (level_ < 2) return;
        druidChosen_ = DruidForm::Bear;
        druidChoiceMade_ = true;
        applyDruidForm(druidForm_ == DruidForm::Bear ? DruidForm::Human : druidForm_);
        setCooldown(0.2f);
    } else if (slot.type == "druid_select_wolf") {
        if (level_ < 2) return;
        druidChosen_ = DruidForm::Wolf;
        druidChoiceMade_ = true;
        applyDruidForm(druidForm_ == DruidForm::Wolf ? DruidForm::Human : druidForm_);
        setCooldown(0.2f);
    } else if (slot.type == "druid_toggle") {
        if (!druidChoiceMade_) return;
        spendEnergy();
        if (druidForm_ == DruidForm::Human) {
            applyDruidForm(druidChosen_);
        } else {
            applyDruidForm(DruidForm::Human);
        }
        setCooldown(std::max(0.2f, slot.cooldownMax));
    } else if (slot.type == "druid_regrowth") {
        spendEnergy();
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            float heal = hp->maxHealth * (0.12f + 0.03f * slot.level);
            hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + heal);
            hp->regenDelay = 0.0f;
        }
        setCooldown(std::max(6.0f, slot.cooldownMax));
    } else if (slot.type == "wizard_fireball") {
        spendEnergy();
        float ang = std::atan2(mouseWorld_.y - heroTf->position.y, mouseWorld_.x - heroTf->position.x);
        Engine::Vec2 dir{std::cos(ang), std::sin(ang)};
        Engine::Gameplay::DamageEvent dmg{};
        dmg.type = Engine::Gameplay::DamageType::Spell;
        dmg.rpgDamageType = static_cast<int>(Engine::Gameplay::RPG::DamageType::Fire);
        dmg.baseDamage = projectileDamage_ * 1.8f * slot.powerScale * zoneDmgMul;
        // Fireball: applies Cauterize (regen block) with a saving-throw style check (TEN) and CC DR handled by the resolver.
        dmg.rpgOnHitStatuses.push_back(
            Engine::Gameplay::DamageEvent::RpgOnHitStatus{
                static_cast<int>(Engine::Status::EStatusId::Cauterize),
                0.55f,
                4.5f,
                false});
        auto proj = spawnProjectile(dir, projectileSpeed_ * 0.9f, dmg, 1.3f);
        if (registry_.has<Game::SpellEffect>(proj)) registry_.remove<Game::SpellEffect>(proj);
        registry_.emplace<Game::SpellEffect>(proj, Game::SpellEffect{Game::ElementType::Fire, 3});
        registry_.emplace<Game::AreaDamage>(proj, Game::AreaDamage{96.0f, 1.0f});
        applyWizardVisual(proj, Game::ElementType::Fire, 3, 1.3f);
        setCooldown(std::max(4.0f, slot.cooldownMax));
    } else if (slot.type == "wizard_flamewall") {
        spendEnergy();
        int segments = 8 + slot.level;
        Engine::Vec2 dir{mouseWorld_.x - heroTf->position.x, mouseWorld_.y - heroTf->position.y};
        float len2 = dir.x * dir.x + dir.y * dir.y;
        if (len2 < 0.0001f) {
            dir = {1.0f, 0.0f};
            len2 = 1.0f;
        }
        float inv = 1.0f / std::sqrt(len2);
        dir = {dir.x * inv, dir.y * inv};
        float spacing = 32.0f;
        Engine::Vec2 start = heroTf->position;
        Engine::TexturePtr tex = loadTextureOptional("assets/Sprites/Spells/Large_Fire_Anti-Alias_glow_28x28.png");
        for (int i = 0; i < segments; ++i) {
            Engine::Vec2 pos{start.x + dir.x * spacing * (i + 1), start.y + dir.y * spacing * (i + 1)};
            Engine::ECS::Entity vis = registry_.create();
            registry_.emplace<Engine::ECS::Transform>(vis, pos);
            registry_.emplace<Engine::ECS::Renderable>(vis,
                Engine::ECS::Renderable{Engine::Vec2{28.0f, 28.0f}, Engine::Color{255, 160, 90, 200}, tex});
            flameWalls_.push_back(FlameWallInstance{pos, 4.0f + slot.level * 0.4f, vis});
        }
        setCooldown(std::max(8.0f, slot.cooldownMax));
    } else if (slot.type == "wizard_lbolt") {
        spendEnergy();
        float dirSign = (mouseWorld_.x >= heroTf->position.x) ? 1.0f : -1.0f;
        float length = 340.0f;
        float halfWidth = 42.0f;
        float dmgBase = projectileDamage_ * (1.6f + 0.1f * slot.level) * zoneDmgMul;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = std::abs(tf.position.y - heroTf->position.y);
                if (dx * dirSign < 0.0f) return;
                if (dx * dirSign > length) return;
                if (dy > halfWidth) return;
                Engine::Gameplay::DamageEvent dmg{};
                dmg.type = Engine::Gameplay::DamageType::Spell;
                dmg.rpgDamageType = static_cast<int>(Engine::Gameplay::RPG::DamageType::Shock);
                dmg.baseDamage = dmgBase;
                // L-Bolt: apply Stasis as a CC through the RPG pipeline (TEN + CC DR).
                float stunDur = 0.6f + 0.2f * wizardStage();
                dmg.rpgOnHitStatuses.push_back(
                    Engine::Gameplay::DamageEvent::RpgOnHitStatus{
                        static_cast<int>(Engine::Status::EStatusId::Stasis),
                        1.0f,
                        stunDur,
                        true});
                Engine::Gameplay::BuffState buff{};
                if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                    if (st->container.isStasis()) return;
                    float armorDelta = st->container.armorDeltaTotal();
                    buff.healthArmorBonus += armorDelta;
                    buff.shieldArmorBonus += armorDelta;
                    buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                }
                (void)Game::RpgDamage::apply(registry_, hero_, e, hp, dmg, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                             "wizard_lbolt", [this](const std::string& line) { pushCombatDebugLine(line); });
            });
        setCooldown(std::max(6.0f, slot.cooldownMax));
    } else if (slot.type == "wizard_ldome") {
        spendEnergy();
        lightningDomeTimer_ = std::max(lightningDomeTimer_, 5.0f + 0.5f * wizardStage());
        immortalTimer_ = std::max(immortalTimer_, lightningDomeTimer_);
        setCooldown(std::max(10.0f, slot.cooldownMax));
    } else {
        // Generic placeholder
        spendEnergy();
        setCooldown(slot.cooldownMax);
    }
}

}  // namespace Game
