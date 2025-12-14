#include "Game.h"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <limits>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <array>
#include <unordered_set>
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <ifaddrs.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#endif

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
#include "../engine/gameplay/Combat.h"
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
#include "components/EscortObjective.h"
#include "components/Spawner.h"
#include "components/Invulnerable.h"
#include "components/Shopkeeper.h"
#include "components/OffensiveType.h"
#include "components/HitFlash.h"
#include "systems/BuffSystem.h"
#include "meta/SaveManager.h"
#include "meta/GlobalUpgrades.h"
#include "meta/ItemDefs.h"
#include "systems/HotzoneSystem.h"
#include "../engine/gameplay/FogOfWar.h"
#include "../engine/render/FogOfWarRenderer.h"
#include "../engine/platform/SDLTexture.h"
#include <cmath>

namespace Game {

namespace {

inline Game::OffensiveType offensiveTypeFromString(const std::string& s) {
    if (s == "Melee") return Game::OffensiveType::Melee;
    if (s == "Plasma") return Game::OffensiveType::Plasma;
    if (s == "ThornTank") return Game::OffensiveType::ThornTank;
    if (s == "Builder") return Game::OffensiveType::Builder;
    return Game::OffensiveType::Ranged;
}

inline Game::MiniUnitClass miniUnitClassFromString(const std::string& s) {
    if (s == "heavy" || s == "Heavy") return Game::MiniUnitClass::Heavy;
    if (s == "medic" || s == "Medic") return Game::MiniUnitClass::Medic;
    return Game::MiniUnitClass::Light;
}

inline Game::BuildingType buildingTypeFromString(const std::string& s) {
    if (s == "barracks" || s == "Barracks") return Game::BuildingType::Barracks;
    if (s == "bunker" || s == "Bunker") return Game::BuildingType::Bunker;
    if (s == "house" || s == "House") return Game::BuildingType::House;
    return Game::BuildingType::Turret;
}

inline std::string buildingKey(Game::BuildingType type) {
    switch (type) {
        case Game::BuildingType::Turret: return "turret";
        case Game::BuildingType::Barracks: return "barracks";
        case Game::BuildingType::Bunker: return "bunker";
        case Game::BuildingType::House: return "house";
    }
    return "turret";
}

constexpr std::size_t offensiveTypeIndex(Game::OffensiveType type) {
    return static_cast<std::size_t>(type);
}

}  // namespace

// Forward decl for traveling shop dynamic pricing.
static int effectiveShopCost(ItemEffect eff,
                             int base,
                             int dmgPct,
                             int atkPct,
                             int vitals,
                             int cdr,
                             int rangeVision,
                             int charges,
                             int speedBoots,
                             int vitalAust);

bool GameRoot::onInitialize(Engine::Application& app) {
    Engine::logInfo("GameRoot initialized. Spawning placeholder hero entity.");
    app_ = &app;
    render_ = &app.renderer();
    SDL_StartTextInput();
    detectLocalIp();
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
    miniUnitSystem_ = std::make_unique<MiniUnitSystem>();
    enemyAISystem_ = std::make_unique<EnemyAISystem>();
    enemySpriteStateSystem_ = std::make_unique<EnemySpriteStateSystem>();
    heroSpriteStateSystem_ = std::make_unique<HeroSpriteStateSystem>();
    animationSystem_ = std::make_unique<AnimationSystem>();
    hitFlashSystem_ = std::make_unique<HitFlashSystem>();
    damageNumberSystem_ = std::make_unique<DamageNumberSystem>();
    buffSystem_ = std::make_unique<BuffSystem>();
    shopSystem_ = std::make_unique<ShopSystem>();
    pickupSystem_ = std::make_unique<PickupSystem>();
    eventSystem_ = std::make_unique<EventSystem>();
    hotzoneSystem_ = std::make_unique<HotzoneSystem>(static_cast<float>(hotzoneRotation_), 1.25f, 1.5f, 1.2f, 1.15f,
                                                     hotzoneMapRadius_, hotzoneRadiusMin_, hotzoneRadiusMax_,
                                                     hotzoneMinSeparation_, hotzoneSpawnClearance_, rng_());
    // Wave settings loaded later from gameplay config.
    textureManager_ = std::make_unique<Engine::TextureManager>(*render_);
    if (eventSystem_) eventSystem_->setTextureManager(textureManager_.get());
    loadProgress();
    netSession_ = std::make_unique<Game::Net::NetSession>();
    netSession_->setSnapshotProvider([this]() { return this->collectNetSnapshot(); });
    netSession_->setSnapshotConsumer([this](const Game::Net::SnapshotMsg& snap) { this->applyRemoteSnapshot(snap); });

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
        bindings_.buildMenu = {"v"};
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
    loadUnitDefinitions();
    loadPickupTextures();
    loadProjectileTextures();
    itemCatalog_ = defaultItemCatalog();
    goldCatalog_ = goldShopCatalog();
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
    float heroShields = heroShield_;
    float heroHealthArmor = heroHealthArmor_;
    float heroShieldArmor = heroShieldArmor_;
    float heroShieldRegen = heroShieldRegen_;
    float heroHealthRegen = heroHealthRegen_;
    float heroRegenDelay = heroRegenDelay_;
    float heroSize = heroSize_;
    float projectileSize = projectileSize_;
    float projectileHitbox = projectileHitboxSize_;
    float projectileLifetime = projectileLifetime_;
    double initialWarmup = waveWarmup_;
    double waveInterval = waveInterval_;
    double graceDuration = graceDuration_;
    int copperPerKill = 5;
    int waveClearBonus = waveClearBonus_;
    int enemyLowThreshold = enemyLowThreshold_;
    double combatDuration = combatDuration_;
    double intermissionDuration = intermissionDuration_;
    int bountyGold = bountyGoldBonus_;
    int bossWave = bossWave_;
    float bossHpMul = bossHpMultiplier_;
    float bossSpeedMul = bossSpeedMultiplier_;
    int bossGold = bossGoldBonus_;
    int eventGold = 1;
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
    float xpPerDmgDealt = xpPerDamageDealt_;
    float xpPerDmgTaken = xpPerDamageTaken_;
    int xpPerEvent = xpPerEvent_;
    int xpBase = xpToNext_;
    float xpGrowth = xpGrowth_;
    float levelHpBonus = levelHpBonus_;
    float levelDmgBonus = levelDmgBonus_;
    float levelSpeedBonus = levelSpeedBonus_;
    int wavesPerRoundBase = wavesPerRoundBase_;
    int wavesPerRoundGrowth = wavesPerRoundGrowthInterval_;
    int spawnBatchInterval = spawnBatchRoundInterval_;
    float energyMax = energyMax_;
    float energyRegen = energyRegen_;
    float energyIntermission = energyRegenIntermission_;
    {
        std::ifstream gp("data/gameplay.json");
        if (gp.is_open()) {
            nlohmann::json j;
            gp >> j;
            float globalSpeedMul = j.value("speedMultiplier", 1.0f);
            globalSpeedMul_ = std::clamp(globalSpeedMul, 0.1f, 3.0f);
            if (j.contains("enemy")) {
                waveSettings.enemyHp = j["enemy"].value("hp", waveSettings.enemyHp);
                waveSettings.enemyShields = j["enemy"].value("shields", waveSettings.enemyShields);
                waveSettings.enemyHealthArmor = j["enemy"].value("healthArmor", waveSettings.enemyHealthArmor);
                waveSettings.enemyShieldArmor = j["enemy"].value("shieldArmor", waveSettings.enemyShieldArmor);
                waveSettings.enemyShieldRegen = j["enemy"].value("shieldRegen", waveSettings.enemyShieldRegen);
                waveSettings.enemyRegenDelay = j["enemy"].value("regenDelay", waveSettings.enemyRegenDelay);
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
                heroShields = j["hero"].value("shields", heroShields);
                heroHealthArmor = j["hero"].value("healthArmor", heroHealthArmor);
                heroShieldArmor = j["hero"].value("shieldArmor", heroShieldArmor);
                heroShieldRegen = j["hero"].value("shieldRegen", heroShieldRegen);
                heroHealthRegen = j["hero"].value("healthRegen", heroHealthRegen);
                heroRegenDelay = j["hero"].value("regenDelay", heroRegenDelay);
            }
            if (j.contains("offensiveTypes") && j["offensiveTypes"].is_object()) {
                const auto& o = j["offensiveTypes"];
                auto loadOffense = [&](const std::string& key, Game::OffensiveType type) {
                    if (!o.contains(key)) return;
                    const auto& v = o[key];
                    const std::size_t idx = offensiveTypeIndex(type);
                    offensiveTypeModifiers_[idx].healthArmorBonus = v.value("healthArmorBonus", offensiveTypeModifiers_[idx].healthArmorBonus);
                    offensiveTypeModifiers_[idx].shieldArmorBonus = v.value("shieldArmorBonus", offensiveTypeModifiers_[idx].shieldArmorBonus);
                    offensiveTypeModifiers_[idx].healthRegenBonus = v.value("healthRegenBonus", offensiveTypeModifiers_[idx].healthRegenBonus);
                    offensiveTypeModifiers_[idx].shieldRegenBonus = v.value("shieldRegenBonus", offensiveTypeModifiers_[idx].shieldRegenBonus);
                };
                loadOffense("Ranged", Game::OffensiveType::Ranged);
                loadOffense("Melee", Game::OffensiveType::Melee);
                loadOffense("Plasma", Game::OffensiveType::Plasma);
                loadOffense("ThornTank", Game::OffensiveType::ThornTank);
                loadOffense("Builder", Game::OffensiveType::Builder);
            }
            if (j.contains("melee") && j["melee"].is_object()) {
                meleeConfig_.range = j["melee"].value("range", meleeConfig_.range);
                meleeConfig_.arcDegrees = j["melee"].value("arcDegrees", meleeConfig_.arcDegrees);
                meleeConfig_.damageMultiplier = j["melee"].value("damageMultiplier", meleeConfig_.damageMultiplier);
            }
            if (j.contains("plasma") && j["plasma"].is_object()) {
                plasmaConfig_.shieldDamageMultiplier = j["plasma"].value("shieldDamageMultiplier", plasmaConfig_.shieldDamageMultiplier);
                plasmaConfig_.healthDamageMultiplier = j["plasma"].value("healthDamageMultiplier", plasmaConfig_.healthDamageMultiplier);
            }
            if (j.contains("thornTank") && j["thornTank"].is_object()) {
                thornConfig_.reflectPercent = j["thornTank"].value("reflectPercent", thornConfig_.reflectPercent);
                thornConfig_.maxReflectPerHit = j["thornTank"].value("maxReflectPerHit", thornConfig_.maxReflectPerHit);
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
                copperPerKill = j["rewards"].value("currencyPerKill", copperPerKill);
                waveClearBonus = j["rewards"].value("waveClearBonus", waveClearBonus);
                bountyGold = j["rewards"].value("bountyBonus", bountyGold);
            }
            if (j.contains("gold")) {
                bossGold = j["gold"].value("boss", bossGold);
                eventGold = j["gold"].value("event", eventGold);
            }
            if (j.contains("xp")) {
                xpPerKill = j["xp"].value("perKill", xpPerKill);
                xpPerWave = j["xp"].value("perWave", xpPerWave);
                xpPerDmgDealt = j["xp"].value("perDamageDealt", xpPerDmgDealt);
                xpPerDmgTaken = j["xp"].value("perDamageTaken", xpPerDmgTaken);
                xpPerEvent = j["xp"].value("perEvent", xpPerEvent);
                xpBase = j["xp"].value("baseToLevel", xpBase);
                xpGrowth = j["xp"].value("growth", xpGrowth);
                levelHpBonus = j["xp"].value("hpBonus", levelHpBonus);
                levelDmgBonus = j["xp"].value("damageBonus", levelDmgBonus);
                levelSpeedBonus = j["xp"].value("speedBonus", levelSpeedBonus);
            }
            if (j.contains("rounds")) {
                wavesPerRoundBase = j["rounds"].value("baseWaves", wavesPerRoundBase);
                wavesPerRoundGrowth = j["rounds"].value("wavesPerRoundGrowthInterval", wavesPerRoundGrowth);
                spawnBatchInterval = j["rounds"].value("spawnBatchInterval", spawnBatchInterval);
            }
            if (j.contains("energy")) {
                energyMax = j["energy"].value("baseMax", energyMax);
                energyRegen = j["energy"].value("regenPerSecond", energyRegen);
                energyIntermission = j["energy"].value("intermissionRegenPerSecond", energyIntermission);
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
            if (j.contains("abilityShop")) {
                abilityShopBaseCost_ = j["abilityShop"].value("baseCost", abilityShopBaseCost_);
                abilityShopCostGrowth_ = j["abilityShop"].value("costGrowth", abilityShopCostGrowth_);
                abilityDamagePerLevel_ = j["abilityShop"].value("damagePerLevel", abilityDamagePerLevel_);
                abilityAttackSpeedPerLevel_ = j["abilityShop"].value("attackSpeedPerLevel", abilityAttackSpeedPerLevel_);
                abilityRangePerLevel_ = j["abilityShop"].value("rangePerLevel", abilityRangePerLevel_);
                abilityVisionPerLevel_ = j["abilityShop"].value("visionPerLevel", abilityVisionPerLevel_);
                abilityHealthPerLevel_ = j["abilityShop"].value("healthPerLevel", abilityHealthPerLevel_);
                abilityArmorPerLevel_ = j["abilityShop"].value("armorPerLevel", abilityArmorPerLevel_);
                abilityRangeMaxBonus_ = j["abilityShop"].value("rangeMax", abilityRangeMaxBonus_);
                abilityVisionMaxBonus_ = j["abilityShop"].value("visionMax", abilityVisionMaxBonus_);
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
                bossGold = j["boss"].value("killBonus", bossGold);
            }
            if (j.contains("drops")) {
                pickupDropChance_ = j["drops"].value("pickupChance", pickupDropChance_);
                pickupPowerupShare_ = j["drops"].value("powerupShare", pickupPowerupShare_);
                copperPickupMin_ = j["drops"].value("copperMin", copperPickupMin_);
                copperPickupMax_ = j["drops"].value("copperMax", copperPickupMax_);
                bossCopperDrop_ = j["drops"].value("bossCopper", bossCopperDrop_);
                miniBossCopperDrop_ = j["drops"].value("miniBossCopper", miniBossCopperDrop_);
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
        // Apply bonus charges multiplier to abilities that conceptually use charges (not modeled yet).
    }
    waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettings);
    waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
    waveSystem_->setEnemyDefinitions(&enemyDefs_);
    if (eventSystem_) eventSystem_->setEnemyDefinitions(&enemyDefs_);
    waveSystem_->setBaseArmor(Engine::Gameplay::BaseStats{waveSettings.enemyHealthArmor, waveSettings.enemyShieldArmor});
    waveSystem_->setSpawnBatchInterval(spawnBatchRoundInterval_);
    waveSystem_->setRound(round_);
    waveSettings.contactDamage = contactDamage;
    waveSettings.enemySpeed *= globalSpeedMul_;
    heroMoveSpeed *= globalSpeedMul_;
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
    heroShield_ = heroShields;
    heroShieldBase_ = heroShields;
    heroShieldPreset_ = heroShields;
    heroBaseStats_ = Engine::Gameplay::BaseStats{heroHealthArmor, heroShieldArmor};
    heroBaseStatsScaled_ = heroBaseStats_;
    heroHealthArmor_ = heroHealthArmor;
    heroShieldArmor_ = heroShieldArmor;
    heroShieldRegen_ = heroShieldRegen;
    heroHealthRegen_ = heroHealthRegen;
    heroShieldRegenBase_ = heroShieldRegen;
    heroHealthRegenBase_ = heroHealthRegen;
    heroRegenDelayBase_ = heroRegenDelay;
    heroRegenDelay_ = heroRegenDelay;
    projectileDamagePreset_ = projectileDamage_;
    heroSize_ = heroSize;
    waveWarmupBase_ = initialWarmup;
    waveWarmup_ = waveWarmupBase_;
    waveInterval_ = waveInterval;
    graceDuration_ = graceDuration;
    copperPerKill_ = copperPerKill;
    waveClearBonus_ = waveClearBonus;
    enemyLowThreshold_ = enemyLowThreshold;
    combatDuration_ = combatDuration;
    intermissionDuration_ = intermissionDuration;
    bountyGoldBonus_ = bountyGold;
    bossWave_ = bossWave;
    bossHpMultiplier_ = bossHpMul;
    bossSpeedMultiplier_ = bossSpeedMul;
    bossGoldBonus_ = bossGold;
    eventGoldReward_ = eventGold;
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
    xpPerDamageDealt_ = xpPerDmgDealt;
    xpPerDamageTaken_ = xpPerDmgTaken;
    xpPerEvent_ = xpPerEvent;
    xpBaseToLevel_ = xpBase;
    xpToNext_ = xpBaseToLevel_;
    xpGrowth_ = xpGrowth;
    energy_ = energyMax_;
    wavesPerRoundBase_ = std::max(1, wavesPerRoundBase);
    wavesPerRoundGrowthInterval_ = std::max(1, wavesPerRoundGrowth);
    spawnBatchRoundInterval_ = std::max(1, spawnBatchInterval);
    energyMax_ = std::max(1.0f, energyMax);
    energyRegen_ = std::max(0.0f, energyRegen);
    energyRegenIntermission_ = std::max(0.0f, energyIntermission);
    levelHpBonus_ = levelHpBonus;
    levelDmgBonus_ = levelDmgBonus;
    levelSpeedBonus_ = levelSpeedBonus;
    fireInterval_ = 1.0 / fireRate;
    fireIntervalBase_ = fireInterval_;
    fireCooldown_ = 0.0;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);
    if (collisionSystem_) collisionSystem_->setThornConfig(thornConfig_.reflectPercent, thornConfig_.maxReflectPerHit);
    heroVisionRadiusBaseTiles_ = heroVisionRadiusTiles_;

    rebuildFogLayer();

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
    hostConfig_.difficultyId = activeDifficulty_.id;
    hostConfig_.lobbyName = "Zeta Lobby";
    localLobbyHeroId_ = activeArchetype_.id;
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
    updateNetwork(step.deltaSeconds);
    if (multiplayerEnabled_) {
        bool remoteAlive = false;
        for (const auto& [id, state] : remoteStates_) {
            (void)id;
            if (state.alive) {
                remoteAlive = true;
                break;
            }
        }
        bool localAlive = true;
        if (const auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            localAlive = hp->currentHealth > 0.0f;
        }
        if (!localAlive && !remoteAlive && !defeated_) {
            defeated_ = true;
            paused_ = true;
        }
    }

        const bool restartPressed = actions.restart && !restartPrev_;
        restartPrev_ = actions.restart;
        const bool shopToggleEdge = actions.toggleShop && !shopTogglePrev_;
        shopTogglePrev_ = actions.toggleShop;
    if (shopToggleEdge && !inMenu_) {
        abilityShopOpen_ = !abilityShopOpen_;
    }
    const bool pausePressed = actions.pause && !pauseTogglePrev_;
    pauseTogglePrev_ = actions.pause;
    bool menuBackEdge = actions.menuBack && !menuBackPrev_;
    menuBackPrev_ = actions.menuBack;
    const bool buildMenuEdge = actions.buildMenu && !buildMenuPrev_;
    buildMenuPrev_ = actions.buildMenu;
    if (buildMenuEdge && !inMenu_ && activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
        buildMenuOpen_ = !buildMenuOpen_;
        if (!buildMenuOpen_) {
            clearBuildPreview();
        }
    }
    if (activeArchetype_.offensiveType != Game::OffensiveType::Builder) {
        buildMenuOpen_ = false;
        clearBuildPreview();
    }
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
        abilityShopOpen_ = false;
        levelChoiceOpen_ = false;
        pauseMenuBlink_ = 0.0;
        pauseClickPrev_ = false;
    }
    paused_ = userPaused_ || itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_;
    if (actions.toggleFollow && (itemShopOpen_ || abilityShopOpen_)) {
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

    // Inventory selection (Tab cycles forward).
    bool invCycleEdge = input.isDown(Engine::InputKey::InventoryCycle) && !inventoryCyclePrev_;
    inventoryCyclePrev_ = input.isDown(Engine::InputKey::InventoryCycle);
    if (inventory_.empty()) {
        inventorySelected_ = -1;
    } else {
        clampInventorySelection();
        if (invCycleEdge) {
            inventorySelected_ = (inventorySelected_ + 1) % static_cast<int>(inventory_.size());
        }
    }

    // Timers.
        if (shopNoFundsTimer_ > 0.0) {
            shopNoFundsTimer_ -= step.deltaSeconds;
            if (shopNoFundsTimer_ < 0.0) shopNoFundsTimer_ = 0.0;
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
    if (paused_ && !itemShopOpen_ && !abilityShopOpen_ && !levelChoiceOpen_ && !defeated_) {
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
                paused_ = itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_;
                pauseMenuBlink_ = 0.0;
            } else if (inside(resumeX, quitY)) {
                bool hostQuit = netSession_ && netSession_->isHost();
                leaveNetworkSession(hostQuit);
                resetRun();
                inMenu_ = true;
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
                userPaused_ = false;
                paused_ = false;
                itemShopOpen_ = false;
                abilityShopOpen_ = false;
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

    // Mini-unit selection and orders (RTS-style). Available in both movement modes.
    if (!inMenu_ && !paused_) {
        const bool leftDown = input.isMouseButtonDown(0);
        const bool rightDown = input.isMouseButtonDown(2);
        Engine::Vec2 worldMouse = Engine::Gameplay::mouseWorldPosition(input, camera_, viewportWidth_, viewportHeight_);
        const bool leftEdge = leftDown && !miniSelectMousePrev_;
        const bool leftRelease = !leftDown && miniSelectMousePrev_;

        // Avoid starting selection when clicking UI panels.
        auto pointInRect = [&](int mx, int my, float rx, float ry, float rw, float rh) {
            return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh;
        };
        bool mouseOverUI = false;
        if (buildMenuOpen_ && activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
            const float panelW = 260.0f;
            const float panelH = 220.0f;
            const float rx = 22.0f;
            const float ry = static_cast<float>(viewportHeight_) - panelH - 22.0f;
            mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }
        if (selectedBuilding_ != Engine::ECS::kInvalidEntity &&
            registry_.has<Game::Building>(selectedBuilding_) &&
            registry_.has<Engine::ECS::Health>(selectedBuilding_)) {
            const auto* b = registry_.get<Game::Building>(selectedBuilding_);
            const float panelW = 270.0f;
            const float panelH = (b && b->type == Game::BuildingType::Barracks) ? 170.0f : 120.0f;
            const float rx = 20.0f;
            const float ry = static_cast<float>(viewportHeight_) - panelH - 20.0f;
            mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }
        if (!selectedMiniUnits_.empty()) {
            const float hudW = 300.0f;
            const float hudH = 70.0f;
            const float hudX = static_cast<float>(viewportWidth_) * 0.5f - hudW * 0.5f;
            const float hudY = static_cast<float>(viewportHeight_) - hudH - 18.0f;
            mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, hudX, hudY, hudW, hudH);
        }

        if (leftEdge && !mouseOverUI) {
            selectingMiniUnits_ = true;
            selectionStart_ = worldMouse;
            selectionEnd_ = worldMouse;
        }
        if (leftDown && selectingMiniUnits_) {
            selectionEnd_ = worldMouse;
        }
        if (leftRelease) {
            selectingMiniUnits_ = false;
            Engine::Vec2 start = selectionStart_;
            Engine::Vec2 end = selectionEnd_;
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            const float dragThreshold = 4.0f;
            if (std::abs(dx) < dragThreshold && std::abs(dy) < dragThreshold) {
                Engine::ECS::Entity picked = Engine::ECS::kInvalidEntity;
                Game::MiniUnitClass pickedClass = Game::MiniUnitClass::Light;
                registry_.view<Engine::ECS::Transform, Game::MiniUnit>(
                    [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Game::MiniUnit& mu) {
                        const float pickRadius = 12.0f;
                        float px = tf.position.x - worldMouse.x;
                        float py = tf.position.y - worldMouse.y;
                        if ((px * px + py * py) <= pickRadius * pickRadius) {
                            picked = e;
                            pickedClass = mu.cls;
                        }
                    });
                if (picked != Engine::ECS::kInvalidEntity) {
                    const double now = step.elapsedSeconds;
                    bool doubleClick = lastMiniSelectClickTime_ > 0.0 && (now - lastMiniSelectClickTime_) < 0.35;
                    lastMiniSelectClickTime_ = now;
                    selectedMiniUnits_.clear();
                    if (doubleClick) {
                        registry_.view<Engine::ECS::Transform, Game::MiniUnit>(
                            [&](Engine::ECS::Entity e, const Engine::ECS::Transform&, const Game::MiniUnit& mu) {
                                if (mu.cls == pickedClass) {
                                    selectedMiniUnits_.push_back(e);
                                }
                            });
                    } else {
                        selectedMiniUnits_.push_back(picked);
                    }
                } else {
                    selectedMiniUnits_.clear();
                }
            } else {
                float minX = std::min(start.x, end.x);
                float maxX = std::max(start.x, end.x);
                float minY = std::min(start.y, end.y);
                float maxY = std::max(start.y, end.y);
                selectedMiniUnits_.clear();
                registry_.view<Engine::ECS::Transform, Game::MiniUnit>(
                    [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Game::MiniUnit&) {
                        if (tf.position.x >= minX && tf.position.x <= maxX && tf.position.y >= minY &&
                            tf.position.y <= maxY) {
                            selectedMiniUnits_.push_back(e);
                        }
                    });
            }
        }
        miniSelectMousePrev_ = leftDown;

        if (rightDown && !miniRightClickPrev_ && !selectedMiniUnits_.empty()) {
            for (auto e : selectedMiniUnits_) {
                if (auto* cmd = registry_.get<Game::MiniUnitCommand>(e)) {
                    cmd->hasOrder = true;
                    cmd->target = worldMouse;
                } else {
                    registry_.emplace<Game::MiniUnitCommand>(e, Game::MiniUnitCommand{true, worldMouse});
                }
            }
        }
        miniRightClickPrev_ = rightDown;

        // Selection HUD combat toggle (bottom-center).
        const float hudW = 300.0f;
        const float hudH = 70.0f;
        const float hudX = static_cast<float>(viewportWidth_) * 0.5f - hudW * 0.5f;
        const float hudY = static_cast<float>(viewportHeight_) - hudH - 18.0f;
        const float toggleW = 92.0f;
        const float toggleH = 26.0f;
        const float toggleX = hudX + hudW - toggleW - 12.0f;
        const float toggleY = hudY + hudH - toggleH - 10.0f;
        bool hudLeftEdge = leftDown && !miniHudClickPrev_;
        if (hudLeftEdge && !selectingMiniUnits_ && !selectedMiniUnits_.empty() &&
            pointInRect(lastMouseX_, lastMouseY_, toggleX, toggleY, toggleW, toggleH)) {
            bool anyOff = false;
            for (auto ent : selectedMiniUnits_) {
                if (auto* mu = registry_.get<Game::MiniUnit>(ent)) {
                    if (!mu->combatEnabled) { anyOff = true; break; }
                }
            }
            bool newState = anyOff; // if any are off, turn all on, else turn all off
            for (auto ent : selectedMiniUnits_) {
                if (auto* mu = registry_.get<Game::MiniUnit>(ent)) {
                    mu->combatEnabled = newState;
                }
            }
        }
        miniHudClickPrev_ = leftDown;
    } else {
        selectingMiniUnits_ = false;
        miniSelectMousePrev_ = false;
        miniRightClickPrev_ = false;
        miniHudClickPrev_ = false;
    }

    // Update hero velocity from actions.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
        Engine::Vec2 heroPos = heroTf ? heroTf->position : Engine::Vec2{0.0f, 0.0f};
        const bool leftClick = input.isMouseButtonDown(0);
        const bool rightClick = input.isMouseButtonDown(2);
        const bool midClick = input.isMouseButtonDown(1);

        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            if (paused_ || defeatDelayActive_) {
                vel->value = {0.0f, 0.0f};
            } else if (movementMode_ == MovementMode::Modern) {
                vel->value = {actions.moveX * heroMoveSpeed_, actions.moveY * heroMoveSpeed_};
            } else {
                const bool allowCommands = !abilityShopOpen_ && !itemShopOpen_ && !levelChoiceOpen_;
                const bool moveEdge = allowCommands && rightClick && !moveCommandPrev_ && selectedMiniUnits_.empty();
                moveCommandPrev_ = rightClick;
                if (moveEdge) {
                    moveTarget_ = Engine::Gameplay::mouseWorldPosition(input, camera_, viewportWidth_, viewportHeight_);
                    moveTargetActive_ = true;
                    moveMarkerTimer_ = 1.2f;
                }
                Engine::Vec2 desiredVel{0.0f, 0.0f};
                if (moveTargetActive_ && heroTf) {
                    Engine::Vec2 toTarget{moveTarget_.x - heroPos.x, moveTarget_.y - heroPos.y};
                    float dist2 = toTarget.x * toTarget.x + toTarget.y * toTarget.y;
                    const float arrive = moveArriveRadius_;
                    if (dist2 <= arrive * arrive) {
                        moveTargetActive_ = false;
                        moveMarkerTimer_ = std::max(moveMarkerTimer_, 0.35f);
                    } else {
                        float dist = std::sqrt(dist2);
                        float invLen = heroMoveSpeed_ / std::max(dist, 0.0001f);
                        desiredVel = {toTarget.x * invLen, toTarget.y * invLen};
                    }
                }
                vel->value = desiredVel;
            }
        }
        // Dash trigger.
        const bool dashEdge = actions.dash && !dashPrev_;
        dashPrev_ = actions.dash;
        if (!paused_ && !defeatDelayActive_ && dashEdge && dashCooldownTimer_ <= 0.0f) {
            Engine::Vec2 dir{actions.moveX, actions.moveY};
            if (movementMode_ == MovementMode::RTS) {
                if (moveTargetActive_) {
                    dir = {moveTarget_.x - heroPos.x, moveTarget_.y - heroPos.y};
                } else {
                    dir = {mouseWorld_.x - heroPos.x, mouseWorld_.y - heroPos.y};
                }
            }
            float len2 = dir.x * dir.x + dir.y * dir.y;
            if (len2 < 0.01f) {
                dir = {mouseWorld_.x - heroPos.x, mouseWorld_.y - heroPos.y};
                len2 = dir.x * dir.x + dir.y * dir.y;
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
        if (abilityShopOpen_ || itemShopOpen_) {
            // Ability shop point-click purchase: any left click buys the currently selected (hovered) card.
            if (abilityShopOpen_) {
                auto costForLevel = [&](int level) {
                    return static_cast<int>(std::round(static_cast<float>(abilityShopBaseCost_) *
                                                       std::pow(abilityShopCostGrowth_, static_cast<float>(level))));
                };
                struct AbilityCard {
                    int maxLevel{999};
                    int level{0};
                    int cost{0};
                    std::function<bool()> buy;
                };
                auto buildCards = [&]() {
                    std::array<AbilityCard, 6> cards{};
                    cards[0] = AbilityCard{999, abilityDamageLevel_, costForLevel(abilityDamageLevel_), [&]() {
                        int cost = costForLevel(abilityDamageLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityDamageLevel_ += 1;
                        projectileDamage_ += abilityDamagePerLevel_;
                        return true;
                    }};
                    cards[1] = AbilityCard{999, abilityAttackSpeedLevel_, costForLevel(abilityAttackSpeedLevel_), [&]() {
                        int cost = costForLevel(abilityAttackSpeedLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityAttackSpeedLevel_ += 1;
                        attackSpeedMul_ *= (1.0f + abilityAttackSpeedPerLevel_);
                        fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_);
                        return true;
                    }};
                    cards[2] = AbilityCard{abilityRangeMaxBonus_, abilityRangeLevel_, costForLevel(abilityRangeLevel_), [&]() {
                        if (abilityRangeLevel_ >= abilityRangeMaxBonus_) return false;
                        int cost = costForLevel(abilityRangeLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityRangeLevel_ += 1;
                        autoFireRangeBonus_ += abilityRangePerLevel_;
                        projectileLifetime_ += abilityRangePerLevel_ / std::max(1.0f, projectileSpeed_);
                        return true;
                    }};
                    cards[3] = AbilityCard{abilityVisionMaxBonus_, abilityVisionLevel_, costForLevel(abilityVisionLevel_), [&]() {
                        if (abilityVisionLevel_ >= abilityVisionMaxBonus_) return false;
                        int cost = costForLevel(abilityVisionLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityVisionLevel_ += 1;
                        heroVisionRadiusTiles_ = heroVisionRadiusBaseTiles_ + abilityVisionLevel_ * abilityVisionPerLevel_;
                        return true;
                    }};
                    cards[4] = AbilityCard{999, abilityHealthLevel_, costForLevel(abilityHealthLevel_), [&]() {
                        int cost = costForLevel(abilityHealthLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityHealthLevel_ += 1;
                        heroMaxHp_ += abilityHealthPerLevel_;
                        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                            hp->maxHealth = heroMaxHp_;
                            hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + abilityHealthPerLevel_);
                            heroUpgrades_.groundArmorLevel += 1;
                            Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
                        }
                        return true;
                    }};
                    cards[5] = AbilityCard{999, abilityArmorLevel_, costForLevel(abilityArmorLevel_), [&]() {
                        int cost = costForLevel(abilityArmorLevel_);
                        if (copper_ < cost) return false;
                        copper_ -= cost;
                        abilityArmorLevel_ += 1;
                        heroHealthArmor_ += abilityArmorPerLevel_;
                        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                            hp->healthArmor = heroHealthArmor_;
                            Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
                        }
                        return true;
                    }};
                    return cards;
                };
                auto cards = buildCards();

                // Track hovered card to update selection.
                const float cardW = 200.0f;
                const float cardH = 110.0f;
                const float gap = 14.0f;
                float cx = static_cast<float>(viewportWidth_) * 0.5f;
                float cy = static_cast<float>(viewportHeight_) * 0.55f;
                float startX = cx - (cardW * 3.0f + gap * 2.0f) * 0.5f;
                float startY = cy - 320.0f * 0.5f + 60.0f;
                int mx = input.mouseX();
                int my = input.mouseY();
                int hoveredCard = -1;
                for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
                    float x = startX + (i % 3) * (cardW + gap);
                    float y = startY + (i / 3) * (cardH + 12.0f);
                    if (mx >= x && mx <= x + cardW && my >= y && my <= y + cardH) {
                        hoveredCard = i;
                        // Refresh inventory after a capped item purchase so prices/caps update.
                        refreshShopInventory();
                        break;
                    }
                }
                bool leftEdge = leftClick && !shopLeftPrev_;
                if (leftEdge && hoveredCard >= 0) {
                    if (!cards[hoveredCard].buy()) {
                        shopNoFundsTimer_ = 0.6;
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
                        const ItemDefinition& item = shopInventory_[i];
                        int dynCost = effectiveShopCost(item.effect, item.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                                        shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                                        shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_);
                        if (dynCost < 0) {
                            continue;  // capped; ignore click
                        }
                        if (gold_ < dynCost) {
                            shopNoFundsTimer_ = 0.6;
                            break;
                        }
                        gold_ -= dynCost;
                        switch (item.effect) {
                            case ItemEffect::Damage:
                                projectileDamage_ += item.value;
                                break;
                            case ItemEffect::Health:
                                heroMaxHp_ += item.value;
                                if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                    hp->maxHealth = heroMaxHp_;
                                    hp->currentHealth = std::min(hp->currentHealth + item.value, hp->maxHealth);
                                    heroUpgrades_.groundArmorLevel += 1;
                                    Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
                                }
                                break;
                            case ItemEffect::Speed:
                                heroMoveSpeed_ += item.value;
                                break;
                            case ItemEffect::Heal:
                                if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                    hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + item.value * hp->maxHealth);
                                }
                                break;
                            case ItemEffect::DamagePercent:
                                projectileDamage_ *= (1.0f + item.value);
                                shopDamagePctPurchases_ += 1;
                                break;
                            case ItemEffect::RangeVision:
                                autoFireRangeBonus_ += item.value * abilityRangePerLevel_;
                                projectileLifetime_ += (item.value * abilityRangePerLevel_) / std::max(1.0f, projectileSpeed_);
                                heroVisionRadiusTiles_ += item.value;
                                shopRangeVisionPurchases_ += 1;
                                break;
                            case ItemEffect::AttackSpeedPercent:
                                attackSpeedMul_ *= (1.0f + item.value);
                                fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_);
                                shopAttackSpeedPctPurchases_ += 1;
                                break;
                            case ItemEffect::MoveSpeedFlat:
                                heroMoveSpeed_ += item.value;
                                shopSpeedBootsPurchases_ += 1;
                                break;
                            case ItemEffect::BonusVitalsPercent: {
                                float mul = 1.0f + item.value;
                                heroMaxHp_ *= mul;
                                heroShield_ *= mul;
                                energyMax_ *= mul;
                                if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                    hp->maxHealth = heroMaxHp_;
                                    hp->currentHealth = heroMaxHp_;
                                    hp->maxShields = heroShield_;
                                    hp->currentShields = heroShield_;
                                }
                                energy_ = energyMax_;
                                shopVitalsPctPurchases_ += 1;
                                break;
                            }
                            case ItemEffect::CooldownFaster:
                                abilityCooldownMul_ = std::max(0.1f, abilityCooldownMul_ * (1.0f - item.value));
                                shopCooldownPurchases_ += 1;
                                break;
                            case ItemEffect::AbilityCharges:
                                abilityChargesBonus_ += static_cast<int>(item.value);
                                shopChargePurchases_ += 1;
                                break;
                            case ItemEffect::VitalCostReduction:
                                abilityVitalCostMul_ = std::max(0.1f, abilityVitalCostMul_ * (1.0f - item.value));
                                for (auto& slot : abilities_) {
                                    slot.energyCost *= abilityVitalCostMul_;
                                }
                                shopVitalAusterityPurchases_ += 1;
                                break;
                            default:
                                break;
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
                        sellItemFromInventory(i, gold_);
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
                        paused_ = userPaused_ || abilityShopOpen_;
                    } else {
                        itemShopOpen_ = true;
                        paused_ = userPaused_ || itemShopOpen_ || abilityShopOpen_;
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
                auto isConsumable = [](const ItemDefinition& d) {
                    return d.effect == ItemEffect::Heal || d.effect == ItemEffect::FreezeTime ||
                           d.effect == ItemEffect::Turret || d.effect == ItemEffect::Lifesteal ||
                           d.effect == ItemEffect::AttackSpeed || d.effect == ItemEffect::Chain;
                };
                // Prefer the selected consumable (support or lifesteal); otherwise pick the first available.
                int targetIdx = -1;
                if (inventorySelected_ >= 0 && inventorySelected_ < static_cast<int>(inventory_.size()) &&
                    isConsumable(inventory_[inventorySelected_].def)) {
                    targetIdx = inventorySelected_;
                } else {
                    for (std::size_t i = 0; i < inventory_.size(); ++i) {
                        if (isConsumable(inventory_[i].def)) { targetIdx = static_cast<int>(i); break; }
                    }
                }
                if (targetIdx != -1) {
                    const auto& inst = inventory_[targetIdx];
                    switch (inst.def.effect) {
                        case ItemEffect::Heal: {
                            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + inst.def.value * hp->maxHealth);
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
                            t.visEntity = registry_.create();
                            registry_.emplace<Engine::ECS::Transform>(t.visEntity, t.pos);
                            Engine::TexturePtr tex = pickupTurretTex_;
                            Engine::Vec2 size{16.0f, 16.0f};
                            registry_.emplace<Engine::ECS::Renderable>(t.visEntity,
                                Engine::ECS::Renderable{size, Engine::Color{255, 255, 255, 255}, tex});
                            turrets_.push_back(t);
                            break;
                        }
                        case ItemEffect::Lifesteal: {
                            lifestealBuff_ = inst.def.value;
                            lifestealPercent_ = lifestealBuff_;
                            lifestealTimer_ = std::max(lifestealTimer_, 60.0);
                            break;
                        }
                        case ItemEffect::AttackSpeed: {
                            attackSpeedBuffMul_ = std::max(attackSpeedBuffMul_, 1.0f + inst.def.value);
                            chronoTimer_ = std::max(chronoTimer_, 60.0);
                            fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);
                            break;
                        }
                        case ItemEffect::Chain: {
                            chainBonusTemp_ = std::max(chainBonusTemp_, static_cast<int>(std::round(inst.def.value)));
                            stormTimer_ = std::max(stormTimer_, 60.0);
                            chainBounces_ = chainBase_ + chainBonusTemp_;
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
            if (dashInvulnTimer_ < 0.0f) dashInvulnTimer_ = 0.0f;
        }
        // Primary/auto fire spawns projectile toward target.
        fireCooldown_ -= step.deltaSeconds;
        if (!paused_ && !defeatDelayActive_ && fireCooldown_ <= 0.0) {
            const auto* heroHp = registry_.get<Engine::ECS::Health>(hero_);
            if (heroHp && !heroHp->alive()) {
                // Do not attack while downed/knocked down.
            } else {
            Game::OffensiveType heroOffense = Game::OffensiveType::Ranged;
            if (auto* ot = registry_.get<Game::OffensiveTypeTag>(hero_)) {
                heroOffense = ot->type;
            }
            switch (heroOffense) {
                case Game::OffensiveType::Ranged:
                    performRangedAutoFire(step, actions, Game::OffensiveType::Ranged);
                    break;
                case Game::OffensiveType::Plasma:
                    performRangedAutoFire(step, actions, Game::OffensiveType::Plasma);
                    break;
                case Game::OffensiveType::Melee:
                    performMeleeAttack(step, actions);
                    break;
                case Game::OffensiveType::ThornTank:
                    performMeleeAttack(step, actions);
                    break;
                case Game::OffensiveType::Builder:
                    performRangedAutoFire(step, actions, Game::OffensiveType::Ranged);
                    break;
            }
            }
        }
    }

    if (miniUnitSystem_ && !paused_) {
        miniUnitSystem_->update(registry_, step);
    }

    // Buildings that auto-produce mini units (barracks).
    if (!paused_) {
        registry_.view<Game::Building, Engine::ECS::Transform>(
            [&](Engine::ECS::Entity, Game::Building& b, Engine::ECS::Transform& tf) {
                auto defIt = buildingDefs_.find(buildingKey(b.type));
                if (defIt == buildingDefs_.end()) return;
                const BuildingDef& def = defIt->second;
                if (b.type == Game::BuildingType::Barracks && def.autoSpawn && def.spawnInterval > 0.0f) {
                    b.spawnTimer -= static_cast<float>(step.deltaSeconds);
                    if (b.spawnTimer <= 0.0f) {
                        auto miniIt = miniUnitDefs_.find("light");
                        if (miniIt != miniUnitDefs_.end()) {
                            if (spawnMiniUnit(miniIt->second, tf.position)) {
                                b.spawnTimer = def.spawnInterval;
                            } else {
                                b.spawnTimer = std::min(def.spawnInterval, 1.0f);  // retry soon if blocked by supply/copper
                            }
                        }
                    }
                }
            });
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
                registry_.emplace<Game::LookDirection>(e, Game::LookDirection{});
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
    if (frenzyTimer_ > 0.0f) {
        frenzyTimer_ -= static_cast<float>(step.deltaSeconds);
        if (frenzyTimer_ <= 0.0f) {
            frenzyTimer_ = 0.0f;
            frenzyRateBuff_ = 1.0f;
        }
    }
    if (immortalTimer_ > 0.0f) {
        immortalTimer_ -= static_cast<float>(step.deltaSeconds);
        if (immortalTimer_ < 0.0f) immortalTimer_ = 0.0f;
    }

    // Keep invulnerable marker in sync with dash/immortal timers.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        float invRemaining = std::max(dashInvulnTimer_, immortalTimer_);
        if (invRemaining > 0.0f) {
            if (auto* inv = registry_.get<Game::Invulnerable>(hero_)) {
                inv->timer = invRemaining;
            } else {
                registry_.emplace<Game::Invulnerable>(hero_, Game::Invulnerable{invRemaining});
            }
        } else if (registry_.has<Game::Invulnerable>(hero_)) {
            registry_.remove<Game::Invulnerable>(hero_);
        }
    }
    // Tick turrets (timers only; firing handled in combat loop).
    for (auto it = turrets_.begin(); it != turrets_.end();) {
        it->timer -= static_cast<float>(step.deltaSeconds);
        it->fireCooldown = std::max(0.0f, it->fireCooldown - static_cast<float>(step.deltaSeconds));
        if (it->timer <= 0.0f) {
            if (it->visEntity != Engine::ECS::kInvalidEntity && registry_.has<Engine::ECS::Transform>(it->visEntity)) {
                registry_.destroy(it->visEntity);
            }
            it = turrets_.erase(it);
        } else {
            ++it;
        }
    }

    Engine::ActionState cameraActions = actions;
    if (movementMode_ == MovementMode::RTS) {
        cameraActions.camX = std::clamp(actions.camX + actions.moveX, -1.0f, 1.0f);
        cameraActions.camY = std::clamp(actions.camY + actions.moveY, -1.0f, 1.0f);
    }

    // Camera system.
    if (cameraSystem_) {
        cameraSystem_->update(camera_, registry_, hero_, cameraActions, input, step, viewportWidth_, viewportHeight_);
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
                Engine::TexturePtr turretTex = projectileTexTurret_ ? projectileTexTurret_ : projectileTexRed_;
                float sz = projectileSize_;
                if (turretTex) sz = static_cast<float>(turretTex->width());
                registry_.emplace<Engine::ECS::Renderable>(proj,
                                                           Engine::ECS::Renderable{Engine::Vec2{sz, sz},
                                                                                   Engine::Color{120, 220, 255, 230},
                                                                                   turretTex});
                Engine::Gameplay::DamageEvent dmgEvent{};
                dmgEvent.baseDamage = projectileDamage_ * 0.8f;
                dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Light] = 1.0f;
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                   dmgEvent, projectileLifetime_, lifestealPercent_, chainBounces_});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
            }
            // keep turret marker on its position
            if (t.visEntity != Engine::ECS::kInvalidEntity) {
                if (auto* tf = registry_.get<Engine::ECS::Transform>(t.visEntity)) {
                    tf->position = t.pos;
                }
            }
        }
    }

    // Placed turret buildings fire at nearest enemy in range.
    if (!paused_) {
        registry_.view<Game::Building, Engine::ECS::Transform, Engine::ECS::Health>(
            [&](Engine::ECS::Entity be, Game::Building& b, Engine::ECS::Transform& tf, Engine::ECS::Health& hp) {
                if (b.type != Game::BuildingType::Turret) return;
                if (!hp.alive()) return;
                auto defIt = buildingDefs_.find(buildingKey(b.type));
                if (defIt == buildingDefs_.end()) return;
                const BuildingDef& def = defIt->second;
                if (def.attackRange <= 0.0f || def.attackRate <= 0.0f || def.damage <= 0.0f) return;

                b.spawnTimer = std::max(0.0f, b.spawnTimer - static_cast<float>(step.deltaSeconds));
                if (b.spawnTimer > 0.0f) return;

                Engine::Vec2 dir{0.0f, 0.0f};
                float bestDist2 = def.attackRange * def.attackRange;
                Engine::Vec2 bestPos{};
                bool found = false;
                registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                    [&](Engine::ECS::Entity, const Engine::ECS::Transform& etf, const Engine::ECS::Health& ehp, Engine::ECS::EnemyTag&) {
                        if (!ehp.alive()) return;
                        float dx = etf.position.x - tf.position.x;
                        float dy = etf.position.y - tf.position.y;
                        float d2 = dx * dx + dy * dy;
                        if (d2 <= bestDist2) {
                            bestDist2 = d2;
                            bestPos = etf.position;
                            found = true;
                        }
                    });
                if (!found || bestDist2 < 0.0001f) {
                    b.spawnTimer = 0.2f; // retry soon if no target
                    return;
                }
                float inv = 1.0f / std::sqrt(bestDist2);
                dir = {(bestPos.x - tf.position.x) * inv, (bestPos.y - tf.position.y) * inv};

                auto proj = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(proj, tf.position);
                registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{0.0f, 0.0f});
                const float halfSize = projectileHitboxSize_ * 0.5f;
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
                Engine::TexturePtr turretTex = projectileTexTurret_ ? projectileTexTurret_ : projectileTexRed_;
                float sz = projectileSize_;
                if (turretTex) sz = static_cast<float>(turretTex->width());
                registry_.emplace<Engine::ECS::Renderable>(proj,
                                                           Engine::ECS::Renderable{Engine::Vec2{sz, sz},
                                                                                   Engine::Color{120, 220, 255, 230},
                                                                                   turretTex});
                Engine::Gameplay::DamageEvent dmgEvent{};
                dmgEvent.baseDamage = def.damage;
                dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Light] = 1.0f;
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                   dmgEvent, projectileLifetime_, 0.0f, 0});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
                b.spawnTimer = def.attackRate;
            });
    }

    // Enemy AI.
    if (enemyAISystem_ && !defeated_ && !paused_) {
        if (freezeTimer_ <= 0.0) {
            enemyAISystem_->update(registry_, hero_, step);
        }
    }

    // Hero animation row selection (idle/walk/pickup/attack/knockdown).
    if (heroSpriteStateSystem_ && (!paused_ || defeated_)) {
        heroSpriteStateSystem_->update(registry_, step, hero_);
    }

    // Enemy animation row selection (idle/move/swing/death).
    if (enemySpriteStateSystem_ && !paused_) {
        enemySpriteStateSystem_->update(registry_, step);
    }

    // Advance sprite animations.
    if (animationSystem_ && (!paused_ || defeated_)) {
        animationSystem_->update(registry_, step);
    }

        // Cleanup dead enemies (visual cleanup can be added later).
        {
            struct DeathInfo {
                Engine::ECS::Entity e;
                Engine::Vec2 pos;
                bool boss{false};
                bool bounty{false};
                int eventId{0};
            };
            std::vector<DeathInfo> newDeaths;
            std::vector<DeathInfo> toDestroy;
            float xpMul = hotzoneSystem_ ? hotzoneSystem_->xpMultiplier() : 1.0f;
            float copperMul = hotzoneSystem_ ? hotzoneSystem_->creditMultiplier() : 1.0f;
            enemiesAlive_ = 0;
            registry_.view<Engine::ECS::Health, Engine::ECS::EnemyTag, Engine::ECS::Transform>(
                [&](Engine::ECS::Entity e, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&, Engine::ECS::Transform& tf) {
                    bool isSpawnerEventEnemy = false;
                    if (auto* mark = registry_.get<Game::EventMarker>(e)) {
                        if (eventSystem_ && eventSystem_->isSpawnerEvent(mark->eventId)) {
                            isSpawnerEventEnemy = true;
                        }
                    }
                    if (hp.alive()) {
                        if (!isSpawnerEventEnemy) {
                            enemiesAlive_++;
                        }
                    } else {
                        if (auto* dying = registry_.get<Game::Dying>(e)) {
                            dying->timer -= static_cast<float>(step.deltaSeconds);
                            if (dying->timer <= 0.0f) {
                                toDestroy.push_back(DeathInfo{e, tf.position});
                            }
                        } else {
                            DeathInfo info{};
                            info.e = e;
                            info.pos = tf.position;
                            info.boss = registry_.has<Engine::ECS::BossTag>(e);
                            info.bounty = registry_.has<Game::BountyTag>(e);
                            if (const auto* mark = registry_.get<Game::EventMarker>(e)) {
                                info.eventId = mark->eventId;
                            }
                            newDeaths.push_back(info);
                        }
                    }
                });
            for (const auto& death : newDeaths) {
                kills_ += 1;
                copper_ += static_cast<int>(std::round(copperPerKill_ * copperMul));
                xp_ += static_cast<int>(std::round(xpPerKill_ * xpMul));
                if (death.boss) {
                    xp_ += static_cast<int>(std::round(xpPerKill_ * 5 * xpMul));
                    clearBannerTimer_ = 1.75;
                    waveClearedPending_ = true;
                }
                if (eventSystem_ && death.eventId > 0) {
                    eventSystem_->notifyTargetKilled(registry_, death.eventId);
                }

                // Spawn pickups and powerups based on death type.
                auto scatterPos = [&](const Engine::Vec2& base) {
                    std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
                    std::uniform_real_distribution<float> rad(12.0f, 38.0f);
                    float a = ang(rng_);
                    float r = rad(rng_);
                    return Engine::Vec2{base.x + std::cos(a) * r, base.y + std::sin(a) * r};
                };
                auto spawnPickupEntity = [&](const Pickup& payload, const Engine::Vec2& pos, Engine::Color color,
                                             float size, float amp, float speed) {
                    auto drop = registry_.create();
                    registry_.emplace<Engine::ECS::Transform>(drop, pos);
                    Engine::TexturePtr tex{};
                    bool animated = false;
                    float scale = 1.0f;
                    if (payload.kind == Pickup::Kind::Copper) { tex = pickupCopperTex_; animated = true; scale = 1.75f; }
                    if (payload.kind == Pickup::Kind::Gold)   { tex = pickupGoldTex_;   animated = false; scale = 1.3f; }
                    if (payload.kind == Pickup::Kind::Powerup) {
                        switch (payload.powerup) {
                            case Pickup::Powerup::Heal: tex = pickupHealPowerupTex_; break;
                            case Pickup::Powerup::Kaboom: tex = pickupKaboomTex_; break;
                            case Pickup::Powerup::Recharge: tex = pickupRechargeTex_; scale *= 1.8f; break;  // make recharge orb much more visible
                            case Pickup::Powerup::Frenzy: tex = pickupFrenzyTex_; break;
                            case Pickup::Powerup::Immortal: tex = pickupImmortalTex_; animated = true; break;
                            case Pickup::Powerup::Freeze: tex = pickupFreezeTex_; break;
                            default: break;
                        }
                    }
                    if (payload.kind == Pickup::Kind::Item) {
                        switch (payload.item.effect) {
                            case ItemEffect::Turret: tex = pickupTurretTex_; break;
                            case ItemEffect::Heal: tex = pickupHealTex_; break;
                            case ItemEffect::FreezeTime: tex = pickupFreezeTex_; scale *= 1.8f; break;  // boss Cryo Capsule pickup bigger
                            case ItemEffect::AttackSpeed: tex = pickupChronoTex_; animated = true; break;
                            case ItemEffect::Lifesteal: tex = pickupPhaseLeechTex_; animated = true; break;
                            case ItemEffect::Chain: tex = pickupStormCoreTex_; animated = true; break;
                            default: break;
                        }
                    }
                    Engine::Vec2 rendSize{size, size};
                    float aabbHalf = size * 0.5f;
                    if (tex) {
                        if (animated) {
                            rendSize = Engine::Vec2{16.0f * scale, 16.0f * scale};
                        } else {
                            rendSize = Engine::Vec2{static_cast<float>(tex->width()) * scale,
                                                    static_cast<float>(tex->height()) * scale};
                        }
                        aabbHalf = std::max(rendSize.x, rendSize.y) * 0.5f;
                    }
                    registry_.emplace<Engine::ECS::Renderable>(drop,
                        Engine::ECS::Renderable{rendSize, color, tex});
                    registry_.emplace<Engine::ECS::AABB>(drop, Engine::ECS::AABB{Engine::Vec2{aabbHalf, aabbHalf}});
                    registry_.emplace<Game::Pickup>(drop, payload);
                    registry_.emplace<Game::PickupBob>(drop, Game::PickupBob{pos, 0.0f, amp, speed});
                    registry_.emplace<Engine::ECS::PickupTag>(drop, Engine::ECS::PickupTag{});
                    if (animated && tex) {
                        registry_.emplace<Engine::ECS::SpriteAnimation>(drop, Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f});
                    }
                };
                auto pickItemByKind = [&](ItemKind kind) -> std::optional<ItemDefinition> {
                    std::vector<ItemDefinition> pool;
                    for (const auto& def : itemCatalog_) {
                        if (def.kind == kind) pool.push_back(def);
                    }
                    if (pool.empty()) return std::nullopt;
                    std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
                    return pool[pick(rng_)];
                };

                if (death.boss) {
                    // Money
                    Pickup goldPickup{};
                    goldPickup.kind = Pickup::Kind::Gold;
                    goldPickup.amount = bossGoldBonus_;
                    spawnPickupEntity(goldPickup, scatterPos(death.pos), Engine::Color{255, 220, 140, 255}, 13.0f, 5.0f, 3.6f);

                    Pickup copperPickup{};
                    copperPickup.kind = Pickup::Kind::Copper;
                    copperPickup.amount = bossCopperDrop_;
                    spawnPickupEntity(copperPickup, scatterPos(death.pos), Engine::Color{255, 200, 90, 255}, 12.0f, 4.5f, 3.4f);

                    if (auto unique = pickItemByKind(ItemKind::Unique)) {
                        Pickup item{};
                        item.kind = Pickup::Kind::Item;
                        item.item = *unique;
                        spawnPickupEntity(item, scatterPos(death.pos), Engine::Color{210, 180, 255, 255}, 14.0f, 4.5f, 3.2f);
                    }
                    if (auto support = pickItemByKind(ItemKind::Support)) {
                    Pickup item{};
                    item.kind = Pickup::Kind::Item;
                    item.item = *support;
                    spawnPickupEntity(item, scatterPos(death.pos), Engine::Color{180, 230, 255, 255}, 13.0f, 4.0f, 3.0f);
                }
                Pickup revive{};
                revive.kind = Pickup::Kind::Revive;
                spawnPickupEntity(revive, scatterPos(death.pos), Engine::Color{180, 255, 200, 255}, 12.0f, 4.0f, 3.0f);
                    travelShopUnlocked_ = true;
                } else if (death.bounty) {
                    Pickup goldPickup{};
                    goldPickup.kind = Pickup::Kind::Gold;
                    goldPickup.amount = bountyGoldBonus_;
                    spawnPickupEntity(goldPickup, scatterPos(death.pos), Engine::Color{255, 215, 150, 255}, 12.0f, 4.5f, 3.4f);

                    Pickup copperPickup{};
                    copperPickup.kind = Pickup::Kind::Copper;
                    copperPickup.amount = miniBossCopperDrop_;
                    spawnPickupEntity(copperPickup, scatterPos(death.pos), Engine::Color{255, 200, 110, 255}, 11.0f, 4.0f, 3.2f);

                    if (auto support = pickItemByKind(ItemKind::Support)) {
                        Pickup item{};
                        item.kind = Pickup::Kind::Item;
                        item.item = *support;
                        spawnPickupEntity(item, scatterPos(death.pos), Engine::Color{170, 220, 255, 255}, 11.0f, 3.8f, 3.0f);
                    }
                } else {
                    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                    float drop = roll(rng_);
                    if (drop < pickupDropChance_) {
                        float powerRoll = roll(rng_);
                        if (powerRoll < pickupPowerupShare_) {
                            // Powerup drop
                            Pickup p{};
                            p.kind = Pickup::Kind::Powerup;
                            // Exclude Freeze powerup to avoid confusing it with the Cryo Capsule item.
                            std::uniform_int_distribution<int> pick(0, 4);
                            int choice = pick(rng_);
                            switch (choice) {
                                case 0: p.powerup = Pickup::Powerup::Heal; break;
                                case 1: p.powerup = Pickup::Powerup::Kaboom; break;
                                case 2: p.powerup = Pickup::Powerup::Recharge; break;
                                case 3: p.powerup = Pickup::Powerup::Frenzy; break;
                                case 4: p.powerup = Pickup::Powerup::Immortal; break;
                                default: p.powerup = Pickup::Powerup::Heal; break;
                            }
                            spawnPickupEntity(p, scatterPos(death.pos), Engine::Color{150, 220, 255, 255}, 11.0f, 4.0f, 3.2f);
                        } else {
                            // Copper drop
                            std::uniform_int_distribution<int> amt(copperPickupMin_, copperPickupMax_);
                            Pickup p{};
                            p.kind = Pickup::Kind::Copper;
                            p.amount = amt(rng_);
                            spawnPickupEntity(p, scatterPos(death.pos), Engine::Color{255, 210, 90, 255}, 10.0f, 3.8f, 3.0f);
                        }
                    }
                }

                // Switch the entity into a short-lived corpse state so the death animation can play.
                float despawnAfter = 0.45f;
                if (auto* anim = registry_.get<Engine::ECS::SpriteAnimation>(death.e)) {
                    float fd = anim->frameDuration > 0.0f ? anim->frameDuration : 0.01f;
                    despawnAfter = static_cast<float>(anim->frameCount) * fd;
                    int deathRow = 12;
                    if (const auto* look = registry_.get<Game::LookDirection>(death.e)) {
                        deathRow = (look->dir == Game::LookDir4::Left) ? 13 : 12;
                    }
                    anim->row = deathRow;
                    anim->currentFrame = 0;
                    anim->accumulator = 0.0f;
                    anim->allowFlipX = false;
                }
                if (auto* vel = registry_.get<Engine::ECS::Velocity>(death.e)) {
                    vel->value = {0.0f, 0.0f};
                }
                registry_.emplace<Game::Dying>(death.e, Game::Dying{std::max(0.05f, despawnAfter)});
            }
        for (const auto& death : toDestroy) {
            registry_.destroy(death.e);
        }

        // Handle event spawners (assassin spire) deaths/lifetime completions so events can finish and they don't block flow.
        {
            std::vector<Engine::ECS::Entity> deadSpawners;
            registry_.view<Engine::ECS::Health, Game::Spawner>(
                [&](Engine::ECS::Entity e, Engine::ECS::Health& hp, Game::Spawner& sp) {
                    if (!hp.alive()) {
                        deadSpawners.push_back(e);
                        if (eventSystem_) {
                            eventSystem_->notifyTargetKilled(registry_, sp.eventId);
                        }
                    }
                });
            for (auto e : deadSpawners) {
                registry_.destroy(e);
            }
        }

        // If no enemies remain and a wave was active, grant wave clear bonus once.
        const bool anyEnemies = enemiesAlive_ > 0;
        if (!anyEnemies && !waveClearedPending_ && wave_ > 0) {
            copper_ += waveClearBonus_;
            waveClearedPending_ = true;
            clearBannerTimer_ = 1.5;
            wavesClearedThisRound_ += 1;
            waveSpawned_ = false;
            // Ensure the next wave timer restarts immediately after a full clear (prevents stalls post-boss).
            if (waveSystem_) {
                waveSystem_->resetTimer();
            }
        }

        // Cleanup dead mini units and free supply.
        {
            std::vector<Engine::ECS::Entity> deadMinis;
            registry_.view<Game::MiniUnit, Engine::ECS::Health>(
                [&](Engine::ECS::Entity e, Game::MiniUnit&, Engine::ECS::Health& hp) {
                    if (!hp.alive()) {
                        deadMinis.push_back(e);
                    }
                });
            if (!deadMinis.empty()) {
                for (auto e : deadMinis) {
                    if (miniUnitSupplyUsed_ > 0) miniUnitSupplyUsed_--;
                    registry_.destroy(e);
                }
            }
        }

        // Cleanup dead buildings (despawn on destruction).
        {
            std::vector<std::pair<Engine::ECS::Entity, Game::BuildingType>> deadBuildings;
            registry_.view<Game::Building, Engine::ECS::Health>(
                [&](Engine::ECS::Entity e, const Game::Building& b, Engine::ECS::Health& hp) {
                    if (!hp.alive()) {
                        deadBuildings.push_back({e, b.type});
                    }
                });
            if (!deadBuildings.empty()) {
                for (auto [e, type] : deadBuildings) {
                    // If a house was providing supply, remove it.
                    if (type == Game::BuildingType::House) {
                        auto it = buildingDefs_.find("house");
                        int supplyProvided = (it != buildingDefs_.end()) ? it->second.supplyProvided : 0;
                        if (supplyProvided > 0) {
                            miniUnitSupplyMax_ = std::max(0, miniUnitSupplyMax_ - supplyProvided);
                            if (miniUnitSupplyUsed_ > miniUnitSupplyMax_) {
                                miniUnitSupplyUsed_ = miniUnitSupplyMax_;
                            }
                        }
                    }
                    registry_.destroy(e);
                }
            }
        }
        // Fast-forward combat timer when field is empty and a wave actually spawned.
        if (inCombat_ && waveSpawned_ && enemiesAlive_ == 0 && combatTimer_ > 1.0) {
            combatTimer_ = 1.0;
        }
        // Safety: if combat timer expires with no enemies (e.g., boss dead) force-progress.
        if (inCombat_ && waveSpawned_ && enemiesAlive_ == 0 && combatTimer_ <= 0.0) {
            if (!waveClearedPending_ && wave_ > 0) {
                copper_ += waveClearBonus_;
                waveClearedPending_ = true;
                clearBannerTimer_ = 1.5;
                wavesClearedThisRound_ += 1;
                waveSpawned_ = false;
                if (waveSystem_) waveSystem_->resetTimer();
            }
            inCombat_ = false;
            intermissionTimer_ = intermissionDuration_;
            xp_ += xpPerWave_;
            waveSpawned_ = false;
            itemShopOpen_ = false;
            abilityShopOpen_ = false;
            paused_ = userPaused_;
            refreshShopInventory();
            if (multiplayerEnabled_ && reviveNextRound_) {
                reviveLocalPlayer();
            }
            Engine::Vec2 heroPos{0.0f, 0.0f};
            if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = tf->position;
            if (travelShopUnlocked_ && gold_ > 0) {
                spawnShopkeeper(heroPos);
            }
            if (waveSystem_) waveSystem_->setTimer(0.0);
            waveWarmup_ = intermissionTimer_;
            round_ += 1;
            wavesClearedThisRound_ = 0;
            wavesTargetThisRound_ = wavesPerRoundBase_ + (round_ - 1) / wavesPerRoundGrowthInterval_;
            if (waveSystem_) waveSystem_->setRound(round_);
        }
    }

    // Waves and phases.
    // Phase timing continues unless user-paused (shop should not freeze phase timers).
    if (!defeated_ && !userPaused_ && !levelChoiceOpen_) {
    if (inCombat_) {
        double stepScale = freezeTimer_ > 0.0 ? 0.0 : 1.0;
        combatTimer_ -= step.deltaSeconds * stepScale;
        if (waveSystem_) {
                waveSystem_->setPlayerCount(currentPlayerCount());
                if (!waveSpawned_) {
                    // Scale enemy armor upgrades over time.
                    enemyUpgrades_.groundArmorLevel = wave_ / 5;
                    enemyUpgrades_.shieldArmorLevel = wave_ / 8;
                    waveSystem_->setUpgrades(enemyUpgrades_);
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
                // End of round: once required waves cleared and field is empty, go to intermission.
                if (wavesClearedThisRound_ >= wavesTargetThisRound_ && enemiesAlive_ == 0) {
                inCombat_ = false;
                intermissionTimer_ = intermissionDuration_;
                xp_ += xpPerWave_;  // small wave completion xp
                waveSpawned_ = false;
                itemShopOpen_ = false;
                abilityShopOpen_ = false;
                paused_ = userPaused_;
                refreshShopInventory();
                if (multiplayerEnabled_ && reviveNextRound_) {
                    reviveLocalPlayer();
                }
                Engine::Vec2 heroPos{0.0f, 0.0f};
                if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = tf->position;
                if (travelShopUnlocked_ && gold_ > 0) {
                    spawnShopkeeper(heroPos);
                }
                if (waveSystem_) waveSystem_->setTimer(0.0);
                waveWarmup_ = intermissionTimer_;
                round_ += 1;
                wavesClearedThisRound_ = 0;
                wavesTargetThisRound_ = wavesPerRoundBase_ + (round_ - 1) / wavesPerRoundGrowthInterval_;
                waveSystem_->setRound(round_);
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
                abilityShopOpen_ = false;
                paused_ = userPaused_;
                despawnShopkeeper();
                refreshShopInventory();
                if (waveSystem_) {
                    waveSystem_->setRound(round_);
                    waveSystem_->setTimer(waveWarmupBase_);
                }
                waveWarmup_ = waveWarmupBase_;
                roundBanner_ = round_;
                roundBannerTimer_ = 1.6;
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

        if (pickupSystem_ && !paused_ && !defeatDelayActive_) {
            const auto* heroHp = registry_.get<Engine::ECS::Health>(hero_);
            if (heroHp && !heroHp->alive()) {
                // Do not auto-collect while down.
            } else {
            auto onCollect = [&](const Pickup& p) {
                // Trigger pickup animation for coins/powerups.
                if (registry_.has<Game::HeroSpriteSheets>(hero_) &&
                    (p.kind == Pickup::Kind::Copper || p.kind == Pickup::Kind::Gold || p.kind == Pickup::Kind::Powerup)) {
                    float dur = 0.35f;
                    if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_); sheets && sheets->movement) {
                        int frames = std::max(1, sheets->movement->width() / std::max(1, sheets->frameWidth));
                        dur = std::max(0.05f, static_cast<float>(frames) * sheets->movementFrameDuration);
                    }
                    if (auto* pick = registry_.get<Game::HeroPickupAnim>(hero_)) {
                        pick->timer = dur;
                    } else {
                        registry_.emplace<Game::HeroPickupAnim>(hero_, Game::HeroPickupAnim{dur});
                    }
                }
                switch (p.kind) {
                    case Pickup::Kind::Copper:
                        copper_ += p.amount;
                        break;
                    case Pickup::Kind::Gold:
                        gold_ += p.amount;
                        break;
                    case Pickup::Kind::Powerup:
                        applyPowerupPickup(p.powerup);
                        break;
                    case Pickup::Kind::Item: {
                        if (!addItemToInventory(p.item)) {
                            copper_ += std::max(1, p.item.cost / 2);
                        }
                        break;
                    }
                    case Pickup::Kind::Revive:
                        reviveCharges_ += 1;
                        break;
                }
            };
            pickupSystem_->update(registry_, hero_, onCollect);
            }
        }

        // Shields/health regeneration ticks.
        if (!paused_) {
            const float dt = static_cast<float>(step.deltaSeconds);
            registry_.view<Engine::ECS::Health>([dt](Engine::ECS::Entity, Engine::ECS::Health& hp) {
                Engine::Gameplay::updateRegen(hp, dt);
            });
            if (buffSystem_) {
                buffSystem_->update(registry_, step);
            }
            if (lifestealTimer_ > 0.0) {
                lifestealTimer_ -= step.deltaSeconds;
                if (lifestealTimer_ <= 0.0) {
                    lifestealTimer_ = 0.0;
                    lifestealPercent_ = globalModifiers_.playerLifestealAdd;
                    lifestealBuff_ = 0.0f;
                }
            }
            if (chronoTimer_ > 0.0) {
                chronoTimer_ -= step.deltaSeconds;
                if (chronoTimer_ <= 0.0) {
                    chronoTimer_ = 0.0;
                    attackSpeedBuffMul_ = 1.0f;
                    fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);
                }
            }
            if (stormTimer_ > 0.0) {
                stormTimer_ -= step.deltaSeconds;
                if (stormTimer_ <= 0.0) {
                    stormTimer_ = 0.0;
                    chainBonusTemp_ = 0;
                    chainBounces_ = chainBase_;
                }
            }
            // Energy regeneration (faster during intermission).
            float regen = inCombat_ ? energyRegen_ : energyRegenIntermission_;
            energy_ = std::min(energyMax_, energy_ + regen * static_cast<float>(step.deltaSeconds));
            if (energyWarningTimer_ > 0.0) {
                energyWarningTimer_ -= step.deltaSeconds;
                if (energyWarningTimer_ < 0.0) energyWarningTimer_ = 0.0;
            }
        }

    if (eventSystem_ && !paused_ && !levelChoiceOpen_) {
        Engine::Vec2 heroPos{0.0f, 0.0f};
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
            heroPos = tf->position;
        }
        eventSystem_->update(registry_, step, wave_, gold_, inCombat_, heroPos, salvageReward_);
        eventCountdownSeconds_ = 0.0;
        eventCountdownLabel_.clear();
        std::string lbl;
        float seconds = 0.0f;
        if (eventSystem_->getCountdownText(lbl, seconds) && seconds > 0.0f) {
            eventCountdownLabel_ = lbl;
            eventCountdownSeconds_ = seconds;
        }
        std::string evLabel = eventSystem_->lastEventLabel().empty() ? "Event" : eventSystem_->lastEventLabel();
        if (eventSystem_->lastSuccess()) {
            std::ostringstream eb;
            eb << evLabel << " Success";
            if (evLabel == "Escort Duty") {
                eb << " +" << salvageReward_ << "g";
            }
            eventBannerText_ = eb.str();
            eventBannerTimer_ = 1.5;
            xp_ += xpPerEvent_;
            gold_ += eventGoldReward_;
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
            float total = hp->currentHealth + hp->currentShields;
            if (lastHeroHp_ < 0.0f) lastHeroHp_ = total;
            if (total < lastHeroHp_ && screenShake_) {
                shakeTimer_ = 0.25f;
                shakeMagnitude_ = 6.0f;
            }
            lastHeroHp_ = total;
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

    handleHeroDeath(step);
    processDefeatInput(actions, input);

    // Update fog-of-war visibility before rendering.
    updateFogVision();

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
            renderSystem_->draw(registry_, cameraShaken, viewportWidth_, viewportHeight_, gridPtr, gridVariants,
                                fogLayer_.get(), fogTileSize_, fogOriginOffsetX_, fogOriginOffsetY_);
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

        // RTS move target marker.
        if (movementMode_ == MovementMode::RTS && (moveTargetActive_ || moveMarkerTimer_ > 0.0f)) {
            Engine::Vec2 screen = Engine::worldToScreen(moveTarget_, cameraShaken,
                                                        static_cast<float>(viewportWidth_),
                                                        static_cast<float>(viewportHeight_));
            float alpha = std::clamp(moveMarkerTimer_ / 1.2f, 0.0f, 1.0f);
            Engine::Color ring{120, 230, 255, static_cast<uint8_t>(180 * alpha)};
            Engine::Color cross{220, 255, 255, static_cast<uint8_t>(220 * alpha)};
            float r = 12.0f;
            const float thickness = 2.0f;
            // simple square ring using four filled rects
            render_->drawFilledRect(Engine::Vec2{screen.x - r, screen.y - r}, Engine::Vec2{r * 2.0f, thickness}, ring);
            render_->drawFilledRect(Engine::Vec2{screen.x - r, screen.y + r - thickness}, Engine::Vec2{r * 2.0f, thickness}, ring);
            render_->drawFilledRect(Engine::Vec2{screen.x - r, screen.y - r}, Engine::Vec2{thickness, r * 2.0f}, ring);
            render_->drawFilledRect(Engine::Vec2{screen.x + r - thickness, screen.y - r}, Engine::Vec2{thickness, r * 2.0f}, ring);
            render_->drawFilledRect(Engine::Vec2{screen.x - 1.0f, screen.y - r}, Engine::Vec2{2.0f, r * 2.0f}, cross);
            render_->drawFilledRect(Engine::Vec2{screen.x - r, screen.y - 1.0f}, Engine::Vec2{r * 2.0f, 2.0f}, cross);
        }

        // Fog overlay drawn after world elements, before UI.
        if (fogLayer_ && fogTexture_ && sdlRenderer_) {
            Engine::renderFog(sdlRenderer_, *fogLayer_, fogTileSize_, fogOriginOffsetX_, fogOriginOffsetY_,
                              cameraShaken, viewportWidth_, viewportHeight_, fogTexture_);
        }

        // Mini-unit selection rectangle and selected rings (RTS UI).
        if (selectingMiniUnits_) {
            Engine::Vec2 s0 = Engine::worldToScreen(selectionStart_, cameraShaken,
                                                    static_cast<float>(viewportWidth_),
                                                    static_cast<float>(viewportHeight_));
            Engine::Vec2 s1 = Engine::worldToScreen(selectionEnd_, cameraShaken,
                                                    static_cast<float>(viewportWidth_),
                                                    static_cast<float>(viewportHeight_));
            float minX = std::min(s0.x, s1.x);
            float maxX = std::max(s0.x, s1.x);
            float minY = std::min(s0.y, s1.y);
            float maxY = std::max(s0.y, s1.y);
            const float t = 2.0f;
            Engine::Color c{120, 220, 255, 200};
            render_->drawFilledRect(Engine::Vec2{minX, minY}, Engine::Vec2{maxX - minX, t}, c);
            render_->drawFilledRect(Engine::Vec2{minX, maxY - t}, Engine::Vec2{maxX - minX, t}, c);
            render_->drawFilledRect(Engine::Vec2{minX, minY}, Engine::Vec2{t, maxY - minY}, c);
            render_->drawFilledRect(Engine::Vec2{maxX - t, minY}, Engine::Vec2{t, maxY - minY}, c);
        }
        if (!selectedMiniUnits_.empty()) {
            for (auto ent : selectedMiniUnits_) {
                if (!registry_.has<Engine::ECS::Transform>(ent)) continue;
                const auto* tf = registry_.get<Engine::ECS::Transform>(ent);
                if (!tf) continue;
                Engine::Vec2 sp = Engine::worldToScreen(tf->position, cameraShaken,
                                                       static_cast<float>(viewportWidth_),
                                                       static_cast<float>(viewportHeight_));
                const float r = 10.0f;
                const float t = 2.0f;
                Engine::Color ring{90, 255, 160, 220};
                render_->drawFilledRect(Engine::Vec2{sp.x - r, sp.y - r}, Engine::Vec2{r * 2.0f, t}, ring);
                render_->drawFilledRect(Engine::Vec2{sp.x - r, sp.y + r - t}, Engine::Vec2{r * 2.0f, t}, ring);
                render_->drawFilledRect(Engine::Vec2{sp.x - r, sp.y - r}, Engine::Vec2{t, r * 2.0f}, ring);
                render_->drawFilledRect(Engine::Vec2{sp.x + r - t, sp.y - r}, Engine::Vec2{t, r * 2.0f}, ring);
            }
        }

        // Mini-unit selection HUD (bottom-center).
        if (!selectedMiniUnits_.empty()) {
            int lightN = 0, heavyN = 0, medicN = 0;
            bool allCombatOn = true;
            bool anyCombatOn = false;
            for (auto ent : selectedMiniUnits_) {
                if (auto* mu = registry_.get<Game::MiniUnit>(ent)) {
                    switch (mu->cls) {
                        case Game::MiniUnitClass::Light: ++lightN; break;
                        case Game::MiniUnitClass::Heavy: ++heavyN; break;
                        case Game::MiniUnitClass::Medic: ++medicN; break;
                    }
                    allCombatOn &= mu->combatEnabled;
                    anyCombatOn |= mu->combatEnabled;
                }
            }
            const float hudW = 300.0f;
            const float hudH = 70.0f;
            const float hudX = static_cast<float>(viewportWidth_) * 0.5f - hudW * 0.5f;
            const float hudY = static_cast<float>(viewportHeight_) - hudH - 18.0f;
            render_->drawFilledRect(Engine::Vec2{hudX, hudY}, Engine::Vec2{hudW, hudH},
                                    Engine::Color{18, 24, 34, 215});
            std::ostringstream sel;
            sel << "Selected: " << selectedMiniUnits_.size()
                << "  L " << lightN << "  H " << heavyN << "  M " << medicN;
            drawTextUnified(sel.str(), Engine::Vec2{hudX + 10.0f, hudY + 8.0f}, 0.9f,
                            Engine::Color{220, 240, 255, 240});
            std::string combatLabel = allCombatOn ? "Combat: ON" : (anyCombatOn ? "Combat: MIXED" : "Combat: OFF");
            drawTextUnified(combatLabel, Engine::Vec2{hudX + 10.0f, hudY + 34.0f}, 0.85f,
                            Engine::Color{200, 230, 240, 230});

            const float toggleW = 92.0f;
            const float toggleH = 26.0f;
            const float toggleX = hudX + hudW - toggleW - 12.0f;
            const float toggleY = hudY + hudH - toggleH - 10.0f;
            Engine::Color toggleBg = allCombatOn ? Engine::Color{40, 90, 60, 230} : Engine::Color{60, 50, 50, 230};
            render_->drawFilledRect(Engine::Vec2{toggleX, toggleY}, Engine::Vec2{toggleW, toggleH}, toggleBg);
            drawTextUnified(allCombatOn ? "Stop" : "Engage",
                            Engine::Vec2{toggleX + 10.0f, toggleY + 5.0f}, 0.85f,
                            Engine::Color{240, 255, 240, 240});
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
                float maxPool = heroHealth->maxHealth + heroHealth->maxShields;
                float currPool = heroHealth->currentHealth + heroHealth->currentShields;
                dbg << " | HP " << std::setprecision(0) << currPool << "/" << maxPool;
            }
            dbg << " | Round " << round_ << " | Wave " << wave_ << " | Kills " << kills_;
            dbg << " | Copper " << copper_ << " | Gold " << gold_;
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
            if (roundBannerTimer_ > 0.0) {
                roundBannerTimer_ -= step.deltaSeconds;
                std::ostringstream rb;
                rb << "ROUND " << roundBanner_;
                float scale = 1.5f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 90.0f,
                                 static_cast<float>(viewportHeight_) * 0.18f};
                drawTextTTF(rb.str(), pos, scale, Engine::Color{255, 220, 180, 230});
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
            if (moveMarkerTimer_ > 0.0f) {
                moveMarkerTimer_ = std::max(0.0f, moveMarkerTimer_ - static_cast<float>(step.deltaSeconds));
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
            if (eventCountdownSeconds_ > 0.0) {
                std::ostringstream eb;
                eb << eventCountdownLabel_ << " starts in " << std::fixed << std::setprecision(1)
                   << eventCountdownSeconds_ << "s";
                float scale = 1.1f;
                Engine::Vec2 pos{static_cast<float>(viewportWidth_) * 0.5f - 150.0f,
                                 static_cast<float>(viewportHeight_) * 0.24f};
                drawTextTTF(eb.str(), pos, scale, Engine::Color{200, 240, 255, 235});
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
            inter << "Intermission: next round in " << std::fixed << std::setprecision(1) << intermissionLeft
                  << "s | Copper: " << copper_ << " | Gold: " << gold_;
            Engine::Color c{140, 220, 255, 220};
        drawTextTTF(inter.str(), Engine::Vec2{10.0f, 46.0f}, 1.0f, c);
                if (abilityShopOpen_) {
                    drawTextTTF("Ability Shop OPEN (B closes)", Engine::Vec2{10.0f, 64.0f}, 1.0f,
                                Engine::Color{180, 255, 180, 220});
                } else {
                    std::string shopPrompt = "Press B to open Ability Shop";
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
                    drawTextUnified("Inventory empty (Tab to cycle)", Engine::Vec2{invPos.x + 10.0f, invPos.y + 6.0f},
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
                    hint << "Tab to cycle   Q to " << (usable ? "use" : "use support item");
                    drawTextUnified(hint.str(), Engine::Vec2{invPos.x + 10.0f, invPos.y + 12.0f},
                                    0.85f, Engine::Color{180, 200, 220, 210});
                }

                // Wallet
                std::ostringstream wallet;
                wallet << "Copper " << copper_ << " | Gold " << gold_;
                drawTextUnified(wallet.str(), Engine::Vec2{origin.x, origin.y - 16.0f}, 0.85f,
                                Engine::Color{200, 230, 255, 210});

                // HP bar
                float hpRatio = 0.0f;
                float shieldRatio = 0.0f;
                if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                    float maxPool = heroHealth->maxHealth + heroHealth->maxShields;
                    if (maxPool > 0.0f) {
                        hpRatio = std::clamp(heroHealth->currentHealth / maxPool, 0.0f, 1.0f);
                        shieldRatio = std::clamp(heroHealth->currentShields / maxPool, 0.0f, 1.0f);
                    }
                }
                render_->drawFilledRect(origin, Engine::Vec2{barW, hpH}, Engine::Color{50, 30, 30, 200});
                if (shieldRatio > 0.0f) {
                    render_->drawFilledRect(origin, Engine::Vec2{barW * shieldRatio, hpH}, Engine::Color{90, 140, 240, 230});
                }
                if (hpRatio > 0.0f) {
                    render_->drawFilledRect(origin, Engine::Vec2{barW * hpRatio, hpH}, Engine::Color{200, 80, 80, 230});
                }
                std::ostringstream hpTxt;
                float totalRatio = std::clamp(hpRatio + shieldRatio, 0.0f, 1.0f);
                hpTxt << "HP " << static_cast<int>(std::round(totalRatio * 100)) << "%";
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
                // Energy bar beneath dash
                float energyRatio = energyMax_ > 0.0f ? std::clamp(energy_ / energyMax_, 0.0f, 1.0f) : 0.0f;
                Engine::Vec2 epos{dpos.x, dpos.y + 12.0f};
                render_->drawFilledRect(epos, Engine::Vec2{barW, 8.0f}, Engine::Color{28, 30, 46, 200});
                render_->drawFilledRect(epos, Engine::Vec2{barW * energyRatio, 8.0f}, Engine::Color{120, 200, 255, 230});
                drawTextUnified("Energy", Engine::Vec2{epos.x + 8.0f, epos.y + 0.5f}, 0.8f,
                                energyWarningTimer_ > 0.0 ? Engine::Color{255, 200, 160, 220}
                                                          : Engine::Color{180, 210, 240, 210});
            }
            // Active event HUD.
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity evEnt, const Game::EventActive& ev) {
                float t = std::max(0.0f, ev.timer);
                std::ostringstream evMsg;
                if (ev.type == Game::EventType::Escort) {
                    float distLeft = 0.0f;
                    if (auto* escortObj = registry_.get<Game::EscortObjective>(evEnt)) {
                        if (auto* escortTf = registry_.get<Engine::ECS::Transform>(escortObj->escort)) {
                            Engine::Vec2 delta{escortObj->destination.x - escortTf->position.x,
                                               escortObj->destination.y - escortTf->position.y};
                            distLeft = std::sqrt(std::max(0.0f, delta.x * delta.x + delta.y * delta.y));
                        }
                    }
                    evMsg << "Event: Escort - " << std::fixed << std::setprecision(0) << distLeft << "u | "
                          << std::setprecision(1) << t << "s";
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
            registry_.view<Game::EventActive>([&](Engine::ECS::Entity e, const Game::EventActive& ev) {
                std::ostringstream msg;
                if (ev.type == Game::EventType::Escort) {
                    float distLeft = 0.0f;
                    if (auto* escortObj = registry_.get<Game::EscortObjective>(e)) {
                        if (auto* escortTf = registry_.get<Engine::ECS::Transform>(escortObj->escort)) {
                            Engine::Vec2 delta{escortObj->destination.x - escortTf->position.x,
                                               escortObj->destination.y - escortTf->position.y};
                            distLeft = std::sqrt(std::max(0.0f, delta.x * delta.x + delta.y * delta.y));
                        }
                    }
                    msg << "Event: Escort | " << std::fixed << std::setprecision(0) << distLeft << "u | "
                        << std::setprecision(1) << ev.timer << "s left";
                } else if (ev.type == Game::EventType::Bounty) {
                    msg << "Event: Bounty | " << ev.requiredKills << " targets | "
                        << std::fixed << std::setprecision(1) << ev.timer << "s left";
                } else {
                    msg << "Event: Spire Hunt | " << ev.requiredKills << " spires | "
                        << std::fixed << std::setprecision(1) << ev.timer << "s left";
                }
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
            float hpRatio = 0.0f;
            float shieldRatio = 0.0f;
            if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                float maxPool = heroHealth->maxHealth + heroHealth->maxShields;
                if (maxPool > 0.0f) {
                    hpRatio = std::clamp(heroHealth->currentHealth / maxPool, 0.0f, 1.0f);
                    shieldRatio = std::clamp(heroHealth->currentShields / maxPool, 0.0f, 1.0f);
                }
            }
            Engine::Vec2 pos{10.0f, static_cast<float>(viewportHeight_) - 24.0f};
            render_->drawFilledRect(pos, Engine::Vec2{barW, barH}, Engine::Color{50, 50, 60, 220});
            if (shieldRatio > 0.0f) {
                render_->drawFilledRect(pos, Engine::Vec2{barW * shieldRatio, barH}, Engine::Color{80, 140, 240, 220});
            }
            if (hpRatio > 0.0f) {
                render_->drawFilledRect(pos, Engine::Vec2{barW * hpRatio, barH}, Engine::Color{120, 220, 120, 240});
            }
            // Wave-clear cue (fallback bar).
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
    if (abilityShopOpen_) {
        drawAbilityShopOverlay();
    }
    bool buildLeftEdge = input.isMouseButtonDown(0) && !buildMenuClickPrev_;
    buildMenuClickPrev_ = input.isMouseButtonDown(0);
        drawBuildMenuOverlay(lastMouseX_, lastMouseY_, buildLeftEdge);
        // Building selection click in world (when not previewing).
        // Suppress world-selection when clicking bottom-left UI panels (build menu, selected building panel),
        // otherwise UI clicks can clear the selection before the panel handles them.
        auto pointInRect = [&](int mx, int my, float rx, float ry, float rw, float rh) {
            return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh;
        };
        bool mouseOverBottomLeftUI = false;
        if (buildMenuOpen_ && activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
            const float panelW = 260.0f;
            const float panelH = 220.0f;
            const float rx = 22.0f;
            const float ry = static_cast<float>(viewportHeight_) - panelH - 22.0f;
            mouseOverBottomLeftUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }
        if (selectedBuilding_ != Engine::ECS::kInvalidEntity &&
            registry_.has<Game::Building>(selectedBuilding_) &&
            registry_.has<Engine::ECS::Health>(selectedBuilding_)) {
            const auto* b = registry_.get<Game::Building>(selectedBuilding_);
            const float panelW = 270.0f;
            const float panelH = (b && b->type == Game::BuildingType::Barracks) ? 170.0f : 120.0f;
            const float rx = 20.0f;
            const float ry = static_cast<float>(viewportHeight_) - panelH - 20.0f;
            mouseOverBottomLeftUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }

        bool worldLeftEdge = input.isMouseButtonDown(0) && !buildPreviewActive_ && !buildingSelectPrev_;
        if (worldLeftEdge && !mouseOverBottomLeftUI && !inMenu_ && !paused_) {
            selectBuildingAt(mouseWorld_);
            buildingJustSelected_ = true;
        }
        bool buildingPanelLeftEdge = input.isMouseButtonDown(0) && !buildingPanelClickPrev_;
        drawSelectedBuildingPanel(lastMouseX_, lastMouseY_, buildingPanelLeftEdge);
        buildingPanelClickPrev_ = input.isMouseButtonDown(0);
        buildingSelectPrev_ = input.isMouseButtonDown(0);
    if (paused_ && !itemShopOpen_ && !abilityShopOpen_ && !levelChoiceOpen_ && !defeated_) {
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

    if (buildPreviewActive_) {
        if (auto* tf = registry_.get<Engine::ECS::Transform>(buildPreviewEntity_)) {
            tf->position = mouseWorld_;
        }
        bool previewLeftEdge = input.isMouseButtonDown(0) && !buildPreviewClickPrev_;
        bool previewRightEdge = input.isMouseButtonDown(2) && !buildPreviewRightPrev_;
        if (!paused_ && previewLeftEdge) {
            placeBuilding(mouseWorld_);
        } else if (previewRightEdge) {
            clearBuildPreview();
        }
        buildPreviewClickPrev_ = input.isMouseButtonDown(0);
        buildPreviewRightPrev_ = input.isMouseButtonDown(2);
    } else {
        buildPreviewClickPrev_ = input.isMouseButtonDown(0);
        buildPreviewRightPrev_ = input.isMouseButtonDown(2);
    }
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

bool GameRoot::performRangedAutoFire(const Engine::TimeStep& step, const Engine::ActionState& actions,
                                     Game::OffensiveType offenseType) {
    (void)step;
    const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
    if (!heroTf) return false;

    Engine::Vec2 dir{};
    bool haveTarget = false;
    float visionRange = heroVisionRadiusTiles_ * static_cast<float>(fogTileSize_ > 0 ? fogTileSize_ : 32);
    float allowedRange = std::min(visionRange, autoFireBaseRange_ + autoFireRangeBonus_);
    float allowedRange2 = allowedRange * allowedRange;
    if (actions.primaryFire) {
        dir = {mouseWorld_.x - heroTf->position.x, mouseWorld_.y - heroTf->position.y};
        float len2 = dir.x * dir.x + dir.y * dir.y;
        if (len2 <= allowedRange2 && len2 > 0.0001f) {
            float inv = 1.0f / std::sqrt(len2);
            dir.x *= inv;
            dir.y *= inv;
            haveTarget = true;
        }
    } else if (autoAttackEnabled_) {
        float best = allowedRange2;
        Engine::Vec2 target{};
        registry_.view<Engine::ECS::Transform, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity, const Engine::ECS::Transform& tf, const Engine::ECS::EnemyTag&) {
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < best) {
                    best = d2;
                    target = tf.position;
                }
            });
        if (best < allowedRange2) {
            Engine::Vec2 raw{target.x - heroTf->position.x, target.y - heroTf->position.y};
            float len2 = raw.x * raw.x + raw.y * raw.y;
            if (len2 > 0.0001f) {
                float inv = 1.0f / std::sqrt(len2);
                dir = {raw.x * inv, raw.y * inv};
                haveTarget = true;
            }
        }
    }
    if (!haveTarget) return false;

    // Trigger hero attack animation (Damage Dealer uses melee attack rows for all offense types).
    if (registry_.has<Game::HeroSpriteSheets>(hero_)) {
        int variant = 0;
        if (auto* cycle = registry_.get<Game::HeroAttackCycle>(hero_)) {
            variant = cycle->nextVariant;
            cycle->nextVariant = 1 - cycle->nextVariant;
        } else {
            variant = 0;
            registry_.emplace<Game::HeroAttackCycle>(hero_, Game::HeroAttackCycle{1});
        }
        float dur = 0.28f;
        if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_); sheets && sheets->combat) {
            int frames = std::max(1, sheets->combat->width() / std::max(1, sheets->frameWidth));
            dur = std::max(0.05f, static_cast<float>(frames) * sheets->combatFrameDuration);
        }
        if (auto* atk = registry_.get<Game::HeroAttackAnim>(hero_)) {
            *atk = Game::HeroAttackAnim{dur, dir, variant};
        } else {
            registry_.emplace<Game::HeroAttackAnim>(hero_, Game::HeroAttackAnim{dur, dir, variant});
        }
    }

    auto proj = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(proj, heroTf->position);
    registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{0.0f, 0.0f});
    const float halfSize = projectileHitboxSize_ * 0.5f;
    registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
    float sz = projectileSize_;
    if (projectileTexRed_) {
        // Use native tex size to avoid squish; scale by size_ factor.
        sz = static_cast<float>(projectileTexRed_->width());
    }
    registry_.emplace<Engine::ECS::Renderable>(proj,
                                               Engine::ECS::Renderable{Engine::Vec2{sz, sz},
                                                                       Engine::Color{255, 230, 90, 255},
                                                                       projectileTexRed_});
    float zoneDmgMul = hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f;
    float dmgMul = rageDamageBuff_ * zoneDmgMul;
    Engine::Gameplay::DamageEvent dmgEvent{};
    dmgEvent.type = Engine::Gameplay::DamageType::Normal;
    dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Biological] = 1.0f;
    float baseDamage = projectileDamage_ * dmgMul;
    if (offenseType == Game::OffensiveType::Plasma) {
        float healthScaled = baseDamage * plasmaConfig_.healthDamageMultiplier;
        float shieldBonus = baseDamage * (plasmaConfig_.shieldDamageMultiplier - plasmaConfig_.healthDamageMultiplier);
        dmgEvent.baseDamage = healthScaled;
        if (shieldBonus > 0.0f) {
            dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Mechanical] = shieldBonus;
        }
    } else {
        dmgEvent.baseDamage = baseDamage;
    }
    registry_.emplace<Engine::ECS::Projectile>(proj,
                                               Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                     dir.y * projectileSpeed_},
                                                                       dmgEvent, projectileLifetime_, lifestealPercent_,
                                                                       chainBounces_});
    registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
    float rateMul = rageRateBuff_ * frenzyRateBuff_ * (hotzoneSystem_ ? hotzoneSystem_->rateMultiplier() : 1.0f);
    fireCooldown_ = fireInterval_ / std::max(0.1f, rateMul);
    return true;
}

bool GameRoot::performMeleeAttack(const Engine::TimeStep& step, const Engine::ActionState& actions) {
    (void)step;
    const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
    if (!heroTf) return false;

    Engine::Vec2 dir{};
    bool haveTarget = false;
    float visionRange = heroVisionRadiusTiles_ * static_cast<float>(fogTileSize_ > 0 ? fogTileSize_ : 32);
    float meleeRange = std::min(visionRange, meleeConfig_.range);
    float meleeRange2 = meleeRange * meleeRange;
    if (actions.primaryFire) {
        Engine::Vec2 raw{mouseWorld_.x - heroTf->position.x, mouseWorld_.y - heroTf->position.y};
        float len2 = raw.x * raw.x + raw.y * raw.y;
        if (len2 > 0.0001f) {
            float inv = 1.0f / std::sqrt(len2);
            dir = {raw.x * inv, raw.y * inv};
            haveTarget = len2 <= meleeRange2;
        }
    } else if (autoAttackEnabled_) {
        float best = meleeRange2;
        Engine::Vec2 target{};
        registry_.view<Engine::ECS::Transform, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity, const Engine::ECS::Transform& tf, const Engine::ECS::EnemyTag&) {
                float dx = tf.position.x - heroTf->position.x;
                float dy = tf.position.y - heroTf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < best) {
                    best = d2;
                    target = tf.position;
                }
            });
        if (best < meleeRange2) {
            Engine::Vec2 raw{target.x - heroTf->position.x, target.y - heroTf->position.y};
            float len2 = raw.x * raw.x + raw.y * raw.y;
            if (len2 > 0.0001f) {
                float inv = 1.0f / std::sqrt(len2);
                dir = {raw.x * inv, raw.y * inv};
                haveTarget = true;
            }
        }
    }
    if (!haveTarget) return false;

    if (registry_.has<Game::HeroSpriteSheets>(hero_)) {
        int variant = 0;
        if (auto* cycle = registry_.get<Game::HeroAttackCycle>(hero_)) {
            variant = cycle->nextVariant;
            cycle->nextVariant = 1 - cycle->nextVariant;
        } else {
            variant = 0;
            registry_.emplace<Game::HeroAttackCycle>(hero_, Game::HeroAttackCycle{1});
        }
        float dur = 0.28f;
        if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_); sheets && sheets->combat) {
            int frames = std::max(1, sheets->combat->width() / std::max(1, sheets->frameWidth));
            dur = std::max(0.05f, static_cast<float>(frames) * sheets->combatFrameDuration);
        }
        if (auto* atk = registry_.get<Game::HeroAttackAnim>(hero_)) {
            *atk = Game::HeroAttackAnim{dur, dir, variant};
        } else {
            registry_.emplace<Game::HeroAttackAnim>(hero_, Game::HeroAttackAnim{dur, dir, variant});
        }
    }

    const float cosHalfArc = std::cos(meleeConfig_.arcDegrees * 0.5f * 0.0174532925f);
    float baseDamage = projectileDamage_ * meleeConfig_.damageMultiplier *
                       rageDamageBuff_ * (hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f);
    Engine::Gameplay::DamageEvent dmgEvent{};
    dmgEvent.baseDamage = baseDamage;
    dmgEvent.type = Engine::Gameplay::DamageType::Normal;
    dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Biological] = 1.0f;

    bool hitAny = false;
    registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
        [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& health, Engine::ECS::EnemyTag&) {
            if (!health.alive()) return;
            float dx = tf.position.x - heroTf->position.x;
            float dy = tf.position.y - heroTf->position.y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 > meleeRange2) return;
            float invLen = 1.0f / std::max(0.0001f, std::sqrt(dist2));
            Engine::Vec2 toEnemy{dx * invLen, dy * invLen};
            float dot = dir.x * toEnemy.x + dir.y * toEnemy.y;
            if (dot < cosHalfArc) return;

            const float preHealth = health.currentHealth;
            const float preShields = health.currentShields;
            Engine::Gameplay::BuffState buff{};
            if (auto* armorBuff = registry_.get<Game::ArmorBuff>(e)) {
                buff = armorBuff->state;
            }
            Engine::Gameplay::applyDamage(health, dmgEvent, buff);

            float dealt = (preHealth + preShields) - (health.currentHealth + health.currentShields);
            if (dealt > 0.0f && dealt < Engine::Gameplay::MIN_DAMAGE_PER_HIT) {
                dealt = Engine::Gameplay::MIN_DAMAGE_PER_HIT;
            }
            if (dealt > 0.0f) {
                hitAny = true;
                if (xpPerDamageDealt_ > 0.0f) {
                    xp_ += static_cast<int>(std::round(dealt * xpPerDamageDealt_));
                }
                if (auto* flash = registry_.get<Game::HitFlash>(e)) {
                    flash->timer = 0.12f;
                } else {
                    registry_.emplace<Game::HitFlash>(e, Game::HitFlash{0.12f});
                }
                auto dn = registry_.create();
                registry_.emplace<Engine::ECS::Transform>(dn, tf.position);
                registry_.emplace<Game::DamageNumber>(dn, Game::DamageNumber{dealt, 0.0f});
            }
        });

    if (hitAny) {
        float rateMul = rageRateBuff_ * frenzyRateBuff_ * (hotzoneSystem_ ? hotzoneSystem_->rateMultiplier() : 1.0f);
        fireCooldown_ = fireInterval_ / std::max(0.1f, rateMul);
    }
    return hitAny;
}

void GameRoot::onShutdown() {
    if (netSession_) {
        netSession_->stop();
    }
    saveProgress();
    clearBuildPreview();
    if (fogTexture_) {
        SDL_DestroyTexture(fogTexture_);
        fogTexture_ = nullptr;
    }
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

void GameRoot::reviveLocalPlayer() {
    level_ = 1;
    xp_ = 0;
    xpToNext_ = xpBaseToLevel_;
    heroUpgrades_ = {};
    projectileDamage_ = projectileDamagePreset_;
    heroMoveSpeed_ = heroMoveSpeedPreset_;
    heroMaxHp_ = heroMaxHpPreset_;
    heroShield_ = heroShieldPreset_;
    heroHealthArmor_ = heroBaseStatsScaled_.baseHealthArmor;
    heroShieldArmor_ = heroBaseStatsScaled_.baseShieldArmor;
    if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        hp->maxHealth = heroMaxHp_;
        hp->currentHealth = heroMaxHp_;
        hp->maxShields = heroShield_;
        hp->currentShields = heroShield_;
        hp->healthArmor = heroHealthArmor_;
        hp->shieldArmor = heroShieldArmor_;
    }
    reviveNextRound_ = false;
    Engine::logInfo("Revived for next round (multiplayer).");
}

void GameRoot::leaveNetworkSession(bool isHostQuit) {
    if (netSession_) {
        if (isHostQuit && netSession_->isHost()) {
            netSession_->broadcastGoodbyeHost();
        }
        netSession_->stop();
    }
    // Destroy any remote entities immediately.
    for (auto& kv : remoteEntities_) {
        registry_.destroy(kv.second);
    }
    remoteEntities_.clear();
    remoteStates_.clear();
    remoteTargets_.clear();
    multiplayerEnabled_ = false;
    isHosting_ = false;
    menuPage_ = MenuPage::Main;
    inMenu_ = true;
}

void GameRoot::detectLocalIp() {
    hostLocalIp_ = "0.0.0.0";
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        hostLocalIp_ = "127.0.0.1";
        return;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* info = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
            for (addrinfo* p = info; p; p = p->ai_next) {
                auto* sa = reinterpret_cast<sockaddr_in*>(p->ai_addr);
                std::string ip = inet_ntoa(sa->sin_addr);
                if (ip != "127.0.0.1") {
                    hostLocalIp_ = ip;
                    break;
                }
                if (hostLocalIp_ == "0.0.0.0") {
                    hostLocalIp_ = ip;  // fallback to loopback if nothing else
                }
            }
            freeaddrinfo(info);
        }
    }
    WSACleanup();
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        return;
    }
    for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf))) {
            std::string ip(buf);
            if (ip != "127.0.0.1") {
                hostLocalIp_ = ip;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
#endif
}

void GameRoot::handleHeroDeath(const Engine::TimeStep& step) {
    auto* hp = registry_.get<Engine::ECS::Health>(hero_);
    if (!hp || defeated_) return;

    if (defeatDelayActive_) {
        defeatDelayTimer_ -= static_cast<float>(step.deltaSeconds);
        if (defeatDelayTimer_ > 0.0f) return;
        defeatDelayActive_ = false;
        defeatDelayTimer_ = 0.0f;

        defeated_ = true;
        paused_ = true;
        itemShopOpen_ = false;
        abilityShopOpen_ = false;
        levelChoiceOpen_ = false;
        Engine::logWarn("Hero defeated. Run over.");
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {0.0f, 0.0f};
        }
        if (runStarted_) {
            totalRuns_ += 1;
            totalKillsAccum_ += kills_;
            int naturalWave = std::max(0, wave_ - std::max(0, startWaveBase_ - 1));
            bestWave_ = std::max(bestWave_, naturalWave);
            depositRunGoldOnce("match_end_defeat");
            runStarted_ = false;
            saveProgress();
        }
        return;
    }

    if (hp->currentHealth > 0.0f) return;

    if (reviveCharges_ > 0) {
        reviveCharges_ -= 1;
        hp->currentHealth = std::max(1.0f, hp->maxHealth * 0.75f);
        hp->currentShields = hp->maxShields;
        hp->regenDelay = 0.0f;
        immortalTimer_ = std::max(immortalTimer_, 2.5f);
        defeatDelayActive_ = false;
        defeatDelayTimer_ = 0.0f;
        Engine::logInfo("Resurrection Tome consumed. You are back in the fight.");
        return;
    }

    bool remoteAlive = false;
    if (multiplayerEnabled_) {
        for (const auto& [id, state] : remoteStates_) {
            if (id != localPlayerId_ && state.alive) {
                remoteAlive = true;
                break;
            }
        }
    }
    if (multiplayerEnabled_ && remoteAlive) {
        reviveNextRound_ = true;
        Engine::logInfo("You are down. Awaiting next round revive (teammates still alive).");
        return;
    }

    // Delay the defeat overlay so the knockdown animation can play.
    float delay = 0.6f;
    if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_)) {
        if (sheets->movement) {
            int frames = std::max(1, sheets->movement->width() / std::max(1, sheets->frameWidth));
            delay = std::max(0.1f, static_cast<float>(frames) * sheets->movementFrameDuration);
        }
    }
    defeatDelayActive_ = true;
    defeatDelayTimer_ = delay;
    if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
        vel->value = {0.0f, 0.0f};
    }
    dashTimer_ = 0.0f;
    moveTargetActive_ = false;
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
        abilityShopOpen_ = false;
        levelChoiceOpen_ = false;
        runStarted_ = false;
        // Leave network session appropriately.
        bool hostQuit = netSession_ && netSession_->isHost();
        leaveNetworkSession(hostQuit);
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
        saveData_.version = 3;
        totalRuns_ = data.totalRuns;
        bestWave_ = data.bestWave;
        totalKillsAccum_ = data.totalKills;
        movementMode_ = (data.movementMode == 1) ? MovementMode::RTS : MovementMode::Modern;
        vaultGold_ = data.vaultGold;
        lastDepositedMatchId_ = data.lastDepositedMatchId;
        upgradeLevels_ = data.upgrades;
        Engine::logInfo("Save loaded.");
    } else {
        Engine::logWarn("No valid save found; starting fresh profile.");
    }
    recomputeGlobalModifiers();
}

void GameRoot::saveProgress() {
    saveData_.version = 3;
    saveData_.totalRuns = totalRuns_;
    saveData_.bestWave = bestWave_;
    saveData_.totalKills = totalKillsAccum_;
    saveData_.movementMode = (movementMode_ == MovementMode::RTS) ? 1 : 0;
    saveData_.vaultGold = vaultGold_;
    saveData_.lastDepositedMatchId = lastDepositedMatchId_;
    saveData_.upgrades = upgradeLevels_;
    SaveManager mgr(savePath_);
    if (!mgr.save(saveData_)) {
        Engine::logWarn("Failed to write save file.");
    }
}

void GameRoot::recomputeGlobalModifiers() {
    globalModifiers_ = Meta::computeModifiers(upgradeLevels_);
    applyGlobalModifiersToPresets();
}

void GameRoot::applyGlobalModifiersToPresets() {
    // Armor bases need scaling before being passed into unit upgrade helpers.
    heroBaseStatsScaled_ = heroBaseStats_;
    heroBaseStatsScaled_.baseHealthArmor = heroBaseStats_.baseHealthArmor * globalModifiers_.playerArmorMult;
    heroBaseStatsScaled_.baseShieldArmor = heroBaseStats_.baseShieldArmor * globalModifiers_.playerArmorMult;
    attackSpeedBaseMul_ = globalModifiers_.playerAttackSpeedMult;
    applyArchetypePreset();
    rebuildWaveSettings();
}

std::string GameRoot::generateMatchId() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    uint64_t randBits = (static_cast<uint64_t>(std::random_device{}()) << 32) ^ static_cast<uint64_t>(std::random_device{}());
    std::ostringstream ss;
    ss << now << "-" << randBits;
    return ss.str();
}

void GameRoot::depositRunGoldOnce(const std::string& reason) {
    auto res = Meta::applyDepositGuard(vaultGold_, gold_, currentMatchId_, lastDepositedMatchId_);
    if (!res.performed) return;
    vaultGold_ = res.newVault;
    lastDepositedMatchId_ = res.newLastId;
    gold_ = 0;
    saveProgress();
    std::ostringstream ss;
    ss << "Deposited " << res.deposited << " gold to vault (" << reason << ").";
    Engine::logInfo(ss.str());
}

void GameRoot::resetUpgradeError() {
    upgradeError_.clear();
    upgradeErrorTimer_ = 0.0;
}

bool GameRoot::tryPurchaseUpgrade(int index) {
    const auto& defs = Meta::upgradeDefinitions();
    if (index < 0 || index >= static_cast<int>(defs.size())) return false;
    const auto& def = defs[static_cast<std::size_t>(index)];
    int* levelPtr = Meta::levelPtrByKey(upgradeLevels_, def.key);
    if (!levelPtr) return false;
    int currentLevel = *levelPtr;
    if (def.maxLevel >= 0 && currentLevel >= def.maxLevel) {
        upgradeError_ = "MAX level reached";
        upgradeErrorTimer_ = 1.6;
        return false;
    }
    int64_t cost = Meta::costForNext(def, currentLevel);
    if (vaultGold_ < cost) {
        upgradeError_ = "Not enough gold";
        upgradeErrorTimer_ = 1.6;
        return false;
    }
    vaultGold_ -= cost;
    *levelPtr = Meta::clampLevel(def, currentLevel + 1);
    resetUpgradeError();
    recomputeGlobalModifiers();
    saveProgress();
    return true;
}

Engine::TexturePtr GameRoot::loadTextureOptional(const std::string& path) {
    if (!textureManager_ || path.empty()) return {};
    auto tex = textureManager_->getOrLoadBMP(path);
    if (!tex) return {};
    return *tex;
}

void GameRoot::rebuildFogLayer() {
    // Derive tile size from grid texture if available; fallback to 32 px.
    if (!gridTileTextures_.empty() && gridTileTextures_[0]) {
        fogTileSize_ = std::max(1, gridTileTextures_[0]->width());
    } else if (gridTexture_) {
        fogTileSize_ = std::max(1, gridTexture_->width());
    } else {
        fogTileSize_ = 32;
    }

    // Generous extent to cover normal play space; outside bounds render as unexplored black.
    const int minTiles = 512;
    const float desiredWorld = hotzoneMapRadius_ * 3.0f;  // cover beyond combat space
    const int tilesNeeded = static_cast<int>(std::ceil(desiredWorld / static_cast<float>(fogTileSize_)));
    fogWidthTiles_ = std::max(minTiles, tilesNeeded);
    fogHeightTiles_ = fogWidthTiles_;

    fogLayer_ = std::make_unique<Engine::Gameplay::FogOfWarLayer>(fogWidthTiles_, fogHeightTiles_);
    fogUnits_.clear();
    const float halfWidth = static_cast<float>(fogWidthTiles_ * fogTileSize_) * 0.5f;
    const float halfHeight = static_cast<float>(fogHeightTiles_ * fogTileSize_) * 0.5f;
    fogOriginOffsetX_ = halfWidth;
    fogOriginOffsetY_ = halfHeight;
    if (movementSystem_) {
        movementSystem_->setBounds({-halfWidth, -halfHeight}, {halfWidth, halfHeight});
    }

    // Create simple 1x1 white texture for fog overlay if missing.
    if (!fogTexture_ && sdlRenderer_) {
        fogTexture_ = SDL_CreateTexture(sdlRenderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
        if (fogTexture_) {
            const Uint32 pixel = 0xFFFFFFFFu;
            SDL_UpdateTexture(fogTexture_, nullptr, &pixel, sizeof(pixel));
            SDL_SetTextureBlendMode(fogTexture_, SDL_BLENDMODE_BLEND);
        } else {
            Engine::logWarn(std::string("Failed to create fog texture: ") + SDL_GetError());
        }
    }
}

void GameRoot::updateFogVision() {
    if (!fogLayer_) return;
    fogUnits_.clear();

    if (hero_ != Engine::ECS::kInvalidEntity) {
        const auto* tf = registry_.get<Engine::ECS::Transform>(hero_);
        const auto* hp = registry_.get<Engine::ECS::Health>(hero_);
        const bool alive = !hp || hp->currentHealth > 0.0f;
        if (tf) {
            fogUnits_.push_back(Engine::Gameplay::Unit{tf->position.x + fogOriginOffsetX_,
                                                       tf->position.y + fogOriginOffsetY_,
                                                       heroVisionRadiusTiles_, alive, 0});
        }
    }

    Engine::Gameplay::updateFogOfWar(*fogLayer_, fogUnits_, 0, fogTileSize_);
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

    // Optional external overrides for quick asset iteration.
    // Tries each name in: ~/assets/Sprites/Enemies/<name>.png then project assets/Sprites/Enemies/<name>.png.
    const std::array<std::string, 6> overrideNames{"orc", "goblin", "mummy", "skelly", "wraith", "zombie"};
    std::vector<std::pair<std::string, Engine::TexturePtr>> overrideTextures;
    overrideTextures.reserve(overrideNames.size());
    {
        const char* home = std::getenv("HOME");
        auto tryLoad = [&](const std::string& name) -> Engine::TexturePtr {
            if (home && *home) {
                fs::path p = fs::path(home) / "assets" / "Sprites" / "Enemies" / (name + ".png");
                if (fs::exists(p)) return loadTextureOptional(p.string());
            }
            fs::path p = root / (name + ".png");
            if (fs::exists(p)) return loadTextureOptional(p.string());
            return {};
        };

        for (const auto& name : overrideNames) {
            if (auto tex = tryLoad(name)) {
                overrideTextures.push_back({name, std::move(tex)});
            }
        }

        if (!overrideTextures.empty()) {
            std::string label;
            for (const auto& [name, _] : overrideTextures) {
                if (!label.empty()) label += ", ";
                label += name + ".png";
            }
            Engine::logInfo("Enemy sprite overrides enabled: " + label);
        }
    }

    if (!overrideTextures.empty()) {
        for (const auto& [name, tex] : overrideTextures) {
            EnemyDefinition def{};
            def.id = name;
            def.texture = tex;
            def.frameWidth = 16;
            def.frameHeight = 16;

            std::size_t h = std::hash<std::string>{}(def.id);
            def.sizeMultiplier = 0.9f + static_cast<float>((h >> 4) % 30) / 100.0f;     // 0.9 - 1.19
            def.hpMultiplier = 0.9f + static_cast<float>((h >> 12) % 35) / 100.0f;      // 0.9 - 1.24
            def.speedMultiplier = 0.85f + static_cast<float>((h >> 20) % 30) / 100.0f;  // 0.85 - 1.14
            def.frameDuration = 0.12f + static_cast<float>((h >> 2) % 6) * 0.01f;       // 0.12 - 0.17

            enemyDefs_.push_back(std::move(def));
        }
        Engine::logInfo("Loaded " + std::to_string(enemyDefs_.size()) + " enemy archetypes from overrides.");
        return;
    }

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

void GameRoot::loadUnitDefinitions() {
    miniUnitDefs_.clear();
    buildingDefs_.clear();

    std::ifstream f("data/units.json");
    if (!f.is_open()) {
        Engine::logWarn("data/units.json missing; mini-unit and building definitions not loaded.");
        return;
    }

    nlohmann::json j;
    f >> j;

    if (j.contains("miniUnits") && j["miniUnits"].is_object()) {
        for (const auto& [id, v] : j["miniUnits"].items()) {
            MiniUnitDef def{};
            def.id = id;
            def.cls = miniUnitClassFromString(v.value("class", id));
            def.hp = v.value("hp", 0.0f);
            def.shields = v.value("shields", 0.0f);
            def.moveSpeed = v.value("moveSpeed", 0.0f);
            def.damage = v.value("damage", 0.0f);
            def.healPerSecond = v.value("healPerSecond", 0.0f);
            def.armor = v.value("armor", 0.0f);
            def.shieldArmor = v.value("shieldArmor", 0.0f);
            // Optional tuning knobs for RTS mini-units.
            def.attackRange = v.value("attackRange", 0.0f);
            def.attackRate = v.value("attackRate", 0.0f);
            def.preferredDistance = v.value("preferredDistance", 0.0f);
            def.tauntRadius = v.value("tauntRadius", 0.0f);
            def.healRange = v.value("healRange", 0.0f);
            def.frameDuration = v.value("frameDuration", 0.0f);
            def.offensiveType = offensiveTypeFromString(v.value("offensiveType", std::string("Ranged")));
            def.costCopper = v.value("costCopper", 0);
            def.texturePath = v.value("texture", std::string{});
            miniUnitDefs_[id] = def;
        }
        auto it = miniUnitDefs_.find("light");
        if (miniUnitSystem_ && it != miniUnitDefs_.end() && it->second.moveSpeed > 0.0f) {
            miniUnitSystem_->setDefaultMoveSpeed(it->second.moveSpeed);
        }
    }

    if (j.contains("buildings") && j["buildings"].is_object()) {
        for (const auto& [id, v] : j["buildings"].items()) {
            BuildingDef def{};
            def.id = id;
            def.type = buildingTypeFromString(id);
            def.hp = v.value("hp", 0.0f);
            def.shields = v.value("shields", 0.0f);
            def.armor = v.value("armor", 0.0f);
            def.attackRange = v.value("attackRange", 0.0f);
            def.attackRate = v.value("attackRate", 0.0f);
            def.damage = v.value("damage", 0.0f);
            def.spawnInterval = v.value("spawnInterval", 0.0f);
            def.autoSpawn = v.value("autoSpawn", false);
            def.maxQueue = v.value("maxQueue", 0);
            def.capacity = v.value("capacity", 0);
            def.damageMultiplierPerUnit = v.value("damageMultiplierPerUnit", 0.0f);
            def.supplyProvided = v.value("supplyProvided", 0);
            def.costCopper = v.value("costCopper", 0);
            def.texturePath = v.value("texturePath", std::string{});
            buildingDefs_[id] = def;
        }
    }
}

void GameRoot::loadPickupTextures() {
    pickupCopperTex_ = loadTextureOptional("assets/Sprites/Misc/copperpickup.png");
    pickupGoldTex_ = loadTextureOptional("assets/Sprites/Misc/moneybag.png");
    pickupHealTex_ = loadTextureOptional("assets/Sprites/Misc/fieldmedickit.png");
    pickupHealPowerupTex_ = loadTextureOptional("assets/Sprites/Misc/healthpickup.png");
    // Visuals: explosive = kaboompickup, freeze grenade = cryonade.
    pickupKaboomTex_ = loadTextureOptional("assets/Sprites/Misc/kaboompickup.png");
    pickupFreezeTex_ = loadTextureOptional("assets/Sprites/Misc/cryonade.png");
    pickupRechargeTex_ = loadTextureOptional("assets/Sprites/Misc/rechargepickup.png");
    pickupFrenzyTex_ = loadTextureOptional("assets/Sprites/Misc/frenzypickup.png");
    pickupImmortalTex_ = loadTextureOptional("assets/Sprites/Misc/immortalpickup.png");
    pickupTurretTex_ = loadTextureOptional("assets/Sprites/Misc/turretdeployable.png");
    pickupChronoTex_ = loadTextureOptional("assets/Sprites/Misc/chronoprism.png");
    pickupPhaseLeechTex_ = loadTextureOptional("assets/Sprites/Misc/phaseleech.png");
    pickupStormCoreTex_ = loadTextureOptional("assets/Sprites/Misc/stormcore.png");
}

void GameRoot::loadProjectileTextures() {
    projectileTextures_.clear();
    projectileTexRed_ = {};
    projectileTexTurret_ = {};
    if (!sdlRenderer_) return;
    SDL_Surface* sheet = IMG_Load("assets/Sprites/Misc/playerprojectile.png");
    if (!sheet) {
        Engine::logWarn(std::string("Failed to load projectile texture: ") + IMG_GetError());
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdlRenderer_, sheet);
    if (tex) {
        auto wrapped = std::make_shared<Engine::SDLTexture>(tex);
        projectileTextures_.push_back(wrapped);
        projectileTexRed_ = wrapped;
        projectileTexTurret_ = wrapped;
    }
    SDL_FreeSurface(sheet);
}

void GameRoot::beginBuildPreview(Game::BuildingType type) {
    if (activeArchetype_.offensiveType != Game::OffensiveType::Builder) return;
    clearBuildPreview();
    buildPreviewActive_ = true;
    buildPreviewType_ = type;
    buildMenuOpen_ = false;  // hide menu while placing
    // Consume the click that opened the preview so placement waits for the next click.
    buildPreviewClickPrev_ = true;
    buildPreviewRightPrev_ = true;
    // Pick texture: prefer def texture, fallback to house/shop sprite.
    std::string texPath = "assets/Sprites/Buildings/house1.png";
    auto it = buildingDefs_.find(buildingKey(type));
    if (it != buildingDefs_.end() && !it->second.texturePath.empty()) {
        texPath = it->second.texturePath;
    }
    buildPreviewTex_ = loadTextureOptional(texPath);

    buildPreviewEntity_ = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(buildPreviewEntity_, mouseWorld_);
    Engine::Vec2 size{24.0f, 24.0f};
    if (buildPreviewTex_) {
        size = Engine::Vec2{static_cast<float>(buildPreviewTex_->width()),
                            static_cast<float>(buildPreviewTex_->height())};
    }
    registry_.emplace<Engine::ECS::Renderable>(buildPreviewEntity_,
                                               Engine::ECS::Renderable{size,
                                                                       Engine::Color{255, 255, 255, 140},
                                                                       buildPreviewTex_});
    registry_.emplace<Engine::ECS::AABB>(buildPreviewEntity_, Engine::ECS::AABB{Engine::Vec2{size.x * 0.5f, size.y * 0.5f}});
}

void GameRoot::clearBuildPreview() {
    if (buildPreviewEntity_ != Engine::ECS::kInvalidEntity) {
        registry_.destroy(buildPreviewEntity_);
    }
    buildPreviewEntity_ = Engine::ECS::kInvalidEntity;
    buildPreviewActive_ = false;
    selectedBuilding_ = Engine::ECS::kInvalidEntity;
}

bool GameRoot::spawnMiniUnit(const MiniUnitDef& def, const Engine::Vec2& pos) {
    if (miniUnitSupplyUsed_ >= miniUnitSupplyMax_) {
        Engine::logWarn("Mini-unit spawn blocked: supply cap reached.");
        return false;
    }
    if (copper_ < def.costCopper) {
        Engine::logWarn("Mini-unit spawn blocked: not enough copper.");
        return false;
    }
    miniUnitSupplyUsed_++;
    copper_ -= def.costCopper;
    auto e = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(e, pos);
    registry_.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
    const float size = 16.0f;
    registry_.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
    Engine::TexturePtr tex{};
    if (textureManager_ && !def.texturePath.empty()) {
        tex = loadTextureOptional(def.texturePath);
    }
    registry_.emplace<Engine::ECS::Renderable>(e,
        Engine::ECS::Renderable{Engine::Vec2{size, size}, Engine::Color{160, 240, 200, 255}, tex});
    if (tex) {
        int frameW = 16;
        int frameH = 16;
        int frames = std::max(1, tex->width() / frameW);
        float frameDur = def.frameDuration > 0.0f ? def.frameDuration : 0.14f;
        registry_.emplace<Engine::ECS::SpriteAnimation>(e, Engine::ECS::SpriteAnimation{frameW, frameH, frames, frameDur});
    }
    Engine::ECS::Health hp{};
    hp.maxHealth = hp.currentHealth = def.hp;
    hp.maxShields = hp.currentShields = def.shields;
    // Armor per mini unit (defaults by class if not specified).
    float armor = def.armor;
    float shieldArmor = def.shieldArmor;
    if (armor <= 0.0f && def.cls == Game::MiniUnitClass::Heavy) armor = 1.0f;
    if (shieldArmor <= 0.0f && def.cls == Game::MiniUnitClass::Heavy) shieldArmor = armor;
    hp.healthArmor = armor;
    hp.shieldArmor = shieldArmor;
    registry_.emplace<Engine::ECS::Health>(e, hp);
    registry_.emplace<Game::MiniUnit>(e, Game::MiniUnit{def.cls, 0, false, 0.0f, 0.0f});
    registry_.emplace<Game::MiniUnitCommand>(e, Game::MiniUnitCommand{false, {}});
    // Cache stats per-unit for data-driven AI/RTS control.
    Game::MiniUnitStats stats{};
    stats.moveSpeed = (def.moveSpeed > 0.0f ? def.moveSpeed : 160.0f) * globalSpeedMul_;
    stats.damage = def.damage;
    stats.healPerSecond = def.healPerSecond;
    // Per-class defaults if not specified in json.
    if (def.cls == Game::MiniUnitClass::Light) {
        stats.attackRange = def.attackRange > 0.0f ? def.attackRange : 180.0f;
        stats.attackRate = def.attackRate > 0.0f ? def.attackRate : 0.9f;
        stats.preferredDistance = def.preferredDistance > 0.0f ? def.preferredDistance : 130.0f;
    } else if (def.cls == Game::MiniUnitClass::Heavy) {
        stats.attackRange = def.attackRange > 0.0f ? def.attackRange : 140.0f;
        stats.attackRate = def.attackRate > 0.0f ? def.attackRate : 1.1f;
        stats.tauntRadius = def.tauntRadius > 0.0f ? def.tauntRadius : 180.0f;
        stats.preferredDistance = 0.0f;
    } else { // Medic
        stats.attackRange = 0.0f;
        stats.attackRate = 0.0f;
        stats.healRange = def.healRange > 0.0f ? def.healRange : 95.0f;
    }
    registry_.emplace<Game::MiniUnitStats>(e, stats);
    registry_.emplace<Game::OffensiveTypeTag>(e, Game::OffensiveTypeTag{def.offensiveType});
    return true;
}

void GameRoot::placeBuilding(const Engine::Vec2& pos) {
    auto it = buildingDefs_.find(buildingKey(buildPreviewType_));
    if (it == buildingDefs_.end()) {
        Engine::logWarn("Cannot place building: definition missing.");
        clearBuildPreview();
        return;
    }
    const BuildingDef& def = it->second;
    if (copper_ < def.costCopper) {
        Engine::logWarn("Not enough copper to place building.");
        return;
    }
    copper_ -= def.costCopper;
    auto e = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(e, pos);
    Engine::Vec2 size{28.0f, 28.0f};
    Engine::TexturePtr tex = buildPreviewTex_;
    if (!def.texturePath.empty()) {
        tex = loadTextureOptional(def.texturePath);
    }
    if (tex) {
        size = Engine::Vec2{static_cast<float>(tex->width()), static_cast<float>(tex->height())};
    }
    registry_.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{size, Engine::Color{255, 255, 255, 255}, tex});
    registry_.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{size.x * 0.5f, size.y * 0.5f}});
    Engine::ECS::Health hp{};
    hp.maxHealth = hp.currentHealth = def.hp;
    hp.maxShields = hp.currentShields = def.shields;
    hp.healthArmor = def.armor;
    hp.shieldArmor = def.armor;
    registry_.emplace<Engine::ECS::Health>(e, hp);
    registry_.emplace<Game::Building>(e, Game::Building{def.type, 0, true, def.autoSpawn ? def.spawnInterval : 0.0f});
    if (def.type == Game::BuildingType::House && def.supplyProvided > 0) {
        miniUnitSupplyMax_ = std::min(miniUnitSupplyCap_, miniUnitSupplyMax_ + def.supplyProvided);
    }
    selectedBuilding_ = e;
    clearBuildPreview();
}

void GameRoot::startNewGame() {
    applyLocalHeroFromLobby();
    applyDifficultyPreset();
    applyArchetypePreset();
    inMenu_ = false;
    userPaused_ = false;
    paused_ = false;
    itemShopOpen_ = false;
    abilityShopOpen_ = false;
    currentMatchId_ = generateMatchId();
    runStarted_ = true;
    resetRun();
}

void GameRoot::updateMenuInput(const Engine::ActionState& actions, const Engine::InputState& input, double dt) {
    menuPulse_ += dt;
    if (upgradeErrorTimer_ > 0.0) {
        upgradeErrorTimer_ -= dt;
        if (upgradeErrorTimer_ < 0.0) {
            upgradeErrorTimer_ = 0.0;
            upgradeError_.clear();
        }
    }
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
    bool menuBackEdge = actions.menuBack && !menuBackPrev_;
    menuBackPrev_ = actions.menuBack;
    bool clickEdge = leftClick && !menuClickPrev_;
    menuClickPrev_ = leftClick;

    auto processTextField = [&](bool& editing, std::string& value, int maxLen) {
        if (!editing) return;
        if (!input.textInput().empty() && static_cast<int>(value.size()) < maxLen) {
            value.append(input.textInput().substr(0, std::max(0, maxLen - static_cast<int>(value.size()))));
        }
        if (input.backspacePressed() && !value.empty()) {
            value.pop_back();
        }
        if (input.enterPressed()) {
            editing = false;
            SDL_StopTextInput();
        }
    };

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
    // Process ongoing text edits every frame.
    processTextField(editingLobbyName_, hostConfig_.lobbyName, 24);
    processTextField(editingLobbyPassword_, hostConfig_.password, 24);
    processTextField(editingJoinPassword_, joinPassword_, 24);
    processTextField(editingHostPlayerName_, hostPlayerName_, 24);
    processTextField(editingJoinPlayerName_, joinPlayerName_, 24);
    processTextField(editingDirectIp_, directConnectIp_, 48);

    if (inMenu_) {
        if (menuPage_ == MenuPage::Main) {
            const int itemCount = 7;
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
                    case 0: menuPage_ = MenuPage::CharacterSelect; menuSelection_ = 0; return;  // Solo
                    case 1: menuPage_ = MenuPage::HostConfig; menuSelection_ = 0; return;
                    case 2: menuPage_ = MenuPage::JoinSelect; menuSelection_ = 0; return;
                    case 3: menuPage_ = MenuPage::Upgrades; upgradesSelection_ = 0; upgradeConfirmOpen_ = false; return;
                    case 4: menuPage_ = MenuPage::Stats; menuSelection_ = 0; return;
                    case 5: menuPage_ = MenuPage::Options; menuSelection_ = 0; return;
                    case 6: if (app_) app_->requestQuit("Exit from main menu"); return;
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
        } else if (menuPage_ == MenuPage::Upgrades) {
            const auto& defs = Meta::upgradeDefinitions();
            int itemCount = static_cast<int>(defs.size());
            if (itemCount > 0) {
                upgradesSelection_ = std::clamp(upgradesSelection_, 0, itemCount - 1);
            }
            const float panelW = 760.0f;
            const float panelX = centerX - panelW * 0.5f;
            const float panelY = topY + 90.0f;
            const float rowH = 44.0f;
            if (!upgradeConfirmOpen_) {
                advanceSelection(std::max(1, itemCount));
            }
            // Hover rows to move selection; click purchase button to open confirm.
            for (int i = 0; i < itemCount; ++i) {
                float y = panelY + 50.0f + static_cast<float>(i) * (rowH + 8.0f);
                const auto& def = defs[static_cast<std::size_t>(i)];
                const int* lvlPtr = Meta::levelPtrByKey(upgradeLevels_, def.key);
                int lvl = lvlPtr ? *lvlPtr : 0;
                bool maxed = (def.maxLevel >= 0 && lvl >= def.maxLevel);
                if (inside(mx, my, panelX + 8.0f, y, panelW - 16.0f, rowH)) {
                    upgradesSelection_ = i;
                }
                float btnW = 110.0f;
                float btnH = 28.0f;
                float btnX = panelX + panelW - btnW - 18.0f;
                float btnY = y + 8.0f;
                if (clickEdge && inside(mx, my, btnX, btnY, btnW, btnH) && !upgradeConfirmOpen_) {
                    if (maxed) {
                        upgradeError_ = "MAX level reached";
                        upgradeErrorTimer_ = 1.2;
                    } else {
                        upgradeConfirmOpen_ = true;
                        upgradeConfirmIndex_ = i;
                    }
                }
            }
            if (confirmEdge && !upgradeConfirmOpen_ && itemCount > 0) {
                const auto& def = defs[static_cast<std::size_t>(upgradesSelection_)];
                const int* lvlPtr = Meta::levelPtrByKey(upgradeLevels_, def.key);
                int lvl = lvlPtr ? *lvlPtr : 0;
                bool maxed = (def.maxLevel >= 0 && lvl >= def.maxLevel);
                if (maxed) {
                    upgradeError_ = "MAX level reached";
                    upgradeErrorTimer_ = 1.2;
                } else {
                    upgradeConfirmOpen_ = true;
                    upgradeConfirmIndex_ = upgradesSelection_;
                }
            }
            if (upgradeConfirmOpen_) {
                float pw = 420.0f;
                float ph = 150.0f;
                float px = centerX - pw * 0.5f;
                float py = topY + 130.0f;
                float cancelX = px + pw - 190.0f;
                float confirmX = px + pw - 90.0f;
                float btnY = py + ph - 44.0f;
                bool clickConfirm = clickEdge && inside(mx, my, confirmX, btnY, 80.0f, 32.0f);
                bool clickCancel = clickEdge && inside(mx, my, cancelX, btnY, 80.0f, 32.0f);
                if (confirmEdge || clickConfirm) {
                    tryPurchaseUpgrade(upgradeConfirmIndex_);
                    upgradeConfirmOpen_ = false;
                } else if (pauseEdge || menuBackEdge || clickCancel) {
                    upgradeConfirmOpen_ = false;
                }
            }
            if ((pauseEdge || menuBackEdge) && !upgradeConfirmOpen_) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 3;  // Upgrades slot
            }
        } else if (menuPage_ == MenuPage::Options) {
            const int itemCount = 3;  // toggles + movement mode, back via Esc
            advanceSelection(itemCount);
            // hover selection
            float boxY0 = topY + 110.0f;
            for (int i = 0; i < itemCount; ++i) {
                float y = boxY0 + i * 30.0f;
                if (inside(mx, my, centerX - 150.0f, y, 280.0f, 26.0f)) {
                    menuSelection_ = i;
                }
            }
            if (confirmEdge || (clickEdge && inside(mx, my, centerX - 150.0f, boxY0, 280.0f, 26.0f * itemCount + 10.0f))) {
                switch (menuSelection_) {
                    case 0: showDamageNumbers_ = !showDamageNumbers_; break;
                    case 1: screenShake_ = !screenShake_; break;
                    case 2: {
                        movementMode_ = (movementMode_ == MovementMode::Modern) ? MovementMode::RTS : MovementMode::Modern;
                        moveTargetActive_ = false;
                        moveCommandPrev_ = false;
                        moveMarkerTimer_ = 0.0f;
                        if (cameraSystem_) {
                            const bool defaultFollow = (movementMode_ == MovementMode::Modern);
                            cameraSystem_->resetFollow(defaultFollow);
                        }
                        break;
                    }
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
                localLobbyHeroId_ = archetypes_.empty() ? localLobbyHeroId_ : archetypes_[selectedArchetype_].id;
                applyArchetypePreset();
            } else if (menuSelection_ == 1) {
                adjustIndex(selectedDifficulty_, static_cast<int>(difficulties_.size()));
                applyDifficultyPreset();
            } else if (menuSelection_ == 2 || menuSelection_ == 3) {
                // Up/down do nothing for Start/Back
            }
            // Mouse click to select entries (hover should not auto-select)
            const float panelW = 260.0f;
            const float gap = 30.0f;
            const float listStartY = topY + 80.0f;
            float leftX = centerX - panelW - gap * 0.5f;
            float rightX = centerX + gap * 0.5f;
            const float entryH = 26.0f;
            for (std::size_t i = 0; i < archetypes_.size(); ++i) {
                float y = listStartY + 32.0f + static_cast<float>(i) * entryH;
                bool hovered = inside(mx, my, leftX + 8.0f, y, panelW - 16.0f, entryH - 4.0f);
                if (hovered) {
                    menuSelection_ = 0;  // move focus but wait for click to lock choice
                    if (clickEdge) {
                        selectedArchetype_ = static_cast<int>(i);
                        localLobbyHeroId_ = archetypes_[selectedArchetype_].id;
                        applyArchetypePreset();
                    }
                }
            }
            for (std::size_t i = 0; i < difficulties_.size(); ++i) {
                float y = listStartY + 32.0f + static_cast<float>(i) * entryH;
                bool hovered = inside(mx, my, rightX + 8.0f, y, panelW - 16.0f, entryH - 4.0f);
                if (hovered) {
                    menuSelection_ = 1;
                    if (clickEdge) {
                        selectedDifficulty_ = static_cast<int>(i);
                        applyDifficultyPreset();
                    }
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
        } else if (menuPage_ == MenuPage::HostConfig) {
            // Mouse primary; allow A/D to cycle selected dropdowns.
            float boxYPlayer = topY + 104.0f;
            float boxYName = topY + 134.0f;
            float boxYPass = topY + 164.0f;
            float boxYMax = topY + 194.0f;
            float boxYDiff = topY + 224.0f;
            float startBtnY = topY + 254.0f;
            float boxX = centerX - 190.0f;
            float boxW = 380.0f;
            auto hoverBox = [&](float y) { return inside(mx, my, boxX, y, boxW, 26.0f); };

            if (clickEdge) {
                if (hoverBox(boxYPlayer)) {
                    editingHostPlayerName_ = true;
                    editingLobbyName_ = editingLobbyPassword_ = false;
                    SDL_StartTextInput();
                    menuSelection_ = 0;
                } else if (hoverBox(boxYName)) {
                    editingLobbyName_ = true;
                    editingLobbyPassword_ = false;
                    editingHostPlayerName_ = false;
                    SDL_StartTextInput();
                    menuSelection_ = 1;
                } else if (hoverBox(boxYPass)) {
                    editingLobbyPassword_ = true;
                    editingLobbyName_ = false;
                    editingHostPlayerName_ = false;
                    SDL_StartTextInput();
                    menuSelection_ = 2;
                } else if (hoverBox(boxYMax)) {
                    editingLobbyName_ = editingLobbyPassword_ = false;
                    editingHostPlayerName_ = false;
                    menuSelection_ = 3;
                    if (mx < boxX + boxW * 0.5f) hostConfig_.maxPlayers = std::max(2, hostConfig_.maxPlayers - 1);
                    else hostConfig_.maxPlayers = std::min(6, hostConfig_.maxPlayers + 1);
                } else if (hoverBox(boxYDiff)) {
                    editingLobbyName_ = editingLobbyPassword_ = false;
                    editingHostPlayerName_ = false;
                    menuSelection_ = 4;
                    if (!difficulties_.empty()) {
                        int dir = (mx < boxX + boxW * 0.5f) ? -1 : 1;
                        selectedDifficulty_ = (selectedDifficulty_ + dir + static_cast<int>(difficulties_.size())) %
                                              static_cast<int>(difficulties_.size());
                        applyDifficultyPreset();
                        hostConfig_.difficultyId = activeDifficulty_.id;
                    }
                } else if (inside(mx, my, centerX - 110.0f, startBtnY, 220.0f, 32.0f)) {
                    menuSelection_ = 5;
                    editingLobbyName_ = editingLobbyPassword_ = false;
                    editingHostPlayerName_ = false;
                    connectHost();
                    menuPage_ = MenuPage::Lobby;
                    menuSelection_ = 0;
                    return;
                }
            }
            // A/D carousel behavior for max players and difficulty.
            auto cycleDifficulty = [&](int dir) {
                if (difficulties_.empty()) return;
                selectedDifficulty_ = (selectedDifficulty_ + dir + static_cast<int>(difficulties_.size())) %
                                      static_cast<int>(difficulties_.size());
                applyDifficultyPreset();
                hostConfig_.difficultyId = activeDifficulty_.id;
            };
            if ((leftEdge || rightEdge) && !editingLobbyName_ && !editingLobbyPassword_) {
                // Prefer the currently hovered row; fallback to selected row.
                int dir = rightEdge ? 1 : -1;
                if (hoverBox(boxYMax) || menuSelection_ == 3) {
                    hostConfig_.maxPlayers = std::clamp(hostConfig_.maxPlayers + dir, 2, 6);
                    menuSelection_ = 3;
                } else if (hoverBox(boxYDiff) || menuSelection_ == 4) {
                    cycleDifficulty(dir);
                    menuSelection_ = 4;
                }
            }
            // no keyboard confirm here
            confirmEdge = false;
            if (pauseEdge || menuBackEdge) {
                editingLobbyName_ = editingLobbyPassword_ = false;
                SDL_StopTextInput();
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        } else if (menuPage_ == MenuPage::JoinSelect) {
            const int itemCount = 3;  // direct, browser, back
            // Point-click only; ignore keyboard navigation.
            upEdge = downEdge = leftEdge = rightEdge = false;
            // Only trigger confirm when the click is inside a button; otherwise ignore.
            confirmEdge = false;
            // Precompute hitboxes
            const float nameX = centerX - 150.0f;
            const float nameY = topY + 110.0f;
            const float nameW = 300.0f;
            const float nameH = 26.0f;
            auto buttonRect = [&](int idx) {
                float y = topY + 150.0f + idx * 38.0f;
                return std::tuple<float, float, float, float>{centerX - 130.0f, y, 260.0f, 30.0f};
            };
            const float passX = centerX - 150.0f;
            const float passY = topY + 270.0f;
            const float passW = 300.0f;
            const float passH = 26.0f;

            if (clickEdge) {
                if (inside(mx, my, nameX, nameY, nameW, nameH)) {
                    editingJoinPlayerName_ = true;
                    editingJoinPassword_ = false;
                    SDL_StartTextInput();
                } else {
                    editingJoinPlayerName_ = false;
                }
                if (inside(mx, my, passX, passY, passW, passH)) {
                    editingJoinPassword_ = true;
                    editingJoinPlayerName_ = false;
                    SDL_StartTextInput();
                } else if (!editingJoinPlayerName_) {
                    editingJoinPassword_ = false;
                }
                // Buttons (only if not clicking text fields)
                if (!editingJoinPlayerName_ && !editingJoinPassword_) {
                    for (int i = 0; i < itemCount; ++i) {
                        auto [bx, by, bw, bh] = buttonRect(i);
                        if (inside(mx, my, bx, by, bw, bh)) {
                            menuSelection_ = i;
                            if (i == 0) {
                                // Open direct connect popup instead of immediate connect.
                                directConnectPopup_ = true;
                                editingDirectIp_ = true;
                                SDL_StartTextInput();
                            } else {
                                confirmEdge = true;
                            }
                            break;
                        }
                    }
                }
            }
            // Handle direct connect popup interactions
            if (directConnectPopup_) {
                if (input.enterPressed()) {
                    connectDirectAddress(directConnectIp_);
                    directConnectPopup_ = false;
                    editingDirectIp_ = false;
                }
                if (pauseEdge || menuBackEdge) {
                    directConnectPopup_ = false;
                    editingDirectIp_ = false;
                }
                if (clickEdge) {
                    float pw = 420.0f;
                    float ph = 160.0f;
                    float px = centerX - pw * 0.5f;
                    float py = topY + 120.0f;
                    float ipX = px + 14.0f;
                    float ipY = py + 40.0f;
                    float ipW = pw - 28.0f;
                    float ipH = 26.0f;
                    float btnW = 140.0f;
                    float btnH = 32.0f;
                    float gapBtns = 20.0f;
                    float btnX = px + pw - (btnW * 2 + gapBtns + 14.0f);
                    float btnY = py + ph - 48.0f;
                    if (inside(mx, my, ipX, ipY, ipW, ipH)) {
                        editingDirectIp_ = true;
                        SDL_StartTextInput();
                    } else if (inside(mx, my, btnX, btnY, btnW, btnH)) {
                        // Cancel
                        directConnectPopup_ = false;
                        editingDirectIp_ = false;
                        SDL_StopTextInput();
                    } else if (inside(mx, my, btnX + btnW + gapBtns, btnY, btnW, btnH)) {
                        connectDirectAddress(directConnectIp_);
                        directConnectPopup_ = false;
                        editingDirectIp_ = false;
                        SDL_StopTextInput();
                    }
                }
                // Suppress other actions while popup open
                confirmEdge = false;
                // Skip rest of JoinSelect handling
                goto join_popup_block_end;
            }
            if (confirmEdge) {
                switch (menuSelection_) {
                    case 0: connectDirect(); menuPage_ = MenuPage::Lobby; menuSelection_ = 0; return;
                    case 1: menuPage_ = MenuPage::ServerBrowser; menuSelection_ = 0; return;
                    case 2: menuPage_ = MenuPage::Main; menuSelection_ = 0; return;
                }
            }
            if (pauseEdge) {
                menuPage_ = MenuPage::Main;
                menuSelection_ = 0;
            }
        join_popup_block_end:
            ;
        } else if (menuPage_ == MenuPage::Lobby) {
            const int itemCount = 2;  // ready/start, leave
            if (upEdge) menuSelection_ = (menuSelection_ - 1 + itemCount) % itemCount;
            if (downEdge) menuSelection_ = (menuSelection_ + 1) % itemCount;
            // Local hero picker clicks
            float pickerX = centerX - 360.0f;
            float pickerY = topY + 100.0f;
            float pickerW = 180.0f;
            float pickerH = 30.0f;
            if (clickEdge) {
                for (std::size_t i = 0; i < archetypes_.size(); ++i) {
                    float y = pickerY + static_cast<float>(i) * (pickerH + 6.0f);
                    if (y > pickerY + 200.0f) break;
                    if (inside(mx, my, pickerX, y, pickerW, pickerH)) {
                        localLobbyHeroId_ = archetypes_[i].id;
                        selectedArchetype_ = static_cast<int>(i);
                        activeArchetype_ = archetypes_[i];
                        applyArchetypePreset();
                        // send selection
                        if (netSession_) {
                            if (netSession_->isHost()) {
                                netSession_->updateHostHero(localLobbyHeroId_);
                            } else {
                                netSession_->sendHeroSelect(localLobbyHeroId_);
                            }
                        }
                        break;
                    }
                }
            }
            if (confirmEdge) {
                if (menuSelection_ == 0) {
                    if (isHosting_) {
                        if (netSession_) netSession_->beginMatch();
                        startNewGame();
                        return;
                    }
                } else if (menuSelection_ == 1) {
                    if (netSession_) netSession_->stop();
                    multiplayerEnabled_ = false;
                    isHosting_ = false;
                    menuPage_ = MenuPage::Main;
                    return;
                }
            }
            if (pauseEdge || menuBackEdge) {
    if (netSession_) {
        if (!isHosting_) {
            netSession_->sendGoodbye();
        }
        netSession_->stop();
    }
    remoteEntities_.clear();
    remoteStates_.clear();
    remoteTargets_.clear();
    menuSelection_ = 0;
            }
        } else if (menuPage_ == MenuPage::ServerBrowser) {
            if (confirmEdge || pauseEdge) {
                menuPage_ = MenuPage::JoinSelect;
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
    int mx = lastMouseX_;
    int my = lastMouseY_;
    auto inside = [](int mx, int my, float x, float y, float w, float h) {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    };
    drawTextUnified("PROJECT AURORA ZETA", Engine::Vec2{centerX - 140.0f, topY}, 1.7f, Engine::Color{180, 230, 255, 240});
    drawTextUnified("Pre-Alpha | Build v0.0.1",
                    Engine::Vec2{centerX, topY + 28.0f},
                    0.85f, Engine::Color{150, 200, 230, 220});

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
        drawButton("New Game (Solo)", 0);
        drawButton("Host", 1);
        drawButton("Join", 2);
        drawButton("Upgrades", 3);
        drawButton("Stats", 4);
        drawButton("Options", 5);
        drawButton("Exit", 6);
        float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(menuPulse_) * 2.0f);
        Engine::Color hint{static_cast<uint8_t>(180 + 40 * pulse), 220, 255, 220};
        // Position the credit text well beneath the Exit button row.
        float creditsY = topY + 80.0f + 6 * 38.0f + 48.0f;
        drawTextUnified("          A Major Bonghit Production", Engine::Vec2{centerX - 185.0f, creditsY},
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
    } else if (menuPage_ == MenuPage::Upgrades) {
        const auto& defs = Meta::upgradeDefinitions();
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Global Upgrades", Engine::Vec2{centerX - 110.0f, topY + 52.0f}, 1.2f, c);
        const float panelW = 760.0f;
        const float rowH = 44.0f;
        const float panelH = std::max(420.0f, 80.0f + static_cast<float>(defs.size()) * (rowH + 8.0f));
        const float panelX = centerX - panelW * 0.5f;
        const float panelY = topY + 80.0f;
        render_->drawFilledRect(Engine::Vec2{panelX, panelY}, Engine::Vec2{panelW, panelH},
                                Engine::Color{22, 30, 44, 220});
        drawTextUnified("Name", Engine::Vec2{panelX + 16.0f, panelY + 12.0f}, 1.05f, c);
        drawTextUnified("Level", Engine::Vec2{panelX + 200.0f, panelY + 12.0f}, 1.05f, c);
        drawTextUnified("Current", Engine::Vec2{panelX + 280.0f, panelY + 12.0f}, 1.05f, c);
        drawTextUnified("Next", Engine::Vec2{panelX + 470.0f, panelY + 12.0f}, 1.05f, c);
        std::ostringstream vault;
        vault << "Vault Gold: " << vaultGold_;
        drawTextUnified(vault.str(), Engine::Vec2{panelX + panelW - 160.0f, panelY + 12.0f}, 1.0f,
                        Engine::Color{240, 210, 120, 240});
        float rowY = panelY + 50.0f;
        for (std::size_t i = 0; i < defs.size(); ++i) {
            const auto& def = defs[i];
            const int* lvlPtr = Meta::levelPtrByKey(upgradeLevels_, def.key);
            int lvl = lvlPtr ? *lvlPtr : 0;
            bool selected = static_cast<int>(i) == upgradesSelection_;
            bool maxed = (def.maxLevel >= 0 && lvl >= def.maxLevel);
            int64_t cost = Meta::costForNext(def, lvl);
            bool affordable = !maxed && vaultGold_ >= cost && cost < std::numeric_limits<int64_t>::max();
            Engine::Color bg = selected ? Engine::Color{46, 68, 96, 235} : Engine::Color{30, 40, 56, 215};
            render_->drawFilledRect(Engine::Vec2{panelX + 8.0f, rowY}, Engine::Vec2{panelW - 16.0f, rowH}, bg);
            drawTextUnified(def.displayName, Engine::Vec2{panelX + 16.0f, rowY + 6.0f}, 1.0f,
                            Engine::Color{220, 240, 255, 240});
            std::ostringstream lvlTxt;
            lvlTxt << "Lv " << lvl;
            if (def.maxLevel >= 0) lvlTxt << "/" << def.maxLevel;
            drawTextUnified(lvlTxt.str(), Engine::Vec2{panelX + 200.0f, rowY + 6.0f}, 0.98f,
                            Engine::Color{200, 220, 235, 230});
            auto curStr = Meta::currentEffectString(def, upgradeLevels_);
            auto nextStr = Meta::nextEffectString(def, upgradeLevels_);
            drawTextUnified(curStr, Engine::Vec2{panelX + 280.0f, rowY + 6.0f}, 0.98f,
                            Engine::Color{200, 230, 255, 230});
            drawTextUnified(nextStr, Engine::Vec2{panelX + 470.0f, rowY + 6.0f}, 0.98f,
                            Engine::Color{180, 210, 240, 220});
            float btnW = 110.0f;
            float btnH = 28.0f;
            float btnX = panelX + panelW - btnW - 18.0f;
            float btnY = rowY + 8.0f;
            Engine::Color btnCol =
                maxed ? Engine::Color{90, 90, 90, 200}
                      : (affordable ? Engine::Color{80, 140, 100, 235} : Engine::Color{140, 80, 80, 220});
            render_->drawFilledRect(Engine::Vec2{btnX, btnY}, Engine::Vec2{btnW, btnH}, btnCol);
            std::ostringstream btnLabel;
            if (maxed || cost >= std::numeric_limits<int64_t>::max()) {
                btnLabel << "MAX";
            } else {
                btnLabel << cost;
            }
            float textX = btnX + (btnW * 0.5f) - 16.0f;
            drawTextUnified(btnLabel.str(), Engine::Vec2{textX, btnY + 6.0f}, 0.9f,
                            Engine::Color{220, 240, 255, 240});
            rowY += rowH + 8.0f;
        }
        if (!upgradeError_.empty()) {
            drawTextUnified(upgradeError_, Engine::Vec2{panelX + 12.0f, panelY + panelH - 28.0f}, 0.9f,
                            Engine::Color{255, 160, 160, 230});
        }
        if (upgradeConfirmOpen_ && upgradeConfirmIndex_ >= 0 &&
            upgradeConfirmIndex_ < static_cast<int>(defs.size())) {
            const auto& def = defs[static_cast<std::size_t>(upgradeConfirmIndex_)];
            const int* lvlPtr = Meta::levelPtrByKey(upgradeLevels_, def.key);
            int lvl = lvlPtr ? *lvlPtr : 0;
            int64_t cost = Meta::costForNext(def, lvl);
            float pw = 420.0f;
            float ph = 150.0f;
            float px = centerX - pw * 0.5f;
            float py = topY + 130.0f;
            render_->drawFilledRect(Engine::Vec2{px, py}, Engine::Vec2{pw, ph}, Engine::Color{24, 34, 48, 235});
            std::ostringstream prompt;
            prompt << "Buy " << def.displayName << " Lv." << (lvl + 1) << " for " << cost << " Gold?";
            drawTextUnified(prompt.str(), Engine::Vec2{px + 14.0f, py + 20.0f}, 0.95f,
                            Engine::Color{220, 240, 255, 240});
            drawTextUnified("Enter / Click confirm | Esc / Cancel", Engine::Vec2{px + 14.0f, py + 52.0f}, 0.9f,
                            Engine::Color{190, 210, 235, 220});
            float cancelX = px + pw - 190.0f;
            float confirmX = px + pw - 90.0f;
            float btnY = py + ph - 44.0f;
            render_->drawFilledRect(Engine::Vec2{cancelX, btnY}, Engine::Vec2{80.0f, 32.0f},
                                    Engine::Color{80, 90, 110, 230});
            render_->drawFilledRect(Engine::Vec2{confirmX, btnY}, Engine::Vec2{80.0f, 32.0f},
                                    Engine::Color{90, 140, 110, 235});
            drawTextUnified("Cancel", Engine::Vec2{cancelX + 12.0f, btnY + 7.0f}, 0.9f,
                            Engine::Color{220, 230, 240, 240});
            drawTextUnified("Confirm", Engine::Vec2{confirmX + 8.0f, btnY + 7.0f}, 0.9f,
                            Engine::Color{220, 240, 255, 240});
        }
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
        auto drawModeOpt = [&](const std::string& label, const std::string& value, int idx, float yOff) {
            Engine::Color box{40, 70, 90, 220};
            float y = topY + yOff;
            render_->drawFilledRect(Engine::Vec2{centerX - 150.0f, y}, Engine::Vec2{280.0f, 24.0f}, box);
            bool focused = (menuSelection_ == idx);
            Engine::Color lc = focused ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 230};
            drawTextUnified(label, Engine::Vec2{centerX - 140.0f, y + 3.0f}, 0.95f, lc);
            drawTextUnified(value, Engine::Vec2{centerX + 20.0f, y + 3.0f}, 0.95f, lc);
        };
        drawOpt("Damage Numbers", showDamageNumbers_, 0, 110.0f);
        drawOpt("Screen Shake", screenShake_, 1, 140.0f);
        const char* moveLabel = (movementMode_ == MovementMode::Modern)
                                    ? "Modern (WASD move, C toggles follow)"
                                    : "RTS-Like (RMB move, WASD pan)";
        drawModeOpt("Movement Mode", moveLabel, 2, 170.0f);
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
    } else if (menuPage_ == MenuPage::HostConfig) {
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Host Multiplayer", Engine::Vec2{centerX - 120.0f, topY + 50.0f}, 1.25f, c);
        const float boxW = 420.0f;
        Engine::Color panel{26, 36, 52, 220};
        // Extend height to cover all rows plus buttons.
        render_->drawFilledRect(Engine::Vec2{centerX - boxW * 0.5f, topY + 90.0f}, Engine::Vec2{boxW, 210.0f}, panel);
        auto drawRow = [&](const std::string& label, const std::string& value, int idx, float yOff) {
            bool focused = (menuSelection_ == idx);
            Engine::Color col = focused ? Engine::Color{230, 255, 255, 255} : Engine::Color{200, 220, 240, 230};
            drawTextUnified(label, Engine::Vec2{centerX - boxW * 0.5f + 16.0f, yOff}, 1.0f, col);
            drawTextUnified(value, Engine::Vec2{centerX + 40.0f, yOff}, 1.0f, col);
        };
        drawRow("Player Name", hostPlayerName_, 0, topY + 110.0f);
        drawRow("Lobby Name", hostConfig_.lobbyName, 1, topY + 140.0f);
        std::string maskedPass(hostConfig_.password.size(), '*');
        drawRow("Password", maskedPass.empty() ? "<optional>" : maskedPass, 2, topY + 170.0f);
        drawRow("Max Players", std::to_string(hostConfig_.maxPlayers), 3, topY + 200.0f);
        drawRow("Difficulty", activeDifficulty_.name, 4, topY + 230.0f);
        Engine::Color startCol = (menuSelection_ == 5) ? Engine::Color{200, 255, 200, 255} : Engine::Color{190, 230, 200, 230};
        drawTextUnified("Start Hosting", Engine::Vec2{centerX - 110.0f, topY + 260.0f}, 1.05f, startCol);
        std::ostringstream hostIpLine;
        hostIpLine << "Direct IP: " << hostLocalIp_ << ":" << hostConfig_.port;
        // place at bottom-right inside the host panel
        float ipX = centerX + 17.0f;
        drawTextUnified(hostIpLine.str(), Engine::Vec2{ipX, topY + 278.0f}, 0.9f,
                        Engine::Color{180, 210, 240, 220});
        drawTextUnified("Esc to cancel", Engine::Vec2{centerX - 90.0f, topY + 296.0f}, 0.9f,
                        Engine::Color{180, 200, 220, 220});
    } else if (menuPage_ == MenuPage::JoinSelect) {
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Join Multiplayer", Engine::Vec2{centerX - 120.0f, topY + 60.0f}, 1.25f, c);
        // Player name box (top)
        render_->drawFilledRect(Engine::Vec2{centerX - 150.0f, topY + 110.0f}, Engine::Vec2{300.0f, 26.0f},
                                Engine::Color{28, 38, 52, 220});
        bool nameFocus = editingJoinPlayerName_;
        Engine::Color nameCol = nameFocus ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 220};
        drawTextUnified("Player Name: " + (joinPlayerName_.empty() ? "<enter name>" : joinPlayerName_),
                        Engine::Vec2{centerX - 140.0f, topY + 114.0f}, 0.95f, nameCol);
        const char* options[3] = {"Direct Connect", "Server Browser", "Back"};
        for (int i = 0; i < 3; ++i) {
            bool focused = (menuSelection_ == i);
            Engine::Color col = focused ? Engine::Color{230, 255, 255, 255} : Engine::Color{200, 220, 240, 230};
            render_->drawFilledRect(Engine::Vec2{centerX - 130.0f, topY + 150.0f + i * 38.0f},
                                    Engine::Vec2{260.0f, 30.0f}, Engine::Color{30, 42, 56, 220});
            drawTextUnified(options[i], Engine::Vec2{centerX - 110.0f, topY + 154.0f + i * 38.0f}, 1.0f, col);
        }
        std::string passMask(joinPassword_.size(), '*');
        if (passMask.empty()) passMask = "<password>";
        bool passFocus = editingJoinPassword_;
        Engine::Color passCol = passFocus ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 220};
        render_->drawFilledRect(Engine::Vec2{centerX - 150.0f, topY + 270.0f}, Engine::Vec2{300.0f, 26.0f},
                                Engine::Color{28, 38, 52, 220});
        drawTextUnified("Password: " + passMask, Engine::Vec2{centerX - 140.0f, topY + 274.0f}, 0.95f, passCol);

        // Direct connect popup
        if (directConnectPopup_) {
            Engine::Color overlay{0, 0, 0, 180};
            render_->drawFilledRect(Engine::Vec2{0, 0}, Engine::Vec2{static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_)}, overlay);
            float pw = 420.0f;
            float ph = 160.0f;
            float px = centerX - pw * 0.5f;
            float py = topY + 120.0f;
            render_->drawFilledRect(Engine::Vec2{px, py}, Engine::Vec2{pw, ph}, Engine::Color{24, 34, 48, 235});
            drawTextUnified("Direct Connect", Engine::Vec2{px + 14.0f, py + 10.0f}, 1.05f,
                            Engine::Color{200, 230, 255, 240});
            render_->drawFilledRect(Engine::Vec2{px + 14.0f, py + 40.0f}, Engine::Vec2{pw - 28.0f, 26.0f},
                                    Engine::Color{32, 46, 66, 230});
            Engine::Color ipCol = editingDirectIp_ ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 220};
            drawTextUnified("IP: " + directConnectIp_, Engine::Vec2{px + 20.0f, py + 44.0f}, 0.95f, ipCol);
            // Buttons
            float btnW = 140.0f;
            float btnH = 32.0f;
            float gapBtns = 20.0f;
            float btnX = px + pw - (btnW * 2 + gapBtns + 14.0f);
            float btnY = py + ph - 48.0f;
            render_->drawFilledRect(Engine::Vec2{btnX, btnY}, Engine::Vec2{btnW, btnH}, Engine::Color{34, 48, 64, 230});
            render_->drawFilledRect(Engine::Vec2{btnX + btnW + gapBtns, btnY}, Engine::Vec2{btnW, btnH},
                                    Engine::Color{34, 48, 64, 230});
            drawTextUnified("Cancel", Engine::Vec2{btnX + 26.0f, btnY + 6.0f}, 0.98f, Engine::Color{210, 220, 230, 230});
            drawTextUnified("Connect", Engine::Vec2{btnX + btnW + gapBtns + 18.0f, btnY + 6.0f}, 0.98f,
                            Engine::Color{200, 255, 200, 230});
        }
    } else if (menuPage_ == MenuPage::Lobby) {
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Lobby: " + lobbyCache_.lobbyName, Engine::Vec2{centerX - 150.0f, topY + 50.0f}, 1.2f, c);
        drawTextUnified("Players (" + std::to_string(lobbyCache_.players.size()) + "/" + std::to_string(lobbyCache_.maxPlayers) + ")",
                        Engine::Vec2{centerX - 140.0f, topY + 80.0f}, 0.95f, c);
        // Local hero picker panel (left side)
        float pickerX = centerX - 400.0f;
        float pickerY = topY + 100.0f;
        float pickerW = 180.0f;
        float pickerH = 30.0f;
        Engine::Color panel{26, 36, 52, 220};
        float panelHeight = std::min(260.0f, static_cast<float>(archetypes_.size()) * (pickerH + 6.0f) + 34.0f);
        render_->drawFilledRect(Engine::Vec2{pickerX - 10.0f, pickerY - 12.0f}, Engine::Vec2{pickerW + 20.0f, panelHeight}, panel);
        drawTextUnified("Select Hero", Engine::Vec2{pickerX, pickerY - 10.0f}, 0.95f, Engine::Color{200, 230, 255, 235});
        float optionStartY = pickerY + 10.0f;
        for (std::size_t i = 0; i < archetypes_.size(); ++i) {
            float y = optionStartY + static_cast<float>(i) * (pickerH + 6.0f);
            if (y > pickerY + 200.0f) break;
            bool selected = archetypes_[i].id == localLobbyHeroId_;
            Engine::Color bg = selected ? Engine::Color{40, 80, 110, 230} : Engine::Color{32, 48, 64, 210};
            render_->drawFilledRect(Engine::Vec2{pickerX, y}, Engine::Vec2{pickerW, pickerH}, bg);
            drawTextUnified(archetypes_[i].name, Engine::Vec2{pickerX + 8.0f, y + 6.0f}, 0.9f,
                            selected ? Engine::Color{220, 255, 255, 255} : Engine::Color{200, 220, 240, 230});
        }

        float listY = topY + 110.0f;
        const float rowH = 26.0f;
        for (std::size_t i = 0; i < lobbyCache_.players.size(); ++i) {
            const auto& p = lobbyCache_.players[i];
            Engine::Color row{28, 40, 54, 220};
            render_->drawFilledRect(Engine::Vec2{centerX - 190.0f, listY + static_cast<float>(i) * rowH},
                                    Engine::Vec2{380.0f, rowH - 2.0f}, row);
            std::ostringstream line;
            line << (p.isHost ? "[H] " : "") << p.name << " (" << (p.heroId.empty() ? "hero?" : p.heroId) << ") - "
                 << (p.ready ? "Ready" : "Not Ready");
            drawTextUnified(line.str(), Engine::Vec2{centerX - 180.0f, listY + static_cast<float>(i) * rowH + 4.0f},
                            0.95f, Engine::Color{210, 230, 250, 230});
        }
        Engine::Color btnColStart = (menuSelection_ == 0) ? Engine::Color{200, 255, 200, 255} : Engine::Color{180, 220, 200, 230};
        Engine::Color btnColBack = (menuSelection_ == 1) ? Engine::Color{230, 200, 200, 255} : Engine::Color{210, 190, 190, 230};
        float btnY = topY + 220.0f;
        Engine::Vec2 startPos{centerX - 190.0f, btnY};
        Engine::Vec2 leavePos{centerX + 10.0f, btnY};
        Engine::Vec2 btnSize{180.0f, 32.0f};
        bool hoverStart = inside(mx, my, startPos.x, startPos.y, btnSize.x, btnSize.y);
        bool hoverLeave = inside(mx, my, leavePos.x, leavePos.y, btnSize.x, btnSize.y);
        if (hoverStart) menuSelection_ = 0;
        if (hoverLeave) menuSelection_ = 1;
        render_->drawFilledRect(startPos, btnSize, Engine::Color{34, 48, 60, 220});
        render_->drawFilledRect(leavePos, btnSize, Engine::Color{34, 48, 60, 220});
        drawTextUnified(isHosting_ ? "Start Match" : "Waiting...", Engine::Vec2{startPos.x + 20.0f, btnY + 4.0f}, 0.98f, btnColStart);
        drawTextUnified("Leave Lobby", Engine::Vec2{leavePos.x + 30.0f, btnY + 4.0f}, 0.98f, btnColBack);
    } else if (menuPage_ == MenuPage::ServerBrowser) {
        Engine::Color c{200, 230, 255, 240};
        drawTextUnified("Server Browser (placeholder)", Engine::Vec2{centerX - 180.0f, topY + 60.0f}, 1.2f, c);
        drawTextUnified("No broadcasts yet. Press Enter/Esc to go back.", Engine::Vec2{centerX - 210.0f, topY + 100.0f},
                        0.95f, Engine::Color{190, 210, 230, 230});
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
                def.offensiveType = offensiveTypeFromString(a.value("offensiveType", std::string("Ranged")));
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
                            float spd, float dmg, Engine::Color col, Game::OffensiveType offense) {
        ArchetypeDef def{id, name, desc, hp, spd, dmg, col, "", offense};
        archetypes_.push_back(def);
    };
    if (archetypes_.empty()) {
        addArchetype("tank", "Tank", "High survivability frontliner with slower stride.", 1.35f, 0.9f, 0.9f,
                     Engine::Color{110, 190, 255, 255}, Game::OffensiveType::Melee);
        addArchetype("healer", "Healer", "Supportive sustain, balanced speed, lighter offense.", 1.05f, 1.0f, 0.95f,
                     Engine::Color{170, 220, 150, 255}, Game::OffensiveType::Ranged);
        addArchetype("damage", "Damage Dealer", "Glass-cannon firepower, slightly faster pacing.", 0.95f, 1.05f, 1.15f,
                     Engine::Color{255, 180, 120, 255}, Game::OffensiveType::Ranged);
        addArchetype("assassin", "Assassin", "Very quick and lethal but fragile.", 0.85f, 1.25f, 1.2f,
                     Engine::Color{255, 110, 180, 255}, Game::OffensiveType::Melee);
        addArchetype("builder", "Builder", "Sturdier utility specialist with slower advance.", 1.1f, 0.95f, 0.9f,
                     Engine::Color{200, 200, 120, 255}, Game::OffensiveType::Builder);
        addArchetype("support", "Support", "All-rounder tuned for team buffs.", 1.0f, 1.0f, 1.0f,
                     Engine::Color{150, 210, 230, 255}, Game::OffensiveType::Ranged);
        addArchetype("special", "Special", "Experimental kit with slight boosts across the board.", 1.1f, 1.05f, 1.05f,
                     Engine::Color{200, 160, 240, 255}, Game::OffensiveType::Ranged);
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

void GameRoot::buildNetSession() {
    if (!netSession_) {
        netSession_ = std::make_unique<Game::Net::NetSession>();
    }
    netSession_->setSnapshotProvider([this]() { return collectNetSnapshot(); });
    netSession_->setSnapshotConsumer([this](const Game::Net::SnapshotMsg& snap) { applyRemoteSnapshot(snap); });
}

void GameRoot::connectHost() {
    buildNetSession();
    if (!netSession_) return;
    hostConfig_.maxPlayers = std::clamp(hostConfig_.maxPlayers, 2, 6);
    hostConfig_.playerName = hostPlayerName_.empty() ? "Host" : hostPlayerName_;
    bool ok = netSession_->host(hostConfig_);
    if (ok) {
        multiplayerEnabled_ = true;
        isHosting_ = true;
        localPlayerId_ = netSession_->localPlayerId();
        lobbyCache_ = netSession_->lobby();
        Engine::logInfo("Lobby hosted: " + hostConfig_.lobbyName);
    } else {
        Engine::logError("Failed to host lobby.");
    }
}

void GameRoot::connectDirect() {
    buildNetSession();
    if (!netSession_) return;
    std::ostringstream ip;
    ip << joinIpSegments_[0] << "." << joinIpSegments_[1] << "." << joinIpSegments_[2] << "." << joinIpSegments_[3];
    std::string playerName = joinPlayerName_.empty() ? "Player" : joinPlayerName_;
    bool ok = netSession_->join(ip.str(), joinPort_, joinPassword_, playerName);
    if (ok) {
        multiplayerEnabled_ = true;
        isHosting_ = false;
        Engine::logInfo("Joining host at " + ip.str() + ":" + std::to_string(joinPort_));
    } else {
        Engine::logError("Failed to join host.");
    }
}

void GameRoot::connectDirectAddress(const std::string& address) {
    std::string host = address;
    uint16_t port = joinPort_;
    auto pos = address.find(':');
    if (pos != std::string::npos) {
        host = address.substr(0, pos);
        try {
            port = static_cast<uint16_t>(std::stoi(address.substr(pos + 1)));
        } catch (...) {
            port = joinPort_;
        }
    }
    buildNetSession();
    if (!netSession_) return;
    std::string playerName = joinPlayerName_.empty() ? "Player" : joinPlayerName_;
    bool ok = netSession_->join(host, port, joinPassword_, playerName);
    if (ok) {
        multiplayerEnabled_ = true;
        isHosting_ = false;
        Engine::logInfo("Joining host at " + host + ":" + std::to_string(port));
    } else {
        Engine::logError("Failed to join host.");
    }
}

void GameRoot::updateNetwork(double dt) {
    if (!netSession_) return;
    netSession_->update(dt);
    lobbyCache_ = netSession_->lobby();
    localPlayerId_ = netSession_->localPlayerId();
    // Align difficulty with lobby host only when we're actually in a multiplayer lobby.
    // Without this guard, the local menu would snap back to the lobby default ("normal")
    // even while browsing Solo/Host setup screens before a session exists.
    if (multiplayerEnabled_ && netSession_->inLobby()) {
        for (std::size_t i = 0; i < difficulties_.size(); ++i) {
            if (difficulties_[i].id == lobbyCache_.difficultyId) {
                selectedDifficulty_ = static_cast<int>(i);
                applyDifficultyPreset();
                break;
            }
        }
    }
    if (multiplayerEnabled_ && netSession_->inMatch() && inMenu_) {
        inMenu_ = false;
        applyLocalHeroFromLobby();
        startNewGame();
    }
    // Transition client from Join page into Lobby once welcome/lobby info arrives.
    if (multiplayerEnabled_ && netSession_->inLobby() && menuPage_ == MenuPage::JoinSelect && netSession_->localPlayerId() != 0) {
        menuPage_ = MenuPage::Lobby;
        menuSelection_ = 0;
    }
    // Remove stale remote avatars not present in lobby (host) or peers list (client).
    std::unordered_set<uint32_t> liveIds;
    for (const auto& p : lobbyCache_.players) liveIds.insert(p.id);
    for (auto it = remoteEntities_.begin(); it != remoteEntities_.end();) {
        uint32_t rid = it->first;
        if (liveIds.find(rid) == liveIds.end()) {
            registry_.destroy(it->second);
            remoteStates_.erase(rid);
            remoteTargets_.erase(rid);
            it = remoteEntities_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<Game::Net::PlayerNetState> GameRoot::collectNetSnapshot() {
    std::vector<Game::Net::PlayerNetState> out;
    Game::Net::PlayerNetState self{};
    self.id = localPlayerId_ == 0 ? 1 : localPlayerId_;
    self.heroId = activeArchetype_.id;
    if (auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) {
        self.x = tf->position.x;
        self.y = tf->position.y;
    }
    if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        self.hp = hp->currentHealth;
        self.shields = hp->currentShields;
        self.alive = hp->currentHealth > 0.0f;
    } else {
        self.hp = 0.0f;
        self.alive = false;
    }
    self.level = level_;
    out.push_back(self);
    for (const auto& [id, state] : remoteStates_) {
        if (id == self.id) continue;
        out.push_back(state);
    }
    return out;
}

void GameRoot::applyRemoteSnapshot(const Game::Net::SnapshotMsg& snap) {
    for (const auto& p : snap.players) {
        remoteStates_[p.id] = p;
        if (p.id == localPlayerId_) continue;
        Engine::ECS::Entity ent = Engine::ECS::kInvalidEntity;
        auto it = remoteEntities_.find(p.id);
        if (it != remoteEntities_.end()) {
            ent = it->second;
        } else {
            ent = registry_.create();
            float size = heroSize_;
            registry_.emplace<Engine::ECS::Transform>(ent, Engine::Vec2{p.x, p.y});
            registry_.emplace<Engine::ECS::Velocity>(ent, Engine::Vec2{0.0f, 0.0f});
            registry_.emplace<Engine::ECS::Renderable>(ent,
                                                       Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                               Engine::Color{120, 210, 255, 220},
                                                                               Engine::TexturePtr{}});
            registry_.emplace<Engine::ECS::AABB>(ent, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
            registry_.emplace<Engine::ECS::Health>(ent, Engine::ECS::Health{p.hp, std::max(1.0f, p.hp)});
            registry_.emplace<Engine::ECS::SpriteAnimation>(ent,
                                                            Engine::ECS::SpriteAnimation{16, 16, 4, 0.14f});
            remoteEntities_[p.id] = ent;
        }
        if (auto* tf = registry_.get<Engine::ECS::Transform>(ent)) {
            Engine::Vec2 target{p.x, p.y};
            Engine::Vec2 current = tf->position;
            Engine::Vec2 delta = target - current;
            remoteTargets_[p.id] = target;
            // Smooth toward target over ~80ms; clamp to avoid warp.
            float smoothTime = 0.08f;
            Engine::Vec2 vel = (smoothTime > 0.0f) ? delta * (1.0f / smoothTime) : Engine::Vec2{};
            const float maxSpeed = 900.0f;
            float magSq = vel.x * vel.x + vel.y * vel.y;
            if (magSq > maxSpeed * maxSpeed) {
                float mag = std::sqrt(magSq);
                float scale = (mag > 0.0001f) ? (maxSpeed / mag) : 0.0f;
                vel = vel * scale;
            }
            if (auto* v = registry_.get<Engine::ECS::Velocity>(ent)) {
                v->value = vel;
            }
            // Snap if very close.
            if (std::abs(delta.x) < 2.0f && std::abs(delta.y) < 2.0f) {
                tf->position = target;
                if (auto* v = registry_.get<Engine::ECS::Velocity>(ent)) v->value = {0.0f, 0.0f};
            }
        }
        if (auto* hp = registry_.get<Engine::ECS::Health>(ent)) {
            hp->maxHealth = std::max(hp->maxHealth, p.hp);
            hp->currentHealth = p.hp;
            hp->maxShields = std::max(hp->maxShields, p.shields);
            hp->currentShields = p.shields;
        }
        // Update appearance based on heroId
        if (auto* rend = registry_.get<Engine::ECS::Renderable>(ent)) {
            Engine::TexturePtr tex{};
            if (textureManager_) {
                const ArchetypeDef* def = findArchetypeById(p.heroId);
                std::string texPath = def ? resolveArchetypeTexture(*def) : manifest_.heroTexture;
                if (!texPath.empty()) tex = loadTextureOptional(texPath);
                if (def) {
                    rend->color = def->color;
                }
            }
            if (tex) rend->texture = tex;
        }
    }
}

int GameRoot::currentPlayerCount() const {
    if (!netSession_) return 1;
    int players = netSession_->playerCount();
    return std::max(1, players);
}

void GameRoot::applyLocalHeroFromLobby() {
    // Find our lobby entry and set archetype accordingly.
    uint32_t me = localPlayerId_;
    for (const auto& p : lobbyCache_.players) {
        if (p.id == me) {
            localLobbyHeroId_ = p.heroId.empty() ? localLobbyHeroId_ : p.heroId;
            break;
        }
    }
    if (!localLobbyHeroId_.empty()) {
        for (std::size_t i = 0; i < archetypes_.size(); ++i) {
            if (archetypes_[i].id == localLobbyHeroId_) {
                selectedArchetype_ = static_cast<int>(i);
                activeArchetype_ = archetypes_[i];
                applyArchetypePreset();
                break;
            }
        }
    }
}

const GameRoot::ArchetypeDef* GameRoot::findArchetypeById(const std::string& id) const {
    for (const auto& a : archetypes_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

void GameRoot::rebuildWaveSettings() {
    waveSettingsBase_ = waveSettingsDefault_;
    startWaveBase_ = std::max(1, activeDifficulty_.startWave);
    float hpMul = activeDifficulty_.enemyHpMul;
    float hpGrowth = std::pow(1.08f, static_cast<float>(startWaveBase_ - 1));
    float speedGrowth = std::pow(1.01f, static_cast<float>(startWaveBase_ - 1));
    float enemyPower = globalModifiers_.enemyPowerMult;
    waveSettingsBase_.enemyHp *= hpMul * hpGrowth * enemyPower;
    waveSettingsBase_.enemyShields *= hpMul * hpGrowth * enemyPower;
    waveSettingsBase_.enemySpeed *= speedGrowth;
    waveSettingsBase_.enemyHealthArmor *= enemyPower;
    waveSettingsBase_.enemyShieldArmor *= enemyPower;
    waveSettingsBase_.contactDamage *= enemyPower;
    int batchIncrease = (startWaveBase_ - 1) / 2;
    int baseBatch = std::min(12, waveSettingsBase_.batchSize + batchIncrease);
    int scaledBatch =
        std::max(1, static_cast<int>(std::floor(static_cast<float>(baseBatch) * globalModifiers_.enemyCountMult)));
    waveSettingsBase_.batchSize = std::min(16, scaledBatch);
    waveBatchBase_ = waveSettingsBase_.batchSize;
    contactDamage_ = waveSettingsBase_.contactDamage;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);
    if (collisionSystem_) collisionSystem_->setThornConfig(thornConfig_.reflectPercent, thornConfig_.maxReflectPerHit);
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
    heroMaxHpPreset_ = heroMaxHpBase_ * activeArchetype_.hpMul * globalModifiers_.playerHealthMult;
    heroShieldPreset_ = heroShieldBase_ * activeArchetype_.hpMul * globalModifiers_.playerShieldsMult;
    heroMoveSpeedPreset_ = heroMoveSpeedBase_ * activeArchetype_.speedMul * globalModifiers_.playerSpeedMult;
    projectileDamagePreset_ = projectileDamageBase_ * activeArchetype_.damageMul * globalModifiers_.playerAttackPowerMult;
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
    abilityCooldownMul_ = 1.0f;
    abilityVitalCostMul_ = 1.0f;
    abilityChargesBonus_ = 0;
    freezeTimer_ = 0.0;
    attackSpeedMul_ = attackSpeedBaseMul_;
    lifestealPercent_ = globalModifiers_.playerLifestealAdd;
    lifestealBuff_ = 0.0f;
    lifestealTimer_ = 0.0;
    attackSpeedBuffMul_ = 1.0f;
    chronoTimer_ = 0.0;
    chainBonusTemp_ = 0;
    stormTimer_ = 0.0;
    chainBase_ = 0;
    attackSpeedBuffMul_ = 1.0f;
    chronoTimer_ = 0.0;
    chainBonusTemp_ = 0;
    stormTimer_ = 0.0;
    chainBase_ = 0;
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
        slot.energyCost = static_cast<float>(def.baseCost);
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
                hp->maxHealth = heroMaxHp_;
                hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + choice.amount * 0.5f);
                heroUpgrades_.groundArmorLevel += 1;
                Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
            }
            break;
        case LevelChoiceType::Speed:
            heroMoveSpeed_ += choice.amount;
            break;
    }
    levelChoiceOpen_ = false;
    paused_ = userPaused_ || itemShopOpen_ || abilityShopOpen_;
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
        case ItemEffect::DamagePercent: return "Damage +50%";
        case ItemEffect::RangeVision: return "Range & Vision";
        case ItemEffect::AttackSpeedPercent: return "Attack Speed +50%";
        case ItemEffect::MoveSpeedFlat: return "Speed Boots";
        case ItemEffect::BonusVitalsPercent: return "Bonus Vitals";
        case ItemEffect::CooldownFaster: return "Faster CDs";
        case ItemEffect::AbilityCharges: return "+Ability Charges";
        case ItemEffect::VitalCostReduction: return "Cheaper Vitals";
    }
    return "Item";
}

static int effectiveShopCost(ItemEffect eff,
                             int base,
                             int dmgPct,
                             int atkPct,
                             int vitals,
                             int cdr,
                             int rangeVision,
                             int charges,
                             int speedBoots,
                             int vitalAust) {
    switch (eff) {
        case ItemEffect::DamagePercent:
            if (dmgPct >= 2) return -1;
            return (dmgPct == 0) ? 1000 : 2500;
        case ItemEffect::AttackSpeedPercent:
            if (atkPct >= 1) return -1;
            return 1500;
        case ItemEffect::BonusVitalsPercent:
            if (vitals >= 1) return -1;
            return 1500;
        case ItemEffect::CooldownFaster:
            if (cdr >= 2) return -1;
            return (cdr == 0) ? 500 : 1000;
        case ItemEffect::RangeVision:
            if (rangeVision >= 5) return -1;
            return 500 * (rangeVision + 1);
        case ItemEffect::AbilityCharges:
            if (charges >= 3) return -1;
            return 500;
        case ItemEffect::MoveSpeedFlat:
            if (speedBoots >= 10) return -1;
            return 200;
        case ItemEffect::VitalCostReduction:
            if (vitalAust >= 3) return -1;
            return 500;
        default:
            return base;
    }
}

void GameRoot::refreshShopInventory() {
    shopPool_ = travelShopUnlocked_ ? goldCatalog_ : itemCatalog_;
    // Shuffle and take first 4.
    std::shuffle(shopPool_.begin(), shopPool_.end(), rng_);
    shopInventory_.clear();
    for (const auto& def : shopPool_) {
        int cost = effectiveShopCost(def.effect, def.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                     shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                     shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_);
        if (cost < 0) continue;  // capped out; don't offer
        ItemDefinition priced = def;
        priced.cost = cost;
        shopInventory_.push_back(priced);
        if (shopInventory_.size() >= 4) break;
    }
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
    drawTextUnified("Traveling Shop (Gold, E to close)", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 12.0f}, 1.0f,
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
            case ItemEffect::DamagePercent: desc << "+50% all damage"; break;
            case ItemEffect::RangeVision: desc << "+3 range and +3 vision"; break;
            case ItemEffect::AttackSpeedPercent: desc << "+50% attack speed"; break;
            case ItemEffect::MoveSpeedFlat: desc << "+1 move speed"; break;
            case ItemEffect::BonusVitalsPercent: desc << "+25% HP/Shields/Energy"; break;
            case ItemEffect::CooldownFaster: desc << "Abilities cooldown 25% faster"; break;
            case ItemEffect::AbilityCharges: desc << "Abilities with charges +1 max"; break;
            case ItemEffect::VitalCostReduction: desc << "-25% vital ability costs"; break;
        }
        drawTextUnified(title, Engine::Vec2{x + 12.0f, y + 10.0f}, 1.0f, Engine::Color{220, 240, 255, 240});
        drawTextUnified(desc.str(), Engine::Vec2{x + 12.0f, y + 34.0f}, 0.95f, Engine::Color{200, 220, 240, 230});
        std::ostringstream cost;
        int dynCost = effectiveShopCost(item.effect, item.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                        shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                        shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_);
        cost << (dynCost < 0 ? 0 : dynCost) << "g";
        bool affordable = dynCost >= 0 && gold_ >= dynCost;
        Engine::Color costCol = affordable ? Engine::Color{180, 255, 200, 240}
                                           : Engine::Color{220, 160, 160, 220};
        drawTextUnified(cost.str(), Engine::Vec2{x + 12.0f, y + 62.0f}, 0.9f, costCol);
        Engine::Color hintCol{170, 200, 230, 200};
        if (!affordable && shopNoFundsTimer_ > 0.0) {
            float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(shopNoFundsTimer_) * 18.0f);
            hintCol = Engine::Color{static_cast<uint8_t>(220), static_cast<uint8_t>(140 * pulse),
                                    static_cast<uint8_t>(140 * pulse), 230};
        }
        drawTextUnified("Click to buy", Engine::Vec2{x + 12.0f, y + 96.0f}, 0.85f, hintCol);
    }
    std::ostringstream wallet;
    wallet << "Gold: " << gold_;
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
        sell << refund << "g";
        drawTextUnified(sell.str(), Engine::Vec2{invX + invW - 52.0f, ry + 4.0f}, 0.85f,
                        Engine::Color{200, 255, 200, 220});
    }
}

void GameRoot::drawBuildMenuOverlay(int mouseX, int mouseY, bool leftClickEdge) {
    if (!render_ || !buildMenuOpen_ || activeArchetype_.offensiveType != Game::OffensiveType::Builder) return;
    const float panelW = 260.0f;
    const float panelH = 220.0f;
    Engine::Vec2 topLeft{22.0f, static_cast<float>(viewportHeight_) - panelH - 22.0f};
    render_->drawFilledRect(topLeft, Engine::Vec2{panelW, panelH}, Engine::Color{20, 26, 38, 230});
    drawTextUnified("Build Menu (V)", Engine::Vec2{topLeft.x + 12.0f, topLeft.y + 10.0f}, 1.0f,
                    Engine::Color{210, 240, 255, 240});
    drawTextUnified("Click to select a building", Engine::Vec2{topLeft.x + 12.0f, topLeft.y + 36.0f}, 0.9f,
                    Engine::Color{180, 210, 230, 230});
    struct Entry { Game::BuildingType type; const char* label; };
    std::array<Entry, 4> entries{{{Game::BuildingType::House, "House (Supply)"},
                                  {Game::BuildingType::Turret, "Turret (Defense)"},
                                  {Game::BuildingType::Barracks, "Barracks (Spawns)"},
                                  {Game::BuildingType::Bunker, "Bunker (Garrison)"}}};
    float y = topLeft.y + 60.0f;
    const float rowH = 32.0f;
    for (const auto& e : entries) {
        bool hover = mouseX >= static_cast<int>(topLeft.x + 10.0f) && mouseX <= static_cast<int>(topLeft.x + panelW - 10.0f) &&
                     mouseY >= static_cast<int>(y) && mouseY <= static_cast<int>(y + rowH);
        Engine::Color bg = hover ? Engine::Color{40, 60, 80, 230} : Engine::Color{28, 36, 48, 210};
        render_->drawFilledRect(Engine::Vec2{topLeft.x + 8.0f, y}, Engine::Vec2{panelW - 16.0f, rowH}, bg);
        auto it = buildingDefs_.find(buildingKey(e.type));
        int cost = (it != buildingDefs_.end()) ? it->second.costCopper : 0;
        std::ostringstream label;
        label << e.label << " - " << cost << "c";
        drawTextUnified(label.str(), Engine::Vec2{topLeft.x + 14.0f, y + 6.0f}, 0.9f,
                        Engine::Color{220, 240, 255, 240});
        if (hover && leftClickEdge) {
            beginBuildPreview(e.type);
            buildMenuOpen_ = false;
        }
        y += rowH + 6.0f;
    }
    std::ostringstream cp;
    cp << "Copper: " << copper_;
    drawTextUnified(cp.str(), Engine::Vec2{topLeft.x + 12.0f, topLeft.y + panelH - 26.0f}, 0.9f,
                    Engine::Color{200, 255, 200, 230});
}

void GameRoot::drawAbilityShopOverlay() {
    if (!render_ || !abilityShopOpen_) return;
    const float panelW = 720.0f;
    const float panelH = 320.0f;
    const float cx = static_cast<float>(viewportWidth_) * 0.5f;
    const float cy = static_cast<float>(viewportHeight_) * 0.55f;
    Engine::Vec2 topLeft{cx - panelW * 0.5f, cy - panelH * 0.5f};
    render_->drawFilledRect(topLeft, Engine::Vec2{panelW, panelH}, Engine::Color{20, 24, 34, 230});
    drawTextUnified("Ability Shop (Copper) - B toggles", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 12.0f}, 1.0f,
                    Engine::Color{200, 255, 200, 240});
    drawTextUnified("Hover a card, then click to buy", Engine::Vec2{topLeft.x + 18.0f, topLeft.y + 32.0f},
                    0.9f, Engine::Color{180, 220, 200, 220});

    auto costForLevel = [&](int level) {
        return static_cast<int>(std::round(static_cast<float>(abilityShopBaseCost_) *
                                           std::pow(abilityShopCostGrowth_, static_cast<float>(level))));
    };

    struct UpgradeCard {
        std::string title;
        std::string desc;
        int level{0};
        int maxLevel{999};
        int cost{0};
        std::function<bool()> buy;
    };

    std::array<UpgradeCard, 6> cards;
    cards[0] = {"Weapon Damage", "+1 damage per level", abilityDamageLevel_, 999, costForLevel(abilityDamageLevel_), [&]() {
        int cost = costForLevel(abilityDamageLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityDamageLevel_ += 1;
        projectileDamage_ += abilityDamagePerLevel_;
        return true;
    }};
    cards[1] = {"Attack Speed", "+5% rate per level", abilityAttackSpeedLevel_, 999, costForLevel(abilityAttackSpeedLevel_), [&]() {
        int cost = costForLevel(abilityAttackSpeedLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityAttackSpeedLevel_ += 1;
        attackSpeedMul_ *= (1.0f + abilityAttackSpeedPerLevel_);
        fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_);
        return true;
    }};
    cards[2] = {"Weapon Range", "+1 range (max +5)", abilityRangeLevel_, abilityRangeMaxBonus_,
                costForLevel(abilityRangeLevel_), [&]() {
        if (abilityRangeLevel_ >= abilityRangeMaxBonus_) return false;
        int cost = costForLevel(abilityRangeLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityRangeLevel_ += 1;
        autoFireRangeBonus_ += abilityRangePerLevel_;
        projectileLifetime_ += abilityRangePerLevel_ / std::max(1.0f, projectileSpeed_);
        return true;
    }};
    cards[3] = {"Sight Range", "+1 vision (max +5)", abilityVisionLevel_, abilityVisionMaxBonus_,
                costForLevel(abilityVisionLevel_), [&]() {
        if (abilityVisionLevel_ >= abilityVisionMaxBonus_) return false;
        int cost = costForLevel(abilityVisionLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityVisionLevel_ += 1;
        heroVisionRadiusTiles_ = heroVisionRadiusBaseTiles_ + abilityVisionLevel_ * abilityVisionPerLevel_;
        return true;
    }};
    cards[4] = {"Max Health", "+5 HP per level", abilityHealthLevel_, 999, costForLevel(abilityHealthLevel_), [&]() {
        int cost = costForLevel(abilityHealthLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityHealthLevel_ += 1;
        heroMaxHp_ += abilityHealthPerLevel_;
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            hp->maxHealth = heroMaxHp_;
            hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + abilityHealthPerLevel_);
            heroUpgrades_.groundArmorLevel += 1;
            Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
        }
        return true;
    }};
    cards[5] = {"Armor", "+1 armor per level", abilityArmorLevel_, 999, costForLevel(abilityArmorLevel_), [&]() {
        int cost = costForLevel(abilityArmorLevel_);
        if (copper_ < cost) return false;
        copper_ -= cost;
        abilityArmorLevel_ += 1;
        heroHealthArmor_ += abilityArmorPerLevel_;
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            hp->healthArmor = heroHealthArmor_;
            Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
        }
        return true;
    }};

    const float cardW = 200.0f;
    const float cardH = 110.0f;
    const float gap = 14.0f;
    float startX = cx - (cardW * 3.0f + gap * 2.0f) * 0.5f;
    float y = topLeft.y + 60.0f;
    int mx = lastMouseX_;
    int my = lastMouseY_;
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        float x = startX + (i % 3) * (cardW + gap);
        float yRow = y + (i / 3) * (cardH + 12.0f);
        bool hovered = mx >= x && mx <= x + cardW && my >= yRow && my <= yRow + cardH;
        Engine::Color bg = hovered ? Engine::Color{46, 70, 96, 230}
                                   : Engine::Color{34, 46, 66, 220};
        render_->drawFilledRect(Engine::Vec2{x, yRow}, Engine::Vec2{cardW, cardH}, bg);
        drawTextUnified(cards[i].title, Engine::Vec2{x + 12.0f, yRow + 10.0f}, 1.0f, Engine::Color{220, 240, 255, 240});
        drawTextUnified(cards[i].desc, Engine::Vec2{x + 12.0f, yRow + 32.0f}, 0.9f, Engine::Color{200, 220, 240, 230});
        std::ostringstream lvl;
        lvl << "Lv " << cards[i].level << "/" << (cards[i].maxLevel >= 900 ? "∞" : std::to_string(cards[i].maxLevel));
        drawTextUnified(lvl.str(), Engine::Vec2{x + 12.0f, yRow + 52.0f}, 0.9f, Engine::Color{200, 230, 220, 220});
        std::ostringstream cost;
        cost << cards[i].cost << "c";
        bool affordable = copper_ >= cards[i].cost && cards[i].level < cards[i].maxLevel;
        Engine::Color costCol = affordable ? Engine::Color{180, 255, 200, 240}
                                           : Engine::Color{220, 160, 160, 220};
        drawTextUnified(cost.str(), Engine::Vec2{x + 12.0f, yRow + 70.0f}, 0.9f, costCol);
        drawTextUnified("Click to buy", Engine::Vec2{x + 12.0f, yRow + 86.0f}, 0.85f, Engine::Color{170, 200, 230, 200});
    }

    std::ostringstream wallet;
    wallet << "Copper: " << copper_;
    drawTextUnified(wallet.str(), Engine::Vec2{topLeft.x + panelW - 200.0f, topLeft.y + 12.0f}, 0.95f,
                    Engine::Color{200, 255, 200, 240});
}

void GameRoot::drawPauseOverlay() {
    if (!render_ || !paused_ || itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_ || defeated_) return;
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
    registry_.emplace<Game::LookDirection>(hero_, Game::LookDirection{Game::LookDir4::Front});
    registry_.emplace<Engine::ECS::Renderable>(hero_, Engine::ECS::Renderable{Engine::Vec2{heroSize_, heroSize_},
                                                                               heroColorPreset_});
    const float heroHalf = heroSize_ * 0.5f;
    registry_.emplace<Engine::ECS::AABB>(hero_, Engine::ECS::AABB{Engine::Vec2{heroHalf, heroHalf}});
    registry_.emplace<Engine::ECS::Health>(hero_, Engine::ECS::Health{heroMaxHp_, heroMaxHp_});
    if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        hp->tags = {Engine::Gameplay::Tag::Biological, Engine::Gameplay::Tag::Light};
        hp->maxShields = heroShield_;
        hp->currentShields = heroShield_;
        hp->healthArmor = heroHealthArmor_;
        hp->shieldArmor = heroShieldArmor_;
        hp->shieldRegenRate = heroShieldRegen_;
        hp->healthRegenRate = heroHealthRegen_;
        hp->regenDelay = heroRegenDelay_;
        const auto& mod = offensiveTypeModifiers_[offensiveTypeIndex(activeArchetype_.offensiveType)];
        hp->healthArmor += mod.healthArmorBonus;
        hp->shieldArmor += mod.shieldArmorBonus;
        hp->healthRegenRate += mod.healthRegenBonus;
        hp->shieldRegenRate += mod.shieldRegenBonus;
        Engine::Gameplay::applyUpgradesToUnit(*hp, heroBaseStatsScaled_, heroUpgrades_, false);
    }
    registry_.emplace<Engine::ECS::HeroTag>(hero_, Engine::ECS::HeroTag{});
    registry_.emplace<Game::OffensiveTypeTag>(hero_, Game::OffensiveTypeTag{activeArchetype_.offensiveType});

    // Dual-sheet character support (movement + combat).
    Engine::TexturePtr moveTex{};
    Engine::TexturePtr combatTex{};
    {
        auto folder = activeArchetype_.name.empty() ? std::string("Damage Dealer") : activeArchetype_.name;
        std::string prefix = folder;
        prefix.erase(std::remove(prefix.begin(), prefix.end(), ' '), prefix.end());  // e.g. "Damage Dealer" -> "DamageDealer"

        const char* home = std::getenv("HOME");
        auto tryLoad = [&](const std::string& rel) -> Engine::TexturePtr {
            if (home && *home) {
                std::filesystem::path p = std::filesystem::path(home) / "assets" / "Sprites" / "Characters" / folder / rel;
                if (std::filesystem::exists(p)) return loadTextureOptional(p.string());
            }
            std::filesystem::path p = std::filesystem::path("assets") / "Sprites" / "Characters" / folder / rel;
            if (std::filesystem::exists(p)) return loadTextureOptional(p.string());
            return {};
        };

        moveTex = tryLoad(prefix + "Movement.png");
        combatTex = tryLoad(prefix + "Combat.png");
    }

    if (moveTex && combatTex) {
        registry_.emplace<Game::HeroSpriteSheets>(hero_, Game::HeroSpriteSheets{moveTex, combatTex, 16, 16, 0.12f, 0.10f});
        if (!registry_.has<Game::HeroAttackCycle>(hero_)) {
            registry_.emplace<Game::HeroAttackCycle>(hero_, Game::HeroAttackCycle{0});
        }
        if (auto* rend = registry_.get<Engine::ECS::Renderable>(hero_)) {
            rend->texture = moveTex;
        }
        const int frames = std::max(1, moveTex->width() / 16);
        registry_.emplace<Engine::ECS::SpriteAnimation>(hero_, Engine::ECS::SpriteAnimation{16, 16, frames, 0.12f});
    } else {
        // Fallback to single-sheet hero texture if loaded.
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
    }

    camera_.position = {0.0f, 0.0f};
    camera_.zoom = 1.0f;
}

void GameRoot::resetRun() {
    Engine::logInfo("Resetting run state.");
    registry_ = {};
    clearBuildPreview();
    hero_ = Engine::ECS::kInvalidEntity;
    remoteEntities_.clear();
    remoteStates_.clear();
    remoteTargets_.clear();
    kills_ = 0;
    copper_ = 0;
    gold_ = 0;
    miniUnitSupplyUsed_ = 0;
    miniUnitSupplyMax_ = 0;
    miniUnitSupplyCap_ = 10;
    if (activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
        miniUnitSupplyMax_ = std::max(miniUnitSupplyMax_, 2);
    }
    level_ = 1;
    xp_ = 0;
    xpToNext_ = xpBaseToLevel_;
    round_ = 1;
    wavesClearedThisRound_ = 0;
    wavesTargetThisRound_ = wavesPerRoundBase_ + (round_ - 1) / wavesPerRoundGrowthInterval_;
    roundBanner_ = 1;
    roundBannerTimer_ = 0.0;
    energy_ = energyMax_;
    energyWarningTimer_ = 0.0;
    heroMaxHp_ = heroMaxHpPreset_;
    heroUpgrades_ = {};
    heroShield_ = heroShieldPreset_;
    heroHealthArmor_ = heroBaseStatsScaled_.baseHealthArmor;
    heroShieldArmor_ = heroBaseStatsScaled_.baseShieldArmor;
    heroMoveSpeed_ = heroMoveSpeedPreset_;
    heroVisionRadiusTiles_ = heroVisionRadiusBaseTiles_;
    projectileDamage_ = projectileDamagePreset_;
    abilityDamageLevel_ = abilityAttackSpeedLevel_ = abilityRangeLevel_ = abilityVisionLevel_ = abilityHealthLevel_ = abilityArmorLevel_ = 0;
    attackSpeedMul_ = attackSpeedBaseMul_;
    heroHealthRegen_ = heroHealthRegenBase_ + globalModifiers_.playerRegenAdd;
    heroShieldRegen_ = heroShieldRegenBase_;
    heroRegenDelay_ = heroRegenDelayBase_;
    lifestealPercent_ = globalModifiers_.playerLifestealAdd;
    lifestealBuff_ = 0.0f;
    reviveCharges_ = globalModifiers_.playerLivesAdd;
    autoFireRangeBonus_ = 0.0f;
    wave_ = std::max(0, startWaveBase_ - 1);
    defeated_ = false;
    defeatDelayActive_ = false;
    defeatDelayTimer_ = 0.0f;
    accumulated_ = 0.0;
    tickCount_ = 0;
    fireCooldown_ = 0.0;
    mouseWorld_ = {};
    moveTarget_ = {};
    moveTargetActive_ = false;
    moveCommandPrev_ = false;
    camera_ = {};
    restartPrev_ = true;  // consume the key that triggered the reset.
    waveWarmup_ = waveWarmupBase_;
    waveBannerTimer_ = 0.0;
    waveBannerWave_ = 0;
    shakeTimer_ = 0.0f;
    shakeMagnitude_ = 0.0f;
    lastHeroHp_ = -1.0f;
    inCombat_ = false;
    waveSpawned_ = false;
    combatTimer_ = combatDuration_;
    intermissionTimer_ = waveWarmupBase_;
    dashTimer_ = 0.0f;
    dashCooldownTimer_ = 0.0f;
    dashInvulnTimer_ = 0.0f;
    dashDir_ = {0.0f, 0.0f};
    travelShopUnlocked_ = false;
    inventory_.clear();
    inventory_.shrink_to_fit();
    inventorySelected_ = -1;
    inventoryScrollLeftPrev_ = false;
    inventoryScrollRightPrev_ = false;
    inventoryCyclePrev_ = false;
    shopkeeper_ = Engine::ECS::kInvalidEntity;
    interactPrev_ = false;
    useItemPrev_ = false;
    chainBounces_ = 0;
    selectedMiniUnits_.clear();
    selectingMiniUnits_ = false;
    miniSelectMousePrev_ = false;
    miniRightClickPrev_ = false;
    lastMiniSelectClickTime_ = -1.0;
    buildMenuOpen_ = false;
    buildPreviewActive_ = false;
    buildMenuClickPrev_ = false;
    buildPreviewClickPrev_ = false;
    buildPreviewRightPrev_ = false;
    selectedBuilding_ = Engine::ECS::kInvalidEntity;
    buildingPanelClickPrev_ = false;
    buildingSelectPrev_ = false;
    buildingJustSelected_ = false;

    rebuildFogLayer();

    if (waveSystem_) {
        waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettingsBase_);
        waveSystem_->setBossConfig(bossWave_, bossHpMultiplier_, bossSpeedMultiplier_);
        waveSystem_->setEnemyDefinitions(&enemyDefs_);
        waveSystem_->setBaseArmor(Engine::Gameplay::BaseStats{waveSettingsBase_.enemyHealthArmor, waveSettingsBase_.enemyShieldArmor});
        if (eventSystem_) eventSystem_->setEnemyDefinitions(&enemyDefs_);
        waveSystem_->setSpawnBatchInterval(spawnBatchRoundInterval_);
        waveSystem_->setRound(round_);
        waveSystem_->setTimer(waveWarmupBase_);
    }
    if (collisionSystem_) {
        collisionSystem_->setContactDamage(contactDamage_);
        collisionSystem_->setThornConfig(thornConfig_.reflectPercent, thornConfig_.maxReflectPerHit);
    }
    if (hotzoneSystem_) {
        hotzoneSystem_->initialize(registry_, 5);
    }
    refreshShopInventory();

    spawnHero();
    if (collisionSystem_) {
        collisionSystem_->setXpHooks(&xp_, hero_, xpPerDamageDealt_, xpPerDamageTaken_);
    }
    if (cameraSystem_) {
        const bool defaultFollow = (movementMode_ == MovementMode::Modern);
        cameraSystem_->resetFollow(defaultFollow);
    }
    itemShopOpen_ = false;
    abilityShopOpen_ = false;
    shopLeftPrev_ = shopRightPrev_ = shopMiddlePrev_ = false;
    shopNoFundsTimer_ = 0.0;
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
    fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);
}

bool GameRoot::addItemToInventory(const ItemDefinition& def) {
    if (static_cast<int>(inventory_.size()) >= inventoryCapacity_) return false;
    inventory_.push_back(ItemInstance{def, 1});
    clampInventorySelection();
    // Apply passive bonuses for Unique items.
    switch (def.effect) {
        case ItemEffect::AttackSpeed:
            // Chrono Prism is consumable; effect applied on use.
            break;
        case ItemEffect::Lifesteal:
            // Phase Leech is consumable; effect applied on use.
            lifestealBuff_ = std::max(lifestealBuff_, 0.0f);
            break;
        case ItemEffect::Chain:
            // Storm Core is consumable; effect applied on use.
            break;
        default:
            break;
    }
    return true;
}

bool GameRoot::sellItemFromInventory(std::size_t idx, int& copperOut) {
    if (idx >= inventory_.size()) return false;
    const auto& inst = inventory_[idx];
    int refund = std::max(1, inst.def.cost / 2);
    copperOut += refund;
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
