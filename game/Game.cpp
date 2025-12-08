#include "Game.h"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <limits>

#include "../engine/core/Application.h"
#include "../engine/core/Logger.h"
#include "../engine/core/Time.h"
#include "../engine/ecs/components/Transform.h"
#include "../engine/ecs/components/Velocity.h"
#include "../engine/ecs/components/Renderable.h"
#include "../engine/ecs/components/AABB.h"
#include "../engine/ecs/components/Health.h"
#include "../engine/ecs/components/Projectile.h"
#include "../engine/ecs/components/Tags.h"
#include "../engine/input/InputState.h"
#include "../engine/render/RenderDevice.h"
#include "../engine/render/BitmapTextRenderer.h"
#include "../engine/platform/SDLRenderDevice.h"
#include <SDL_ttf.h>
#include "components/DamageNumber.h"
#include "components/Pickup.h"
#include "components/PickupBob.h"
#include "components/BountyTag.h"
#include <cmath>

namespace Game {

bool GameRoot::onInitialize(Engine::Application& app) {
    Engine::logInfo("GameRoot initialized. Spawning placeholder hero entity.");
    app_ = &app;
    render_ = &app.renderer();
    if (auto* sdl = dynamic_cast<Engine::SDLRenderDevice*>(render_)) {
        sdlRenderer_ = sdl->rawRenderer();
    }
    if (sdlRenderer_) {
        if (TTF_Init() != 0) {
            Engine::logWarn(std::string("TTF_Init failed: ") + TTF_GetError());
        } else {
            constexpr int kUIFontSize = 28;
            uiFont_ = TTF_OpenFont("src/TinyUnicode.ttf", kUIFontSize);
            if (!uiFont_) {
                Engine::logWarn(std::string("TTF_OpenFont failed: ") + TTF_GetError());
            } else {
                Engine::logInfo("Loaded TTF font src/TinyUnicode.ttf");
            }
        }
    } else {
        Engine::logWarn("SDL renderer missing; TTF text disabled.");
    }
    viewportWidth_ = app.config().width;
    viewportHeight_ = app.config().height;
    renderSystem_ = std::make_unique<RenderSystem>(*render_);
    movementSystem_ = std::make_unique<MovementSystem>();
    cameraSystem_ = std::make_unique<CameraSystem>();
    projectileSystem_ = std::make_unique<ProjectileSystem>();
    collisionSystem_ = std::make_unique<CollisionSystem>();
    enemyAISystem_ = std::make_unique<EnemyAISystem>();
    hitFlashSystem_ = std::make_unique<HitFlashSystem>();
    damageNumberSystem_ = std::make_unique<DamageNumberSystem>();
    shopSystem_ = std::make_unique<ShopSystem>();
    pickupSystem_ = std::make_unique<PickupSystem>();
    eventSystem_ = std::make_unique<EventSystem>();
    // Wave settings loaded later from gameplay config.
    textureManager_ = std::make_unique<Engine::TextureManager>(*render_);

    // Load bindings from data file; fallback to defaults.
    auto loaded = Engine::InputLoader::loadFromFile("data/input_bindings.json");
    if (loaded) {
        bindings_ = *loaded;
        Engine::logInfo("Loaded input bindings from data/input_bindings.json");
    } else {
        Engine::logWarn("Failed to load input bindings; using defaults.");
        bindings_.move.positiveX = {"d"};
        bindings_.move.negativeX = {"a"};
        bindings_.move.positiveY = {"s"};
        bindings_.move.negativeY = {"w"};
        bindings_.camera.positiveX = {"right"};
        bindings_.camera.negativeX = {"left"};
        bindings_.camera.positiveY = {"down"};
        bindings_.camera.negativeY = {"up"};
        bindings_.toggleFollow = {"c"};
        bindings_.restart = {"r"};
    }
    actionMapper_ = Engine::ActionMapper(bindings_);

    // Load asset manifest (optional).
    auto manifestLoaded = Engine::AssetManifestLoader::load("data/assets.json");
    if (manifestLoaded) {
        manifest_ = *manifestLoaded;
        Engine::logInfo("Loaded asset manifest from data/assets.json");
    } else {
        Engine::logWarn("Failed to load asset manifest; using defaults.");
        manifest_.heroTexture = "assets/hero.bmp";
    }

    // Load gameplay tunables.
    WaveSettings waveSettings{};
    float fireRate = 5.0f;
    float projectileSpeed = 400.0f;
    float projectileDamage = 15.0f;
    float contactDamage = 10.0f;
    float heroMoveSpeed = heroMoveSpeed_;
    float heroHp = heroMaxHp_;
    float heroSize = heroSize_;
    float projectileSize = projectileSize_;
    float projectileHitbox = projectileHitboxSize_;
    float projectileLifetime = projectileLifetime_;
    double initialWarmup = waveWarmup_;
    double waveInterval = waveInterval_;
    double graceDuration = graceDuration_;
    int currencyPerKill = 5;
    int waveClearBonus = waveClearBonus_;
    int enemyLowThreshold = enemyLowThreshold_;
    double combatDuration = combatDuration_;
    double intermissionDuration = intermissionDuration_;
    int bountyBonus = bountyBonus_;
    int bossWave = bossWave_;
    float bossHpMul = bossHpMultiplier_;
    float bossSpeedMul = bossSpeedMultiplier_;
    int bossKillBonus = bossKillBonus_;
    int shopDamageCost = shopDamageCost_;
    int shopHpCost = shopHpCost_;
    int shopSpeedCost = shopSpeedCost_;
    float shopDamageBonus = shopDamageBonus_;
    float shopHpBonus = shopHpBonus_;
    float shopSpeedBonus = shopSpeedBonus_;
    int xpPerKill = xpPerKill_;
    int xpPerWave = xpPerWave_;
    int xpBase = xpToNext_;
    float xpGrowth = xpGrowth_;
    float levelHpBonus = levelHpBonus_;
    float levelDmgBonus = levelDmgBonus_;
    float levelSpeedBonus = levelSpeedBonus_;
    {
        std::ifstream gp("data/gameplay.json");
        if (gp.is_open()) {
            nlohmann::json j;
            gp >> j;
            if (j.contains("enemy")) {
                waveSettings.enemyHp = j["enemy"].value("hp", waveSettings.enemyHp);
                waveSettings.enemySpeed = j["enemy"].value("speed", waveSettings.enemySpeed);
                contactDamage = j["enemy"].value("contactDamage", contactDamage);
                waveSettings.enemySize = j["enemy"].value("size", waveSettings.enemySize);
                waveSettings.enemyHitbox = j["enemy"].value("hitboxSize", waveSettings.enemyHitbox);
            }
            if (j.contains("projectile")) {
                projectileSpeed = j["projectile"].value("speed", projectileSpeed);
                projectileDamage = j["projectile"].value("damage", projectileDamage);
                fireRate = j["projectile"].value("fireRate", fireRate);
                projectileSize = j["projectile"].value("size", projectileSize);
                projectileHitbox = j["projectile"].value("hitboxSize", projectileHitbox);
                projectileLifetime = j["projectile"].value("lifetime", projectileLifetime);
            }
            if (j.contains("hero")) {
                heroHp = j["hero"].value("hp", heroHp);
                heroMoveSpeed = j["hero"].value("moveSpeed", heroMoveSpeed);
                heroSize = j["hero"].value("size", heroSize);
            }
            if (j.contains("spawner")) {
                waveSettings.interval = j["spawner"].value("interval", waveSettings.interval);
                waveSettings.batchSize = j["spawner"].value("batchSize", waveSettings.batchSize);
                initialWarmup = j["spawner"].value("initialDelay", initialWarmup);
                waveSettings.grace = j["spawner"].value("grace", waveSettings.grace);
                waveInterval = waveSettings.interval;
                graceDuration = waveSettings.grace;
            }
            if (j.contains("rewards")) {
                currencyPerKill = j["rewards"].value("currencyPerKill", currencyPerKill);
                waveClearBonus = j["rewards"].value("waveClearBonus", waveClearBonus);
                bountyBonus = j["rewards"].value("bountyBonus", bountyBonus);
            }
            if (j.contains("xp")) {
                xpPerKill = j["xp"].value("perKill", xpPerKill);
                xpPerWave = j["xp"].value("perWave", xpPerWave);
                xpBase = j["xp"].value("baseToLevel", xpBase);
                xpGrowth = j["xp"].value("growth", xpGrowth);
                levelHpBonus = j["xp"].value("hpBonus", levelHpBonus);
                levelDmgBonus = j["xp"].value("damageBonus", levelDmgBonus);
                levelSpeedBonus = j["xp"].value("speedBonus", levelSpeedBonus);
            }
            if (j.contains("ui")) {
                enemyLowThreshold = j["ui"].value("enemyLowThreshold", enemyLowThreshold);
            }
            if (j.contains("shop")) {
                shopDamageCost = j["shop"].value("damageCost", shopDamageCost);
                shopHpCost = j["shop"].value("hpCost", shopHpCost);
                shopSpeedCost = j["shop"].value("speedCost", shopSpeedCost);
                shopDamageBonus = j["shop"].value("damageBonus", shopDamageBonus);
                shopHpBonus = j["shop"].value("hpBonus", shopHpBonus);
                shopSpeedBonus = j["shop"].value("speedBonus", shopSpeedBonus);
            }
            if (j.contains("timers")) {
                combatDuration = j["timers"].value("combat", combatDuration);
                intermissionDuration = j["timers"].value("intermission", intermissionDuration);
            }
            if (j.contains("boss")) {
                bossWave = j["boss"].value("wave", bossWave);
                bossHpMul = j["boss"].value("hpMultiplier", bossHpMul);
                bossSpeedMul = j["boss"].value("speedMultiplier", bossSpeedMul);
                bossKillBonus = j["boss"].value("killBonus", bossKillBonus);
            }
        } else {
            Engine::logWarn("Failed to open data/gameplay.json; using defaults.");
        }
    }
    waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettings);
    waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
    waveSettings.contactDamage = contactDamage;
    waveSettingsBase_ = waveSettings;
    projectileSpeed_ = projectileSpeed;
    projectileDamage_ = projectileDamage;
    projectileSize_ = projectileSize;
    projectileHitboxSize_ = projectileHitbox;
    projectileLifetime_ = projectileLifetime;
    contactDamage_ = waveSettingsBase_.contactDamage;
    heroMoveSpeed_ = heroMoveSpeed;
    heroMaxHp_ = heroHp;
    heroSize_ = heroSize;
    waveWarmupBase_ = initialWarmup;
    waveWarmup_ = waveWarmupBase_;
    waveInterval_ = waveInterval;
    graceDuration_ = graceDuration;
    currencyPerKill_ = currencyPerKill;
    waveClearBonus_ = waveClearBonus;
    enemyLowThreshold_ = enemyLowThreshold;
    combatDuration_ = combatDuration;
    intermissionDuration_ = intermissionDuration;
    bountyBonus_ = bountyBonus;
    bossWave_ = bossWave;
    bossHpMultiplier_ = bossHpMul;
    bossSpeedMultiplier_ = bossSpeedMul;
    bossKillBonus_ = bossKillBonus;
    shopDamageCost_ = shopDamageCost;
    shopHpCost_ = shopHpCost;
    shopSpeedCost_ = shopSpeedCost;
    shopDamageBonus_ = shopDamageBonus;
    shopHpBonus_ = shopHpBonus;
    shopSpeedBonus_ = shopSpeedBonus;
    xpPerKill_ = xpPerKill;
    xpPerWave_ = xpPerWave;
    xpToNext_ = xpBase;
    xpGrowth_ = xpGrowth;
    levelHpBonus_ = levelHpBonus;
    levelDmgBonus_ = levelDmgBonus;
    levelSpeedBonus_ = levelSpeedBonus;
    fireInterval_ = 1.0 / fireRate;
    fireCooldown_ = 0.0;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);

    // Attempt to load a placeholder sprite for hero (optional).
    if (render_ && textureManager_) {
        if (!manifest_.gridTexture.empty()) {
            auto grid = textureManager_->getOrLoadBMP(manifest_.gridTexture);
            if (grid) gridTexture_ = *grid;
        }
        // Load bitmap font only if TTF is unavailable.
        if (!hasTTF()) {
            if (auto font = Engine::FontLoader::load("data/font.json", *textureManager_)) {
                debugText_ = std::make_unique<Engine::BitmapTextRenderer>(*render_, *font);
                Engine::logInfo("Loaded debug bitmap font.");
            } else {
                Engine::logWarn("Debug font missing; overlay disabled.");
            }
        }
    }
    // Start in main menu; actual run spawns on New Game.
    resetRun();
    inMenu_ = true;
    return true;
}

void GameRoot::onUpdate(const Engine::TimeStep& step, const Engine::InputState& input) {
    accumulated_ += step.deltaSeconds;
    ++tickCount_;

    // Sample high-level actions.
    Engine::ActionState actions = actionMapper_.sample(input);

    const bool restartPressed = actions.restart && !restartPrev_;
    restartPrev_ = actions.restart;
    const bool shopToggleEdge = actions.toggleShop && !shopTogglePrev_;
    shopTogglePrev_ = actions.toggleShop;
    if (shopToggleEdge && !inCombat_ && !inMenu_) {
        shopOpen_ = !shopOpen_;
    }
    if (shopToggleEdge && inCombat_ && !inMenu_) {
        shopUnavailableTimer_ = 1.0;
    }
    if (shopOpen_ && waveWarmup_ <= waveInterval_) {
        shopOpen_ = false;  // auto-close when combat resumes
    }
    const bool pausePressed = actions.pause && !pauseTogglePrev_;
    pauseTogglePrev_ = actions.pause;
    if (!inMenu_ && pausePressed) {
        userPaused_ = !userPaused_;
        pauseMenuBlink_ = 0.0;
    }
    paused_ = userPaused_ || shopOpen_ || levelChoiceOpen_;
    if (actions.toggleFollow && shopOpen_) {
        // ignore camera toggle while paused/shop
    }

    // Menu handling before gameplay.
    updateMenuInput(actions, input, step.deltaSeconds);
    if (inMenu_) {
        renderMenu();
        return;
    }

    // Timers.
    if (shopUnavailableTimer_ > 0.0) {
        shopUnavailableTimer_ -= step.deltaSeconds;
        if (shopUnavailableTimer_ < 0.0) shopUnavailableTimer_ = 0.0;
    }
    if (clearBannerTimer_ > 0.0 && debugText_) {
        // timer already decremented later for text; keep consistent for fallback too
    }
    if (paused_) {
        pauseMenuBlink_ += step.deltaSeconds;
    } else {
        pauseMenuBlink_ = 0.0;
    }
    if (restartPressed) {
        resetRun();
        return;
    }

    // Update hero velocity from actions.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            if (paused_) {
                vel->value = {0.0f, 0.0f};
            } else {
                vel->value = {actions.moveX * heroMoveSpeed_, actions.moveY * heroMoveSpeed_};
            }
        }
        // Shop purchase via mouse buttons when shop is open.
        const bool leftClick = input.isMouseButtonDown(0);
        const bool rightClick = input.isMouseButtonDown(2);
        const bool midClick = input.isMouseButtonDown(1);
        if (shopOpen_) {
            if (leftClick && !shopLeftPrev_) {
                // Buy damage upgrade.
                if (credits_ >= shopDamageCost_) {
                    credits_ -= shopDamageCost_;
                    projectileDamage_ += shopDamageBonus_;
                }
            }
            if (rightClick && !shopRightPrev_) {
                // Buy HP upgrade.
                if (credits_ >= shopHpCost_) {
                    credits_ -= shopHpCost_;
                    heroMaxHp_ += shopHpBonus_;
                    if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                        hp->max = heroMaxHp_;
                        hp->current = std::min(hp->current + shopHpBonus_, hp->max);
                    }
                }
            }
            if (midClick && !shopMiddlePrev_) {
                if (credits_ >= shopSpeedCost_) {
                    credits_ -= shopSpeedCost_;
                    heroMoveSpeed_ += shopSpeedBonus_;
                }
            }
        }
        shopLeftPrev_ = leftClick;
        shopRightPrev_ = rightClick;
        shopMiddlePrev_ = midClick;
        // Primary fire spawns projectile toward mouse.
        fireCooldown_ -= step.deltaSeconds;
        if (!paused_ && actions.primaryFire && fireCooldown_ <= 0.0) {
            const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
            if (heroTf) {
                Engine::Vec2 dir{mouseWorld_.x - heroTf->position.x, mouseWorld_.y - heroTf->position.y};
                float len2 = dir.x * dir.x + dir.y * dir.y;
                if (len2 > 0.0001f) {
                    float inv = 1.0f / std::sqrt(len2);
                    dir.x *= inv;
                    dir.y *= inv;
                }
                auto proj = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(proj, heroTf->position);
                registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{0.0f, 0.0f});
                const float halfSize = projectileHitboxSize_ * 0.5f;
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
                registry_.emplace<Engine::ECS::Renderable>(proj,
                                                           Engine::ECS::Renderable{Engine::Vec2{projectileSize_, projectileSize_},
                                                                                   Engine::Color{255, 230, 90, 255}});
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                     projectileDamage_, projectileLifetime_});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
                fireCooldown_ = fireInterval_;
            }
        }
    }

    // Movement system.
    if (movementSystem_ && !paused_) {
        movementSystem_->update(registry_, step);
    }

    // Camera system.
    if (cameraSystem_) {
        cameraSystem_->update(camera_, registry_, hero_, actions, input, step, viewportWidth_, viewportHeight_);
    }

    // Enemy AI.
    if (enemyAISystem_ && !defeated_ && !paused_) {
        enemyAISystem_->update(registry_, hero_, step);
    }

    // Cleanup dead enemies (visual cleanup can be added later).
    {
        std::vector<Engine::ECS::Entity> toDestroy;
        enemiesAlive_ = 0;
        registry_.view<Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (hp.alive()) {
                    enemiesAlive_++;
                } else {
                    toDestroy.push_back(e);
                }
            });
        for (auto e : toDestroy) {
            kills_ += 1;
            credits_ += currencyPerKill_;
            xp_ += xpPerKill_;
            if (registry_.has<Engine::ECS::BossTag>(e)) {
                credits_ += bossKillBonus_;
                xp_ += xpPerKill_ * 5;
                clearBannerTimer_ = 1.75;
                waveClearedPending_ = true;
            }
            // Chance to drop a credit pickup.
            std::uniform_real_distribution<float> dropRoll(0.0f, 1.0f);
            if (dropRoll(rng_) < 0.25f) {
                auto drop = registry_.create();
                Engine::Vec2 spawnPos{0.0f, 0.0f};
                if (auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
                    spawnPos = tf->position;
                }
                registry_.emplace<Engine::ECS::Transform>(drop, spawnPos);
                const float size = 10.0f;
                registry_.emplace<Engine::ECS::Renderable>(drop,
                    Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{255, 210, 90, 255}});
                registry_.emplace<Engine::ECS::AABB>(drop, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
                registry_.emplace<Game::Pickup>(drop, Game::Pickup{Game::Pickup::Type::Credits, currencyPerKill_ * 2});
                registry_.emplace<Game::PickupBob>(drop, Game::PickupBob{spawnPos, 0.0f, 4.0f, 3.5f});
                registry_.emplace<Engine::ECS::PickupTag>(drop, Engine::ECS::PickupTag{});
            }
            if (registry_.has<Game::BountyTag>(e)) {
                credits_ += bountyBonus_;
                // Spawn extra pickup for bounty.
                auto drop = registry_.create();
                Engine::Vec2 pos{0.0f, 0.0f};
                if (auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
                    pos = tf->position;
                }
                registry_.emplace<Engine::ECS::Transform>(drop, pos);
                const float size = 12.0f;
                registry_.emplace<Engine::ECS::Renderable>(drop,
                    Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{255, 230, 140, 255}});
                registry_.emplace<Engine::ECS::AABB>(drop, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
                registry_.emplace<Game::Pickup>(drop, Game::Pickup{Game::Pickup::Type::Credits, bountyBonus_});
                registry_.emplace<Game::PickupBob>(drop, Game::PickupBob{pos, 0.0f, 5.0f, 4.0f});
                registry_.emplace<Engine::ECS::PickupTag>(drop, Engine::ECS::PickupTag{});
            }
            registry_.destroy(e);
        }
        // If no enemies remain and a wave was active, grant wave clear bonus once.
        const bool anyEnemies = enemiesAlive_ > 0;
        if (!anyEnemies && !waveClearedPending_ && wave_ > 0) {
            credits_ += waveClearBonus_;
            waveClearedPending_ = true;
            clearBannerTimer_ = 1.5;
        }
    }

    // Waves and phases.
    // Phase timing continues unless user-paused (shop should not freeze phase timers).
    if (!defeated_ && !userPaused_ && !levelChoiceOpen_) {
        if (inCombat_) {
            combatTimer_ -= step.deltaSeconds;
            if (waveSystem_) {
                if (!waveSpawned_) {
                    bool newWave = waveSystem_->update(registry_, step, hero_, wave_);
                    waveWarmup_ = std::max(0.0, waveSystem_->timeToNext());
                    if (newWave) {
                        waveSpawned_ = true;
                        waveBannerWave_ = wave_;
                        waveBannerTimer_ = 1.25;
                        waveClearedPending_ = false;
                        clearBannerTimer_ = 0.0;
                        if (wave_ == bossWave_) {
                            bossBannerTimer_ = 2.0;
                        }
                    }
                }
            }
            // If combat timer elapses AND enemies are cleared, go to intermission.
            if (combatTimer_ <= 0.0 && enemiesAlive_ == 0) {
                inCombat_ = false;
                intermissionTimer_ = intermissionDuration_;
                xp_ += xpPerWave_;  // small wave completion xp
                waveSpawned_ = false;
                shopOpen_ = true;
                paused_ = userPaused_ || shopOpen_;
                if (waveSystem_) waveSystem_->setTimer(0.0);
            }
        } else {  // Intermission
            // If enemies lingering, extend intermission until cleared.
            if (enemiesAlive_ > 0) {
                intermissionTimer_ = std::max(intermissionTimer_, 5.0);
            } else {
                intermissionTimer_ -= step.deltaSeconds;
            }
            waveWarmup_ = intermissionTimer_;
            if (intermissionTimer_ <= 0.0) {
                // Start next combat phase
                inCombat_ = true;
                combatTimer_ = combatDuration_;
                waveSpawned_ = false;
                shopOpen_ = false;
                paused_ = userPaused_;
                if (waveSystem_) waveSystem_->setTimer(0.0);
            }
        }
    }

    // Projectiles.
    if (projectileSystem_ && !defeated_ && !paused_) {
        projectileSystem_->update(registry_, step);
    }

        // Collision / damage.
        if (collisionSystem_ && !paused_) {
            collisionSystem_->update(registry_);
        }

        if (pickupSystem_ && !paused_) {
            pickupSystem_->update(registry_, hero_, credits_);
        }

    if (eventSystem_ && !paused_ && !levelChoiceOpen_) {
        Engine::Vec2 heroPos{0.0f, 0.0f};
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
            heroPos = tf->position;
        }
        eventSystem_->update(registry_, step, wave_, credits_, inCombat_, heroPos);
        if (eventSystem_->lastSuccess()) {
            eventBannerText_ = "Event Success +Credits";
            eventBannerTimer_ = 1.5;
        }
        if (eventSystem_->lastFail()) {
            eventBannerText_ = "Event Failed";
            eventBannerTimer_ = 1.5;
        }
        eventSystem_->clearOutcome();
    }

    // Camera shake when hero takes damage.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        if (const auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            if (lastHeroHp_ < 0.0f) lastHeroHp_ = hp->current;
            if (hp->current < lastHeroHp_) {
                shakeTimer_ = 0.25f;
                shakeMagnitude_ = 6.0f;
            }
            lastHeroHp_ = hp->current;
        }
    }
    if (shakeTimer_ > 0.0f) {
        shakeTimer_ -= static_cast<float>(step.deltaSeconds);
        if (shakeTimer_ < 0.0f) shakeTimer_ = 0.0f;
        shakeMagnitude_ = std::max(0.0f, shakeMagnitude_ - 20.0f * static_cast<float>(step.deltaSeconds));
    }

    if (hitFlashSystem_) {
        hitFlashSystem_->update(registry_, step);
    }

    if (damageNumberSystem_) {
        damageNumberSystem_->update(registry_, step);
    }

    // Level up check.
    while (xp_ >= xpToNext_) {
        xp_ -= xpToNext_;
        levelUp();
    }

    handleHeroDeath();
    processDefeatInput(actions);

    // Render all rectangles.
    if (render_) {
        render_->clear({12, 12, 18, 255});

        // Apply camera shake offset when active.
        Engine::Camera2D cameraShaken = camera_;
        if (shakeTimer_ > 0.0f) {
            std::uniform_real_distribution<float> jitter(-shakeMagnitude_, shakeMagnitude_);
            cameraShaken.position.x += jitter(rng_);
            cameraShaken.position.y += jitter(rng_);
        }

        if (renderSystem_) {
            renderSystem_->draw(registry_, cameraShaken, viewportWidth_, viewportHeight_,
                                gridTexture_ ? gridTexture_.get() : nullptr);
        }

        // Off-screen pickup indicator for nearest pickup.
        Engine::Vec2 screenCenter{static_cast<float>(viewportWidth_) * 0.5f, static_cast<float>(viewportHeight_) * 0.5f};
        Engine::Vec2 nearestScreen{};
        float nearestDist2 = std::numeric_limits<float>::max();
        registry_.view<Engine::ECS::Transform, Engine::ECS::PickupTag>(
            [&](Engine::ECS::Entity /*e*/, const Engine::ECS::Transform& tf, const Engine::ECS::PickupTag&) {
                Engine::Vec2 sp = Engine::worldToScreen(tf.position, cameraShaken, static_cast<float>(viewportWidth_),
                                                        static_cast<float>(viewportHeight_));
                float dx = sp.x - screenCenter.x;
                float dy = sp.y - screenCenter.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < nearestDist2) {
                    nearestDist2 = d2;
                    nearestScreen = sp;
                }
            });
        const float margin = 18.0f;
        if (nearestDist2 < std::numeric_limits<float>::max()) {
            bool offscreen = nearestScreen.x < 0.0f || nearestScreen.x > viewportWidth_ || nearestScreen.y < 0.0f ||
                             nearestScreen.y > viewportHeight_;
            if (offscreen) {
                Engine::Vec2 clamped = nearestScreen;
                clamped.x = std::clamp(clamped.x, margin, static_cast<float>(viewportWidth_) - margin);
                clamped.y = std::clamp(clamped.y, margin, static_cast<float>(viewportHeight_) - margin);
                Engine::Vec2 size{12.0f, 4.0f};
                render_->drawFilledRect(clamped, size, Engine::Color{255, 230, 90, 255});
                Engine::Vec2 size2{4.0f, 12.0f};
                render_->drawFilledRect(Engine::Vec2{clamped.x + 4.0f, clamped.y - 4.0f}, size2,
                                        Engine::Color{255, 200, 70, 255});
            }
        }

        // Off-screen arrow to active event (salvage).
        Engine::Vec2 eventScreen{};
        float eventDist2 = std::numeric_limits<float>::max();
        registry_.view<Engine::ECS::Transform, Game::EventActive>(
            [&](Engine::ECS::Entity /*e*/, const Engine::ECS::Transform& tf, const Game::EventActive&) {
                Engine::Vec2 sp = Engine::worldToScreen(tf.position, cameraShaken, static_cast<float>(viewportWidth_),
                                                        static_cast<float>(viewportHeight_));
                float dx = sp.x - screenCenter.x;
                float dy = sp.y - screenCenter.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < eventDist2) {
                    eventDist2 = d2;
                    eventScreen = sp;
                }
            });
        if (eventDist2 < std::numeric_limits<float>::max()) {
            bool offscreen = eventScreen.x < 0.0f || eventScreen.x > viewportWidth_ || eventScreen.y < 0.0f ||
                             eventScreen.y > viewportHeight_;
            if (offscreen) {
                Engine::Vec2 clamped = eventScreen;
                clamped.x = std::clamp(clamped.x, margin, static_cast<float>(viewportWidth_) - margin);
                clamped.y = std::clamp(clamped.y, margin, static_cast<float>(viewportHeight_) - margin);
                Engine::Color c{120, 200, 255, 255};
                Engine::Vec2 size{14.0f, 4.0f};
                render_->drawFilledRect(clamped, size, c);
                Engine::Vec2 size2{4.0f, 14.0f};
                render_->drawFilledRect(Engine::Vec2{clamped.x + 5.0f, clamped.y - 5.0f}, size2, c);
            }
        }
    if (hasTTF()) {
        std::ostringstream dbg;
        dbg << "FPS ~" << std::fixed << std::setprecision(1) << fpsSmooth_;
        dbg << " | Cam " << (cameraSystem_ && cameraSystem_->followEnabled() ? "Follow" : "Free");
        dbg << " | Zoom " << std::setprecision(2) << camera_.zoom;
            if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                dbg << " | HP " << std::setprecision(0) << heroHealth->current << "/" << heroHealth->max;
            }
            dbg << " | Wave " << wave_ << " | Kills " << kills_;
            dbg << " | Credits " << credits_;
            dbg << " | Enemies " << enemiesAlive_;
            dbg << " | WaveClear +" << waveClearBonus_;
            dbg << " | Lv " << level_ << " (" << xp_ << "/" << xpToNext_ << ")";
            drawTextTTF(dbg.str(), Engine::Vec2{10.0f, 10.0f}, 1.0f, Engine::Color{255, 255, 255, 200});
            if (waveWarmup_ > 0.0) {
                std::ostringstream warm;
                warm << "Wave " << (wave_ + 1) << " in " << std::fixed << std::setprecision(1)
                     << std::max(0.0, waveWarmup_) << "s";
                drawTextTTF(warm.str(), Engine::Vec2{10.0f, 28.0f}, 1.0f, Engine::Color{255, 220, 120, 220});
            }
            if (waveBannerTimer_ > 0.0) {
                waveBannerTimer_ -= step.deltaSeconds;
                std::ostringstream banner;
                banner << "WAVE " << waveBannerWave_;
                float scale = 1.6f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 80.0f,
                                 static_cast<float>(viewportHeight_) * 0.08f};
                drawTextTTF(banner.str(), pos, scale, Engine::Color{180, 230, 255, 230});
            }
            if (levelBannerTimer_ > 0.0) {
                levelBannerTimer_ -= step.deltaSeconds;
                std::ostringstream lb;
                lb << "LEVEL UP! " << level_;
                float scale = 1.3f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 90.0f,
                                 static_cast<float>(viewportHeight_) * 0.26f};
                drawTextTTF(lb.str(), pos, scale, Engine::Color{200, 255, 200, 235});
            }
            if (clearBannerTimer_ > 0.0) {
                clearBannerTimer_ -= step.deltaSeconds;
                std::ostringstream cb;
                cb << "WAVE CLEARED +" << waveClearBonus_;
                float scale = 1.2f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 120.0f,
                                 static_cast<float>(viewportHeight_) * 0.14f};
                drawTextTTF(cb.str(), pos, scale, Engine::Color{200, 255, 180, 230});
            }
            if (bossBannerTimer_ > 0.0) {
                bossBannerTimer_ -= step.deltaSeconds;
                std::ostringstream bb;
                bb << "BOSS INCOMING";
                float scale = 1.4f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 90.0f,
                                 static_cast<float>(viewportHeight_) * 0.20f};
                drawTextTTF(bb.str(), pos, scale, Engine::Color{255, 120, 200, 240});
            }
            if (eventBannerTimer_ > 0.0 && !eventBannerText_.empty()) {
                eventBannerTimer_ -= step.deltaSeconds;
                Engine::Vec2 pos{10.0f, 190.0f};
                drawTextTTF(eventBannerText_, pos, 1.0f, Engine::Color{200, 240, 255, 230});
            }
            // Intermission overlay when we are in grace time.
            if (!inCombat_) {
                double intermissionLeft = waveWarmup_;
        std::ostringstream inter;
        inter << "Intermission: next wave in " << std::fixed << std::setprecision(1) << intermissionLeft
              << "s | Credits: " << credits_;
        Engine::Color c{140, 220, 255, 220};
        drawTextTTF(inter.str(), Engine::Vec2{10.0f, 46.0f}, 1.0f, c);
                if (shopOpen_) {
                std::ostringstream shopMsg;
                shopMsg << "[SHOP] Mouse1: +" << shopDamageBonus_ << " dmg (" << shopDamageCost_ << "c) | "
                        << "Mouse2: +" << shopHpBonus_ << " HP (" << shopHpCost_ << "c) | "
                        << "Mouse3: +" << shopSpeedBonus_ << " move (" << shopSpeedCost_ << "c) — B closes";
                drawTextTTF(shopMsg.str(), Engine::Vec2{10.0f, 64.0f}, 1.0f, Engine::Color{180, 255, 180, 220});
                } else {
                    std::string shopPrompt = "Press B to open Shop (placeholder)";
                    drawTextTTF(shopPrompt, Engine::Vec2{10.0f, 64.0f}, 1.0f, Engine::Color{150, 220, 255, 200});
                }
            }
            // Wave clear prompt when banner active (even if not intermission).
            if (clearBannerTimer_ > 0.0 && waveWarmup_ <= waveInterval_) {
                std::ostringstream note;
                note << "Wave clear bonus +" << waveClearBonus_;
                drawTextUnified(note.str(), Engine::Vec2{10.0f, 82.0f}, 0.9f, Engine::Color{180, 240, 180, 200});
            }
            // XP / Level bar.
            {
                float barW = 240.0f;
                float ratio = xpToNext_ > 0 ? static_cast<float>(xp_) / static_cast<float>(xpToNext_) : 0.0f;
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                Engine::Vec2 pos{10.0f, static_cast<float>(viewportHeight_) - 46.0f};
                render_->drawFilledRect(pos, Engine::Vec2{barW, 10.0f}, Engine::Color{40, 60, 90, 200});
                render_->drawFilledRect(pos, Engine::Vec2{barW * ratio, 10.0f}, Engine::Color{90, 200, 255, 220});
                std::ostringstream lvl;
                lvl << "Lv " << level_ << " (" << xp_ << "/" << xpToNext_ << ")";
                drawTextUnified(lvl.str(), Engine::Vec2{pos.x, pos.y - 14.0f}, 0.9f,
                                Engine::Color{200, 230, 255, 220});
            }
            if (shopUnavailableTimer_ > 0.0 && inCombat_) {
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(shopUnavailableTimer_) * 12.0f);
                Engine::Color c{static_cast<uint8_t>(255),
                                static_cast<uint8_t>(140 + 80 * pulse),
                                static_cast<uint8_t>(120 + 60 * pulse), 220};
                drawTextUnified("Shop unavailable during combat", Engine::Vec2{10.0f, 46.0f}, 1.0f, c);
            }
            // Active event HUD.
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity, const Game::EventActive& ev) {
                float t = ev.timer;
                std::ostringstream evMsg;
                evMsg << "Event: Salvage crate - " << std::fixed << std::setprecision(1) << std::max(0.0f, t) << "s";
                drawTextUnified(evMsg.str(), Engine::Vec2{10.0f, 154.0f}, 0.9f, Engine::Color{200, 240, 255, 220});
            });
            if (paused_ && !shopOpen_) {
                float blink = 0.6f + 0.4f * std::sin(static_cast<float>(pauseMenuBlink_) * 6.0f);
                drawTextUnified("PAUSED (Esc)", Engine::Vec2{10.0f, 118.0f}, 1.0f,
                                Engine::Color{static_cast<uint8_t>(160 + 40 * blink),
                                              static_cast<uint8_t>(210 + 30 * blink),
                                              static_cast<uint8_t>(255), 230});
                drawTextUnified("Resume: Esc   Quit: close window", Engine::Vec2{10.0f, 136.0f}, 0.9f,
                                Engine::Color{180, 200, 230, 200});
            }
            // Enemies remaining warning.
            if (enemiesAlive_ > 0 && enemiesAlive_ <= enemyLowThreshold_) {
                std::ostringstream warn;
                warn << enemiesAlive_ << " enemies remain";
                drawTextUnified(warn.str(), Engine::Vec2{10.0f, 100.0f}, 0.9f, Engine::Color{255, 220, 140, 220});
            }
            // Event status (fallback if multiple).
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity, const Game::EventActive& ev) {
                std::ostringstream msg;
                msg << "Event: Salvage | " << std::fixed << std::setprecision(1) << ev.timer << "s left";
                drawTextUnified(msg.str(), Engine::Vec2{10.0f, 172.0f}, 0.9f,
                                Engine::Color{200, 240, 255, 200});
            });
            // Floating damage numbers.
            registry_.view<Engine::ECS::Transform, Game::DamageNumber>(
                [&](Engine::ECS::Entity /*e*/, const Engine::ECS::Transform& tf, const Game::DamageNumber& dn) {
                    Engine::Vec2 screen = Engine::worldToScreen(tf.position, camera_, static_cast<float>(viewportWidth_),
                                                                static_cast<float>(viewportHeight_));
                    std::ostringstream txt;
                    txt << static_cast<int>(dn.value + 0.5f);
                    float alphaScale = std::clamp(dn.timer / 0.8f, 0.0f, 1.0f);
                    Engine::Color c{255, static_cast<uint8_t>(180 * alphaScale),
                                    static_cast<uint8_t>(120 * alphaScale), static_cast<uint8_t>(220 * alphaScale)};
                    drawTextUnified(txt.str(), screen, 0.9f, c);
                });
            }
        // HUD fallback (rectangles) if debug font missing.
        if (!debugText_) {
            const float barW = 200.0f;
            const float barH = 12.0f;
            float hpRatio = 1.0f;
            if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                hpRatio = heroHealth->max > 0.0f ? heroHealth->current / heroHealth->max : 0.0f;
                hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);
            }
            Engine::Vec2 pos{10.0f, static_cast<float>(viewportHeight_) - 24.0f};
            render_->drawFilledRect(pos, Engine::Vec2{barW, barH}, Engine::Color{50, 50, 60, 220});
            render_->drawFilledRect(pos, Engine::Vec2{barW * hpRatio, barH}, Engine::Color{120, 220, 120, 240});
            // Shop unavailable and wave-clear cues.
            if (shopUnavailableTimer_ > 0.0 && waveWarmup_ <= waveInterval_) {
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(shopUnavailableTimer_) * 12.0f);
                Engine::Color c{255, static_cast<uint8_t>(120 + 100 * pulse), static_cast<uint8_t>(100 + 60 * pulse),
                                static_cast<uint8_t>(180 + 40 * pulse)};
                render_->drawFilledRect(Engine::Vec2{10.0f, 10.0f}, Engine::Vec2{160.0f, 12.0f}, c);
            }
            if (clearBannerTimer_ > 0.0) {
                render_->drawFilledRect(Engine::Vec2{10.0f, 28.0f}, Engine::Vec2{140.0f, 12.0f},
                                        Engine::Color{120, 220, 140, 200});
            }
            if (paused_ && !shopOpen_) {
    render_->drawFilledRect(Engine::Vec2{10.0f, 46.0f}, Engine::Vec2{120.0f, 12.0f},
                            Engine::Color{120, 160, 255, 200});
    // block letters "PAUSED"
    auto blk = [&](float x, float y) {
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{5.0f, 8.0f},
                                            Engine::Color{20, 40, 80, 230});
                };
                float x = 14.0f, y = 48.0f;
                // P
                blk(x, y); blk(x+5, y); blk(x+10, y); blk(x, y+4); blk(x+10, y+4); blk(x, y+8); blk(x+5, y+8);
                x += 15.0f;
                // A
                blk(x, y+8); blk(x+5, y+4); blk(x+10, y+4); blk(x+15, y+8); blk(x+5, y); blk(x+10, y); blk(x+7.5f, y+6);
                x += 18.0f;
                // U
                blk(x, y); blk(x+15, y); blk(x, y+4); blk(x+15, y+4); blk(x, y+8); blk(x+5, y+8); blk(x+10, y+8); blk(x+15, y+8);
                x += 20.0f;
                // S
                blk(x, y); blk(x+5, y); blk(x+10, y); blk(x, y+4); blk(x, y+8); blk(x+5, y+8); blk(x+10, y+8);
                x += 15.0f;
                // E
                blk(x, y); blk(x+5, y); blk(x+10, y); blk(x, y+4); blk(x, y+8); blk(x+5, y+8); blk(x+10, y+8);
                x += 15.0f;
                // D
                blk(x, y); blk(x+5, y); blk(x+10, y+2); blk(x+10, y+6); blk(x+5, y+8); blk(x, y+8); blk(x, y+4);
            }
            if (enemiesAlive_ > 0 && enemiesAlive_ <= enemyLowThreshold_) {
                render_->drawFilledRect(Engine::Vec2{10.0f, 64.0f}, Engine::Vec2{100.0f, 10.0f},
                                        Engine::Color{255, 210, 120, 200});
            }
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity, const Game::EventActive& ev) {
                float tRatio = std::clamp(ev.timer / ev.maxTimer, 0.0f, 1.0f);
                render_->drawFilledRect(Engine::Vec2{10.0f, 82.0f}, Engine::Vec2{140.0f * tRatio, 8.0f},
                                        Engine::Color{120, 200, 255, 200});
            });
        }
        showDefeatOverlay();
    }

    // Track mouse world position for debugging/aiming.
    mouseWorld_ = Engine::Gameplay::mouseWorldPosition(input, camera_, viewportWidth_, viewportHeight_);

    // Log once every ~1 second to avoid spamming the console.
    if (accumulated_ >= 1.0) {
        const auto* tf = registry_.get<Engine::ECS::Transform>(hero_);
        // Exponential moving average for FPS.
        const float currentFps = step.deltaSeconds > 0.0 ? static_cast<float>(1.0 / step.deltaSeconds) : 0.0f;
        const float alpha = 0.2f;
        fpsSmooth_ = fpsSmooth_ * (1.0f - alpha) + currentFps * alpha;

        std::ostringstream oss;
        oss << "Heartbeat: " << tickCount_ << " ticks, elapsed " << std::fixed << std::setprecision(2)
            << step.elapsedSeconds << "s";
        if (tf) {
            oss << " | Hero pos (" << std::setprecision(1) << tf->position.x << ", " << tf->position.y << ")";
        }
        oss << " | Mouse world (" << std::setprecision(1) << mouseWorld_.x << ", " << mouseWorld_.y << ")";
        oss << " | FPS ~" << std::setprecision(1) << fpsSmooth_;
        Engine::logInfo(oss.str());
        accumulated_ = 0.0;
    }
}

void GameRoot::onShutdown() {
    if (uiFont_) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    Engine::logInfo("GameRoot shutdown.");
}

void GameRoot::handleHeroDeath() {
    auto* hp = registry_.get<Engine::ECS::Health>(hero_);
    if (hp && hp->current <= 0.0f && !defeated_) {
        defeated_ = true;
        Engine::logWarn("Hero defeated. Run over.");
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {0.0f, 0.0f};
        }
        if (runStarted_) {
            totalRuns_ += 1;
            totalKillsAccum_ += kills_;
            bestWave_ = std::max(bestWave_, wave_);
            runStarted_ = false;
        }
    }
}

void GameRoot::showDefeatOverlay() {
    if (!defeated_) return;
    std::string msg = "DEFEATED - Press R to Restart or ESC to Quit";
    drawTextUnified(msg, Engine::Vec2{40.0f, 60.0f}, 1.5f, Engine::Color{255, 80, 80, 220});
}

void GameRoot::levelUp() {
    level_ += 1;
    xpToNext_ = static_cast<int>(static_cast<float>(xpToNext_) * xpGrowth_);
    levelBannerTimer_ = 1.0;
    levelChoiceOpen_ = true;
    paused_ = true;
    rollLevelChoices();
}

void GameRoot::processDefeatInput(const Engine::ActionState& /*actions*/) {
    // Reserved for future defeat-specific inputs (e.g., confirm exit/menu).
}

void GameRoot::startNewGame() {
    inMenu_ = false;
    userPaused_ = false;
    paused_ = false;
    shopOpen_ = false;
    runStarted_ = true;
    resetRun();
}

void GameRoot::updateMenuInput(const Engine::ActionState& actions, const Engine::InputState& input, double dt) {
    menuPulse_ += dt;
    bool up = actions.moveY < -0.5f;
    bool down = actions.moveY > 0.5f;
    bool upEdge = up && !menuUpPrev_;
    bool downEdge = down && !menuDownPrev_;
    menuUpPrev_ = up;
    menuDownPrev_ = down;

    bool leftClick = input.isMouseButtonDown(0);
    bool confirmEdge = (leftClick && !menuConfirmPrev_) || (actions.primaryFire && !menuConfirmPrev_);
    menuConfirmPrev_ = leftClick || actions.primaryFire;

    bool pauseEdge = actions.pause && !menuPausePrev_;
    menuPausePrev_ = actions.pause;

    auto advanceSelection = [&](int count) {
        const int maxIndex = count - 1;
        if (menuSelection_ < 0) menuSelection_ = 0;
        if (menuSelection_ > maxIndex) menuSelection_ = maxIndex;
        if (upEdge) menuSelection_ = (menuSelection_ - 1 + count) % count;
        if (downEdge) menuSelection_ = (menuSelection_ + 1) % count;
    };

    if (inMenu_) {
        if (menuPage_ == MenuPage::Main) {
            const int itemCount = 4;
            advanceSelection(itemCount);
            if (confirmEdge) {
                switch (menuSelection_) {
                    case 0: startNewGame(); return;
                    case 1: menuPage_ = MenuPage::Stats; menuSelection_ = 0; return;
                    case 2: menuPage_ = MenuPage::Options; menuSelection_ = 0; return;
                    case 3: if (app_) app_->requestQuit("Exit from main menu"); return;
                }
            }
            if (pauseEdge && app_) {
                app_->requestQuit("Exit from main menu");
                return;
            }
        } else if (menuPage_ == MenuPage::Stats) {
            const int itemCount = 1;  // Back
            advanceSelection(itemCount);
            if (confirmEdge || pauseEdge) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        } else if (menuPage_ == MenuPage::Options) {
            const int itemCount = 1;  // Back
            advanceSelection(itemCount);
            if (confirmEdge || pauseEdge) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        }
    }
}

void GameRoot::renderMenu() {
    if (!render_) return;
    render_->clear({10, 12, 16, 255});

    const float centerX = static_cast<float>(viewportWidth_) * 0.5f;
    const float topY = 140.0f;
    drawTextUnified("PROJECT ZETA", Engine::Vec2{centerX - 140.0f, topY}, 1.7f, Engine::Color{180, 230, 255, 240});
    drawTextUnified("Placeholder build | WASD move | Mouse aim | B shop | Esc pause", Engine::Vec2{centerX - 220.0f, topY + 28.0f},
                    0.9f, Engine::Color{150, 200, 230, 220});

    auto drawButton = [&](const std::string& label, int index) {
        float y = topY + 80.0f + index * 38.0f;
        Engine::Vec2 pos{centerX - 120.0f, y};
        Engine::Vec2 size{240.0f, 30.0f};
        bool focused = (menuPage_ == MenuPage::Main && menuSelection_ == index);
        uint8_t base = focused ? 90 : 60;
        Engine::Color bg{static_cast<uint8_t>(base), static_cast<uint8_t>(base + 40), static_cast<uint8_t>(base + 70), 220};
        render_->drawFilledRect(pos, size, bg);
        Engine::Color textCol = focused ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 230};
        drawTextUnified(label, Engine::Vec2{pos.x + 16.0f, pos.y + 6.0f}, 1.0f, textCol);
    };

    if (menuPage_ == MenuPage::Main) {
        drawButton("New Game", 0);
        drawButton("Stats", 1);
        drawButton("Options", 2);
        drawButton("Exit", 3);
        float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(menuPulse_) * 2.0f);
        Engine::Color hint{static_cast<uint8_t>(180 + 40 * pulse), 220, 255, 220};
        drawTextUnified("Navigate: W/S or arrows | Select: Mouse1", Engine::Vec2{centerX - 160.0f, topY + 240.0f},
                        0.9f, hint);
    } else if (menuPage_ == MenuPage::Stats) {
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Stats", Engine::Vec2{centerX - 40.0f, topY + 60.0f}, 1.2f, c);
        std::ostringstream ss;
        ss << "Total Runs: " << totalRuns_;
        drawTextUnified(ss.str(), Engine::Vec2{centerX - 120.0f, topY + 100.0f}, 1.0f, c);
        ss.str(""); ss.clear();
        ss << "Best Wave: " << bestWave_;
        drawTextUnified(ss.str(), Engine::Vec2{centerX - 120.0f, topY + 128.0f}, 1.0f, c);
        ss.str(""); ss.clear();
        ss << "Total Kills: " << totalKillsAccum_;
        drawTextUnified(ss.str(), Engine::Vec2{centerX - 120.0f, topY + 156.0f}, 1.0f, c);
        drawTextUnified("Esc / Mouse1 to return", Engine::Vec2{centerX - 120.0f, topY + 204.0f}, 0.9f,
                        Engine::Color{180, 210, 240, 220});
    } else if (menuPage_ == MenuPage::Options) {
        Engine::Color c{200, 240, 200, 240};
        drawTextUnified("Options (placeholder)", Engine::Vec2{centerX - 120.0f, topY + 80.0f}, 1.1f, c);
        drawTextUnified("- Toggle shop: B", Engine::Vec2{centerX - 120.0f, topY + 118.0f}, 0.95f, c);
        drawTextUnified("- Pause: Esc", Engine::Vec2{centerX - 120.0f, topY + 144.0f}, 0.95f, c);
        drawTextUnified("- Movement: WASD", Engine::Vec2{centerX - 120.0f, topY + 170.0f}, 0.95f, c);
        drawTextUnified("Esc / Mouse1 to return", Engine::Vec2{centerX - 120.0f, topY + 210.0f}, 0.9f,
                        Engine::Color{180, 210, 240, 220});
    }
}

void GameRoot::drawTextTTF(const std::string& text, const Engine::Vec2& pos, float scale, Engine::Color color) {
    if (!uiFont_ || !sdlRenderer_) return;
    SDL_Color sc{color.r, color.g, color.b, color.a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(uiFont_, text.c_str(), sc);
    if (!surf) {
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdlRenderer_, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Rect dst;
    dst.x = static_cast<int>(pos.x);
    dst.y = static_cast<int>(pos.y);
    dst.w = static_cast<int>(surf->w * scale);
    dst.h = static_cast<int>(surf->h * scale);
    SDL_FreeSurface(surf);
    SDL_RenderCopy(sdlRenderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void GameRoot::drawTextUnified(const std::string& text, const Engine::Vec2& pos, float scale, Engine::Color color) {
    if (hasTTF()) {
        drawTextTTF(text, pos, scale, color);
    } else if (debugText_) {
        debugText_->drawText(text, pos, scale, color);
    }
}

void GameRoot::rollLevelChoices() {
    std::uniform_int_distribution<int> pickType(0, 2);
    std::uniform_real_distribution<float> variance(0.9f, 1.15f);
    float dmg = levelDmgBonus_ * variance(rng_);
    float hp = levelHpBonus_ * variance(rng_);
    float spd = levelSpeedBonus_ * variance(rng_);
    LevelChoice base[3] = {
        {LevelChoiceType::Damage, dmg},
        {LevelChoiceType::Health, hp},
        {LevelChoiceType::Speed, spd},
    };
    for (int i = 0; i < 3; ++i) {
        int t = pickType(rng_);
        levelChoices_[i] = base[t];
    }
}

void GameRoot::applyLevelChoice(int index) {
    if (index < 0 || index > 2) return;
    const auto& choice = levelChoices_[index];
    switch (choice.type) {
        case LevelChoiceType::Damage:
            projectileDamage_ += choice.amount;
            break;
        case LevelChoiceType::Health:
            heroMaxHp_ += choice.amount;
            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                hp->max = heroMaxHp_;
                hp->current = std::min(hp->max, hp->current + choice.amount * 0.5f);
            }
            break;
        case LevelChoiceType::Speed:
            heroMoveSpeed_ += choice.amount;
            break;
    }
    levelChoiceOpen_ = false;
    paused_ = userPaused_ || shopOpen_;
}

void GameRoot::drawLevelChoiceOverlay() {
    if (!render_) return;
    const float panelW = 640.0f;
    const float panelH = 280.0f;
    const float cx = static_cast<float>(viewportWidth_) * 0.5f;
    const float cy = static_cast<float>(viewportHeight_) * 0.5f;
    Engine::Vec2 topLeft{cx - panelW * 0.5f, cy - panelH * 0.5f};
    render_->drawFilledRect(topLeft, Engine::Vec2{panelW, panelH}, Engine::Color{18, 26, 34, 230});
    drawTextUnified("Level Up! Pick an upgrade", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 14.0f}, 1.1f,
                    Engine::Color{200, 240, 255, 240});

    const float cardW = 170.0f;
    const float cardH = 190.0f;
    const float gap = 12.0f;
    float startX = cx - (cardW * 1.5f + gap);
    float y = topLeft.y + 50.0f;
    for (int i = 0; i < 3; ++i) {
        float x = startX + i * (cardW + gap);
        Engine::Color bg{40, 60, 90, 220};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cardW, cardH}, bg);

        std::string title;
        std::string desc;
        Engine::Color tcol{200, 230, 255, 240};
        const auto& c = levelChoices_[i];
        switch (c.type) {
            case LevelChoiceType::Damage:
                title = "Sharpen";
                desc = "+ " + std::to_string(static_cast<int>(c.amount)) + " dmg";
                tcol = Engine::Color{230, 200, 120, 240};
                break;
            case LevelChoiceType::Health:
                title = "Reinforce";
                desc = "+ " + std::to_string(static_cast<int>(c.amount)) + " HP";
                tcol = Engine::Color{180, 240, 200, 240};
                break;
            case LevelChoiceType::Speed:
                title = "Sprint";
                desc = "+ " + std::to_string(static_cast<int>(c.amount)) + " move";
                tcol = Engine::Color{200, 220, 255, 240};
                break;
        }
        drawTextUnified(title, Engine::Vec2{x + 14.0f, y + 12.0f}, 1.05f, tcol);
        drawTextUnified(desc, Engine::Vec2{x + 14.0f, y + 38.0f}, 1.0f, Engine::Color{210, 230, 245, 230});
        drawTextUnified("Click to select", Engine::Vec2{x + 14.0f, y + cardH - 26.0f}, 0.85f,
                        Engine::Color{170, 200, 230, 220});
    }
}

void GameRoot::spawnHero() {
    hero_ = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Engine::ECS::Velocity>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Engine::ECS::Renderable>(hero_, Engine::ECS::Renderable{Engine::Vec2{heroSize_, heroSize_},
                                                                               Engine::Color{90, 200, 255, 255}});
    const float heroHalf = heroSize_ * 0.5f;
    registry_.emplace<Engine::ECS::AABB>(hero_, Engine::ECS::AABB{Engine::Vec2{heroHalf, heroHalf}});
    registry_.emplace<Engine::ECS::Health>(hero_, Engine::ECS::Health{heroMaxHp_, heroMaxHp_});
    registry_.emplace<Engine::ECS::HeroTag>(hero_, Engine::ECS::HeroTag{});

    // Apply sprite if loaded.
    if (textureManager_ && !manifest_.heroTexture.empty()) {
        if (auto tex = textureManager_->getOrLoadBMP(manifest_.heroTexture)) {
            if (auto* rend = registry_.get<Engine::ECS::Renderable>(hero_)) {
                rend->texture = *tex;
            }
        }
    }

    // Spawn simple debug markers for spatial sanity checks.
    auto marker1 = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(marker1, Engine::Vec2{200.0f, 0.0f});
    registry_.emplace<Engine::ECS::Renderable>(marker1,
                                               Engine::ECS::Renderable{Engine::Vec2{16.0f, 16.0f},
                                                                       Engine::Color{200, 80, 80, 255}});
    auto marker2 = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(marker2, Engine::Vec2{-200.0f, -120.0f});
    registry_.emplace<Engine::ECS::Renderable>(marker2,
                                               Engine::ECS::Renderable{Engine::Vec2{16.0f, 16.0f},
                                                                       Engine::Color{80, 200, 120, 255}});

    camera_.position = {0.0f, 0.0f};
    camera_.zoom = 1.0f;
}

void GameRoot::resetRun() {
    Engine::logInfo("Resetting run state.");
    registry_ = {};
    hero_ = Engine::ECS::kInvalidEntity;
    kills_ = 0;
    credits_ = 0;
    level_ = 1;
    xp_ = 0;
    wave_ = 0;
    defeated_ = false;
    accumulated_ = 0.0;
    tickCount_ = 0;
    fireCooldown_ = 0.0;
    mouseWorld_ = {};
    camera_ = {};
    restartPrev_ = true;  // consume the key that triggered the reset.
    waveWarmup_ = waveWarmupBase_;
    waveBannerTimer_ = 0.0;
    waveBannerWave_ = 0;
    shakeTimer_ = 0.0f;
    shakeMagnitude_ = 0.0f;
    lastHeroHp_ = -1.0f;
    inCombat_ = true;
    waveSpawned_ = false;
    combatTimer_ = combatDuration_;
    intermissionTimer_ = 0.0;

    if (waveSystem_) {
        waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettingsBase_);
        waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
        waveSystem_->setTimer(waveWarmupBase_);
    }
    if (collisionSystem_) {
        collisionSystem_->setContactDamage(contactDamage_);
    }

    spawnHero();
    if (cameraSystem_) cameraSystem_->resetFollow(true);
    shopOpen_ = false;
    shopLeftPrev_ = shopRightPrev_ = shopMiddlePrev_ = false;
    shopUnavailableTimer_ = 0.0;
    userPaused_ = false;
    paused_ = false;
    levelChoiceOpen_ = false;
    levelChoicePrevClick_ = false;
}

}  // namespace Game
