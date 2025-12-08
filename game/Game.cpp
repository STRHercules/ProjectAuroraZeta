#include "Game.h"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <limits>
#include <vector>
#include <random>
#include <filesystem>
#include <functional>

#include "../engine/core/Application.h"
#include "../engine/core/Logger.h"
#include "../engine/core/Time.h"
#include "../engine/ecs/components/Transform.h"
#include "../engine/ecs/components/Velocity.h"
#include "../engine/ecs/components/Renderable.h"
#include "../engine/ecs/components/SpriteAnimation.h"
#include "../engine/ecs/components/AABB.h"
#include "../engine/ecs/components/Health.h"
#include "../engine/ecs/components/Projectile.h"
#include "../engine/ecs/components/Tags.h"
#include "../engine/input/InputState.h"
#include "../engine/render/RenderDevice.h"
#include "../engine/render/BitmapTextRenderer.h"
#include "../engine/platform/SDLRenderDevice.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "components/DamageNumber.h"
#include "components/Pickup.h"
#include "components/PickupBob.h"
#include "components/BountyTag.h"
#include "components/EnemyAttributes.h"
#include "components/EventActive.h"
#include "components/EventMarker.h"
#include "components/Spawner.h"
#include "components/Invulnerable.h"
#include "components/Shopkeeper.h"
#include "meta/SaveManager.h"
#include "meta/ItemDefs.h"
#include "systems/HotzoneSystem.h"
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
        // Custom cursor
        SDL_Surface* cursorSurf = IMG_Load("assets/GUI/pointer.png");
        if (!cursorSurf) {
            Engine::logWarn(std::string("Failed to load cursor assets/GUI/pointer.png: ") + IMG_GetError());
        } else {
            customCursor_ = SDL_CreateColorCursor(cursorSurf, 0, 0);
            SDL_FreeSurface(cursorSurf);
            if (customCursor_) {
                SDL_SetCursor(customCursor_);
                SDL_ShowCursor(SDL_ENABLE);
            } else {
                Engine::logWarn(std::string("Failed to create SDL cursor: ") + SDL_GetError());
            }
        }
        if (TTF_Init() != 0) {
            Engine::logWarn(std::string("TTF_Init failed: ") + TTF_GetError());
        } else {
            constexpr int kUIFontSize = 28;
            uiFont_ = TTF_OpenFont("data/TinyUnicode.ttf", kUIFontSize);
            if (!uiFont_) {
                Engine::logWarn(std::string("TTF_OpenFont failed: ") + TTF_GetError());
            } else {
                Engine::logInfo("Loaded TTF font data/TinyUnicode.ttf");
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
    animationSystem_ = std::make_unique<AnimationSystem>();
    hitFlashSystem_ = std::make_unique<HitFlashSystem>();
    damageNumberSystem_ = std::make_unique<DamageNumberSystem>();
    shopSystem_ = std::make_unique<ShopSystem>();
    pickupSystem_ = std::make_unique<PickupSystem>();
    eventSystem_ = std::make_unique<EventSystem>();
    hotzoneSystem_ = std::make_unique<HotzoneSystem>(static_cast<float>(hotzoneRotation_), 1.25f, 1.5f, 1.2f, 1.15f,
                                                     hotzoneMapRadius_, hotzoneRadiusMin_, hotzoneRadiusMax_,
                                                     hotzoneMinSeparation_, hotzoneSpawnClearance_, rng_());
    // Wave settings loaded later from gameplay config.
    textureManager_ = std::make_unique<Engine::TextureManager>(*render_);
    loadProgress();

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
        bindings_.interact = {"key:e"};
        bindings_.useItem = {"key:q"};
        bindings_.restart = {"backspace"};
        bindings_.dash = {"space"};
        bindings_.ability1 = {"key:1"};
        bindings_.ability2 = {"key:2"};
        bindings_.ability3 = {"key:3"};
        bindings_.ultimate = {"key:4"};
        bindings_.reload = {"key:f1"};
    }
    actionMapper_ = Engine::ActionMapper(bindings_);

    // Load asset manifest (optional).
    auto manifestLoaded = Engine::AssetManifestLoader::load("data/assets.json");
    if (manifestLoaded) {
        manifest_ = *manifestLoaded;
        Engine::logInfo("Loaded asset manifest from data/assets.json");
    } else {
        Engine::logWarn("Failed to load asset manifest; using defaults.");
    }
    if (manifest_.heroTexture.empty()) {
        manifest_.heroTexture = "assets/Sprites/Characters/DamageDealer.png";
    }
    if (manifest_.gridTexture.empty() && manifest_.gridTextures.empty()) {
        manifest_.gridTextures = {"assets/Tilesheets/floor1.png", "assets/Tilesheets/floor2.png"};
    }

    loadGridTextures();
    loadEnemyDefinitions();
    itemCatalog_ = defaultItemCatalog();
    // capture baseline fire interval for attack speed stacking
    fireIntervalBase_ = fireInterval_;

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
    int salvageReward = salvageReward_;
    float dashSpeedMul = dashSpeedMul_;
    float dashDuration = dashDuration_;
    float dashCooldown = dashCooldown_;
    float dashInvulnFrac = dashInvulnFraction_;
    bool showDamageNumbers = showDamageNumbers_;
    bool screenShake = screenShake_;
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
            if (j.contains("events")) {
                salvageReward = j["events"].value("salvageReward", salvageReward);
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
            if (j.contains("dash")) {
                dashSpeedMul = j["dash"].value("speedMultiplier", dashSpeedMul);
                dashDuration = j["dash"].value("duration", dashDuration);
                dashCooldown = j["dash"].value("cooldown", dashCooldown);
                dashInvulnFrac = j["dash"].value("invulnFraction", dashInvulnFrac);
            }
            if (j.contains("hud")) {
                showDamageNumbers = j["hud"].value("showDamageNumbers", showDamageNumbers);
                screenShake = j["hud"].value("screenShake", screenShake);
            }
        } else {
            Engine::logWarn("Failed to open data/gameplay.json; using defaults.");
        }
    }
    waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettings);
    waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
    waveSystem_->setEnemyDefinitions(&enemyDefs_);
    waveSettings.contactDamage = contactDamage;
    waveSettingsBase_ = waveSettings;
    waveSettingsDefault_ = waveSettings;
    projectileSpeed_ = projectileSpeed;
    projectileDamage_ = projectileDamage;
    projectileDamageBase_ = projectileDamage;
    projectileSize_ = projectileSize;
    projectileHitboxSize_ = projectileHitbox;
    projectileLifetime_ = projectileLifetime;
    contactDamage_ = waveSettingsBase_.contactDamage;
    heroMoveSpeed_ = heroMoveSpeed;
    heroMoveSpeedBase_ = heroMoveSpeed;
    heroMaxHp_ = heroHp;
    heroMaxHpBase_ = heroHp;
    heroMaxHpPreset_ = heroMaxHp_;
    heroMoveSpeedPreset_ = heroMoveSpeed_;
    projectileDamagePreset_ = projectileDamage_;
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
    dashSpeedMul_ = dashSpeedMul;
    dashDuration_ = dashDuration;
    dashCooldown_ = dashCooldown;
    dashInvulnFraction_ = dashInvulnFrac;
    showDamageNumbers_ = showDamageNumbers;
    screenShake_ = screenShake;
    salvageReward_ = salvageReward;
    xpPerKill_ = xpPerKill;
    xpPerWave_ = xpPerWave;
    xpBaseToLevel_ = xpBase;
    xpToNext_ = xpBaseToLevel_;
    xpGrowth_ = xpGrowth;
    levelHpBonus_ = levelHpBonus;
    levelDmgBonus_ = levelDmgBonus;
    levelSpeedBonus_ = levelSpeedBonus;
    fireInterval_ = 1.0 / fireRate;
    fireIntervalBase_ = fireInterval_;
    fireCooldown_ = 0.0;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);

    // Attempt to load a placeholder sprite for hero (optional).
    if (render_ && textureManager_) {
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
    loadMenuPresets();
    // Start in main menu; actual run spawns on New Game.
    resetRun();
    inMenu_ = true;
    return true;
}

void GameRoot::onUpdate(const Engine::TimeStep& step, const Engine::InputState& input) {
    // Keep viewport in sync with resizable window.
    if (sdlRenderer_) {
        int w = viewportWidth_;
        int h = viewportHeight_;
        if (SDL_GetRendererOutputSize(sdlRenderer_, &w, &h) == 0) {
            viewportWidth_ = w;
            viewportHeight_ = h;
        }
    }

    accumulated_ += step.deltaSeconds;
    ++tickCount_;
    lastMouseX_ = input.mouseX();
    lastMouseY_ = input.mouseY();

    // Sample high-level actions.
    Engine::ActionState actions = actionMapper_.sample(input);

        const bool restartPressed = actions.restart && !restartPrev_;
        restartPrev_ = actions.restart;
        const bool shopToggleEdge = actions.toggleShop && !shopTogglePrev_;
        shopTogglePrev_ = actions.toggleShop;
    if (shopToggleEdge && !inCombat_ && !inMenu_) {
        statShopOpen_ = !statShopOpen_;
    }
    if (shopToggleEdge && inCombat_ && !inMenu_) {
        shopUnavailableTimer_ = 1.0;
    }
    if (statShopOpen_ && waveWarmup_ <= waveInterval_) {
        statShopOpen_ = false;  // auto-close when combat resumes
    }
    const bool pausePressed = actions.pause && !pauseTogglePrev_;
    pauseTogglePrev_ = actions.pause;
    bool menuBackEdge = actions.menuBack && !menuBackPrev_;
    menuBackPrev_ = actions.menuBack;
    if (!inMenu_ && pausePressed) {
        userPaused_ = !userPaused_;
        pauseMenuBlink_ = 0.0;
    }
    if (userPaused_ && menuBackEdge) {
        // Return to main menu from pause
        inMenu_ = true;
        menuPage_ = MenuPage::Main;
        menuSelection_ = 0;
        userPaused_ = false;
        paused_ = false;
        itemShopOpen_ = false;
        statShopOpen_ = false;
        levelChoiceOpen_ = false;
        pauseMenuBlink_ = 0.0;
        pauseClickPrev_ = false;
    }
    paused_ = userPaused_ || itemShopOpen_ || statShopOpen_ || levelChoiceOpen_;
    if (actions.toggleFollow && (itemShopOpen_ || statShopOpen_)) {
        // ignore camera toggle while paused/shop
    }

    // Ability focus/upgrade input
    if (actions.scroll != 0) {
        abilityFocus_ = std::clamp(abilityFocus_ + (actions.scroll > 0 ? -1 : 1), 0,
                                   static_cast<int>(abilities_.size() > 0 ? abilities_.size() - 1 : 0));
    }
    bool upgradeEdge = actions.reload && !abilityUpgradePrev_;
    abilityUpgradePrev_ = actions.reload;
    if (upgradeEdge) {
        upgradeFocusedAbility();
    }

    // Level-up choice input (mouse click on cards).
    if (levelChoiceOpen_) {
        bool leftClick = input.isMouseButtonDown(0);
        bool edge = leftClick && !levelChoicePrevClick_;
        levelChoicePrevClick_ = leftClick;
        if (edge) {
            const float cardW = 170.0f;
            const float cardH = 190.0f;
            const float gap = 12.0f;
            float cx = static_cast<float>(viewportWidth_) * 0.5f;
            float startX = cx - (cardW * 1.5f + gap);
            float y = static_cast<float>(viewportHeight_) * 0.5f - cardH * 0.5f;
            int mx = input.mouseX();
            int my = input.mouseY();
            for (int i = 0; i < 3; ++i) {
                float x = startX + i * (cardW + gap);
                if (mx >= x && mx <= x + cardW && my >= y && my <= y + cardH) {
                    applyLevelChoice(i);
                    break;
                }
            }
        }
    }

    // Menu handling before gameplay.
    updateMenuInput(actions, input, step.deltaSeconds);
    if (inMenu_) {
        renderMenu();
        return;
    }

    // Inventory selection (arrow keys).
    bool invLeftEdge = input.isDown(Engine::InputKey::CamLeft) && !inventoryScrollLeftPrev_;
    bool invRightEdge = input.isDown(Engine::InputKey::CamRight) && !inventoryScrollRightPrev_;
    inventoryScrollLeftPrev_ = input.isDown(Engine::InputKey::CamLeft);
    inventoryScrollRightPrev_ = input.isDown(Engine::InputKey::CamRight);
    if (inventory_.empty()) {
        inventorySelected_ = -1;
    } else {
        clampInventorySelection();
        if (invLeftEdge) {
            inventorySelected_ = (inventorySelected_ - 1 + static_cast<int>(inventory_.size())) %
                                 static_cast<int>(inventory_.size());
        } else if (invRightEdge) {
            inventorySelected_ = (inventorySelected_ + 1) % static_cast<int>(inventory_.size());
        }
    }

    // Timers.
        if (shopUnavailableTimer_ > 0.0) {
            shopUnavailableTimer_ -= step.deltaSeconds;
            if (shopUnavailableTimer_ < 0.0) shopUnavailableTimer_ = 0.0;
        }
        if (shopNoCreditTimer_ > 0.0) {
            shopNoCreditTimer_ -= step.deltaSeconds;
            if (shopNoCreditTimer_ < 0.0) shopNoCreditTimer_ = 0.0;
        }
        if (clearBannerTimer_ > 0.0 && debugText_) {
            // timer already decremented later for text; keep consistent for fallback too
        }
    if (paused_) {
        pauseMenuBlink_ += step.deltaSeconds;
    } else {
        pauseMenuBlink_ = 0.0;
    }
    // Pause menu mouse handling
    if (paused_ && !itemShopOpen_ && !statShopOpen_ && !levelChoiceOpen_ && !defeated_) {
        bool leftClick = input.isMouseButtonDown(0);
        bool edge = leftClick && !pauseClickPrev_;
        pauseClickPrev_ = leftClick;
        if (edge) {
            const float panelW = 320.0f;
            const float panelH = 200.0f;
            float cx = static_cast<float>(viewportWidth_) * 0.5f;
            float cy = static_cast<float>(viewportHeight_) * 0.5f;
            float x0 = cx - panelW * 0.5f;
            float y0 = cy - panelH * 0.5f;
            // Buttons
            float btnW = panelW - 40.0f;
            float btnH = 44.0f;
            float resumeX = x0 + 20.0f;
            float resumeY = y0 + 70.0f;
            float quitY = resumeY + btnH + 16.0f;
            int mx = input.mouseX();
            int my = input.mouseY();
            auto inside = [&](float x, float y) {
                return mx >= x && mx <= x + btnW && my >= y && my <= y + btnH;
            };
            if (inside(resumeX, resumeY)) {
                userPaused_ = false;
                paused_ = itemShopOpen_ || statShopOpen_ || levelChoiceOpen_;
                pauseMenuBlink_ = 0.0;
            } else if (inside(resumeX, quitY)) {
                inMenu_ = true;
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
                userPaused_ = false;
                paused_ = false;
                itemShopOpen_ = false;
                statShopOpen_ = false;
                levelChoiceOpen_ = false;
                pauseMenuBlink_ = 0.0;
            }
        }
    } else {
        pauseClickPrev_ = false;
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
        // Dash trigger.
        const bool dashEdge = actions.dash && !dashPrev_;
        dashPrev_ = actions.dash;
        if (!paused_ && dashEdge && dashCooldownTimer_ <= 0.0f) {
            Engine::Vec2 dir{actions.moveX, actions.moveY};
            float len2 = dir.x * dir.x + dir.y * dir.y;
            if (len2 < 0.01f) {
                if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
                    dir = {mouseWorld_.x - tf->position.x, mouseWorld_.y - tf->position.y};
                    len2 = dir.x * dir.x + dir.y * dir.y;
                }
            }
            if (len2 > 0.0001f) {
                float inv = 1.0f / std::sqrt(len2);
                dir.x *= inv;
                dir.y *= inv;
                dashDir_ = dir;
                dashTimer_ = dashDuration_;
                dashInvulnTimer_ = dashDuration_ * dashInvulnFraction_;
                dashCooldownTimer_ = dashCooldown_;
                if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
                    vel->value = {dashDir_.x * heroMoveSpeed_ * dashSpeedMul_,
                                  dashDir_.y * heroMoveSpeed_ * dashSpeedMul_};
                }
                if (auto* inv = registry_.get<Game::Invulnerable>(hero_)) {
                    inv->timer = dashInvulnTimer_;
                } else {
                    registry_.emplace<Game::Invulnerable>(hero_, Game::Invulnerable{dashInvulnTimer_});
                }
                // subtle camera shake cue
                if (screenShake_) {
                    shakeTimer_ = std::max(shakeTimer_, 0.08f);
                    shakeMagnitude_ = std::max(shakeMagnitude_, 4.0f);
                }
            }
        }
    // Shop purchase via mouse buttons when shop is open.
        const bool leftClick = input.isMouseButtonDown(0);
        const bool rightClick = input.isMouseButtonDown(2);
        const bool midClick = input.isMouseButtonDown(1);
        bool ability1Edge = actions.ability1 && !ability1Prev_;
        bool ability2Edge = actions.ability2 && !ability2Prev_;
        bool ability3Edge = actions.ability3 && !ability3Prev_;
        bool abilityUltEdge = actions.ultimate && !abilityUltPrev_;
        bool interactEdge = actions.interact && !interactPrev_;
        bool useItemEdge = actions.useItem && !useItemPrev_;
        ability1Prev_ = actions.ability1;
        ability2Prev_ = actions.ability2;
        ability3Prev_ = actions.ability3;
        abilityUltPrev_ = actions.ultimate;
        interactPrev_ = actions.interact;
        useItemPrev_ = actions.useItem;
        if (statShopOpen_ || itemShopOpen_) {
            // Stat shop quick-buy (mouse buttons) and cards.
            if (statShopOpen_) {
                if (leftClick && !shopLeftPrev_) {
                    if (credits_ >= shopDamageCost_) { credits_ -= shopDamageCost_; projectileDamage_ += shopDamageBonus_; }
                    else { shopNoCreditTimer_ = 0.6; }
                }
                if (rightClick && !shopRightPrev_) {
                    if (credits_ >= shopHpCost_) { credits_ -= shopHpCost_; heroMaxHp_ += shopHpBonus_; if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) { hp->max = heroMaxHp_; hp->current = std::min(hp->current + shopHpBonus_, hp->max); } }
                    else { shopNoCreditTimer_ = 0.6; }
                }
                if (midClick && !shopMiddlePrev_) {
                    if (credits_ >= shopSpeedCost_) { credits_ -= shopSpeedCost_; heroMoveSpeed_ += shopSpeedBonus_; }
                    else { shopNoCreditTimer_ = 0.6; }
                }
                if (leftClick && !shopUIClickPrev_) {
                    // Card hit-test
                    const float panelW = 520.0f;
                    const float cardW = 150.0f;
                    const float cardH = 110.0f;
                    const float gap = 18.0f;
                    float cx = static_cast<float>(viewportWidth_) * 0.5f;
                    float startX = cx - (cardW * 1.5f + gap);
                    float y = static_cast<float>(viewportHeight_) * 0.55f - 200.0f * 0.5f + 46.0f;
                    int mx = input.mouseX();
                    int my = input.mouseY();
                    for (int i = 0; i < 3; ++i) {
                        float x = startX + i * (cardW + gap);
                        if (mx >= x && mx <= x + cardW && my >= y && my <= y + cardH) {
                            if (i == 0 && credits_ >= shopDamageCost_) { credits_ -= shopDamageCost_; projectileDamage_ += shopDamageBonus_; }
                            if (i == 1 && credits_ >= shopHpCost_) { credits_ -= shopHpCost_; heroMaxHp_ += shopHpBonus_; if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) { hp->max = heroMaxHp_; hp->current = std::min(hp->current + shopHpBonus_, hp->max); } }
                            if (i == 2 && credits_ >= shopSpeedCost_) { credits_ -= shopSpeedCost_; heroMoveSpeed_ += shopSpeedBonus_; }
                            break;
                        }
                    }
                }
            }

            // Item shop UI card purchases and selling.
            bool uiClickEdge = leftClick && !shopUIClickPrev_;
            if (uiClickEdge && shopSystem_ && itemShopOpen_) {
                int mx = input.mouseX();
                int my = input.mouseY();
                const float panelW = 760.0f;
                const float panelH = 240.0f;
                const float invW = 200.0f;
                const float popGap = 24.0f;
                const float totalW = panelW + popGap + invW;
                const float cardW = 160.0f;
                const float cardH = 130.0f;
                const float gap = 14.0f;
                float cx = static_cast<float>(viewportWidth_) * 0.5f;
                float cy = static_cast<float>(viewportHeight_) * 0.6f;
                float topLeftX = cx - totalW * 0.5f;
                float topLeftY = cy - panelH * 0.5f;
                float startX = topLeftX + 18.0f;
                float y = topLeftY + 42.0f;
                // Purchases
                for (std::size_t i = 0; i < shopInventory_.size(); ++i) {
                    float x = startX + static_cast<float>(i) * (cardW + gap);
                    if (mx >= x && mx <= x + cardW && my >= y && my <= y + cardH) {
                        float heroHpCurrent = 0.0f;
                        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) heroHpCurrent = hp->current;
                        int creditsBefore = credits_;
                        ItemDefinition purchased{};
                        bool bought = shopSystem_->tryPurchase(static_cast<int>(i), credits_, projectileDamage_,
                                                               heroMaxHp_, heroMoveSpeed_, heroHpCurrent, purchased);
                        if (bought) {
                            if (!addItemToInventory(purchased)) {
                                // refund if inventory full
                                credits_ += purchased.cost;
                            }
                            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                hp->max = heroMaxHp_;
                                hp->current = std::min(hp->max, heroHpCurrent);
                            }
                        } else if (credits_ == creditsBefore) {
                            shopNoCreditTimer_ = 0.6;
                        }
                        break;
                    }
                }
                // Selling inventory rows
                float invX = topLeftX + panelW + popGap;
                float invY = y;
                const float rowH = 28.0f;
                for (std::size_t i = 0; i < inventory_.size(); ++i) {
                    float ry = invY + static_cast<float>(i) * (rowH + 4.0f);
                    if (mx >= invX && mx <= invX + invW && my >= ry && my <= ry + rowH) {
                        sellItemFromInventory(i, credits_);
                        break;
                    }
                }
            }
            shopUIClickPrev_ = leftClick;
        }
        shopLeftPrev_ = leftClick;
        shopRightPrev_ = rightClick;
        shopMiddlePrev_ = midClick;

        // Interact with shopkeeper to open shop during intermission.
        if (!inCombat_ && shopkeeper_ != Engine::ECS::kInvalidEntity && interactEdge) {
            const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
            const auto* shopTf = registry_.get<Engine::ECS::Transform>(shopkeeper_);
            if (heroTf && shopTf) {
                float dx = heroTf->position.x - shopTf->position.x;
                float dy = heroTf->position.y - shopTf->position.y;
                float dist2 = dx * dx + dy * dy;
                if (dist2 < 80.0f * 80.0f) {
                    if (itemShopOpen_) {
                        itemShopOpen_ = false;
                        paused_ = userPaused_ || statShopOpen_;
                    } else {
                        itemShopOpen_ = true;
                        paused_ = userPaused_ || itemShopOpen_ || statShopOpen_;
                    }
                }
            }
        }

        if (!paused_) {
            if (ability1Edge) executeAbility(1);
            if (ability2Edge) executeAbility(2);
            if (ability3Edge) executeAbility(3);
            if (abilityUltEdge) executeAbility(4);
            if (useItemEdge) {
                clampInventorySelection();
                // Prefer the selected support item; otherwise pick the first support available.
                int targetIdx = -1;
                if (inventorySelected_ >= 0 && inventorySelected_ < static_cast<int>(inventory_.size()) &&
                    inventory_[inventorySelected_].def.kind == ItemKind::Support) {
                    targetIdx = inventorySelected_;
                } else {
                    for (std::size_t i = 0; i < inventory_.size(); ++i) {
                        if (inventory_[i].def.kind == ItemKind::Support) { targetIdx = static_cast<int>(i); break; }
                    }
                }
                if (targetIdx != -1) {
                    const auto& inst = inventory_[targetIdx];
                    switch (inst.def.effect) {
                        case ItemEffect::Heal: {
                            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                hp->current = std::min(hp->max, hp->current + inst.def.value * hp->max);
                            }
                            break;
                        }
                        case ItemEffect::FreezeTime:
                            freezeTimer_ = std::max(freezeTimer_, static_cast<double>(inst.def.value));
                            break;
                        case ItemEffect::Turret: {
                            TurretInstance t{};
                            if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) t.pos = tf->position;
                            t.timer = inst.def.value;
                            t.fireCooldown = 0.0f;
                            turrets_.push_back(t);
                            break;
                        }
                        default:
                            break;
                    }
                    inventory_.erase(inventory_.begin() + targetIdx);
                    if (inventory_.empty()) {
                        inventorySelected_ = -1;
                    } else {
                        clampInventorySelection();
                    }
                }
            }
        }
    // Dash timers decrement.
    if (dashTimer_ > 0.0f) {
        dashTimer_ -= static_cast<float>(step.deltaSeconds);
        if (dashTimer_ < 0.0f) dashTimer_ = 0.0f;
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {dashDir_.x * heroMoveSpeed_ * dashSpeedMul_,
                          dashDir_.y * heroMoveSpeed_ * dashSpeedMul_};
        }
        // Trail node
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
            dashTrail_.push_back({tf->position, 0.22f});
        }
    }
    if (dashCooldownTimer_ > 0.0f) {
        dashCooldownTimer_ -= static_cast<float>(step.deltaSeconds);
        if (dashCooldownTimer_ < 0.0f) dashCooldownTimer_ = 0.0f;
    }
        if (dashInvulnTimer_ > 0.0f) {
            dashInvulnTimer_ -= static_cast<float>(step.deltaSeconds);
            if (auto* inv = registry_.get<Game::Invulnerable>(hero_)) {
                inv->timer = dashInvulnTimer_;
                if (inv->timer <= 0.0f) {
                    registry_.remove<Game::Invulnerable>(hero_);
                }
            }
            if (dashInvulnTimer_ < 0.0f) dashInvulnTimer_ = 0.0f;
        }
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
                float zoneDmgMul = hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f;
                float dmgMul = rageDamageBuff_ * zoneDmgMul;
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                     projectileDamage_ * dmgMul, projectileLifetime_, lifestealPercent_, chainBounces_});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
                float rateMul = rageRateBuff_ * (hotzoneSystem_ ? hotzoneSystem_->rateMultiplier() : 1.0f);
                fireCooldown_ = fireInterval_ / std::max(0.1f, rateMul);
            }
        }
    }

    // Movement system. Freeze stops enemies but allows hero movement.
    if (movementSystem_ && !paused_) {
        if (freezeTimer_ > 0.0) {
            registry_.view<Engine::ECS::Velocity, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity, Engine::ECS::Velocity& vel, Engine::ECS::EnemyTag&) { vel.value = {0.0f, 0.0f}; });
        }
        movementSystem_->update(registry_, step);
    }
    if (hotzoneSystem_) {
        hotzoneSystem_->update(registry_, step, hero_);
        if (hotzoneSystem_->consumeFluxSurge()) {
            // Spawn a burst of elites when Warp Flux rotates in to keep pressure up.
            Engine::Vec2 heroPos{0.0f, 0.0f};
            if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
                heroPos = tf->position;
            }
            std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
            std::uniform_real_distribution<float> rad(240.0f, 320.0f);
            for (int i = 0; i < 3; ++i) {
                float a = ang(rng_);
                float r = rad(rng_);
                Engine::Vec2 pos{heroPos.x + std::cos(a) * r, heroPos.y + std::sin(a) * r};
                auto e = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(e, pos);
                registry_.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
                registry_.emplace<Game::Facing>(e, Game::Facing{1});
                const EnemyDefinition* def = nullptr;
                if (!enemyDefs_.empty()) {
                    std::uniform_int_distribution<std::size_t> pick(0, enemyDefs_.size() - 1);
                    def = &enemyDefs_[pick(rng_)];
                }
                float sizeMul = def ? def->sizeMultiplier : 1.0f;
                float hpMul = def ? def->hpMultiplier : 1.0f;
                float spdMul = def ? def->speedMultiplier : 1.0f;
                float size = 18.0f * sizeMul;
                registry_.emplace<Engine::ECS::Renderable>(e,
                    Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{200, 140, 255, 255},
                                            def ? def->texture : Engine::TexturePtr{}});
                registry_.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
                float hp = waveSettingsBase_.enemyHp * 1.6f * hpMul;
                registry_.emplace<Engine::ECS::Health>(e, Engine::ECS::Health{hp, hp});
                registry_.emplace<Engine::ECS::EnemyTag>(e, Engine::ECS::EnemyTag{});
                registry_.emplace<Game::EnemyAttributes>(e, Game::EnemyAttributes{waveSettingsBase_.enemySpeed * 1.7f * spdMul});
                if (def && def->texture) {
                    registry_.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{def->frameWidth,
                                                                                                   def->frameHeight,
                                                                                                   4,
                                                                                                   def->frameDuration});
                }
            }
        }
    }

    // Ability cooldown timers and buffs.
    for (std::size_t i = 0; i < abilities_.size(); ++i) {
        auto& slot = abilities_[i];
        slot.cooldown = std::max(0.0f, slot.cooldown - static_cast<float>(step.deltaSeconds));
    }
    for (auto& st : abilityStates_) {
        st.cooldown = std::max(0.0f, st.cooldown - static_cast<float>(step.deltaSeconds));
    }
    if (rageTimer_ > 0.0f) {
        rageTimer_ -= static_cast<float>(step.deltaSeconds);
        if (rageTimer_ <= 0.0f) {
            rageTimer_ = 0.0f;
            rageDamageBuff_ = 1.0f;
            rageRateBuff_ = 1.0f;
        }
    }
    if (freezeTimer_ > 0.0) {
        freezeTimer_ -= step.deltaSeconds;
        if (freezeTimer_ < 0.0) freezeTimer_ = 0.0;
    }
    // Tick turrets (timers only; firing handled in combat loop).
    for (auto it = turrets_.begin(); it != turrets_.end();) {
        it->timer -= static_cast<float>(step.deltaSeconds);
        it->fireCooldown = std::max(0.0f, it->fireCooldown - static_cast<float>(step.deltaSeconds));
        if (it->timer <= 0.0f) {
            it = turrets_.erase(it);
        } else {
            ++it;
        }
    }

    // Camera system.
    if (cameraSystem_) {
        cameraSystem_->update(camera_, registry_, hero_, actions, input, step, viewportWidth_, viewportHeight_);
    }

    // Simple turret firing when active.
    if (!paused_ && !turrets_.empty()) {
        for (auto& t : turrets_) {
            if (t.fireCooldown <= 0.0f) {
                t.fireCooldown = 0.35f;
                // Aim at nearest enemy; fallback random.
                Engine::Vec2 dir{1.0f, 0.0f};
                float bestDist2 = 999999.0f;
                Engine::Vec2 bestPos{};
                registry_.view<Engine::ECS::Transform, Engine::ECS::EnemyTag>([&](Engine::ECS::Entity, const Engine::ECS::Transform& tf, const Engine::ECS::EnemyTag&) {
                    float dx = tf.position.x - t.pos.x;
                    float dy = tf.position.y - t.pos.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 < bestDist2) {
                        bestDist2 = d2;
                        bestPos = tf.position;
                    }
                });
                if (bestDist2 < 999998.0f && bestDist2 > 0.0001f) {
                    float inv = 1.0f / std::sqrt(bestDist2);
                    dir = {(bestPos.x - t.pos.x) * inv, (bestPos.y - t.pos.y) * inv};
                } else {
                    std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
                    float a = ang(rng_);
                    dir = {std::cos(a), std::sin(a)};
                }
                auto proj = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(proj, t.pos);
                registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{0.0f, 0.0f});
                const float halfSize = projectileHitboxSize_ * 0.5f;
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
                registry_.emplace<Engine::ECS::Renderable>(proj,
                                                           Engine::ECS::Renderable{Engine::Vec2{projectileSize_, projectileSize_},
                                                                                   Engine::Color{120, 220, 255, 230}});
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                     projectileDamage_ * 0.8f, projectileLifetime_, lifestealPercent_, chainBounces_});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
            }
        }
    }

    // Enemy AI.
    if (enemyAISystem_ && !defeated_ && !paused_) {
        if (freezeTimer_ <= 0.0) {
            enemyAISystem_->update(registry_, hero_, step);
        }
    }

    // Advance sprite animations.
    if (animationSystem_ && !paused_) {
        animationSystem_->update(registry_, step);
    }

        // Cleanup dead enemies (visual cleanup can be added later).
        {
            std::vector<Engine::ECS::Entity> toDestroy;
            float xpMul = hotzoneSystem_ ? hotzoneSystem_->xpMultiplier() : 1.0f;
            float creditMul = hotzoneSystem_ ? hotzoneSystem_->creditMultiplier() : 1.0f;
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
                credits_ += static_cast<int>(std::round(currencyPerKill_ * creditMul));
                xp_ += static_cast<int>(std::round(xpPerKill_ * xpMul));
            if (registry_.has<Engine::ECS::BossTag>(e)) {
                credits_ += static_cast<int>(std::round(bossKillBonus_ * creditMul));
                xp_ += static_cast<int>(std::round(xpPerKill_ * 5 * xpMul));
                clearBannerTimer_ = 1.75;
                waveClearedPending_ = true;
            }
            if (const auto* mark = registry_.get<Game::EventMarker>(e)) {
                if (eventSystem_) {
                    eventSystem_->notifyTargetKilled(registry_, mark->eventId);
                }
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
        // Fast-forward combat timer when field is empty.
        if (inCombat_ && enemiesAlive_ == 0 && combatTimer_ > 1.0) {
            combatTimer_ = 1.0;
        }
    }

    // Waves and phases.
    // Phase timing continues unless user-paused (shop should not freeze phase timers).
    if (!defeated_ && !userPaused_ && !levelChoiceOpen_) {
    if (inCombat_) {
        double stepScale = freezeTimer_ > 0.0 ? 0.0 : 1.0;
        combatTimer_ -= step.deltaSeconds * stepScale;
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
                itemShopOpen_ = false;
                statShopOpen_ = false;
                paused_ = userPaused_;
                refreshShopInventory();
                Engine::Vec2 heroPos{0.0f, 0.0f};
                if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = tf->position;
                spawnShopkeeper(heroPos);
                if (waveSystem_) waveSystem_->setTimer(0.0);
                waveWarmup_ = intermissionTimer_;
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
                itemShopOpen_ = false;
                statShopOpen_ = false;
                paused_ = userPaused_;
                despawnShopkeeper();
                refreshShopInventory();
                if (waveSystem_) waveSystem_->setTimer(0.0);
                waveWarmup_ = 0.0;
            }
        }
    }

    // Projectiles (still advance during freeze so player shots travel; keep it simple).
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
        eventSystem_->update(registry_, step, wave_, credits_, inCombat_, heroPos, salvageReward_);
        std::string evLabel = eventSystem_->lastEventLabel().empty() ? "Event" : eventSystem_->lastEventLabel();
        if (eventSystem_->lastSuccess()) {
            std::ostringstream eb;
            eb << evLabel << " Success";
            if (evLabel == "Salvage Run") {
                eb << " +" << salvageReward_ << "c";
            }
            eventBannerText_ = eb.str();
            eventBannerTimer_ = 1.5;
        }
        if (eventSystem_->lastFail()) {
            std::ostringstream eb;
            eb << evLabel << " Failed";
            eventBannerText_ = eb.str();
            eventBannerTimer_ = 1.5;
        }
        eventSystem_->clearOutcome();
    }

    // Camera shake when hero takes damage.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        if (const auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            if (lastHeroHp_ < 0.0f) lastHeroHp_ = hp->current;
            if (hp->current < lastHeroHp_ && screenShake_) {
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
    processDefeatInput(actions, input);

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
            const Engine::Texture* gridPtr = gridTexture_ ? gridTexture_.get() : nullptr;
            const std::vector<Engine::TexturePtr>* gridVariants =
                gridTileTexturesWeighted_.empty() ? (gridTileTextures_.empty() ? nullptr : &gridTileTextures_)
                                                  : &gridTileTexturesWeighted_;
            renderSystem_->draw(registry_, cameraShaken, viewportWidth_, viewportHeight_, gridPtr, gridVariants);
        }

        // Dash trail rendering (simple rectangles).
        {
            for (auto it = dashTrail_.begin(); it != dashTrail_.end();) {
                it->second -= static_cast<float>(step.deltaSeconds);
                if (it->second <= 0.0f) {
                    it = dashTrail_.erase(it);
                    continue;
                }
                Engine::Vec2 screen = Engine::worldToScreen(it->first, cameraShaken, static_cast<float>(viewportWidth_),
                                                            static_cast<float>(viewportHeight_));
                float alphaRatio = std::clamp(it->second / 0.22f, 0.0f, 1.0f);
                Engine::Color c{120, 220, 255, static_cast<uint8_t>(140 * alphaRatio)};
                float size = heroSize_;
                render_->drawFilledRect(Engine::Vec2{screen.x - size * 0.55f, screen.y - size * 0.55f},
                                        Engine::Vec2{size * 1.1f, size * 1.1f}, c);
                Engine::Color cInner{180, 240, 255, static_cast<uint8_t>(120 * alphaRatio)};
                render_->drawFilledRect(Engine::Vec2{screen.x - size * 0.35f, screen.y - size * 0.35f},
                                        Engine::Vec2{size * 0.7f, size * 0.7f}, cInner);
                ++it;
            }
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
            dbg << " | Dash cd " << std::fixed << std::setprecision(1) << dashCooldownTimer_;
            drawTextTTF(dbg.str(), Engine::Vec2{10.0f, 10.0f}, 1.0f, Engine::Color{255, 255, 255, 200});
            if (!inCombat_ && waveWarmup_ > 0.0) {
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
                if (statShopOpen_) {
                    drawTextTTF("STAT SHOP OPEN (B closes)", Engine::Vec2{10.0f, 64.0f}, 1.0f,
                                Engine::Color{180, 255, 180, 220});
                } else {
                    std::string shopPrompt = "Press B to open Stat Shop";
                    drawTextTTF(shopPrompt, Engine::Vec2{10.0f, 64.0f}, 1.0f, Engine::Color{150, 220, 255, 200});
                }
            }
            // Wave clear prompt when banner active (even if not intermission).
            if (clearBannerTimer_ > 0.0 && waveWarmup_ <= waveInterval_) {
                std::ostringstream note;
                note << "Wave clear bonus +" << waveClearBonus_;
                drawTextUnified(note.str(), Engine::Vec2{10.0f, 82.0f}, 0.9f, Engine::Color{180, 240, 180, 200});
            }
            // HP and XP bars (bottom-left).
            {
                const float barW = 300.0f;
                const float hpH = 18.0f;
                const float xpH = 16.0f;
                Engine::Vec2 origin{10.0f, static_cast<float>(viewportHeight_) - 82.0f};
                // Held item display just above the bars.
                Engine::Vec2 invPos{origin.x, origin.y - 50.0f};
                float invW = barW;
                float invH = 32.0f;
                render_->drawFilledRect(invPos, Engine::Vec2{invW, invH}, Engine::Color{18, 24, 34, 210});
                auto rarityCol = [](ItemRarity r) {
                    switch (r) {
                        case ItemRarity::Common: return Engine::Color{200, 220, 240, 240};
                        case ItemRarity::Rare: return Engine::Color{140, 210, 255, 240};
                        case ItemRarity::Legendary: return Engine::Color{255, 215, 140, 240};
                    }
                    return Engine::Color{200, 220, 240, 240};
                };
                if (inventory_.empty()) {
                    inventorySelected_ = -1;
                    drawTextUnified("Inventory empty (use <-/-> to cycle)", Engine::Vec2{invPos.x + 10.0f, invPos.y + 6.0f},
                                    0.9f, Engine::Color{200, 220, 240, 220});
                } else {
                    clampInventorySelection();
                    const auto& held = inventory_[inventorySelected_];
                    bool usable = held.def.kind == ItemKind::Support;
                    std::ostringstream heldText;
                    heldText << "Holding: " << held.def.name;
                    drawTextUnified(heldText.str(), Engine::Vec2{invPos.x + 10.0f, invPos.y + 2.0f}, 1.0f,
                                    rarityCol(held.def.rarity));
                    std::ostringstream hint;
                    hint << "<- / -> to cycle   Q to " << (usable ? "use" : "use support item");
                    drawTextUnified(hint.str(), Engine::Vec2{invPos.x + 10.0f, invPos.y + 12.0f},
                                    0.85f, Engine::Color{180, 200, 220, 210});
                }

                // HP bar
                float hpRatio = 1.0f;
                if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                    hpRatio = heroHealth->max > 0.0f ? heroHealth->current / heroHealth->max : 0.0f;
                    hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);
                }
                render_->drawFilledRect(origin, Engine::Vec2{barW, hpH}, Engine::Color{50, 30, 30, 200});
                render_->drawFilledRect(origin, Engine::Vec2{barW * hpRatio, hpH}, Engine::Color{200, 80, 80, 230});
                std::ostringstream hpTxt;
                hpTxt << "HP " << static_cast<int>(std::round(hpRatio * 100)) << "%";
                drawTextUnified(hpTxt.str(), Engine::Vec2{origin.x + 8.0f, origin.y + 2.0f}, 0.9f,
                                Engine::Color{255, 230, 230, 240});

                // XP bar directly under HP
                float xpRatio = xpToNext_ > 0 ? static_cast<float>(xp_) / static_cast<float>(xpToNext_) : 0.0f;
                xpRatio = std::clamp(xpRatio, 0.0f, 1.0f);
                Engine::Vec2 xpPos{origin.x, origin.y + hpH + 6.0f};
                render_->drawFilledRect(xpPos, Engine::Vec2{barW, xpH}, Engine::Color{30, 40, 60, 200});
                render_->drawFilledRect(xpPos, Engine::Vec2{barW * xpRatio, xpH}, Engine::Color{90, 180, 255, 230});
                std::ostringstream lvl;
                lvl << "Lv " << level_ << "  " << xp_ << "/" << xpToNext_;
                drawTextUnified(lvl.str(), Engine::Vec2{xpPos.x + 8.0f, xpPos.y + 1.0f}, 0.9f,
                                Engine::Color{220, 235, 255, 240});

                // Dash cooldown bar beneath XP
                float dashRatio = dashCooldown_ > 0.0f ? 1.0f - std::clamp(dashCooldownTimer_ / dashCooldown_, 0.0f, 1.0f) : 1.0f;
                Engine::Vec2 dpos{xpPos.x, xpPos.y + xpH + 6.0f};
                render_->drawFilledRect(dpos, Engine::Vec2{barW, 8.0f}, Engine::Color{40, 40, 50, 200});
                render_->drawFilledRect(dpos, Engine::Vec2{barW * dashRatio, 8.0f}, Engine::Color{180, 240, 200, 220});
                drawTextUnified("Dash", Engine::Vec2{dpos.x + 8.0f, dpos.y + 0.5f}, 0.8f, Engine::Color{180, 220, 200, 210});
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
                float t = std::max(0.0f, ev.timer);
                std::ostringstream evMsg;
                if (ev.type == Game::EventType::Salvage) {
                    evMsg << "Event: Salvage Run - " << std::fixed << std::setprecision(1) << t << "s";
                } else if (ev.type == Game::EventType::Bounty) {
                    evMsg << "Event: Bounty Hunt - " << ev.requiredKills << " hunters left | "
                          << std::fixed << std::setprecision(1) << t << "s";
                } else {
                    evMsg << "Event: Assassinate Spawners - " << ev.requiredKills << " spires left | "
                          << std::fixed << std::setprecision(1) << t << "s";
                }
                drawTextUnified(evMsg.str(), Engine::Vec2{10.0f, 154.0f}, 0.9f, Engine::Color{200, 240, 255, 220});
            });
            if (hotzoneSystem_) {
                const auto* zone = hotzoneSystem_->activeZone();
                if (zone) {
                    std::string label = "XP Surge";
                    if (hotzoneSystem_->activeBonus() == HotzoneSystem::Bonus::DangerPay) label = "Danger-Pay";
                    if (hotzoneSystem_->activeBonus() == HotzoneSystem::Bonus::WarpFlux) label = "Warp Flux";
                    float t = hotzoneSystem_->timeRemaining();
                    std::ostringstream hz;
                    hz << "Hotzone: " << label << " (" << std::fixed << std::setprecision(0) << std::max(0.0f, t)
                       << "s) " << (hotzoneSystem_->heroInsideActive() ? "[IN]" : "[OUT]");
                    drawTextUnified(hz.str(), Engine::Vec2{10.0f, 136.0f}, 0.9f, Engine::Color{200, 220, 255, 210});
                    if (hotzoneSystem_->warningActive()) {
                        std::ostringstream hw;
                        hw << "Zone shift in " << std::fixed << std::setprecision(1) << std::max(0.0f, t) << "s";
                        drawTextUnified(hw.str(), Engine::Vec2{10.0f, 118.0f}, 0.9f, Engine::Color{255, 200, 160, 220});
                    }
                }
            }
            // Pause overlay handled separately.
            // Enemies remaining warning.
            if (enemiesAlive_ > 0 && enemiesAlive_ <= enemyLowThreshold_) {
                std::ostringstream warn;
                warn << enemiesAlive_ << " enemies remain";
                drawTextUnified(warn.str(), Engine::Vec2{10.0f, 100.0f}, 0.9f, Engine::Color{255, 220, 140, 220});
            }
            // Shopkeeper prompt during intermission.
            if (!inCombat_ && shopkeeper_ != Engine::ECS::kInvalidEntity && !itemShopOpen_) {
                if (const auto* shopTf = registry_.get<Engine::ECS::Transform>(shopkeeper_)) {
                    Engine::Vec2 screen = Engine::worldToScreen(shopTf->position, cameraShaken,
                                                                static_cast<float>(viewportWidth_),
                                                                static_cast<float>(viewportHeight_));
                    drawTextUnified("Press E to trade", Engine::Vec2{screen.x - 60.0f, screen.y - 24.0f}, 0.9f,
                                    Engine::Color{200, 255, 200, 230});
                }
            }
            // Event status (fallback if multiple).
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity, const Game::EventActive& ev) {
                std::ostringstream msg;
                msg << "Event: Salvage | " << std::fixed << std::setprecision(1) << ev.timer << "s left";
                drawTextUnified(msg.str(), Engine::Vec2{10.0f, 172.0f}, 0.9f,
                                Engine::Color{200, 240, 255, 200});
            });
        // Floating damage numbers.
        if (showDamageNumbers_) {
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
            }
        // HUD fallback (rectangles) if debug font missing.
        // Fallback bars only when TTF is unavailable and we rely on bitmap font.
        if (!hasTTF() && !debugText_) {
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
    // Shop overlays (TTF path)
    if (itemShopOpen_) {
        drawItemShopOverlay();
    }
    if (statShopOpen_) {
        drawStatShopOverlay();
    }
    if (paused_ && !itemShopOpen_ && !statShopOpen_ && !levelChoiceOpen_ && !defeated_) {
        drawPauseOverlay();
    }
        if (!inMenu_) {
            drawAbilityPanel();
        }
        if (levelChoiceOpen_) {
            drawLevelChoiceOverlay();
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
    saveProgress();
    if (customCursor_) {
        SDL_FreeCursor(customCursor_);
        customCursor_ = nullptr;
    }
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
        paused_ = true;
        itemShopOpen_ = false;
        statShopOpen_ = false;
        levelChoiceOpen_ = false;
        Engine::logWarn("Hero defeated. Run over.");
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {0.0f, 0.0f};
        }
        if (runStarted_) {
            totalRuns_ += 1;
            totalKillsAccum_ += kills_;
            bestWave_ = std::max(bestWave_, wave_);
            runStarted_ = false;
            saveProgress();
        }
    }
}

void GameRoot::showDefeatOverlay() {
    if (!defeated_) return;
    float panelW = 380.0f;
    float panelH = 200.0f;
    float cx = static_cast<float>(viewportWidth_) * 0.5f;
    float cy = static_cast<float>(viewportHeight_) * 0.45f;
    Engine::Vec2 panelPos{cx - panelW * 0.5f, cy - panelH * 0.5f};
    Engine::Color border{255, 80, 80, 220};
    Engine::Color bg{20, 24, 34, 230};
    render_->drawFilledRect(panelPos, Engine::Vec2{panelW, panelH}, border);
    render_->drawFilledRect(Engine::Vec2{panelPos.x + 4.0f, panelPos.y + 4.0f},
                            Engine::Vec2{panelW - 8.0f, panelH - 8.0f}, bg);
    drawTextUnified("YOU ARE DOWN", Engine::Vec2{panelPos.x + 36.0f, panelPos.y + 18.0f}, 1.4f,
                    Engine::Color{255, 120, 120, 240});

    float btnW = 150.0f;
    float btnH = 46.0f;
    float gap = 28.0f;
    float startX = cx - btnW - gap * 0.5f;
    float y = panelPos.y + 90.0f;
    int mx = lastMouseX_;
    int my = lastMouseY_;
    auto drawBtn = [&](float x, const std::string& label, bool hovered) {
        Engine::Color base{50, 60, 80, 230};
        Engine::Color border{180, 200, 255, 230};
        if (hovered) {
            base = Engine::Color{70, 90, 120, 240};
            border = Engine::Color{255, 200, 160, 240};
        }
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{btnW, btnH}, border);
        render_->drawFilledRect(Engine::Vec2{x + 3.0f, y + 3.0f}, Engine::Vec2{btnW - 6.0f, btnH - 6.0f}, base);
        drawTextUnified(label, Engine::Vec2{x + 20.0f, y + 14.0f}, 1.0f, Engine::Color{230, 230, 240, 240});
    };
    bool hoverRestart = mx >= startX && mx <= startX + btnW && my >= y && my <= y + btnH;
    bool hoverQuit = mx >= startX + btnW + gap && mx <= startX + btnW + gap + btnW && my >= y && my <= y + btnH;
    drawBtn(startX, "Restart", hoverRestart);
    drawBtn(startX + btnW + gap, "Quit", hoverQuit);
}

void GameRoot::levelUp() {
    level_ += 1;
    xpToNext_ = static_cast<int>(static_cast<float>(xpToNext_) * xpGrowth_);
    levelBannerTimer_ = 1.0;
    levelChoiceOpen_ = true;
    paused_ = true;
    rollLevelChoices();
}

void GameRoot::processDefeatInput(const Engine::ActionState& actions, const Engine::InputState& input) {
    if (!defeated_) return;
    bool restartEdge = actions.restart && !restartPrev_;
    bool quitEdge = actions.menuBack && !menuBackPrev_;
    bool leftClick = input.isMouseButtonDown(0);
    bool clickEdge = leftClick && !defeatClickPrev_;
    defeatClickPrev_ = leftClick;

    float btnW = 150.0f;
    float btnH = 46.0f;
    float gap = 28.0f;
    float cx = static_cast<float>(viewportWidth_) * 0.5f;
    float y = static_cast<float>(viewportHeight_) * 0.45f - 100.0f + 90.0f;  // matches overlay layout
    float startX = cx - btnW - gap * 0.5f;
    int mx = input.mouseX();
    int my = input.mouseY();
    bool hoverRestart = mx >= startX && mx <= startX + btnW && my >= y && my <= y + btnH;
    bool hoverQuit = mx >= startX + btnW + gap && mx <= startX + btnW + gap + btnW && my >= y && my <= y + btnH;

    auto doRestart = [&]() {
        defeated_ = false;
        userPaused_ = false;
        paused_ = false;
        runStarted_ = true;
        resetRun();
    };
    auto doQuit = [&]() {
        defeated_ = false;
        inMenu_ = true;
        menuPage_ = MenuPage::Main;
        menuSelection_ = 0;
        userPaused_ = false;
        paused_ = false;
        itemShopOpen_ = false;
        statShopOpen_ = false;
        levelChoiceOpen_ = false;
        runStarted_ = false;
        resetRun();
    };

    if (restartEdge || (clickEdge && hoverRestart)) {
        doRestart();
    } else if (quitEdge || (clickEdge && hoverQuit)) {
        doQuit();
    }
}

void GameRoot::loadProgress() {
    SaveManager mgr(savePath_);
    SaveData data{};
    if (mgr.load(data)) {
        saveData_ = data;
        totalRuns_ = data.totalRuns;
        bestWave_ = data.bestWave;
        totalKillsAccum_ = data.totalKills;
        Engine::logInfo("Save loaded.");
    } else {
        Engine::logWarn("No valid save found; starting fresh profile.");
    }
}

void GameRoot::saveProgress() {
    saveData_.totalRuns = totalRuns_;
    saveData_.bestWave = bestWave_;
    saveData_.totalKills = totalKillsAccum_;
    SaveManager mgr(savePath_);
    if (!mgr.save(saveData_)) {
        Engine::logWarn("Failed to write save file.");
    }
}

Engine::TexturePtr GameRoot::loadTextureOptional(const std::string& path) {
    if (!textureManager_ || path.empty()) return {};
    auto tex = textureManager_->getOrLoadBMP(path);
    if (!tex) return {};
    return *tex;
}

void GameRoot::spawnShopkeeper(const Engine::Vec2& aroundPos) {
    despawnShopkeeper();
    float r = std::uniform_real_distribution<float>(260.0f, 420.0f)(rng_);
    float a = std::uniform_real_distribution<float>(0.0f, 6.28318f)(rng_);
    Engine::Vec2 pos{aroundPos.x + std::cos(a) * r, aroundPos.y + std::sin(a) * r};
    // Lazy-load shop building texture once.
    if (!shopTexture_) {
        shopTexture_ = loadTextureOptional("assets/Sprites/Buildings/house1.png");
        if (!shopTexture_) {
            Engine::logWarn("Shop texture missing (assets/Sprites/Buildings/house1.png); falling back to placeholder box.");
        }
    }
    shopkeeper_ = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(shopkeeper_, pos);
    if (shopTexture_) {
        Engine::Vec2 size{static_cast<float>(shopTexture_->width()), static_cast<float>(shopTexture_->height())};
        registry_.emplace<Engine::ECS::Renderable>(shopkeeper_, Engine::ECS::Renderable{
            size, Engine::Color{255, 255, 255, 255}, shopTexture_});
    } else {
        registry_.emplace<Engine::ECS::Renderable>(shopkeeper_, Engine::ECS::Renderable{
            Engine::Vec2{28.0f, 28.0f}, Engine::Color{120, 220, 255, 230}});
    }
    registry_.emplace<Game::ShopkeeperTag>(shopkeeper_, Game::ShopkeeperTag{});
}

void GameRoot::despawnShopkeeper() {
    if (shopkeeper_ != Engine::ECS::kInvalidEntity) {
        registry_.destroy(shopkeeper_);
        shopkeeper_ = Engine::ECS::kInvalidEntity;
    }
}

void GameRoot::loadGridTextures() {
    gridTileTextures_.clear();
    gridTileTexturesWeighted_.clear();
    gridTexture_.reset();
    auto pushIfLoaded = [&](const std::string& path) {
        auto tex = loadTextureOptional(path);
        if (tex) {
            gridTileTextures_.push_back(tex);
        } else {
            Engine::logWarn("Grid texture missing or failed to load: " + path);
        }
    };
    for (const auto& path : manifest_.gridTextures) {
        pushIfLoaded(path);
    }
    if (gridTileTextures_.empty() && !manifest_.gridTexture.empty()) {
        gridTexture_ = loadTextureOptional(manifest_.gridTexture);
    }
    if (gridTileTextures_.empty()) {
        pushIfLoaded("assets/Tilesheets/floor.png");
        pushIfLoaded("assets/Tilesheets/floor2.png");
    }
    if (gridTileTextures_.empty() && !gridTexture_) {
        gridTexture_ = loadTextureOptional("assets/Tilesheets/floor.png");
    }
    if (gridTileTextures_.size() == 1) {
        // Ensure at least two variants for visual variety when possible.
        if (gridTexture_ && gridTexture_ != gridTileTextures_.front()) {
            gridTileTextures_.push_back(gridTexture_);
        } else if (auto alt = loadTextureOptional("assets/Tilesheets/floor.png")) {
            gridTileTextures_.push_back(alt);
        }
    }
    if (!gridTileTextures_.empty() && !gridTexture_) {
        gridTexture_ = gridTileTextures_.front();
    }
    // Build weighted variants to control distribution frequency.
    if (!gridTileTextures_.empty()) {
        auto weightForIndex = [](std::size_t idx) {
            if (idx <= 4) return 40;                // 0001-0005 dominant (~85%)
            if (idx >= 5 && idx <= 12) return 4;    // 0006-0013 occasional
            if (idx == 13) return 1;                // 0014 exceedingly rare (48x48)
            if (idx == 14 || idx == 15) return 2;   // 0015-0016 rare
            return 1;                               // 0017-0020 extremely rare
        };
        for (std::size_t i = 0; i < gridTileTextures_.size(); ++i) {
            int w = weightForIndex(i);
            for (int k = 0; k < w; ++k) {
                gridTileTexturesWeighted_.push_back(gridTileTextures_[i]);
            }
        }
    }
    if (gridTileTexturesWeighted_.empty()) {
        gridTileTexturesWeighted_ = gridTileTextures_;  // fallback to uniform
    }
    if (gridTileTextures_.empty() && !gridTexture_) {
        Engine::logWarn("No grid textures found; falling back to debug grid.");
    }
}

void GameRoot::loadEnemyDefinitions() {
    enemyDefs_.clear();
    namespace fs = std::filesystem;
    const fs::path root("assets/Sprites/Enemies");
    if (!fs::exists(root)) {
        Engine::logWarn("Enemy sprites directory missing: assets/Sprites/Enemies");
        return;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".bmp") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& path : files) {
        EnemyDefinition def{};
        def.id = path.stem().string();
        def.texture = loadTextureOptional(path.string());
        if (!def.texture) {
            Engine::logWarn("Failed to load enemy sprite: " + path.string());
        }

        std::size_t h = std::hash<std::string>{}(def.id);
        def.sizeMultiplier = 0.9f + static_cast<float>((h >> 4) % 30) / 100.0f;     // 0.9 - 1.19
        def.hpMultiplier = 0.9f + static_cast<float>((h >> 12) % 35) / 100.0f;      // 0.9 - 1.24
        def.speedMultiplier = 0.85f + static_cast<float>((h >> 20) % 30) / 100.0f;  // 0.85 - 1.14
        def.frameDuration = 0.12f + static_cast<float>((h >> 2) % 6) * 0.01f;       // 0.12 - 0.17

        enemyDefs_.push_back(std::move(def));
    }

    if (enemyDefs_.empty()) {
        Engine::logWarn("No enemy sprites found; spawning fallback colored enemies.");
    } else {
        Engine::logInfo("Loaded " + std::to_string(enemyDefs_.size()) + " enemy archetypes from sprites.");
    }
}

void GameRoot::startNewGame() {
    applyDifficultyPreset();
    applyArchetypePreset();
    inMenu_ = false;
    userPaused_ = false;
    paused_ = false;
    itemShopOpen_ = false;
    statShopOpen_ = false;
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
    bool left = actions.moveX < -0.5f;
    bool right = actions.moveX > 0.5f;
    bool leftEdge = left && !menuLeftPrev_;
    bool rightEdge = right && !menuRightPrev_;
    menuLeftPrev_ = left;
    menuRightPrev_ = right;

    bool leftClick = input.isMouseButtonDown(0);
    bool confirmEdge = (leftClick && !menuConfirmPrev_) || (actions.primaryFire && !menuConfirmPrev_);
    menuConfirmPrev_ = leftClick || actions.primaryFire;

    bool pauseEdge = actions.pause && !menuPausePrev_;
    menuPausePrev_ = actions.pause;
    bool clickEdge = leftClick && !menuClickPrev_;
    menuClickPrev_ = leftClick;

    auto advanceSelection = [&](int count) {
        const int maxIndex = count - 1;
        if (menuSelection_ < 0) menuSelection_ = 0;
        if (menuSelection_ > maxIndex) menuSelection_ = maxIndex;
        if (upEdge) menuSelection_ = (menuSelection_ - 1 + count) % count;
        if (downEdge) menuSelection_ = (menuSelection_ + 1) % count;
    };

    auto inside = [](int mx, int my, float x, float y, float w, float h) {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    };
    const float centerX = static_cast<float>(viewportWidth_) * 0.5f;
    const float topY = 140.0f;
    int mx = input.mouseX();
    int my = input.mouseY();

    if (inMenu_) {
        if (menuPage_ == MenuPage::Main) {
            const int itemCount = 4;
            // Hover to select
            for (int i = 0; i < itemCount; ++i) {
                float y = topY + 80.0f + i * 38.0f;
                float x = centerX - 120.0f;
                if (inside(mx, my, x, y, 240.0f, 30.0f)) {
                    menuSelection_ = i;
                }
            }
            advanceSelection(itemCount);
            if (confirmEdge) {
                switch (menuSelection_) {
                    case 0: menuPage_ = MenuPage::CharacterSelect; menuSelection_ = 0; return;
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
            // click anywhere to go back
            if (clickEdge) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        } else if (menuPage_ == MenuPage::Options) {
            const int itemCount = 2;  // toggles, back via Esc
            advanceSelection(itemCount);
            // hover selection
            float boxY0 = topY + 110.0f;
            for (int i = 0; i < itemCount; ++i) {
                float y = boxY0 + i * 30.0f;
                if (inside(mx, my, centerX - 150.0f, y, 200.0f, 22.0f)) {
                    menuSelection_ = i;
                }
            }
            if (confirmEdge || (clickEdge && inside(mx, my, centerX - 150.0f, boxY0, 200.0f, 22.0f * itemCount + 10.0f))) {
                switch (menuSelection_) {
                    case 0: showDamageNumbers_ = !showDamageNumbers_; break;
                    case 1: screenShake_ = !screenShake_; break;
                }
            }
            if (pauseEdge) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        } else if (menuPage_ == MenuPage::CharacterSelect) {
            const int focusCount = 4;  // archetype, difficulty, start, back
            if (leftEdge) menuSelection_ = (menuSelection_ - 1 + focusCount) % focusCount;
            if (rightEdge) menuSelection_ = (menuSelection_ + 1) % focusCount;
            auto adjustIndex = [&](int& idx, int count) {
                if (count <= 0) return;
                if (idx < 0) idx = 0;
                if (idx >= count) idx = count - 1;
                if (upEdge) idx = (idx - 1 + count) % count;
                if (downEdge) idx = (idx + 1) % count;
            };
            if (menuSelection_ == 0) {
                adjustIndex(selectedArchetype_, static_cast<int>(archetypes_.size()));
                applyArchetypePreset();
            } else if (menuSelection_ == 1) {
                adjustIndex(selectedDifficulty_, static_cast<int>(difficulties_.size()));
                applyDifficultyPreset();
            } else if (menuSelection_ == 2 || menuSelection_ == 3) {
                // Up/down do nothing for Start/Back
            }
            // Mouse hover select and click for lists
            const float panelW = 260.0f;
            const float gap = 30.0f;
            const float listStartY = topY + 80.0f;
            float leftX = centerX - panelW - gap * 0.5f;
            float rightX = centerX + gap * 0.5f;
            const float entryH = 26.0f;
            for (std::size_t i = 0; i < archetypes_.size(); ++i) {
                float y = listStartY + 32.0f + static_cast<float>(i) * entryH;
                if (inside(mx, my, leftX + 8.0f, y, panelW - 16.0f, entryH - 4.0f)) {
                    selectedArchetype_ = static_cast<int>(i);
                    applyArchetypePreset();
                    menuSelection_ = 0;
                }
            }
            for (std::size_t i = 0; i < difficulties_.size(); ++i) {
                float y = listStartY + 32.0f + static_cast<float>(i) * entryH;
                if (inside(mx, my, rightX + 8.0f, y, panelW - 16.0f, entryH - 4.0f)) {
                    selectedDifficulty_ = static_cast<int>(i);
                    applyDifficultyPreset();
                    menuSelection_ = 1;
                }
            }
            float summaryY = listStartY + 220.0f + 18.0f;
            float actionY = summaryY + 98.0f;
            if (inside(mx, my, centerX - 220.0f, actionY, 200.0f, 32.0f)) {
                menuSelection_ = 2;
                if (clickEdge) { startNewGame(); return; }
            }
            if (inside(mx, my, centerX + 20.0f, actionY, 200.0f, 32.0f)) {
                menuSelection_ = 3;
                if (clickEdge) {
                    menuPage_ = MenuPage::Main;
                    menuSelection_ = 0;
                    return;
                }
            }
            if (confirmEdge) {
                if (menuSelection_ == 2) {
                    startNewGame();
                    return;
                }
                if (menuSelection_ == 3) {
                    menuPage_ = MenuPage::Main;
                    menuSelection_ = 0;
                    return;
                }
            }
            if (pauseEdge) {
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
    drawTextUnified("    PROJECT ZETA", Engine::Vec2{centerX - 140.0f, topY}, 1.7f, Engine::Color{180, 230, 255, 240});
    drawTextUnified("Pre-Alpha Build | WASD move | Mouse aim | B shop | Esc pause", Engine::Vec2{centerX - 220.0f, topY + 28.0f},
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
        drawTextUnified("          A Major Bonghit Production", Engine::Vec2{centerX - 160.0f, topY + 240.0f},
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
        drawTextUnified("Options", Engine::Vec2{centerX - 50.0f, topY + 70.0f}, 1.2f, c);
        auto drawOpt = [&](const std::string& label, bool enabled, int idx, float yOff) {
            Engine::Color box{40, 70, 90, 220};
            Engine::Color on{140, 230, 160, 240};
            Engine::Color off{180, 120, 120, 220};
            float y = topY + yOff;
            render_->drawFilledRect(Engine::Vec2{centerX - 150.0f, y}, Engine::Vec2{20.0f, 20.0f}, box);
            if (enabled) {
                render_->drawFilledRect(Engine::Vec2{centerX - 147.0f, y + 3.0f}, Engine::Vec2{14.0f, 14.0f}, on);
            } else {
                render_->drawFilledRect(Engine::Vec2{centerX - 147.0f, y + 3.0f}, Engine::Vec2{14.0f, 14.0f}, off);
            }
            bool focused = (menuSelection_ == idx);
            Engine::Color lc = focused ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 230};
            drawTextUnified(label, Engine::Vec2{centerX - 120.0f, y + 2.0f}, 1.0f, lc);
        };
        drawOpt("Damage Numbers", showDamageNumbers_, 0, 110.0f);
        drawOpt("Screen Shake", screenShake_, 1, 140.0f);
        Engine::Color hint{180, 210, 240, 220};
        drawTextUnified("Esc / Mouse1 to return", Engine::Vec2{centerX - 150.0f, topY + 200.0f}, 0.9f, hint);
    } else if (menuPage_ == MenuPage::CharacterSelect) {
        Engine::Color titleCol{200, 230, 255, 240};
        drawTextUnified("Select Character & Difficulty", Engine::Vec2{centerX - 180.0f, topY + 40.0f}, 1.25f,
                        titleCol);
        const float panelW = 260.0f;
        const float panelH = 220.0f;
        const float gap = 30.0f;
        const float listStartY = topY + 80.0f;
        auto drawList = [&](auto& list, int selected, bool focused, float x, const std::string& title) {
            Engine::Color panel{28, 42, 60, 220};
            render_->drawFilledRect(Engine::Vec2{x, listStartY}, Engine::Vec2{panelW, panelH}, panel);
            drawTextUnified(title, Engine::Vec2{x + 12.0f, listStartY + 8.0f}, 1.05f,
                            Engine::Color{200, 230, 255, 240});
            const float entryH = 26.0f;
            for (std::size_t i = 0; i < list.size(); ++i) {
                float y = listStartY + 32.0f + static_cast<float>(i) * entryH;
                Engine::Color bg{40, 60, 80, static_cast<uint8_t>(focused && static_cast<int>(i) == selected ? 230 : 160)};
                if (static_cast<int>(i) == selected) {
                    bg = Engine::Color{static_cast<uint8_t>(bg.r + 20), static_cast<uint8_t>(bg.g + 40),
                                       static_cast<uint8_t>(bg.b + 60), 230};
                }
                render_->drawFilledRect(Engine::Vec2{x + 8.0f, y}, Engine::Vec2{panelW - 16.0f, entryH - 4.0f}, bg);
                Engine::Color txt = (static_cast<int>(i) == selected)
                                        ? Engine::Color{220, 255, 255, 255}
                                        : Engine::Color{200, 220, 240, 230};
                drawTextUnified(list[i].name, Engine::Vec2{x + 14.0f, y + 4.0f}, 0.95f, txt);
            }
        };
        float leftX = centerX - panelW - gap * 0.5f;
        float rightX = centerX + gap * 0.5f;
        if (!archetypes_.empty()) {
            drawList(archetypes_, selectedArchetype_, menuSelection_ == 0, leftX, "Archetypes");
        }
        if (!difficulties_.empty()) {
            drawList(difficulties_, selectedDifficulty_, menuSelection_ == 1, rightX, "Difficulties");
        }
        // Summary card
        Engine::Color cardCol{24, 34, 48, 220};
        float summaryY = listStartY + panelH + 18.0f;
        render_->drawFilledRect(Engine::Vec2{centerX - 260.0f, summaryY}, Engine::Vec2{520.0f, 86.0f}, cardCol);
        if (!archetypes_.empty()) {
            const auto& a = archetypes_[std::clamp(selectedArchetype_, 0, static_cast<int>(archetypes_.size() - 1))];
            std::ostringstream stats;
            stats << "HP x" << std::fixed << std::setprecision(2) << a.hpMul
                  << " | Speed x" << a.speedMul << " | Damage x" << a.damageMul;
            drawTextUnified(a.name + ": " + a.description, Engine::Vec2{centerX - 248.0f, summaryY + 10.0f}, 0.95f,
                            Engine::Color{210, 235, 255, 235});
            drawTextUnified(stats.str(), Engine::Vec2{centerX - 248.0f, summaryY + 32.0f}, 0.9f,
                            Engine::Color{180, 220, 240, 230});
        }
        if (!difficulties_.empty()) {
            const auto& d =
                difficulties_[std::clamp(selectedDifficulty_, 0, static_cast<int>(difficulties_.size() - 1))];
            std::ostringstream desc;
            desc << d.name << ": " << d.description;
            drawTextUnified(desc.str(), Engine::Vec2{centerX - 248.0f, summaryY + 54.0f}, 0.9f,
                            Engine::Color{200, 220, 255, 220});
        }
        // Action buttons
        auto drawAction = [&](const std::string& label, int index, float x) {
            Engine::Vec2 pos{x, summaryY + 98.0f};
            Engine::Vec2 size{200.0f, 32.0f};
            bool focused = menuSelection_ == index;
            Engine::Color bg{static_cast<uint8_t>(focused ? 90 : 60), static_cast<uint8_t>(focused ? 150 : 110),
                             static_cast<uint8_t>(focused ? 200 : 160), 230};
            render_->drawFilledRect(pos, size, bg);
            drawTextUnified(label, Engine::Vec2{pos.x + 14.0f, pos.y + 6.0f}, 0.98f,
                            Engine::Color{220, 255, 255, 255});
        };
        drawAction("Start Run", 2, centerX - 220.0f);
        drawAction("Back", 3, centerX + 20.0f);
        Engine::Color hint{180, 210, 240, 220};
        drawTextUnified("Left/Right to switch panels | Up/Down to change selection | Enter/Click to confirm",
                        Engine::Vec2{centerX - 260.0f, summaryY + 142.0f}, 0.85f, hint);
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

void GameRoot::loadMenuPresets() {
    archetypes_.clear();
    difficulties_.clear();

    std::ifstream f("data/menu_presets.json");
    if (f.is_open()) {
        nlohmann::json j;
        f >> j;
        if (j.contains("archetypes") && j["archetypes"].is_array()) {
            for (const auto& a : j["archetypes"]) {
                ArchetypeDef def{};
                def.id = a.value("id", "");
                def.name = a.value("name", "");
                def.description = a.value("description", "");
                def.hpMul = a.value("hpMul", 1.0f);
                def.speedMul = a.value("speedMul", 1.0f);
                def.damageMul = a.value("damageMul", 1.0f);
                if (a.contains("color") && a["color"].is_array() && a["color"].size() >= 3) {
                    def.color = Engine::Color{
                        static_cast<uint8_t>(a["color"][0].get<int>()),
                        static_cast<uint8_t>(a["color"][1].get<int>()),
                        static_cast<uint8_t>(a["color"][2].get<int>()),
                        static_cast<uint8_t>(a["color"].size() > 3 ? a["color"][3].get<int>() : 255)};
                }
                if (!def.name.empty()) {
                    archetypes_.push_back(def);
                }
            }
        }
        if (j.contains("difficulties") && j["difficulties"].is_array()) {
            for (const auto& d : j["difficulties"]) {
                DifficultyDef def{};
                def.id = d.value("id", "");
                def.name = d.value("name", "");
                def.description = d.value("description", "");
                def.enemyHpMul = d.value("enemyHpMul", 1.0f);
                def.startWave = d.value("startWave", 1);
                if (!def.name.empty()) {
                    difficulties_.push_back(def);
                }
            }
        }
    } else {
        Engine::logWarn("menu_presets.json missing; using defaults for archetypes/difficulties.");
    }

    auto addArchetype = [&](const std::string& id, const std::string& name, const std::string& desc, float hp,
                            float spd, float dmg, Engine::Color col) {
        ArchetypeDef def{id, name, desc, hp, spd, dmg, col};
        archetypes_.push_back(def);
    };
    if (archetypes_.empty()) {
        addArchetype("tank", "Tank", "High survivability frontliner with slower stride.", 1.35f, 0.9f, 0.9f,
                     Engine::Color{110, 190, 255, 255});
        addArchetype("healer", "Healer", "Supportive sustain, balanced speed, lighter offense.", 1.05f, 1.0f, 0.95f,
                     Engine::Color{170, 220, 150, 255});
        addArchetype("damage", "Damage Dealer", "Glass-cannon firepower, slightly faster pacing.", 0.95f, 1.05f, 1.15f,
                     Engine::Color{255, 180, 120, 255});
        addArchetype("assassin", "Assassin", "Very quick and lethal but fragile.", 0.85f, 1.25f, 1.2f,
                     Engine::Color{255, 110, 180, 255});
        addArchetype("builder", "Builder", "Sturdier utility specialist with slower advance.", 1.1f, 0.95f, 0.9f,
                     Engine::Color{200, 200, 120, 255});
        addArchetype("support", "Support", "All-rounder tuned for team buffs.", 1.0f, 1.0f, 1.0f,
                     Engine::Color{150, 210, 230, 255});
        addArchetype("special", "Special", "Experimental kit with slight boosts across the board.", 1.1f, 1.05f, 1.05f,
                     Engine::Color{200, 160, 240, 255});
    }

    auto addDifficulty = [&](const std::string& id, const std::string& name, const std::string& desc, float hpMul,
                             int startWave) {
        DifficultyDef def{id, name, desc, hpMul, startWave};
        difficulties_.push_back(def);
    };
    if (difficulties_.empty()) {
        addDifficulty("very_easy", "Very Easy", "80% enemy HP, starts at wave 1.", 0.8f, 1);
        addDifficulty("easy", "Easy", "90% enemy HP, starts at wave 1.", 0.9f, 1);
        addDifficulty("normal", "Normal", "Baseline 100% enemy HP, starts at wave 1.", 1.0f, 1);
        addDifficulty("hard", "Hard", "120% enemy HP, starts at wave 10.", 1.2f, 10);
        addDifficulty("chaotic", "Chaotic", "140% enemy HP, starts at wave 20.", 1.4f, 20);
        addDifficulty("insane", "Insane", "160% enemy HP, starts at wave 40.", 1.6f, 40);
        addDifficulty("torment", "Torment I", "200% enemy HP, starts at wave 60 (placeholder target).", 2.0f, 60);
    }

    selectedArchetype_ = std::clamp(selectedArchetype_, 0, static_cast<int>(archetypes_.size() - 1));
    selectedDifficulty_ = std::clamp(selectedDifficulty_, 0, static_cast<int>(difficulties_.size() - 1));
    activeArchetype_ = archetypes_[selectedArchetype_];
    activeDifficulty_ = difficulties_[selectedDifficulty_];
    applyDifficultyPreset();
    applyArchetypePreset();
}

void GameRoot::rebuildWaveSettings() {
    waveSettingsBase_ = waveSettingsDefault_;
    startWaveBase_ = std::max(1, activeDifficulty_.startWave);
    float hpMul = activeDifficulty_.enemyHpMul;
    float hpGrowth = std::pow(1.08f, static_cast<float>(startWaveBase_ - 1));
    float speedGrowth = std::pow(1.01f, static_cast<float>(startWaveBase_ - 1));
    waveSettingsBase_.enemyHp *= hpMul * hpGrowth;
    waveSettingsBase_.enemySpeed *= speedGrowth;
    int batchIncrease = (startWaveBase_ - 1) / 2;
    waveSettingsBase_.batchSize = std::min(12, waveSettingsBase_.batchSize + batchIncrease);
    contactDamage_ = waveSettingsBase_.contactDamage;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);
}

void GameRoot::applyDifficultyPreset() {
    if (difficulties_.empty()) return;
    selectedDifficulty_ = std::clamp(selectedDifficulty_, 0, static_cast<int>(difficulties_.size() - 1));
    activeDifficulty_ = difficulties_[selectedDifficulty_];
    rebuildWaveSettings();
}

void GameRoot::applyArchetypePreset() {
    if (archetypes_.empty()) return;
    selectedArchetype_ = std::clamp(selectedArchetype_, 0, static_cast<int>(archetypes_.size() - 1));
    activeArchetype_ = archetypes_[selectedArchetype_];
    heroMaxHpPreset_ = heroMaxHpBase_ * activeArchetype_.hpMul;
    heroMoveSpeedPreset_ = heroMoveSpeedBase_ * activeArchetype_.speedMul;
    projectileDamagePreset_ = projectileDamageBase_ * activeArchetype_.damageMul;
    heroColorPreset_ = activeArchetype_.color;
    heroTexturePath_ = resolveArchetypeTexture(activeArchetype_);
    buildAbilities();
}

std::string GameRoot::resolveArchetypeTexture(const GameRoot::ArchetypeDef& def) const {
    if (!def.texturePath.empty()) return def.texturePath;
    // Default mapping based on id/name.
    const std::string base = "assets/Sprites/Characters/";
    if (def.id == "assassin") return base + "Assassin.png";
    if (def.id == "healer") return base + "Healer.png";
    if (def.id == "damage") return base + "DamageDealer.png";
    if (def.id == "tank") return base + "Tank.png";
    if (def.id == "builder") return base + "Builder.png";
    if (def.id == "support") return base + "Support.png";
    if (def.id == "special") return base + "Special.png";
    // Fallback to manifest heroTexture.
    return manifest_.heroTexture.empty() ? base + "DamageDealer.png" : manifest_.heroTexture;
}

void GameRoot::buildAbilities() {
    abilities_.clear();
    abilityStates_.clear();
    abilityFocus_ = 0;
    rageTimer_ = 0.0f;
    rageDamageBuff_ = 1.0f;
    rageRateBuff_ = 1.0f;
    freezeTimer_ = 0.0;
    attackSpeedMul_ = 1.0f;
    lifestealPercent_ = 0.0f;
    turrets_.clear();

    // Load from data file
    std::ifstream f("data/abilities.json");
    nlohmann::json j;
    if (f.is_open()) {
        try {
            f >> j;
        } catch (...) {
            Engine::logWarn("abilities.json parse error; using defaults.");
        }
    } else {
        std::ifstream fdef("data/abilities_default.json");
        if (fdef.is_open()) {
            try {
                fdef >> j;
            } catch (...) {
                Engine::logWarn("abilities_default.json parse error; using built-ins.");
            }
        }
    }
    auto pushAbility = [&](const AbilityDef& def) {
        AbilitySlot slot{};
        slot.name = def.name;
        slot.description = def.description;
        slot.keyHint = def.keyHint;
        slot.cooldownMax = def.baseCooldown;
        slot.cooldown = 0.0f;
        slot.upgradeCost = def.baseCost;
        slot.type = def.type;
        slot.powerScale = 1.0f;
        abilities_.push_back(slot);

        AbilityState st{};
        st.def = def;
        st.level = 1;
        st.maxLevel = 5;
        st.cooldown = 0.0f;
        abilityStates_.push_back(st);
    };

    bool loaded = false;
    if (j.contains(activeArchetype_.id)) {
        const auto& arr = j[activeArchetype_.id];
        if (arr.is_array()) {
            for (const auto& a : arr) {
                AbilityDef def{};
                def.name = a.value("name", "");
                def.description = a.value("description", "");
                def.keyHint = a.value("key", "");
                def.baseCooldown = a.value("cooldown", 8.0f);
                def.baseCost = a.value("cost", 25);
                def.type = a.value("type", "generic");
                if (!def.name.empty()) {
                    pushAbility(def);
                }
            }
            loaded = !abilities_.empty();
        }
    }

    if (!loaded) {
        const std::string archetype = activeArchetype_.id;
        if (archetype == "damage" || archetype == "damage dealer" || archetype == "dd") {
            pushAbility({"Primary Fire", "Standard projectile.", "M1", 0.0f, 0, "primary"});
            pushAbility({"Scatter Shot", "Close-range cone blast.", "1", 6.0f, 30, "scatter"});
            pushAbility({"Rage", "Boost damage and fire rate briefly.", "2", 12.0f, 40, "rage"});
            pushAbility({"Nova Barrage", "Heavy projectiles in all directions.", "3", 10.0f, 45, "nova"});
            pushAbility({"Death Blossom", "Ultimate barrage; huge damage.", "4", 35.0f, 60, "ultimate"});
        } else if (archetype == "tank") {
            pushAbility({"Primary Fire", "Sturdy rounds.", "M1", 0.0f, 0, "primary"});
            pushAbility({"Shield Bash", "Short stun bash.", "1", 8.0f, 30, "generic"});
            pushAbility({"Fortify", "Damage reduction surge.", "2", 14.0f, 40, "generic"});
            pushAbility({"Taunt Pulse", "Pull threat nearby.", "3", 16.0f, 45, "generic"});
            pushAbility({"Bulwark Dome", "Ultimate barrier bubble.", "4", 40.0f, 60, "generic"});
        } else {
            pushAbility({"Primary Fire", "Baseline projectile.", "M1", 0.0f, 0, "primary"});
            pushAbility({"Ability 1", "Supplemental attack.", "1", 8.0f, 30, "generic"});
            pushAbility({"Ability 2", "Utility (dash/invis/etc.).", "2", 12.0f, 35, "generic"});
            pushAbility({"Ability 3", "Heavy hit or heal.", "3", 14.0f, 45, "generic"});
            pushAbility({"Ultimate", "Long-cooldown power spike.", "4", 40.0f, 60, "ultimate"});
        }
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
    paused_ = userPaused_ || itemShopOpen_ || statShopOpen_;
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

static std::string shopLabelForEffect(ItemEffect eff) {
    switch (eff) {
        case ItemEffect::Damage: return "+Damage";
        case ItemEffect::Health: return "+Health";
        case ItemEffect::Speed: return "+Speed";
        case ItemEffect::Heal: return "Heal";
        case ItemEffect::FreezeTime: return "Freeze";
        case ItemEffect::Turret: return "Turret";
        case ItemEffect::AttackSpeed: return "Atk Speed";
        case ItemEffect::Lifesteal: return "Lifesteal";
        case ItemEffect::Chain: return "Chain";
    }
    return "Item";
}

void GameRoot::refreshShopInventory() {
    shopPool_ = itemCatalog_;
    // Shuffle and take first 4.
    std::shuffle(shopPool_.begin(), shopPool_.end(), rng_);
    shopInventory_.assign(shopPool_.begin(), shopPool_.begin() + std::min<std::size_t>(4, shopPool_.size()));
    if (shopSystem_) {
        shopSystem_->setInventory(shopInventory_);
    }
}

void GameRoot::drawItemShopOverlay() {
    if (!render_ || !itemShopOpen_) return;
    const float panelW = 760.0f;
    const float panelH = 240.0f;
    const float invW = 200.0f;
    const float popGap = 24.0f;
    const float totalW = panelW + popGap + invW;
    const float cx = static_cast<float>(viewportWidth_) * 0.5f;
    const float cy = static_cast<float>(viewportHeight_) * 0.6f;
    Engine::Vec2 topLeft{cx - totalW * 0.5f, cy - panelH * 0.5f};
    render_->drawFilledRect(topLeft, Engine::Vec2{panelW, panelH}, Engine::Color{16, 20, 30, 230});
    drawTextUnified("Traveling Shop (E to close)", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 12.0f}, 1.0f,
                    Engine::Color{200, 255, 200, 240});
    const float cardW = 160.0f;
    const float cardH = 130.0f;
    const float gap = 14.0f;
    float startX = topLeft.x + 18.0f;
    float y = topLeft.y + 42.0f;
    for (std::size_t i = 0; i < shopInventory_.size(); ++i) {
        float x = startX + static_cast<float>(i) * (cardW + gap);
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cardW, cardH}, Engine::Color{32, 46, 66, 220});
        const auto& item = shopInventory_[i];
        std::string title = shopLabelForEffect(item.effect);
        std::ostringstream desc;
        switch (item.effect) {
            case ItemEffect::Damage: desc << "+" << static_cast<int>(item.value) << " damage"; break;
            case ItemEffect::Health: desc << "+" << static_cast<int>(item.value) << " HP"; break;
            case ItemEffect::Speed: desc << "+" << static_cast<int>(item.value * 100) << "% speed"; break;
            case ItemEffect::Heal: desc << "Heal " << static_cast<int>(item.value * 100) << "% HP"; break;
            case ItemEffect::FreezeTime: desc << "Freeze " << item.value << "s"; break;
            case ItemEffect::Turret: desc << "Turret " << static_cast<int>(item.value) << "s"; break;
            case ItemEffect::AttackSpeed: desc << "+" << static_cast<int>(item.value * 100) << "% atk speed"; break;
            case ItemEffect::Lifesteal: desc << static_cast<int>(item.value * 100) << "% lifesteal"; break;
            case ItemEffect::Chain: desc << "Chains +" << static_cast<int>(item.value); break;
        }
        drawTextUnified(title, Engine::Vec2{x + 12.0f, y + 10.0f}, 1.0f, Engine::Color{220, 240, 255, 240});
        drawTextUnified(desc.str(), Engine::Vec2{x + 12.0f, y + 34.0f}, 0.95f, Engine::Color{200, 220, 240, 230});
        std::ostringstream cost;
        cost << item.cost << "c";
        bool affordable = credits_ >= item.cost;
        Engine::Color costCol = affordable ? Engine::Color{180, 255, 200, 240}
                                           : Engine::Color{220, 160, 160, 220};
        drawTextUnified(cost.str(), Engine::Vec2{x + 12.0f, y + 62.0f}, 0.9f, costCol);
        Engine::Color hintCol{170, 200, 230, 200};
        if (!affordable && shopNoCreditTimer_ > 0.0) {
            float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(shopNoCreditTimer_) * 18.0f);
            hintCol = Engine::Color{static_cast<uint8_t>(220), static_cast<uint8_t>(140 * pulse),
                                    static_cast<uint8_t>(140 * pulse), 230};
        }
        drawTextUnified("Click to buy", Engine::Vec2{x + 12.0f, y + 96.0f}, 0.85f, hintCol);
    }
    std::ostringstream wallet;
    wallet << "Credits: " << credits_;
    drawTextUnified(wallet.str(), Engine::Vec2{topLeft.x + panelW - 170.0f, topLeft.y + 12.0f}, 0.95f,
                    Engine::Color{200, 255, 200, 240});

    // Support item quick-use hint.
    for (const auto& inst : inventory_) {
        if (inst.def.kind == ItemKind::Support) {
            std::ostringstream hint;
            hint << "Press Q to use: " << inst.def.name;
            drawTextUnified(hint.str(), Engine::Vec2{topLeft.x + 18.0f, topLeft.y + panelH - 26.0f}, 0.9f,
                            Engine::Color{200, 230, 255, 220});
            break;
        }
    }

    // Inventory panel (sell)
    float invX = topLeft.x + panelW + popGap;
    float invY = topLeft.y + 42.0f;
    render_->drawFilledRect(Engine::Vec2{invX - 6.0f, invY - 10.0f}, Engine::Vec2{invW + 12.0f, panelH - 60.0f},
                            Engine::Color{24, 32, 46, 210});
    drawTextUnified("Inventory (click to sell)", Engine::Vec2{invX, invY - 20.0f}, 0.9f,
                    Engine::Color{200, 230, 255, 220});
    const float rowH = 28.0f;
    for (std::size_t i = 0; i < inventory_.size(); ++i) {
        float ry = invY + static_cast<float>(i) * (rowH + 4.0f);
        Engine::Color bg{34, 46, 66, 220};
        if (i % 2 == 1) bg = Engine::Color{38, 52, 74, 220};
        render_->drawFilledRect(Engine::Vec2{invX, ry}, Engine::Vec2{invW, rowH}, bg);
        auto rarityCol = [](ItemRarity r) {
            switch (r) {
                case ItemRarity::Common: return Engine::Color{190, 210, 230, 240};
                case ItemRarity::Rare: return Engine::Color{120, 200, 255, 240};
                case ItemRarity::Legendary: return Engine::Color{255, 210, 120, 240};
            }
            return Engine::Color{200, 220, 240, 240};
        };
        drawTextUnified(inventory_[i].def.name, Engine::Vec2{invX + 8.0f, ry + 4.0f}, 0.9f,
                        rarityCol(inventory_[i].def.rarity));
        int refund = std::max(1, inventory_[i].def.cost / 2);
        std::ostringstream sell;
        sell << refund << "c";
        drawTextUnified(sell.str(), Engine::Vec2{invX + invW - 52.0f, ry + 4.0f}, 0.85f,
                        Engine::Color{200, 255, 200, 220});
    }
}

void GameRoot::drawStatShopOverlay() {
    if (!render_ || !statShopOpen_) return;
    const float panelW = 520.0f;
    const float panelH = 200.0f;
    const float cx = static_cast<float>(viewportWidth_) * 0.5f;
    const float cy = static_cast<float>(viewportHeight_) * 0.55f;
    Engine::Vec2 topLeft{cx - panelW * 0.5f, cy - panelH * 0.5f};
    render_->drawFilledRect(topLeft, Engine::Vec2{panelW, panelH}, Engine::Color{20, 24, 34, 230});
    drawTextUnified("Stat Shop - B closes", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 12.0f}, 1.0f,
                    Engine::Color{200, 255, 200, 240});

    struct StatCard { std::string title; std::string desc; int cost; std::function<void()> apply; };
    StatCard cards[3];
    cards[0] = {"Damage", "+" + std::to_string(static_cast<int>(shopDamageBonus_)) + " dmg", shopDamageCost_, [&]() {
        if (credits_ >= shopDamageCost_) { credits_ -= shopDamageCost_; projectileDamage_ += shopDamageBonus_; }
    }};
    cards[1] = {"Health", "+" + std::to_string(static_cast<int>(shopHpBonus_)) + " HP", shopHpCost_, [&]() {
        if (credits_ >= shopHpCost_) { credits_ -= shopHpCost_; heroMaxHp_ += shopHpBonus_; if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) { hp->max = heroMaxHp_; hp->current = std::min(hp->current + shopHpBonus_, hp->max); } }
    }};
    cards[2] = {"Speed", "+" + std::to_string(static_cast<int>(shopSpeedBonus_)) + " move", shopSpeedCost_, [&]() {
        if (credits_ >= shopSpeedCost_) { credits_ -= shopSpeedCost_; heroMoveSpeed_ += shopSpeedBonus_; }
    }};

    const float cardW = 150.0f;
    const float cardH = 110.0f;
    const float gap = 18.0f;
    float startX = cx - (cardW * 1.5f + gap);
    float y = topLeft.y + 46.0f;
    int mx = lastMouseX_;
    int my = lastMouseY_;
    for (int i = 0; i < 3; ++i) {
        float x = startX + i * (cardW + gap);
        bool hovered = mx >= x && mx <= x + cardW && my >= y && my <= y + cardH;
        Engine::Color bg = hovered ? Engine::Color{46, 70, 96, 230} : Engine::Color{34, 46, 66, 220};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cardW, cardH}, bg);
        drawTextUnified(cards[i].title, Engine::Vec2{x + 12.0f, y + 10.0f}, 1.0f, Engine::Color{220, 240, 255, 240});
        drawTextUnified(cards[i].desc, Engine::Vec2{x + 12.0f, y + 36.0f}, 0.95f, Engine::Color{200, 220, 240, 230});
        std::ostringstream cost; cost << cards[i].cost << "c";
        bool affordable = credits_ >= cards[i].cost;
        Engine::Color costCol = affordable ? Engine::Color{180, 255, 200, 240}
                                           : Engine::Color{220, 160, 160, 220};
        drawTextUnified(cost.str(), Engine::Vec2{x + 12.0f, y + 62.0f}, 0.9f, costCol);
        drawTextUnified("Click to buy", Engine::Vec2{x + 12.0f, y + 84.0f}, 0.85f, Engine::Color{170, 200, 230, 200});
    }

    std::ostringstream wallet;
    wallet << "Credits: " << credits_;
    drawTextUnified(wallet.str(), Engine::Vec2{topLeft.x + panelW - 160.0f, topLeft.y + 12.0f}, 0.95f,
                    Engine::Color{200, 255, 200, 240});
}

void GameRoot::drawPauseOverlay() {
    if (!render_ || !paused_ || itemShopOpen_ || statShopOpen_ || levelChoiceOpen_ || defeated_) return;
    const float panelW = 320.0f;
    const float panelH = 200.0f;
    float cx = static_cast<float>(viewportWidth_) * 0.5f;
    float cy = static_cast<float>(viewportHeight_) * 0.5f;
    float x0 = cx - panelW * 0.5f;
    float y0 = cy - panelH * 0.5f;
    render_->drawFilledRect(Engine::Vec2{x0, y0}, Engine::Vec2{panelW, panelH}, Engine::Color{18, 26, 36, 230});
    drawTextUnified("Paused", Engine::Vec2{x0 + 18.0f, y0 + 16.0f}, 1.1f, Engine::Color{200, 230, 255, 240});

    float btnW = panelW - 40.0f;
    float btnH = 44.0f;
    float resumeX = x0 + 20.0f;
    float resumeY = y0 + 70.0f;
    float quitY = resumeY + btnH + 16.0f;
    auto drawBtn = [&](const std::string& label, float bx, float by, bool hovered) {
        Engine::Color bg = hovered ? Engine::Color{44, 70, 96, 240} : Engine::Color{32, 48, 64, 230};
        render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{btnW, btnH}, bg);
        drawTextUnified(label, Engine::Vec2{bx + 18.0f, by + 10.0f}, 1.0f,
                        hovered ? Engine::Color{230, 250, 255, 255} : Engine::Color{220, 240, 255, 240});
    };
    int mx = lastMouseX_;
    int my = lastMouseY_;
    bool hoverResume = mx >= resumeX && mx <= resumeX + btnW && my >= resumeY && my <= resumeY + btnH;
    bool hoverQuit = mx >= resumeX && mx <= resumeX + btnW && my >= quitY && my <= quitY + btnH;
    drawBtn("Resume", resumeX, resumeY, hoverResume);
    drawBtn("Main Menu", resumeX, quitY, hoverQuit);
}

void GameRoot::drawAbilityPanel() {
    if (!render_ || abilities_.empty()) return;
    const float scale = 1.15f;
    const float panelW = 320.0f * scale;
    const float panelH = 240.0f * scale;
    const float margin = 16.0f;
    float x = static_cast<float>(viewportWidth_) - panelW - margin;
    float y = static_cast<float>(viewportHeight_) - panelH - margin;
    render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{panelW, panelH}, Engine::Color{16, 24, 34, 210});
    const float headerH = 26.0f * scale;
    drawTextUnified("Abilities", Engine::Vec2{x + 12.0f, y + 10.0f}, 1.0f * scale, Engine::Color{200, 230, 255, 235});
    const float lineH = 30.0f * scale;
    const float hintH = 36.0f * scale;
    float listHeight = static_cast<float>(abilities_.size()) * lineH;
    float available = panelH - headerH - hintH - 24.0f;  // padding
    float listStartY = y + headerH + std::max(12.0f, (available - listHeight) * 0.5f);
    for (std::size_t i = 0; i < abilities_.size(); ++i) {
        const auto& ab = abilities_[i];
        float ly = listStartY + static_cast<float>(i) * lineH;
        bool focused = static_cast<int>(i) == abilityFocus_;
        Engine::Color keyCol{180, 210, 240, 230};
        Engine::Color nameCol{210, 235, 255, static_cast<uint8_t>(focused ? 255 : 235)};
        Engine::Color lvlCol{180, 220, 190, 230};
        Engine::Color bg{static_cast<uint8_t>(focused ? 32u : 24u), static_cast<uint8_t>(focused ? 44u : 36u),
                         56u, 220};
        render_->drawFilledRect(Engine::Vec2{x + 6.0f, ly - 2.0f}, Engine::Vec2{panelW - 12.0f, lineH - 2.0f}, bg);
        drawTextUnified(ab.keyHint, Engine::Vec2{x + 10.0f, ly}, 0.95f * scale, keyCol);
        drawTextUnified(ab.name, Engine::Vec2{x + 54.0f, ly}, 1.0f * scale, nameCol);
        std::ostringstream lv;
        lv << "Lv " << ab.level << "/" << ab.maxLevel;
        drawTextUnified(lv.str(), Engine::Vec2{x + panelW - 90.0f, ly}, 0.9f * scale, lvlCol);
        if (ab.cooldown > 0.0f && ab.cooldownMax > 0.0f) {
            float cdRatio = std::clamp(ab.cooldown / ab.cooldownMax, 0.0f, 1.0f);
            render_->drawFilledRect(Engine::Vec2{x + 8.0f, ly + 18.0f}, Engine::Vec2{(panelW - 16.0f) * cdRatio, 4.0f},
                                    Engine::Color{80, 140, 200, 220});
        }
    }
    // Separate hint block beneath ability rows
    float hintY = y + panelH - hintH + 6.0f;
    render_->drawFilledRect(Engine::Vec2{x + 6.0f, hintY - 10.0f}, Engine::Vec2{panelW - 12.0f, hintH},
                            Engine::Color{20, 30, 44, 220});
    std::ostringstream hint;
            hint << "Scroll swaps | F upgrades (Cost " << abilities_[abilityFocus_].upgradeCost << "c)";
    drawTextUnified(hint.str(), Engine::Vec2{x + 12.0f, hintY}, 0.95f * scale,
                    Engine::Color{170, 200, 230, 220});
}

void GameRoot::spawnHero() {
    hero_ = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Engine::ECS::Velocity>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Game::Facing>(hero_, Game::Facing{1});
    registry_.emplace<Engine::ECS::Renderable>(hero_, Engine::ECS::Renderable{Engine::Vec2{heroSize_, heroSize_},
                                                                               heroColorPreset_});
    const float heroHalf = heroSize_ * 0.5f;
    registry_.emplace<Engine::ECS::AABB>(hero_, Engine::ECS::AABB{Engine::Vec2{heroHalf, heroHalf}});
    registry_.emplace<Engine::ECS::Health>(hero_, Engine::ECS::Health{heroMaxHp_, heroMaxHp_});
    registry_.emplace<Engine::ECS::HeroTag>(hero_, Engine::ECS::HeroTag{});

    // Apply sprite if loaded.
    std::string heroPath = !heroTexturePath_.empty() ? heroTexturePath_ : manifest_.heroTexture;
    if (textureManager_ && !heroPath.empty()) {
        if (auto tex = loadTextureOptional(heroPath)) {
            if (auto* rend = registry_.get<Engine::ECS::Renderable>(hero_)) {
                rend->texture = tex;
            }
            registry_.emplace<Engine::ECS::SpriteAnimation>(hero_, Engine::ECS::SpriteAnimation{16, 16, 4, 0.14f});
        } else {
            Engine::logWarn("Failed to load hero texture: " + heroPath);
        }
    }

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
    xpToNext_ = xpBaseToLevel_;
    heroMaxHp_ = heroMaxHpPreset_;
    heroMoveSpeed_ = heroMoveSpeedPreset_;
    projectileDamage_ = projectileDamagePreset_;
    wave_ = std::max(0, startWaveBase_ - 1);
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
    dashTimer_ = 0.0f;
    dashCooldownTimer_ = 0.0f;
    dashInvulnTimer_ = 0.0f;
    dashDir_ = {0.0f, 0.0f};
    inventory_.clear();
    inventory_.shrink_to_fit();
    inventorySelected_ = -1;
    inventoryScrollLeftPrev_ = false;
    inventoryScrollRightPrev_ = false;
    shopkeeper_ = Engine::ECS::kInvalidEntity;
    interactPrev_ = false;
    useItemPrev_ = false;
    chainBounces_ = 0;

    if (waveSystem_) {
        waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettingsBase_);
        waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
        waveSystem_->setEnemyDefinitions(&enemyDefs_);
        waveSystem_->setTimer(waveWarmupBase_);
    }
    if (collisionSystem_) {
        collisionSystem_->setContactDamage(contactDamage_);
    }
    if (hotzoneSystem_) {
        hotzoneSystem_->initialize(registry_, 5);
    }
    refreshShopInventory();

    spawnHero();
    if (cameraSystem_) cameraSystem_->resetFollow(true);
    itemShopOpen_ = false;
    statShopOpen_ = false;
    shopLeftPrev_ = shopRightPrev_ = shopMiddlePrev_ = false;
    shopUnavailableTimer_ = 0.0;
    shopNoCreditTimer_ = 0.0;
    userPaused_ = false;
    paused_ = false;
    levelChoiceOpen_ = false;
    levelChoicePrevClick_ = false;
    for (auto& ab : abilities_) {
        ab.cooldown = 0.0f;
    }
    for (auto& st : abilityStates_) {
        st.cooldown = 0.0f;
    }
}

bool GameRoot::addItemToInventory(const ItemDefinition& def) {
    if (static_cast<int>(inventory_.size()) >= inventoryCapacity_) return false;
    inventory_.push_back(ItemInstance{def, 1});
    clampInventorySelection();
    // Apply passive bonuses for Unique items.
    switch (def.effect) {
        case ItemEffect::AttackSpeed:
            attackSpeedMul_ *= (1.0f + def.value);
            fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_);
            break;
        case ItemEffect::Lifesteal:
            lifestealPercent_ = std::max(lifestealPercent_, def.value);
            break;
        case ItemEffect::Chain:
            chainBounces_ = std::max(chainBounces_, static_cast<int>(def.value));
            break;
        default:
            break;
    }
    return true;
}

bool GameRoot::sellItemFromInventory(std::size_t idx, int& creditsOut) {
    if (idx >= inventory_.size()) return false;
    const auto& inst = inventory_[idx];
    int refund = std::max(1, inst.def.cost / 2);
    creditsOut += refund;
    inventory_.erase(inventory_.begin() + static_cast<std::ptrdiff_t>(idx));
    clampInventorySelection();
    return true;
}

void GameRoot::clampInventorySelection() {
    if (inventory_.empty()) {
        inventorySelected_ = -1;
        return;
    }
    if (inventorySelected_ < 0) inventorySelected_ = 0;
    if (inventorySelected_ >= static_cast<int>(inventory_.size())) {
        inventorySelected_ = static_cast<int>(inventory_.size()) - 1;
    }
}

}  // namespace Game
