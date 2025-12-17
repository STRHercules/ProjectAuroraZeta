#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>

#include "../../engine/ecs/components/AABB.h"
#include "../../engine/ecs/components/Health.h"
#include "../../engine/ecs/components/Projectile.h"
#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Tags.h"
#include "../../engine/ecs/components/Renderable.h"
#include "../../engine/ecs/components/Velocity.h"
#include "../../engine/ecs/components/SpriteAnimation.h"
#include "../../engine/gameplay/Combat.h"
#include "BuffSystem.h"
#include "RpgDamage.h"
#include "../components/HitFlash.h"
#include "../components/DamageNumber.h"
#include "../components/Invulnerable.h"
#include "../components/OffensiveType.h"
#include "../components/EnemyAttackSwing.h"
#include "../components/StatusEffects.h"
#include "../components/SpellEffect.h"
#include "../components/AreaDamage.h"
#include "../components/MiniUnit.h"
#include "../components/Building.h"
#include "../components/EscortTarget.h"
#include "../components/EscortPreMove.h"
#include "../../engine/ecs/components/Status.h"

namespace {
bool aabbOverlap(const Engine::ECS::Transform& ta, const Engine::ECS::AABB& aa, const Engine::ECS::Transform& tb,
                 const Engine::ECS::AABB& ab) {
    return std::abs(ta.position.x - tb.position.x) <= (aa.halfExtents.x + ab.halfExtents.x) &&
           std::abs(ta.position.y - tb.position.y) <= (aa.halfExtents.y + ab.halfExtents.y);
}

int elementToRpgDamageType(Game::ElementType e) {
    using DT = Engine::Gameplay::RPG::DamageType;
    switch (e) {
        case Game::ElementType::Fire: return static_cast<int>(DT::Fire);
        case Game::ElementType::Ice: return static_cast<int>(DT::Frost);
        case Game::ElementType::Lightning: return static_cast<int>(DT::Shock);
        case Game::ElementType::Earth: return static_cast<int>(DT::Poison);
        case Game::ElementType::Dark: return static_cast<int>(DT::Arcane);
        case Game::ElementType::Wind: return static_cast<int>(DT::Physical);
        default: return -1;
    }
}
}  // namespace

namespace Game {

void CollisionSystem::update(Engine::ECS::Registry& registry) {
    std::vector<Engine::ECS::Entity> deadProjectiles;

    // Projectile hits enemy.
    registry.view<Engine::ECS::Transform, Engine::ECS::Projectile, Engine::ECS::AABB, Engine::ECS::ProjectileTag>(
        [&](Engine::ECS::Entity projEnt, Engine::ECS::Transform& projTf, Engine::ECS::Projectile& proj,
            Engine::ECS::AABB& projBox, Engine::ECS::ProjectileTag&) {
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity targetEnt, Engine::ECS::Transform& tgtTf, Engine::ECS::Health& health,
                    Engine::ECS::AABB& tgtBox, Engine::ECS::EnemyTag&) {
                    if (!health.alive()) return;
                    if (aabbOverlap(projTf, projBox, tgtTf, tgtBox)) {
                        const float preHealth = health.currentHealth;
                        const float preShields = health.currentShields;

                        Engine::Gameplay::DamageEvent hit = proj.damage;
                        if (const auto* eff = registry.get<Game::SpellEffect>(projEnt)) {
                            hit.rpgDamageType = elementToRpgDamageType(eff->element);
                        }

                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(targetEnt)) {
                            buff = armorBuff->state;
                        }
                        bool stasis = false;
                        if (auto* st = registry.get<Engine::ECS::Status>(targetEnt)) {
                            stasis = st->container.isStasis();
                            float armorDelta = st->container.armorDeltaTotal();
                            buff.healthArmorBonus += armorDelta;
                            buff.shieldArmorBonus += armorDelta;
                            buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                        }
                        if (!stasis) {
                            Engine::ECS::Entity attacker = (proj.owner != Engine::ECS::kInvalidEntity) ? proj.owner : hero_;
                            (void)Game::RpgDamage::apply(registry, attacker, targetEnt, health, hit, buff,
                                                         useRpgCombat_, rpgConfig_, rng_, "proj", debugSink_);
                        }

                        // Elemental spell effects (Wizard)
                        if (registry.has<Game::SpellEffect>(projEnt)) {
                            const auto* eff = registry.get<Game::SpellEffect>(projEnt);
                            auto* status = registry.get<Game::StatusEffects>(targetEnt);
                            if (!status) {
                                registry.emplace<Game::StatusEffects>(targetEnt, Game::StatusEffects{});
                                status = registry.get<Game::StatusEffects>(targetEnt);
                            }
                            const int stage = std::max(1, eff->stage);
                            switch (eff->element) {
                                case Game::ElementType::Ice: {
                                    status->slowMultiplier = std::min(status->slowMultiplier, 1.0f - 0.12f * stage);
                                    status->slowTimer = std::max(status->slowTimer, 1.2f + 0.4f * stage);
                                    break;
                                }
                                case Game::ElementType::Fire: {
                                    status->burnTimer = std::max(status->burnTimer, 2.0f + stage * 0.8f);
                                    status->burnDps = std::max(status->burnDps, proj.damage.baseDamage * (0.18f + 0.04f * stage));
                                    break;
                                }
                                case Game::ElementType::Dark: {
                                    status->blindTimer = std::max(status->blindTimer, 1.0f + 0.5f * stage);
                                    status->blindRetarget = 0.0f;
                                    status->blindDir = {0.0f, 0.0f};
                                    break;
                                }
                                case Game::ElementType::Earth: {
                                    status->earthTimer = std::max(status->earthTimer, 2.2f + 0.4f * stage);
                                    status->earthDps = std::max(status->earthDps, proj.damage.baseDamage * (0.12f + 0.05f * stage));
                                    // Minor AoE thorn splash.
                                    float radius2 = 90.0f * 90.0f;
                                    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                                        [&](Engine::ECS::Entity e2, Engine::ECS::Transform& tf2, Engine::ECS::Health& hp2, Engine::ECS::EnemyTag&) {
                                            if (e2 == targetEnt || !hp2.alive()) return;
                                            float dx = tf2.position.x - tgtTf.position.x;
                                            float dy = tf2.position.y - tgtTf.position.y;
                                            float d2 = dx * dx + dy * dy;
                                            if (d2 <= radius2) {
                                                Engine::Gameplay::DamageEvent thorn{};
                                                thorn.type = Engine::Gameplay::DamageType::Spell;
                                                thorn.baseDamage = proj.damage.baseDamage * 0.35f;
                                                Engine::Gameplay::BuffState thornBuff{};
                                                if (auto* st2 = registry.get<Engine::ECS::Status>(e2)) {
                                                    if (st2->container.isStasis()) return;
                                                    float armorDelta = st2->container.armorDeltaTotal();
                                                    thornBuff.healthArmorBonus += armorDelta;
                                                    thornBuff.shieldArmorBonus += armorDelta;
                                                    thornBuff.damageTakenMultiplier *= st2->container.damageTakenMultiplier();
                                                }
                                                Engine::ECS::Entity attacker = (proj.owner != Engine::ECS::kInvalidEntity) ? proj.owner : hero_;
                                                (void)Game::RpgDamage::apply(registry, attacker, e2, hp2, thorn, thornBuff,
                                                                             useRpgCombat_, rpgConfig_, rng_, "thorn_splash", debugSink_);
                                            }
                                        });
                                    break;
                                }
                                case Game::ElementType::Lightning: {
                                    status->stunTimer = std::max(status->stunTimer, 0.5f + 0.15f * stage);
                                    break;
                                }
                                case Game::ElementType::Wind: {
                                    if (auto* vel = registry.get<Engine::ECS::Velocity>(targetEnt)) {
                                        Engine::Vec2 push{tgtTf.position.x - projTf.position.x, tgtTf.position.y - projTf.position.y};
                                        float len2 = push.x * push.x + push.y * push.y;
                                        float invLen = len2 > 0.0001f ? 1.0f / std::sqrt(len2) : 0.0f;
                                        push = {push.x * invLen * (180.0f + 30.0f * stage), push.y * invLen * (180.0f + 30.0f * stage)};
                                        vel->value = {vel->value.x + push.x, vel->value.y + push.y};
                                    }
                                    break;
                                }
                            }
                        }
                        // AoE damage on impact
                        if (registry.has<Game::AreaDamage>(projEnt)) {
                            const auto* aoe = registry.get<Game::AreaDamage>(projEnt);
                            float radius2 = aoe->radius * aoe->radius;
                            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                                [&](Engine::ECS::Entity e2, Engine::ECS::Transform& tf2, Engine::ECS::Health& hp2, Engine::ECS::EnemyTag&) {
                                    if (e2 == targetEnt || !hp2.alive()) return;
                                    float dx = tf2.position.x - tgtTf.position.x;
                                    float dy = tf2.position.y - tgtTf.position.y;
                                    float d2 = dx * dx + dy * dy;
                                    if (d2 <= radius2) {
                                        Engine::Gameplay::DamageEvent splash = proj.damage;
                                        splash.baseDamage *= aoe->damageMultiplier;
                                        if (const auto* eff = registry.get<Game::SpellEffect>(projEnt)) {
                                            splash.rpgDamageType = elementToRpgDamageType(eff->element);
                                        }
                                        Engine::Gameplay::BuffState splashBuff{};
                                        if (auto* st2 = registry.get<Engine::ECS::Status>(e2)) {
                                            if (st2->container.isStasis()) return;
                                            float armorDelta = st2->container.armorDeltaTotal();
                                            splashBuff.healthArmorBonus += armorDelta;
                                            splashBuff.shieldArmorBonus += armorDelta;
                                            splashBuff.damageTakenMultiplier *= st2->container.damageTakenMultiplier();
                                        }
                                        Engine::ECS::Entity attacker = (proj.owner != Engine::ECS::kInvalidEntity) ? proj.owner : hero_;
                                        (void)Game::RpgDamage::apply(registry, attacker, e2, hp2, splash, splashBuff,
                                                                     useRpgCombat_, rpgConfig_, rng_, "aoe", debugSink_);
                                    }
                                });
                        }

                        const float postHealth = health.currentHealth;
                        const float postShields = health.currentShields;
                        float dealt = (preHealth + preShields) - (postHealth + postShields);
                        if (dealt > 0.0f && dealt < Engine::Gameplay::MIN_DAMAGE_PER_HIT) {
                            dealt = Engine::Gameplay::MIN_DAMAGE_PER_HIT;
                        }
                        deadProjectiles.push_back(projEnt);
                        // Lifesteal to hero if applicable.
                        if (proj.lifesteal > 0.0f) {
                            registry.view<Engine::ECS::Health, Engine::ECS::HeroTag>(
                                [&](Engine::ECS::Entity, Engine::ECS::Health& heroHp, Engine::ECS::HeroTag&) {
                                    float heal = dealt * proj.lifesteal;
                                    heroHp.currentHealth = std::min(heroHp.maxHealth, heroHp.currentHealth + heal);
                                });
                        }
                        // Grant XP for damage dealt.
                        if (xpPtr_ && dealt > 0.0f) {
                            *xpPtr_ += static_cast<int>(std::round(dealt * xpPerDamageDealt_));
                        }
                        // Chain bounce.
                        if (proj.chain > 0) {
                            Engine::ECS::Entity best = Engine::ECS::kInvalidEntity;
                            Engine::Vec2 bestPos{};
                            float bestDist2 = 90000.0f;  // 300 radius squared
                            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                                [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, const Engine::ECS::EnemyTag&) {
                                    if (e == targetEnt || !hp.alive()) return;
                                    float dx = tf.position.x - tgtTf.position.x;
                                    float dy = tf.position.y - tgtTf.position.y;
                                    float d2 = dx * dx + dy * dy;
                                    if (d2 < bestDist2) {
                                        bestDist2 = d2;
                                        bestPos = tf.position;
                                        best = e;
                                    }
                                });
                            if (best != Engine::ECS::kInvalidEntity) {
                                Engine::Vec2 dir{bestPos.x - tgtTf.position.x, bestPos.y - tgtTf.position.y};
                                float len2 = dir.x * dir.x + dir.y * dir.y;
                                float speed = std::sqrt(proj.velocity.x * proj.velocity.x + proj.velocity.y * proj.velocity.y);
                                if (len2 > 0.0001f && speed > 0.0f) {
                                    float inv = 1.0f / std::sqrt(len2);
                                    dir.x *= inv;
                                    dir.y *= inv;
                                    auto np = registry.create();
                                    registry.emplace<Engine::ECS::Transform>(np, tgtTf.position);
                                    registry.emplace<Engine::ECS::Velocity>(np, Engine::Vec2{0.0f, 0.0f});
                                    registry.emplace<Engine::ECS::AABB>(np, projBox);
                                    registry.emplace<Engine::ECS::Renderable>(np,
                                        Engine::ECS::Renderable{Engine::Vec2{projBox.halfExtents.x * 2.0f, projBox.halfExtents.y * 2.0f},
                                                                Engine::Color{255, 230, 140, 220}});
                                    Engine::Gameplay::DamageEvent bounceDmg = proj.damage;
                                    bounceDmg.baseDamage *= 0.8f;
                                    registry.emplace<Engine::ECS::Projectile>(np,
                                        Engine::ECS::Projectile{Engine::Vec2{dir.x * speed, dir.y * speed},
                                                                bounceDmg,
                                                                proj.lifetime,
                                                                proj.lifesteal, proj.chain - 1, proj.owner});
                                    registry.emplace<Engine::ECS::ProjectileTag>(np, Engine::ECS::ProjectileTag{});
                                }
                            }
                        }
                        // Kick a brief hit flash for feedback on the enemy.
                        if (auto* flash = registry.get<Game::HitFlash>(targetEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(targetEnt, Game::HitFlash{0.12f});
                        }
                        // Spawn a floating damage number.
                        auto dn = registry.create();
                        registry.emplace<Engine::ECS::Transform>(dn, tgtTf.position);
                        registry.emplace<Game::DamageNumber>(dn, Game::DamageNumber{dealt, 0.8f, {0.0f, -30.0f}});
                    }
                });
        });

    // Enemy contact damages hero.
    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity enemyEnt, Engine::ECS::Transform& enemyTf, Engine::ECS::Health& enemyHp,
            Engine::ECS::EnemyTag& /*tag*/, Engine::ECS::AABB& enemyBox) {
            if (!enemyHp.alive()) return;
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Engine::ECS::HeroTag>(
                [&](Engine::ECS::Entity heroEnt, Engine::ECS::Transform& heroTf, Engine::ECS::Health& heroHp,
                    Engine::ECS::AABB& heroBox, Engine::ECS::HeroTag&) {
                    if (!heroHp.alive()) return;
                    if (aabbOverlap(enemyTf, enemyBox, heroTf, heroBox)) {
                        // Mark enemy as swinging for visuals even if damage is blocked.
                        Engine::Vec2 dir{heroTf.position.x - enemyTf.position.x, heroTf.position.y - enemyTf.position.y};
                        float swingDur = 0.35f;
                        if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(enemyEnt)) {
                            float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                            swingDur = static_cast<float>(anim->frameCount) * fd;
                        }
                        if (auto* swing = registry.get<Game::EnemyAttackSwing>(enemyEnt)) {
                            swing->timer = swingDur;
                            swing->targetDir = dir;
                        } else {
                            registry.emplace<Game::EnemyAttackSwing>(enemyEnt, Game::EnemyAttackSwing{swingDur, dir});
                        }

                        if (auto* inv = registry.get<Game::Invulnerable>(heroEnt)) {
                            if (inv->timer > 0.0f) return;
                        }
                        const float preHealth = heroHp.currentHealth;
                        const float preShields = heroHp.currentShields;
                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(heroEnt)) {
                            buff = armorBuff->state;
                        }
                        if (auto* stHero = registry.get<Engine::ECS::Status>(heroEnt)) {
                            if (stHero->container.isStasis()) return;
                            float armorDelta = stHero->container.armorDeltaTotal();
                            buff.healthArmorBonus += armorDelta;
                            buff.shieldArmorBonus += armorDelta;
                            buff.damageTakenMultiplier *= stHero->container.damageTakenMultiplier();
                        }
                        (void)Game::RpgDamage::apply(registry, enemyEnt, heroEnt, heroHp, contact, buff,
                                                     useRpgCombat_, rpgConfig_, rng_, "contact_hero", debugSink_);
                        float dealt = (preHealth + preShields) - (heroHp.currentHealth + heroHp.currentShields);
                        if (dealt > 0.0f) {
                            if (auto* flash = registry.get<Game::HitFlash>(heroEnt)) {
                                flash->timer = 0.12f;
                            } else {
                                registry.emplace<Game::HitFlash>(heroEnt, Game::HitFlash{0.12f});
                            }
                            if (xpPtr_ && heroEnt == hero_) {
                                *xpPtr_ += static_cast<int>(std::round(dealt * xpPerDamageTaken_));
                            }
                            bool thorny = false;
                            if (auto* ot = registry.get<Game::OffensiveTypeTag>(heroEnt)) {
                                thorny = ot->type == Game::OffensiveType::ThornTank;
                            }
                            if (thorny) {
                                if (auto* enemyHp = registry.get<Engine::ECS::Health>(enemyEnt)) {
                                    float reflect = std::min(thornMaxReflect_, dealt * thornReflectPercent_);
                                    if (reflect > 0.0f) {
                                        Engine::Gameplay::DamageEvent reflectDmg{};
                                        reflectDmg.baseDamage = reflect;
                                        reflectDmg.type = Engine::Gameplay::DamageType::Normal;
                                        Engine::Gameplay::BuffState enemyBuff{};
                                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(enemyEnt)) {
                                            enemyBuff = armorBuff->state;
                                        }
                                        if (auto* stE = registry.get<Engine::ECS::Status>(enemyEnt)) {
                                            if (stE->container.isStasis()) return;
                                            float armorDelta = stE->container.armorDeltaTotal();
                                            enemyBuff.healthArmorBonus += armorDelta;
                                            enemyBuff.shieldArmorBonus += armorDelta;
                                            enemyBuff.damageTakenMultiplier *= stE->container.damageTakenMultiplier();
                                        }
                                        (void)Game::RpgDamage::apply(registry, heroEnt, enemyEnt, *enemyHp, reflectDmg, enemyBuff,
                                                                     useRpgCombat_, rpgConfig_, rng_, "thorn_reflect", debugSink_);
                                    }
                                }
                            }
                        }
                    }
                });
    });

    // Enemy contact damages escort NPCs.
    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity enemyEnt, Engine::ECS::Transform& enemyTf, Engine::ECS::Health& enemyHp,
            Engine::ECS::EnemyTag& /*tag*/, Engine::ECS::AABB& enemyBox) {
            if (!enemyHp.alive()) return;
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Game::EscortTarget>(
                [&](Engine::ECS::Entity escortEnt, Engine::ECS::Transform& escortTf, Engine::ECS::Health& escortHp,
                    Engine::ECS::AABB& escortBox, Game::EscortTarget&) {
                    if (!escortHp.alive()) return;
                    if (registry.has<Game::EscortPreMove>(escortEnt)) return;
                    if (aabbOverlap(enemyTf, enemyBox, escortTf, escortBox)) {
                        Engine::Vec2 dir{escortTf.position.x - enemyTf.position.x, escortTf.position.y - enemyTf.position.y};
                        float swingDur = 0.35f;
                        if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(enemyEnt)) {
                            float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                            swingDur = static_cast<float>(anim->frameCount) * fd;
                        }
                        if (auto* swing = registry.get<Game::EnemyAttackSwing>(enemyEnt)) {
                            swing->timer = swingDur;
                            swing->targetDir = dir;
                        } else {
                            registry.emplace<Game::EnemyAttackSwing>(enemyEnt, Game::EnemyAttackSwing{swingDur, dir});
                        }

                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        contact.type = Engine::Gameplay::DamageType::Normal;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(escortEnt)) {
                            buff = armorBuff->state;
                        }
                        if (auto* stEscort = registry.get<Engine::ECS::Status>(escortEnt)) {
                            if (stEscort->container.isStasis()) return;
                            float armorDelta = stEscort->container.armorDeltaTotal();
                            buff.healthArmorBonus += armorDelta;
                            buff.shieldArmorBonus += armorDelta;
                            buff.damageTakenMultiplier *= stEscort->container.damageTakenMultiplier();
                        }
                        (void)Game::RpgDamage::apply(registry, enemyEnt, escortEnt, escortHp, contact, buff,
                                                     useRpgCombat_, rpgConfig_, rng_, "contact_escort", debugSink_);
                        if (auto* flash = registry.get<Game::HitFlash>(escortEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(escortEnt, Game::HitFlash{0.12f});
                        }
                    }
                });
        });

    // Enemy contact damages mini units and turrets.
    registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag, Engine::ECS::AABB>(
        [&](Engine::ECS::Entity enemyEnt, Engine::ECS::Transform& enemyTf, Engine::ECS::Health& enemyHp,
            Engine::ECS::EnemyTag& /*tag*/, Engine::ECS::AABB& enemyBox) {
            if (!enemyHp.alive()) return;
            // Mini units.
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Game::MiniUnit>(
                [&](Engine::ECS::Entity miniEnt, Engine::ECS::Transform& miniTf, Engine::ECS::Health& miniHp,
                    Engine::ECS::AABB& miniBox, Game::MiniUnit&) {
                    if (!miniHp.alive()) return;
                    if (aabbOverlap(enemyTf, enemyBox, miniTf, miniBox)) {
                        Engine::Vec2 dir{miniTf.position.x - enemyTf.position.x, miniTf.position.y - enemyTf.position.y};
                        float swingDur = 0.35f;
                        if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(enemyEnt)) {
                            float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                            swingDur = static_cast<float>(anim->frameCount) * fd;
                        }
                        if (auto* swing = registry.get<Game::EnemyAttackSwing>(enemyEnt)) {
                            swing->timer = swingDur;
                            swing->targetDir = dir;
                        } else {
                            registry.emplace<Game::EnemyAttackSwing>(enemyEnt, Game::EnemyAttackSwing{swingDur, dir});
                        }

                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        contact.type = Engine::Gameplay::DamageType::Normal;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(miniEnt)) {
                            buff = armorBuff->state;
                        }
                        if (auto* stMini = registry.get<Engine::ECS::Status>(miniEnt)) {
                            if (stMini->container.isStasis()) return;
                            float armorDelta = stMini->container.armorDeltaTotal();
                            buff.healthArmorBonus += armorDelta;
                            buff.shieldArmorBonus += armorDelta;
                            buff.damageTakenMultiplier *= stMini->container.damageTakenMultiplier();
                        }
                        (void)Game::RpgDamage::apply(registry, enemyEnt, miniEnt, miniHp, contact, buff,
                                                     useRpgCombat_, rpgConfig_, rng_, "contact_mini", debugSink_);
                        if (auto* flash = registry.get<Game::HitFlash>(miniEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(miniEnt, Game::HitFlash{0.12f});
                        }
                    }
                });
            // Turrets only.
            registry.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::AABB, Game::Building>(
                [&](Engine::ECS::Entity bEnt, Engine::ECS::Transform& bTf, Engine::ECS::Health& bHp,
                    Engine::ECS::AABB& bBox, Game::Building& b) {
                    if (b.type != Game::BuildingType::Turret) return;
                    if (!bHp.alive()) return;
                    if (aabbOverlap(enemyTf, enemyBox, bTf, bBox)) {
                        Engine::Vec2 dir{bTf.position.x - enemyTf.position.x, bTf.position.y - enemyTf.position.y};
                        float swingDur = 0.35f;
                        if (const auto* anim = registry.get<Engine::ECS::SpriteAnimation>(enemyEnt)) {
                            float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                            swingDur = static_cast<float>(anim->frameCount) * fd;
                        }
                        if (auto* swing = registry.get<Game::EnemyAttackSwing>(enemyEnt)) {
                            swing->timer = swingDur;
                            swing->targetDir = dir;
                        } else {
                            registry.emplace<Game::EnemyAttackSwing>(enemyEnt, Game::EnemyAttackSwing{swingDur, dir});
                        }

                        Engine::Gameplay::DamageEvent contact{};
                        contact.baseDamage = contactDamage_;
                        contact.type = Engine::Gameplay::DamageType::Normal;
                        Engine::Gameplay::BuffState buff{};
                        if (auto* armorBuff = registry.get<Game::ArmorBuff>(bEnt)) {
                            buff = armorBuff->state;
                        }
                        if (auto* stB = registry.get<Engine::ECS::Status>(bEnt)) {
                            if (stB->container.isStasis()) return;
                            float armorDelta = stB->container.armorDeltaTotal();
                            buff.healthArmorBonus += armorDelta;
                            buff.shieldArmorBonus += armorDelta;
                            buff.damageTakenMultiplier *= stB->container.damageTakenMultiplier();
                        }
                        (void)Game::RpgDamage::apply(registry, enemyEnt, bEnt, bHp, contact, buff,
                                                     useRpgCombat_, rpgConfig_, rng_, "contact_building", debugSink_);
                        if (auto* flash = registry.get<Game::HitFlash>(bEnt)) {
                            flash->timer = 0.12f;
                        } else {
                            registry.emplace<Game::HitFlash>(bEnt, Game::HitFlash{0.12f});
                        }
                    }
                });
        });

    for (auto e : deadProjectiles) {
        registry.destroy(e);
    }
}

}  // namespace Game
