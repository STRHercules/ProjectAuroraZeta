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
#include "components/Facing.h"
#include "components/TauntTarget.h"
#include "components/HeroAttackVisualOverride.h"
#include "components/MultiRowSpriteAnim.h"
#include "components/Invulnerable.h"
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
    } else if (slot.type == "healer_resurrect") {
        // Solo: passive extra lives; Multiplayer: reduce cooldown.
        if (!multiplayerEnabled_) {
            reviveCharges_ = std::max(reviveCharges_, slot.level);
        } else {
            slot.cooldownMax = std::max(10.0f, slot.cooldownMax * 0.90f);
        }
    } else if (slot.type == "support_diamond") {
        // No direct scaling here; charges derive from level.
        slot.cooldownMax = std::max(10.0f, slot.cooldownMax * 0.94f);
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
    if (shadowDanceActive_) return;
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

    auto aimDirFromCursor = [&]() {
        Engine::Vec2 dir{mouseWorld_.x - heroTf->position.x, mouseWorld_.y - heroTf->position.y};
        float len2 = dir.x * dir.x + dir.y * dir.y;
        if (len2 < 0.0001f) return Engine::Vec2{1.0f, 0.0f};
        float inv = 1.0f / std::sqrt(len2);
        return Engine::Vec2{dir.x * inv, dir.y * inv};
    };

    auto targetHeroUnderCursor = [&](float radius) {
        Engine::ECS::Entity best = hero_;
        float bestD2 = radius * radius;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::HeroTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, Engine::ECS::HeroTag&) {
                (void)hp;
                float dx = tf.position.x - mouseWorld_.x;
                float dy = tf.position.y - mouseWorld_.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    best = e;
                }
            });
        return best;
    };

    auto applyTauntPulse = [&](Engine::ECS::Entity taunter, float radius, float duration) {
        const auto* ttf = registry_.get<Engine::ECS::Transform>(taunter);
        const auto* thp = registry_.get<Engine::ECS::Health>(taunter);
        if (!ttf || !thp || !thp->alive()) return;
        float r2 = radius * radius;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - ttf->position.x;
                float dy = tf.position.y - ttf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 > r2) return;
                if (registry_.has<Game::TauntTarget>(e)) {
                    auto* tt = registry_.get<Game::TauntTarget>(e);
                    tt->target = taunter;
                    tt->timer = std::max(tt->timer, duration);
                } else {
                    registry_.emplace<Game::TauntTarget>(e, Game::TauntTarget{taunter, duration});
                }
            });
    };

    auto damageNearestEnemy = [&](float radius, float baseDamage, const char* ctx) {
        Engine::ECS::Entity best = Engine::ECS::kInvalidEntity;
        float bestD2 = radius * radius;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    best = e;
                }
            });
        if (best == Engine::ECS::kInvalidEntity) return;
        auto* hp = registry_.get<Engine::ECS::Health>(best);
        if (!hp || !hp->alive()) return;
        Engine::Gameplay::DamageEvent dmg{};
        dmg.type = Engine::Gameplay::DamageType::Normal;
        dmg.baseDamage = baseDamage;
        Engine::Gameplay::BuffState buff{};
        if (auto* armorBuff = registry_.get<Game::ArmorBuff>(best)) {
            buff = armorBuff->state;
        }
        if (auto* st = registry_.get<Engine::ECS::Status>(best)) {
            if (st->container.isStasis()) return;
            float armorDelta = st->container.armorDeltaTotal();
            buff.healthArmorBonus += armorDelta;
            buff.shieldArmorBonus += armorDelta;
            buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
        }
        (void)Game::RpgDamage::apply(registry_, hero_, best, *hp, dmg, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                     ctx, [this](const std::string& line) { pushCombatDebugLine(line); });
    };

    // --- New/updated class kits ---
    if (slot.type == "tank_dash_strike") {
        spendEnergy();
        setCooldown(std::max(0.5f, slot.cooldownMax));

        // Dash using existing dash movement plumbing (velocity + trail).
        Engine::Vec2 dir = aimDirFromCursor();
        dashDir_ = dir;
        dashTimer_ = std::max(dashTimer_, 0.22f + 0.02f * slot.level);
        dashInvulnTimer_ = std::max(dashInvulnTimer_, dashTimer_ * 0.6f);
        if (auto* inv = registry_.get<Game::Invulnerable>(hero_)) inv->timer = std::max(inv->timer, dashInvulnTimer_);
        else registry_.emplace<Game::Invulnerable>(hero_, Game::Invulnerable{dashInvulnTimer_});

        // Play dash-attack animation sheet.
        if (auto dashTex = loadTextureOptional("assets/Sprites/Characters/Tank/Tank_Dash_Attack.png")) {
            const int frameW = 32;
            const int frameH = 32;
            const int frames = std::max(1, dashTex->width() / frameW);
            const float frameDur = 0.08f;
            const float dur = std::max(0.05f, static_cast<float>(frames) * frameDur);
            if (auto* atk = registry_.get<Game::HeroAttackAnim>(hero_)) *atk = Game::HeroAttackAnim{dur, dir, 0};
            else registry_.emplace<Game::HeroAttackAnim>(hero_, Game::HeroAttackAnim{dur, dir, 0});
            if (registry_.has<Game::HeroAttackVisualOverride>(hero_)) registry_.remove<Game::HeroAttackVisualOverride>(hero_);
            registry_.emplace<Game::HeroAttackVisualOverride>(
                hero_, Game::HeroAttackVisualOverride{dashTex, frameW, frameH, frameDur, /*allowFlipX=*/false,
                                                      /*rowRight=*/0, /*rowLeft=*/1, /*rowFront=*/2, /*rowBack=*/3, /*mirrorLeft=*/false});
        }

        // Immediate strike damage around the hero (simple and readable).
        float dmg = projectileDamage_ * meleeConfig_.damageMultiplier * (1.8f + 0.1f * slot.level) * slot.powerScale * zoneDmgMul;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                float r = 78.0f;
                if (d2 > r * r) return;
                Engine::Gameplay::DamageEvent dmgEvent{};
                dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                dmgEvent.baseDamage = dmg;
                Engine::Gameplay::BuffState buff{};
                if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                    if (st->container.isStasis()) return;
                    float armorDelta = st->container.armorDeltaTotal();
                    buff.healthArmorBonus += armorDelta;
                    buff.shieldArmorBonus += armorDelta;
                    buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                }
                (void)Game::RpgDamage::apply(registry_, hero_, e, hp, dmgEvent, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                             "tank_dash_strike", [this](const std::string& line) { pushCombatDebugLine(line); });
            });
        return;
    } else if (slot.type == "tank_fortify") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        shieldOverchargeActive_ = true;
        shieldOverchargeTimer_ = std::max(shieldOverchargeTimer_, 6.0f + 0.5f * slot.level);
        shieldOverchargeBonusMax_ = std::max(shieldOverchargeBonusMax_, 140.0f + 20.0f * slot.level);
        shieldOverchargeRegenMul_ = std::max(shieldOverchargeRegenMul_, 1.0f);
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            if (!shieldOverchargeBaseMax_) shieldOverchargeBaseMax_ = hp->maxShields;
            if (!shieldOverchargeBaseRegen_) shieldOverchargeBaseRegen_ = hp->shieldRegenRate;
            hp->maxShields = std::max(hp->maxShields, shieldOverchargeBaseMax_ + shieldOverchargeBonusMax_);
            hp->currentShields = std::min(hp->maxShields, std::max(hp->currentShields, hp->maxShields));
        }
        return;
    } else if (slot.type == "tank_taunt") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        applyTauntPulse(hero_, 520.0f, 3.5f + 0.3f * slot.level);
        return;
    } else if (slot.type == "tank_bulwark") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        shieldOverchargeActive_ = true;
        shieldOverchargeTimer_ = std::max(shieldOverchargeTimer_, 8.0f + 0.6f * slot.level);
        shieldOverchargeBonusMax_ = std::max(shieldOverchargeBonusMax_, 220.0f + 25.0f * slot.level);
        shieldOverchargeRegenMul_ = std::max(shieldOverchargeRegenMul_, 4.0f);
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            if (!shieldOverchargeBaseMax_) shieldOverchargeBaseMax_ = hp->maxShields;
            if (!shieldOverchargeBaseRegen_) shieldOverchargeBaseRegen_ = hp->shieldRegenRate;
            hp->maxShields = std::max(hp->maxShields, shieldOverchargeBaseMax_ + shieldOverchargeBonusMax_);
        }
        return;
    } else if (slot.type == "healer_small_heal" || slot.type == "healer_heavy_heal" || slot.type == "special_heavy_heal") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        Engine::ECS::Entity target = hero_;
        if (multiplayerEnabled_) {
            target = targetHeroUnderCursor(90.0f);
        }
        if (auto* hp = registry_.get<Engine::ECS::Health>(target)) {
            float base = (slot.type == "healer_small_heal") ? 45.0f : 90.0f;
            if (slot.type == "special_heavy_heal") base = 90.0f;
            float amount = base * (1.0f + 0.25f * (slot.level - 1)) * slot.powerScale;
            hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + amount);
            hp->regenDelay = 0.0f;
        }
        if (spellSparkle_.texture) {
            if (const auto* tf = registry_.get<Engine::ECS::Transform>(target)) {
                const float ttl = static_cast<float>(std::max(1, spellSparkle_.frameCount)) *
                                  (spellSparkle_.frameDuration > 0.0f ? spellSparkle_.frameDuration : 0.06f);
                (void)spawnSpriteSheetVfx(spellSparkle_,
                                          tf->position,
                                          Engine::Vec2{64.0f, 64.0f},
                                          Engine::Color{255, 255, 255, 220},
                                          /*loop=*/false,
                                          /*holdOnLastFrame=*/false,
                                          /*lifetimeSeconds=*/ttl,
                                          /*ySortBias=*/0.25f);
            }
        }
        return;
    } else if (slot.type == "healer_regen_aura") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        regenAuraTimer_ = std::max(regenAuraTimer_, 6.0f + 0.4f * slot.level);
        regenAuraHps_ = std::max(regenAuraHps_, (6.0f + 1.5f * slot.level) * slot.powerScale);
        return;
    } else if (slot.type == "healer_resurrect") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        if (!multiplayerEnabled_) {
            // Solo: ultimate is passive (extra lives). Charges are granted on kit build/upgrade.
            reviveCharges_ = std::max(reviveCharges_, slot.level);
            return;
        }
        // Multiplayer: host authoritative revive under cursor.
        if (netSession_ && !netSession_->isHost()) {
            return;
        }
        Engine::ECS::Entity target = targetHeroUnderCursor(90.0f);
        if (target == Engine::ECS::kInvalidEntity) return;
        if (auto* hp = registry_.get<Engine::ECS::Health>(target)) {
            if (hp->alive()) return;
            hp->currentHealth = std::max(1.0f, hp->maxHealth * 0.75f);
            hp->currentShields = hp->maxShields;
            hp->regenDelay = 0.0f;
        }
        return;
    } else if (slot.type == "assassin_cloak") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
            st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Cloaking), hero_);
        }
        return;
    } else if (slot.type == "assassin_backstab") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        // Massive single-target melee hit.
        damageNearestEnemy(110.0f, projectileDamage_ * meleeConfig_.damageMultiplier * (4.5f + 0.5f * slot.level) * slot.powerScale * zoneDmgMul,
                           "assassin_backstab");
        return;
    } else if (slot.type == "assassin_escape") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        // Purge CC-like statuses and enable collision phasing for a short time.
        phaseTimer_ = std::max(phaseTimer_, 2.0f + 0.25f * slot.level);
        if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
            auto hasTag = [](const Engine::Status::StatusSpec& spec, Engine::Status::EStatusTag tag) {
                for (auto t : spec.tags) {
                    if (t == tag) return true;
                }
                return false;
            };
            st->container.purgeIf([&](const Engine::Status::StatusInstance& si) {
                return hasTag(si.spec, Engine::Status::EStatusTag::CrowdControl) ||
                       hasTag(si.spec, Engine::Status::EStatusTag::MovementLock) ||
                       hasTag(si.spec, Engine::Status::EStatusTag::CastingLock) ||
                       hasTag(si.spec, Engine::Status::EStatusTag::SpeedImpair);
            });
        }
        return;
    } else if (slot.type == "assassin_shadow_dance") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        if (multiplayerEnabled_ && netSession_ && !netSession_->isHost()) {
            return;
        }
        // Teleport to each target and execute sequentially; return to cast position when done.
        shadowDanceActive_ = true;
        shadowDanceReturnPos_ = heroTf->position;
        startShadowDanceSmoke(shadowDanceReturnPos_);
        shadowDanceTargets_.clear();
        shadowDanceIndex_ = 0;
        shadowDanceStepTimer_ = 0.0f;  // execute immediately next update
        shadowDanceStepInterval_ = 0.10f;

        const int maxTargets = 3 + slot.level;
        struct Cand { Engine::ECS::Entity e; float d2; };
        std::vector<Cand> cands;
        cands.reserve(64);
        const float r2 = 620.0f * 620.0f;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 <= r2) {
                    cands.push_back(Cand{e, d2});
                }
            });
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
        for (const auto& c : cands) {
            if (static_cast<int>(shadowDanceTargets_.size()) >= maxTargets) break;
            shadowDanceTargets_.push_back(c.e);
        }
        // Brief invulnerability while dancing.
        immortalTimer_ = std::max(immortalTimer_, 0.35f * static_cast<float>(std::max(1, static_cast<int>(shadowDanceTargets_.size()))));
        return;
    } else if (slot.type == "support_flurry") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        Engine::Vec2 dir = aimDirFromCursor();
        std::uniform_real_distribution<float> spread(-0.14f, 0.14f);
        for (int i = 0; i < 8 + slot.level * 2; ++i) {
            float ang = std::atan2(dir.y, dir.x) + spread(rng_);
            Engine::Vec2 d{std::cos(ang), std::sin(ang)};
            Engine::Gameplay::DamageEvent dmg{};
            dmg.baseDamage = projectileDamage_ * 0.65f * slot.powerScale * zoneDmgMul;
            dmg.type = Engine::Gameplay::DamageType::Normal;
            spawnProjectile(d, projectileSpeed_ * 0.95f, dmg, 0.85f);
        }
        return;
    } else if (slot.type == "support_whirlwind") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        int count = 10 + slot.level * 2;
        for (int i = 0; i < count; ++i) {
            float ang = (6.28318f * static_cast<float>(i)) / static_cast<float>(count);
            Engine::Vec2 d{std::cos(ang), std::sin(ang)};
            Engine::Gameplay::DamageEvent dmg{};
            dmg.baseDamage = projectileDamage_ * 0.55f * slot.powerScale * zoneDmgMul;
            dmg.type = Engine::Gameplay::DamageType::Normal;
            spawnProjectile(d, projectileSpeed_ * 0.85f, dmg, 0.8f);
        }
        return;
    } else if (slot.type == "support_extend") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        supportExtendTimer_ = std::max(supportExtendTimer_, 6.0f + 0.4f * slot.level);
        supportExtendBonus_ = 12.0f;
        return;
    } else if (slot.type == "support_diamond") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        supportDiamondCharges_ = std::max(supportDiamondCharges_, 3 + slot.level);
        return;
    } else if (slot.type == "special_thrust") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        Engine::Vec2 dir = aimDirFromCursor();

        // Short dash toward cursor (ability-driven).
        dashDir_ = dir;
        dashTimer_ = std::max(dashTimer_, 0.16f + 0.01f * slot.level);
        dashInvulnTimer_ = std::max(dashInvulnTimer_, dashTimer_ * 0.35f);
        dashTrailRedTimer_ = std::max(dashTrailRedTimer_, 0.30f);
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {dashDir_.x * heroMoveSpeed_ * moveSpeedBuffMul_ * dashSpeedMul_,
                          dashDir_.y * heroMoveSpeed_ * moveSpeedBuffMul_ * dashSpeedMul_};
        }
        if (dashInvulnTimer_ > 0.0f) {
            if (auto* inv = registry_.get<Game::Invulnerable>(hero_)) inv->timer = std::max(inv->timer, dashInvulnTimer_);
            else registry_.emplace<Game::Invulnerable>(hero_, Game::Invulnerable{dashInvulnTimer_});
        }

        // Force facing from cursor for mirrored sheets.
        if (auto* face = registry_.get<Game::Facing>(hero_)) {
            face->dirX = (dir.x < 0.0f) ? -1 : 1;
        }

        if (auto thrustTex = loadTextureOptional("assets/Sprites/Characters/Special/Special_Combat_Thrust_with_AttackEffect.png")) {
            const int frameW = 24;
            const int frameH = 24;
            const int frames = std::max(1, thrustTex->width() / frameW);
            const float frameDur = 0.08f;
            const float dur = std::max(0.05f, static_cast<float>(frames) * frameDur);
            if (auto* atk = registry_.get<Game::HeroAttackAnim>(hero_)) *atk = Game::HeroAttackAnim{dur, dir, 0};
            else registry_.emplace<Game::HeroAttackAnim>(hero_, Game::HeroAttackAnim{dur, dir, 0});
            if (registry_.has<Game::HeroAttackVisualOverride>(hero_)) registry_.remove<Game::HeroAttackVisualOverride>(hero_);
            // 4-row sheet: row0 = right (left mirrored), row1 = left (unused), row2 = front, row3 = back.
            registry_.emplace<Game::HeroAttackVisualOverride>(
                hero_, Game::HeroAttackVisualOverride{thrustTex, frameW, frameH, frameDur, /*allowFlipX=*/true,
                                                      /*rowRight=*/0, /*rowLeft=*/0, /*rowFront=*/2, /*rowBack=*/3, /*mirrorLeft=*/true});
        }

        // Thrust damage in a narrow arc.
        float dmg = projectileDamage_ * meleeConfig_.damageMultiplier * (1.9f + 0.1f * slot.level) * slot.powerScale * zoneDmgMul;
        float range = 110.0f;
        float r2 = range * range;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 > r2) return;
                float inv = 1.0f / std::max(0.0001f, std::sqrt(d2));
                Engine::Vec2 to{dx * inv, dy * inv};
                float dot = dir.x * to.x + dir.y * to.y;
                if (dot < 0.55f) return;
                Engine::Gameplay::DamageEvent de{};
                de.type = Engine::Gameplay::DamageType::Normal;
                de.baseDamage = dmg;
                Engine::Gameplay::BuffState buff{};
                if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                    if (st->container.isStasis()) return;
                    float armorDelta = st->container.armorDeltaTotal();
                    buff.healthArmorBonus += armorDelta;
                    buff.shieldArmorBonus += armorDelta;
                    buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                }
                (void)Game::RpgDamage::apply(registry_, hero_, e, hp, de, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                             "special_thrust", [this](const std::string& line) { pushCombatDebugLine(line); });
            });
        return;
    } else if (slot.type == "special_holy_sacrifice") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        lifestealBuff_ = std::max(lifestealBuff_, 0.12f + 0.02f * slot.level);
        lifestealPercent_ = globalModifiers_.playerLifestealAdd + lifestealBuff_;
        lifestealTimer_ = std::max(lifestealTimer_, 8.0);
        return;
    } else if (slot.type == "special_consecration") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        consecrationTimer_ = std::max(consecrationTimer_, 6.0f + 0.4f * slot.level);
        return;
    } else if (slot.type == "druid_bear_primal_rage") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        shieldOverchargeActive_ = true;
        shieldOverchargeTimer_ = std::max(shieldOverchargeTimer_, 6.0f + 0.4f * slot.level);
        shieldOverchargeBonusMax_ = std::max(shieldOverchargeBonusMax_, 120.0f + 15.0f * slot.level);
        shieldOverchargeRegenMul_ = std::max(shieldOverchargeRegenMul_, 1.5f);
        rageDamageBuff_ = std::max(rageDamageBuff_, 1.25f + 0.05f * slot.level);
        rageTimer_ = std::max(rageTimer_, 6.0f + 0.4f * slot.level);
        return;
    } else if (slot.type == "druid_bear_iron_fur") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        shieldOverchargeActive_ = true;
        shieldOverchargeTimer_ = std::max(shieldOverchargeTimer_, 7.0f + 0.4f * slot.level);
        shieldOverchargeBonusMax_ = std::max(shieldOverchargeBonusMax_, 160.0f + 15.0f * slot.level);
        shieldOverchargeRegenMul_ = std::max(shieldOverchargeRegenMul_, 3.0f);
        return;
    } else if (slot.type == "druid_bear_taunt") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        applyTauntPulse(hero_, 520.0f, 3.8f + 0.3f * slot.level);
        return;
    } else if (slot.type == "druid_wolf_frenzy") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        rageRateBuff_ = std::max(rageRateBuff_, 1.35f + 0.05f * slot.level);
        rageTimer_ = std::max(rageTimer_, 6.0f + 0.4f * slot.level);
        return;
    } else if (slot.type == "druid_wolf_roar") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        // Apply Feared to nearby enemies.
        float r2 = 360.0f * 360.0f;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 > r2) return;
                if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                    st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Feared), hero_);
                }
            });
        return;
    } else if (slot.type == "druid_wolf_agility") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        moveSpeedBuffMul_ = std::max(moveSpeedBuffMul_, 2.0f);
        moveSpeedBuffTimer_ = std::max(moveSpeedBuffTimer_, 4.0f + 0.35f * slot.level);
        return;
    } else if (slot.type == "druid_return_human") {
        spendEnergy();
        setCooldown(slot.cooldownMax);
        applyDruidForm(DruidForm::Human);
        buildAbilities(/*resetState=*/false);
        return;
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
        bool ok1 = spawnSummoned("beatle");
        bool ok2 = spawnSummoned("beatle");
        if (ok1 || ok2) setCooldown(std::max(1.5f, slot.cooldownMax));
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
        setCooldown(0.2f);
    } else if (slot.type == "druid_select_wolf") {
        if (level_ < 2) return;
        druidChosen_ = DruidForm::Wolf;
        druidChoiceMade_ = true;
        setCooldown(0.2f);
    } else if (slot.type == "druid_toggle") {
        if (!druidChoiceMade_) return;
        spendEnergy();
        if (druidForm_ == DruidForm::Human) {
            applyDruidForm(druidChosen_);
        } else {
            applyDruidForm(DruidForm::Human);
        }
        buildAbilities(/*resetState=*/false);
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
        Engine::TexturePtr tex = largeFireTex_ ? largeFireTex_ : loadTextureOptional("assets/Sprites/Spells/Large_Fire_Anti-Alias_glow_28x28.png");
        for (int i = 0; i < segments; ++i) {
            Engine::Vec2 pos{start.x + dir.x * spacing * (i + 1), start.y + dir.y * spacing * (i + 1)};
            Engine::ECS::Entity vis = registry_.create();
            registry_.emplace<Engine::ECS::Transform>(vis, pos);
            registry_.emplace<Engine::ECS::Renderable>(vis,
                Engine::ECS::Renderable{Engine::Vec2{28.0f, 28.0f}, Engine::Color{255, 160, 90, 200}, tex});
            // Animate 0..11 across 3 rows (4 columns each).
            Engine::ECS::SpriteAnimation anim{};
            anim.frameWidth = 28;
            anim.frameHeight = 28;
            anim.frameCount = 4;
            anim.frameDuration = 0.06f;
            anim.row = 0;
            anim.loop = true;
            registry_.emplace<Engine::ECS::SpriteAnimation>(vis, anim);
            registry_.emplace<Game::MultiRowSpriteAnim>(vis, Game::MultiRowSpriteAnim{4, 12, 0.06f, true, false});
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
        // Visual strike: animate lightning blast segments from caster outward.
        if (lightningBlastTex_) {
            const int segCount = 7;
            const float segSpacing = 54.0f;
            const float baseX = heroTf->position.x;
            const float y = heroTf->position.y;
            for (int i = 0; i < segCount; ++i) {
                float x = baseX + dirSign * segSpacing * (i + 1);
                auto fx = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(fx, Engine::Vec2{x, y});
                registry_.emplace<Engine::ECS::Renderable>(fx,
                    Engine::ECS::Renderable{Engine::Vec2{108.0f, 36.0f}, Engine::Color{255, 255, 255, 210}, lightningBlastTex_});
                Engine::ECS::SpriteAnimation anim{};
                anim.frameWidth = 54;
                anim.frameHeight = 18;
                anim.frameCount = 9;
                anim.frameDuration = 0.035f;
                anim.loop = false;
                registry_.emplace<Engine::ECS::SpriteAnimation>(fx, anim);
                registry_.emplace<Engine::ECS::Projectile>(fx, Engine::ECS::Projectile{Engine::Vec2{0.0f, 0.0f}, {}, 9 * 0.035f});
                registry_.emplace<Game::Facing>(fx, Game::Facing{dirSign >= 0.0f ? 1 : -1});
            }
        }
        setCooldown(std::max(6.0f, slot.cooldownMax));
    } else if (slot.type == "wizard_ldome") {
        spendEnergy();
        lightningDomeTimer_ = std::max(lightningDomeTimer_, 5.0f + 0.5f * wizardStage());
        immortalTimer_ = std::max(immortalTimer_, lightningDomeTimer_);
        // Spawn/update the dome visual entity (follows hero in Game.cpp while timer > 0).
        if (lightningEnergyTex_) {
            if (lightningDomeVis_ == Engine::ECS::kInvalidEntity) {
                auto fx = registry_.create();
                lightningDomeVis_ = fx;
                registry_.emplace<Engine::ECS::Transform>(fx, heroTf->position);
                registry_.emplace<Engine::ECS::Renderable>(fx,
                    Engine::ECS::Renderable{Engine::Vec2{240.0f, 240.0f}, Engine::Color{255, 255, 255, 160}, lightningEnergyTex_});
                Engine::ECS::SpriteAnimation anim{};
                anim.frameWidth = 48;
                anim.frameHeight = 48;
                anim.frameCount = 9;
                anim.frameDuration = 0.05f;
                anim.row = 0;
                anim.loop = true;
                registry_.emplace<Engine::ECS::SpriteAnimation>(fx, anim);
                Game::MultiRowSpriteAnim mr{};
                mr.columns = 9;
                mr.totalFrames = 9;
                mr.frameDuration = 0.05f;
                mr.loop = true;
                mr.holdFrame = 4;            // linger on 5th frame
                mr.holdExtraSeconds = 0.18f; // a few frames of extra hold
                registry_.emplace<Game::MultiRowSpriteAnim>(fx, mr);
            }
        }
        setCooldown(std::max(10.0f, slot.cooldownMax));
    } else {
        // Generic placeholder
        spendEnergy();
        setCooldown(slot.cooldownMax);
    }
}

}  // namespace Game
