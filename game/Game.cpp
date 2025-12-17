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
#include <cmath>
#include <cstring>
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
#include "../engine/ecs/components/RPGStats.h"
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
#include "systems/RpgDamage.h"
#include "components/StatusEffects.h"
#include "components/SpellEffect.h"
#include "components/Ghost.h"
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

inline int iconIndexFromName(const std::string& name) {
    std::hash<std::string> h;
    return static_cast<int>((h(name) % 66u) + 1u);
}

struct HUDRect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

struct ItemShopLayout {
    float s{1.0f};
    float margin{22.0f};
    float panelX{0.0f};
    float panelY{0.0f};
    float panelW{0.0f};
    float panelH{0.0f};
    float headerH{0.0f};

    float offersX{0.0f};
    float offersY{0.0f};
    float offersW{0.0f};
    float offersH{0.0f};
    float cardW{0.0f};
    float cardH{0.0f};
    float cardGap{0.0f};

    float detailsX{0.0f};
    float detailsY{0.0f};
    float detailsW{0.0f};
    float detailsH{0.0f};
    float buyX{0.0f};
    float buyY{0.0f};
    float buyW{0.0f};
    float buyH{0.0f};

    float invX{0.0f};
    float invY{0.0f};
    float invW{0.0f};
    float invH{0.0f};
    float invRowH{0.0f};
};

inline ItemShopLayout itemShopLayout(int viewportW, int viewportH) {
    constexpr float refW = 1920.0f;
    constexpr float refH = 1080.0f;
    ItemShopLayout l{};
    l.s = std::clamp(std::min(static_cast<float>(viewportW) / refW, static_cast<float>(viewportH) / refH), 0.75f, 1.25f);
    l.margin = 24.0f * l.s;

    l.panelW = 1240.0f * l.s;
    l.panelH = 620.0f * l.s;
    l.panelX = static_cast<float>(viewportW) * 0.5f - l.panelW * 0.5f;
    l.panelY = static_cast<float>(viewportH) * 0.55f - l.panelH * 0.5f;
    l.headerH = 64.0f * l.s;

    const float pad = 18.0f * l.s;
    const float gap = 18.0f * l.s;
    l.offersW = 520.0f * l.s;
    l.invW = 260.0f * l.s;
    l.detailsW = l.panelW - (pad * 2.0f + l.offersW + l.invW + gap * 2.0f);
    l.detailsW = std::max(300.0f * l.s, l.detailsW);

    l.offersX = l.panelX + pad;
    l.offersY = l.panelY + l.headerH;
    l.offersH = l.panelH - l.headerH - pad;

    l.detailsX = l.offersX + l.offersW + gap;
    l.detailsY = l.offersY;
    l.detailsH = l.offersH;

    l.invX = l.detailsX + l.detailsW + gap;
    l.invY = l.offersY;
    l.invH = l.offersH;

    l.cardGap = 14.0f * l.s;
    l.cardW = (l.offersW - l.cardGap) * 0.5f;
    l.cardH = 170.0f * l.s;

    l.buyW = l.detailsW - pad * 2.0f;
    l.buyH = 54.0f * l.s;
    l.buyX = l.detailsX + pad;
    l.buyY = l.detailsY + l.detailsH - pad - l.buyH;

    l.invRowH = 34.0f * l.s;
    return l;
}

struct AbilityShopLayout {
    float s{1.0f};
    float panelX{0.0f};
    float panelY{0.0f};
    float panelW{0.0f};
    float panelH{0.0f};
    float headerH{0.0f};
    float pad{0.0f};
    float gap{0.0f};

    float listX{0.0f};
    float listY{0.0f};
    float listW{0.0f};
    float listH{0.0f};
    float rowH{0.0f};

    float detailsX{0.0f};
    float detailsY{0.0f};
    float detailsW{0.0f};
    float detailsH{0.0f};
    float buyX{0.0f};
    float buyY{0.0f};
    float buyW{0.0f};
    float buyH{0.0f};
};

inline AbilityShopLayout abilityShopLayout(int viewportW, int viewportH) {
    constexpr float refW = 1920.0f;
    constexpr float refH = 1080.0f;
    AbilityShopLayout l{};
    l.s = std::clamp(std::min(static_cast<float>(viewportW) / refW, static_cast<float>(viewportH) / refH), 0.75f, 1.25f);
    l.panelW = 1120.0f * l.s;
    l.panelH = 640.0f * l.s;
    l.panelX = static_cast<float>(viewportW) * 0.5f - l.panelW * 0.5f;
    l.panelY = static_cast<float>(viewportH) * 0.55f - l.panelH * 0.5f;
    l.headerH = 68.0f * l.s;
    l.pad = 18.0f * l.s;
    l.gap = 18.0f * l.s;

    l.listW = 420.0f * l.s;
    l.listX = l.panelX + l.pad;
    l.listY = l.panelY + l.headerH;
    l.listH = l.panelH - l.headerH - l.pad;
    l.rowH = 64.0f * l.s;

    l.detailsX = l.listX + l.listW + l.gap;
    l.detailsY = l.listY;
    l.detailsW = l.panelW - (l.pad * 2.0f + l.listW + l.gap);
    l.detailsH = l.listH;

    l.buyW = l.detailsW - l.pad * 2.0f;
    l.buyH = 56.0f * l.s;
    l.buyX = l.detailsX + l.pad;
    l.buyY = l.detailsY + l.detailsH - l.pad - l.buyH;
    return l;
}

struct IngameHudLayout {
    float s{1.0f};
    float margin{18.0f};
    float resourceX{18.0f};
    float resourceY{18.0f};
    float resourceW{360.0f};
    float resourceH{160.0f};
    float statusX{18.0f};
    float statusY{190.0f};
    float statusW{360.0f};
};

inline IngameHudLayout ingameHudLayout(int viewportW, int viewportH) {
    constexpr float refW = 1920.0f;
    constexpr float refH = 1080.0f;
    IngameHudLayout l{};
    l.s = std::clamp(std::min(static_cast<float>(viewportW) / refW, static_cast<float>(viewportH) / refH), 0.75f, 1.35f);
    l.margin = 18.0f * l.s;
    l.resourceX = l.margin;
    l.resourceY = l.margin;
    l.resourceW = 360.0f * l.s;
    // Resource panel height matches drawResourceCluster layout (4 stacked rows).
    const float pad = 12.0f * l.s;
    const float header = 20.0f * l.s;
    const float barH = 22.0f * l.s;
    const float gap = 8.0f * l.s;
    l.resourceH = pad * 2.0f + header + 4.0f * barH + 3.0f * gap;
    l.statusX = l.resourceX;
    l.statusY = l.resourceY + l.resourceH + 12.0f * l.s;
    l.statusW = l.resourceW;
    return l;
}

inline HUDRect abilityHudRect(int abilityCount, int viewportW, int viewportH) {
    const float margin = 16.0f;
    const float iconBox = 52.0f;  // background box around a scaled 16x16 icon
    const float gap = 10.0f;
    float w = abilityCount > 0 ? abilityCount * iconBox + std::max(0, abilityCount - 1) * gap : 0.0f;
    float h = 64.0f;
    float x = static_cast<float>(viewportW) - w - margin;
    float y = static_cast<float>(viewportH) - h - margin;
    return HUDRect{x, y, w, h};
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
    auto combatDebugSink = [this](const std::string& line) { this->pushCombatDebugLine(line); };
    if (collisionSystem_) collisionSystem_->setCombatDebugSink(combatDebugSink);
    if (enemyAISystem_) enemyAISystem_->setCombatDebugSink(combatDebugSink);
    // Wave settings loaded later from gameplay config.
    textureManager_ = std::make_unique<Engine::TextureManager>(*render_);
    {
        std::filesystem::path iconPath = "assets/Sprites/GUI/abilities.png";
        if (const char* home = std::getenv("HOME")) {
            std::filesystem::path p = std::filesystem::path(home) / "assets" / "Sprites" / "GUI" / "abilities.png";
            if (std::filesystem::exists(p)) {
                iconPath = p;
            }
        }
        abilityIconTex_ = loadTextureOptional(iconPath.string());
        if (!abilityIconTex_) {
            Engine::logWarn("Failed to load ability icons at " + iconPath.string());
        }
    }
    auto loadGuiTex = [&](const std::string& rel) -> Engine::TexturePtr {
        std::filesystem::path path = rel;
        if (const char* home = std::getenv("HOME")) {
            std::filesystem::path p = std::filesystem::path(home) / rel;
            if (std::filesystem::exists(p)) path = p;
        }
        return loadTextureOptional(path.string());
    };
    hpBarTex_ = loadGuiTex("assets/Sprites/GUI/healthbar.png");
    shieldBarTex_ = loadGuiTex("assets/Sprites/GUI/armorbar.png");
    energyBarTex_ = loadGuiTex("assets/Sprites/GUI/energybar.png");
    dashBarTex_ = loadGuiTex("assets/Sprites/GUI/dashbar.png");
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
        bindings_.hotbar1 = {"key:r"};
        bindings_.hotbar2 = {"key:f"};
        bindings_.restart = {"backspace"};
        bindings_.dash = {"space"};
        bindings_.characterScreen = {"i"};
        bindings_.ability1 = {"key:1"};
        bindings_.ability2 = {"key:2"};
        bindings_.ability3 = {"key:3"};
        bindings_.ultimate = {"key:4"};
        bindings_.reload = {"key:f1"};
        bindings_.buildMenu = {"v"};
        bindings_.swapWeapon = {"lalt"};
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
        manifest_.heroTexture = "assets/Sprites/Characters/Damage Dealer/Dps.png";
    }
    if (manifest_.gridTexture.empty() && manifest_.gridTextures.empty()) {
        manifest_.gridTextures = {"assets/Tilesheets/floor1.png", "assets/Tilesheets/floor2.png"};
    }

    loadGridTextures();
    loadEnemyDefinitions();
    loadUnitDefinitions();
    loadPickupTextures();
    loadProjectileTextures();
    loadRpgData();
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
    float meleeShieldBonus = meleeShieldBonus_;
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
    int bossInterval = bossInterval_;
    float bossMaxSize = bossMaxSize_;
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
    bool useRpgCombat = useRpgCombat_;
    bool useRpgLoot = useRpgLoot_;
    bool combatDebugOverlay = combatDebugOverlay_;
    auto rpgCfg = rpgResolverConfig_;
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
    float pickupItemShare = pickupItemShare_;
    {
        std::ifstream gp("data/gameplay.json");
        if (gp.is_open()) {
            nlohmann::json j;
            gp >> j;
            float globalSpeedMul = j.value("speedMultiplier", 1.0f);
            globalSpeedMul_ = std::clamp(globalSpeedMul, 0.1f, 3.0f);
            useRpgCombat = j.value("useRpgCombat", useRpgCombat);
            useRpgLoot = j.value("useRpgLoot", useRpgLoot);
            combatDebugOverlay = j.value("combatDebugOverlay", combatDebugOverlay);
            if (j.contains("rpgCombat") && j["rpgCombat"].is_object()) {
                const auto& rc = j["rpgCombat"];
                rpgCfg.armorConstant = rc.value("armorConstant", rpgCfg.armorConstant);
                rpgCfg.dodgeCap = rc.value("dodgeCap", rpgCfg.dodgeCap);
                rpgCfg.critCap = rc.value("critCap", rpgCfg.critCap);
                rpgCfg.resistMin = rc.value("resistMin", rpgCfg.resistMin);
                rpgCfg.resistMax = rc.value("resistMax", rpgCfg.resistMax);
            }
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
                meleeShieldBonus = j["hero"].value("meleeShieldBonus", meleeShieldBonus);
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
                bossInterval = j["boss"].value("interval", bossInterval);
                bossHpMul = j["boss"].value("hpMultiplier", bossHpMul);
                bossSpeedMul = j["boss"].value("speedMultiplier", bossSpeedMul);
                bossGold = j["boss"].value("killBonus", bossGold);
                bossMaxSize = j["boss"].value("maxSize", bossMaxSize);
            }
            if (j.contains("drops")) {
                pickupDropChance_ = j["drops"].value("pickupChance", pickupDropChance_);
                pickupPowerupShare_ = j["drops"].value("powerupShare", pickupPowerupShare_);
                pickupItemShare = j["drops"].value("itemShare", pickupItemShare);
                rpgEquipChanceNormal_ = j["drops"].value("rpgEquipChanceNormal", rpgEquipChanceNormal_);
                rpgEquipMiniBossCount_ = j["drops"].value("rpgEquipMiniBossCount", rpgEquipMiniBossCount_);
                rpgEquipBossCount_ = j["drops"].value("rpgEquipBossCount", rpgEquipBossCount_);
                rpgMiniBossGemChance_ = j["drops"].value("rpgMiniBossGemChance", rpgMiniBossGemChance_);
                rpgConsumableChanceNormal_ = j["drops"].value("rpgConsumableChanceNormal", rpgConsumableChanceNormal_);
                rpgConsumableMiniBossCount_ = j["drops"].value("rpgConsumableMiniBossCount", rpgConsumableMiniBossCount_);
                rpgConsumableBossCount_ = j["drops"].value("rpgConsumableBossCount", rpgConsumableBossCount_);
                rpgBagChanceMiniBoss_ = j["drops"].value("rpgBagChanceMiniBoss", rpgBagChanceMiniBoss_);
                rpgBagChanceBoss_ = j["drops"].value("rpgBagChanceBoss", rpgBagChanceBoss_);
                copperPickupMin_ = j["drops"].value("copperMin", copperPickupMin_);
                copperPickupMax_ = j["drops"].value("copperMax", copperPickupMax_);
                bossCopperDrop_ = j["drops"].value("bossCopper", bossCopperDrop_);
                miniBossCopperDrop_ = j["drops"].value("miniBossCopper", miniBossCopperDrop_);
            }
            if (j.contains("shop") && j["shop"].is_object()) {
                rpgShopEquipChance_ = j["shop"].value("rpgEquipChance", rpgShopEquipChance_);
                rpgShopEquipCount_ = j["shop"].value("rpgEquipCount", rpgShopEquipCount_);
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
    waveSystem_->setBossConfig(bossWave_, bossInterval_, bossHpMultiplier_, bossSpeedMultiplier_, bossMaxSize_);
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
    contactDamageBase_ = waveSettingsBase_.contactDamage;
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
    meleeShieldBonusBase_ = meleeShieldBonus;
    meleeShieldBonus_ = meleeShieldBonus;
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
    copperPerKillBase_ = copperPerKill;
    waveClearBonus_ = waveClearBonus;
    waveClearBonusBase_ = waveClearBonus;
    enemyLowThreshold_ = enemyLowThreshold;
    combatDuration_ = combatDuration;
    intermissionDuration_ = intermissionDuration;
    bountyGoldBonus_ = bountyGold;
    bossWave_ = bossWave;
    bossInterval_ = bossInterval;
    bossMaxSize_ = bossMaxSize;
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
    pickupItemShare_ = std::clamp(pickupItemShare, 0.0f, 1.0f);
    copperPickupMinBase_ = copperPickupMin_;
    copperPickupMaxBase_ = copperPickupMax_;
    bossCopperDropBase_ = bossCopperDrop_;
    miniBossCopperDropBase_ = miniBossCopperDrop_;
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
    useRpgCombat_ = useRpgCombat;
    useRpgLoot_ = useRpgLoot;
    combatDebugOverlay_ = combatDebugOverlay;
    rpgResolverConfig_ = rpgCfg;
    if (collisionSystem_) collisionSystem_->setRpgCombat(useRpgCombat_, rpgResolverConfig_);
    if (enemyAISystem_) enemyAISystem_->setRpgCombat(useRpgCombat_, rpgResolverConfig_, &rng_);
    if (miniUnitSystem_) miniUnitSystem_->setRpgCombat(useRpgCombat_, rpgResolverConfig_, &rng_);
    if (miniUnitSystem_) miniUnitSystem_->setCombatDebugSink([this](const std::string& line) { pushCombatDebugLine(line); });
    heroVisionRadiusBaseTiles_ = heroVisionRadiusTiles_;

    rebuildFogLayer();
    statusFactory_.load("data/statuses.json");

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
    scrollDeltaFrame_ = input.scrollDelta();

    // Sample high-level actions.
    Engine::ActionState actions = actionMapper_.sample(input);
    if (characterScreenOpen_) {
        actions.scroll = 0;
        actions.moveX = actions.moveY = 0.0f;
        actions.camX = actions.camY = 0.0f;
    }
    const bool swapEdge = actions.swapWeapon && !swapWeaponHeld_;
    swapWeaponHeld_ = actions.swapWeapon;
    if (swapEdge && !inMenu_) {
        if (activeArchetype_.id == "wizard") {
            int next = (static_cast<int>(wizardElement_) + 1) % static_cast<int>(WizardElement::Count);
            wizardElement_ = static_cast<WizardElement>(next);
        } else {
            toggleHeroWeaponMode();
        }
    }
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
	        if (abilityShopOpen_) {
	            abilityShopSelected_ = 0;
	        }
	    }
    const bool pausePressed = actions.pause && !pauseTogglePrev_;
    pauseTogglePrev_ = actions.pause;
    bool characterEdge = actions.characterScreen && !characterScreenPrev_;
    characterScreenPrev_ = actions.characterScreen;
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
    auto closeOverlays = [&]() {
        itemShopOpen_ = false;
        abilityShopOpen_ = false;
        levelChoiceOpen_ = false;
        characterScreenOpen_ = false;
        buildMenuOpen_ = false;
        clearBuildPreview();
        pauseMenuBlink_ = 0.0;
        pauseClickPrev_ = false;
        refreshPauseState();
    };
    // Debug hotkeys to apply/remove statuses quickly (F6-F9).
    {
        const Uint8* kb = SDL_GetKeyboardState(nullptr);
        static bool prevF[9]{false, false, false, false, false, false, false, false, false};
        auto edge = [&](int idx, SDL_Scancode code) {
            bool down = kb[code] != 0;
            bool fire = down && !prevF[idx];
            prevF[idx] = down;
            return fire;
        };
        auto nearestEnemy = [&](float radius) {
            Engine::ECS::Entity best = Engine::ECS::kInvalidEntity;
            float bestD2 = radius * radius;
            Engine::Vec2 heroPos{};
            if (const auto* htf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = htf->position;
            registry_.view<Engine::ECS::Transform, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::EnemyTag&) {
                    float dx = tf.position.x - heroPos.x;
                    float dy = tf.position.y - heroPos.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 < bestD2) {
                        bestD2 = d2;
                        best = e;
                    }
                });
            return best;
        };
        auto applyStatus = [&](Engine::ECS::Entity ent, Engine::Status::EStatusId id) {
            if (ent == Engine::ECS::kInvalidEntity) return;
            if (auto* st = registry_.get<Engine::ECS::Status>(ent)) {
                st->container.apply(statusFactory_.make(id), hero_);
            }
        };
        if (edge(0, SDL_SCANCODE_F1)) {
            applyStatus(nearestEnemy(520.0f), Engine::Status::EStatusId::ArmorReduction);
        }
        if (edge(1, SDL_SCANCODE_F2)) {
            applyStatus(nearestEnemy(520.0f), Engine::Status::EStatusId::Feared);
        }
        if (edge(2, SDL_SCANCODE_F3)) {
            applyStatus(nearestEnemy(520.0f), Engine::Status::EStatusId::Cauterize);
        }
        if (edge(3, SDL_SCANCODE_F4)) {
            applyStatus(nearestEnemy(520.0f), Engine::Status::EStatusId::Blindness);
        }
        if (edge(4, SDL_SCANCODE_F6)) {
            if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
                st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Stasis), hero_);
            }
        }
        if (edge(5, SDL_SCANCODE_F7)) {
            if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
                st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Unstoppable), hero_);
            }
        }
        if (edge(6, SDL_SCANCODE_F8)) {
            if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
                st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Silenced), hero_);
            }
        }
        if (edge(7, SDL_SCANCODE_F9)) {
            if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
                st->container.clear(true);
            }
        }
        if (edge(8, SDL_SCANCODE_F10)) {
            if (auto* st = registry_.get<Engine::ECS::Status>(hero_)) {
                st->container.apply(statusFactory_.make(Engine::Status::EStatusId::Cloaking), hero_);
            }
        }
    }
    if (!inMenu_) {
        bool anyOverlay = userPaused_ || itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_ || characterScreenOpen_ ||
                          buildMenuOpen_;
        if (pausePressed) {
            if (anyOverlay) {
                closeOverlays();
            } else {
                userPaused_ = !userPaused_;
                pauseMenuBlink_ = 0.0;
                refreshPauseState();
            }
        }
        if (menuBackEdge && anyOverlay) {
            closeOverlays();
        }
    }
    if (characterEdge && !inMenu_) {
        characterScreenOpen_ = !characterScreenOpen_;
    }
    refreshPauseState();
    if (actions.toggleFollow && (itemShopOpen_ || abilityShopOpen_)) {
        // ignore camera toggle while paused/shop
    }

    // Ability focus/upgrade input (scroll disabled per UI overhaul; upgrades via HUD click).
    abilityUpgradePrev_ = actions.reload;

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
                paused_ = itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_ ||
                          (characterScreenOpen_ && !multiplayerEnabled_);
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
    if (!inMenu_ && !paused_ && !characterScreenOpen_) {
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
        HUDRect abilityRect = abilityHudRect(static_cast<int>(abilities_.size()), viewportWidth_, viewportHeight_);
        mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, abilityRect.x - 6.0f, abilityRect.y - 6.0f,
                                   abilityRect.w + 12.0f, abilityRect.h + 12.0f);
        const float badgeW = 280.0f;
        const float badgeH = 50.0f;
        const float margin = 16.0f;
        float invX = margin;
        float invY = static_cast<float>(viewportHeight_) - badgeH - margin;
        mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, invX, invY, badgeW, badgeH);
        if (!selectedMiniUnits_.empty()) {
            const float hudW = 300.0f;
            const float hudH = 70.0f;
            const float hudX = static_cast<float>(viewportWidth_) * 0.5f - hudW * 0.5f;
            const float hudY = static_cast<float>(viewportHeight_) - hudH - kHudBottomSafeMargin;
            mouseOverUI |= pointInRect(lastMouseX_, lastMouseY_, hudX, hudY, hudW, hudH);
        }

        bool canDragSelect = activeArchetype_.offensiveType == Game::OffensiveType::Builder || activeArchetype_.id == "summoner";
        if (leftEdge && !mouseOverUI && canDragSelect) {
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
        const float hudY = static_cast<float>(viewportHeight_) - hudH - kHudBottomSafeMargin;
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
        const auto* heroHp = registry_.get<Engine::ECS::Health>(hero_);
        const bool heroAlive = !heroHp || heroHp->alive();
        const bool heroGhost = registry_.has<Game::Ghost>(hero_) && registry_.get<Game::Ghost>(hero_)->active;
        Engine::Vec2 heroPos = heroTf ? heroTf->position : Engine::Vec2{0.0f, 0.0f};
        const bool leftClick = input.isMouseButtonDown(0);
        const bool rightClick = input.isMouseButtonDown(2);
        const bool midClick = input.isMouseButtonDown(1);

        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            const auto* status = registry_.get<Engine::ECS::Status>(hero_);
            const bool movementLocked = status && status->container.blocksMovement();
            const bool feared = status && status->container.isFeared();
            const float moveMul = status ? status->container.moveSpeedMultiplier() : 1.0f;
            if (paused_ || defeatDelayActive_ || characterScreenOpen_ || ((heroHp && !heroAlive) && !heroGhost) || movementLocked) {
                vel->value = {0.0f, 0.0f};
            } else if (feared) {
                std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
                float a = ang(rng_);
                Engine::Vec2 dir{std::cos(a), std::sin(a)};
                vel->value = {dir.x * heroMoveSpeed_ * moveMul, dir.y * heroMoveSpeed_ * moveMul};
            } else if (movementMode_ == MovementMode::Modern) {
                vel->value = {actions.moveX * heroMoveSpeed_ * moveMul, actions.moveY * heroMoveSpeed_ * moveMul};
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
                vel->value = {desiredVel.x * moveMul, desiredVel.y * moveMul};
            }
        }
        // Dash trigger.
        const bool dashEdge = actions.dash && !dashPrev_;
        dashPrev_ = actions.dash;
        const auto* status = registry_.get<Engine::ECS::Status>(hero_);
        const bool dashLocked = status && status->container.blocksMovement();
        if (!paused_ && !defeatDelayActive_ && !characterScreenOpen_ && heroAlive && !dashLocked &&
            dashEdge && dashCooldownTimer_ <= 0.0f) {
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
        bool hotbar1Edge = actions.hotbar1 && !hotbar1Prev_;
        bool hotbar2Edge = actions.hotbar2 && !hotbar2Prev_;
        ability1Prev_ = actions.ability1;
        ability2Prev_ = actions.ability2;
        ability3Prev_ = actions.ability3;
        abilityUltPrev_ = actions.ultimate;
        interactPrev_ = actions.interact;
        useItemPrev_ = actions.useItem;
        hotbar1Prev_ = actions.hotbar1;
        hotbar2Prev_ = actions.hotbar2;
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

                const auto lay = abilityShopLayout(viewportWidth_, viewportHeight_);
                const float rowGap = 10.0f * lay.s;
                int mx = input.mouseX();
                int my = input.mouseY();

                // Hover selects.
                int hoveredRow = -1;
                const float rowsStartY = lay.listY + lay.pad + 42.0f * lay.s;
                const float rowsX = lay.listX + lay.pad;
                const float rowsW = lay.listW - lay.pad * 2.0f;
                for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
                    float y = rowsStartY + static_cast<float>(i) * (lay.rowH + rowGap);
                    if (mx >= rowsX && mx <= rowsX + rowsW && my >= y && my <= y + lay.rowH) {
                        hoveredRow = i;
                        break;
                    }
                }
                if (hoveredRow >= 0) {
                    abilityShopSelected_ = hoveredRow;
                }
                abilityShopSelected_ = std::clamp(abilityShopSelected_, 0, static_cast<int>(cards.size()) - 1);

                // Click buy.
                bool leftEdge = leftClick && !shopLeftPrev_;
                if (leftEdge) {
                    if (mx >= lay.buyX && mx <= lay.buyX + lay.buyW && my >= lay.buyY && my <= lay.buyY + lay.buyH) {
                        if (!cards[abilityShopSelected_].buy()) {
                            shopNoFundsTimer_ = 0.6;
                        }
                    }
                }
            }

            // Item shop UI interactions: select offers, buy, and sell inventory.
            if (itemShopOpen_) {
                const auto lay = itemShopLayout(viewportWidth_, viewportHeight_);
                int mx = input.mouseX();
                int my = input.mouseY();

                auto insideF = [&](float x, float y, float w, float h) {
                    return mx >= static_cast<int>(x) && mx <= static_cast<int>(x + w) &&
                           my >= static_cast<int>(y) && my <= static_cast<int>(y + h);
                };

                const float innerPad = 18.0f * lay.s;
                const float offersStartX = lay.offersX;
                const float offersStartY = lay.offersY + 34.0f * lay.s;

                int hoveredOffer = -1;
                for (int i = 0; i < static_cast<int>(shopInventory_.size()); ++i) {
                    int c = i % 2;
                    int r = i / 2;
                    float x = offersStartX + static_cast<float>(c) * (lay.cardW + lay.cardGap);
                    float y = offersStartY + static_cast<float>(r) * (lay.cardH + lay.cardGap);
                    if (insideF(x, y, lay.cardW, lay.cardH)) {
                        hoveredOffer = i;
                        break;
                    }
                }
                if (hoveredOffer >= 0) {
                    itemShopSelected_ = hoveredOffer;
                }
                if (!shopInventory_.empty()) {
                    itemShopSelected_ = std::clamp(itemShopSelected_, 0, static_cast<int>(shopInventory_.size()) - 1);
                } else {
                    itemShopSelected_ = 0;
                }

                // Inventory sell scroll (mouse wheel while hovering list).
                const float invHeader = 50.0f * lay.s;
                const float invListX = lay.invX + innerPad;
                const float invListY = lay.invY + invHeader;
                const float invListW = lay.invW - innerPad * 2.0f;
                const float invListH = lay.invH - invHeader - innerPad;
                const bool invHover = insideF(invListX, invListY, invListW, invListH);
                const int visibleRows = std::max(1, static_cast<int>(std::floor(invListH / lay.invRowH)));
                const int maxScrollRows = std::max(0, static_cast<int>(inventory_.size()) - visibleRows);
                if (invHover && scrollDeltaFrame_ != 0) {
                    shopSellScroll_ = std::clamp(shopSellScroll_ - static_cast<float>(scrollDeltaFrame_) * 1.0f, 0.0f,
                                                 static_cast<float>(maxScrollRows));
                }
                const int scrollRow = std::clamp(static_cast<int>(std::round(shopSellScroll_)), 0, maxScrollRows);

                auto dynCostFor = [&](const ItemDefinition& item) -> int {
                    return item.rpgTemplateId.empty()
                               ? effectiveShopCost(item.effect, item.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                                   shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                                   shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_)
                               : item.cost;
                };

                auto tryBuyOffer = [&](std::size_t i) {
                    if (i >= shopInventory_.size()) return;
                    const ItemDefinition& item = shopInventory_[i];
                    int dynCost = dynCostFor(item);
                    if (dynCost < 0) return;  // capped
                    if (gold_ < dynCost) {
                        shopNoFundsTimer_ = 0.6;
                        return;
                    }
                    if (!item.rpgTemplateId.empty()) {
                        if (!addItemToInventory(item)) {
                            shopNoFundsTimer_ = 0.6;
                            return;
                        }
                        gold_ -= dynCost;
                        refreshShopInventory();
                        return;
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
                };

                bool uiClickEdge = leftClick && !shopUIClickPrev_;
                if (uiClickEdge && shopSystem_) {
                    // Buying.
                    if (insideF(lay.buyX, lay.buyY, lay.buyW, lay.buyH)) {
                        tryBuyOffer(static_cast<std::size_t>(itemShopSelected_));
                    }

                    // Selling.
                    for (int r = 0; r < visibleRows; ++r) {
                        const int idx = scrollRow + r;
                        if (idx < 0 || idx >= static_cast<int>(inventory_.size())) break;
                        float ry = invListY + static_cast<float>(r) * lay.invRowH;
                        if (insideF(invListX, ry, invListW, lay.invRowH)) {
                            sellItemFromInventory(static_cast<std::size_t>(idx), gold_);
                            break;
                        }
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
                        refreshPauseState();
                    } else {
                        itemShopOpen_ = true;
                        itemShopSelected_ = 0;
                        shopSellScroll_ = 0.0f;
                        refreshPauseState();
                    }
                }
            }
        }

        if (!paused_ && !characterScreenOpen_ && heroAlive) {
            if (ability1Edge) executeAbility(1);
            if (ability2Edge) executeAbility(2);
            if (ability3Edge) executeAbility(3);
            if (abilityUltEdge) executeAbility(4);
            auto consumeAtIndex = [&](int targetIdx) -> bool {
                if (targetIdx < 0 || targetIdx >= static_cast<int>(inventory_.size())) return false;
                const auto& inst = inventory_[static_cast<std::size_t>(targetIdx)];
                const int consumedId = inst.def.id;
                bool consumed = false;
                if (!inst.def.rpgConsumableId.empty()) {
                    const Game::RPG::ConsumableDef* def = nullptr;
                    for (const auto& c : rpgConsumables_) {
                        if (c.id == inst.def.rpgConsumableId) { def = &c; break; }
                    }
                    if (def && !def->cooldownGroup.empty()) {
                        const double cd = rpgConsumableCooldowns_[def->cooldownGroup];
                        if (cd <= 0.0001) {
                            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                for (const auto& eff : def->effects) {
                                    const float maxPool =
                                        (eff.resource == Game::RPG::ConsumableResource::Shields) ? hp->maxShields : hp->maxHealth;
                                    if (maxPool <= 0.0f) continue;
                                    if (eff.category == Game::RPG::ConsumableCategory::Heal ||
                                        (eff.category == Game::RPG::ConsumableCategory::Food && eff.duration <= 0.0f)) {
                                        const float delta = std::max(0.0f, eff.magnitude) * maxPool;
                                        if (eff.resource == Game::RPG::ConsumableResource::Shields) {
                                            hp->currentShields = std::min(hp->maxShields, hp->currentShields + delta);
                                        } else {
                                            hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + delta);
                                        }
                                    } else if (eff.category == Game::RPG::ConsumableCategory::Food && eff.duration > 0.0f) {
                                        rpgConsumableOverTime_.push_back(
                                            ActiveConsumableOverTime{eff.resource, std::max(0.0f, eff.magnitude), eff.duration});
                                    } else if (eff.category == Game::RPG::ConsumableCategory::Buff && eff.duration > 0.0f) {
                                        Engine::Gameplay::RPG::StatContribution c{};
                                        c.mult.moveSpeed = eff.magnitude;
                                        rpgActiveBuffs_.push_back(ActiveRpgBuff{c, eff.duration});
                                    }
                                }
                            }
                            rpgConsumableCooldowns_[def->cooldownGroup] = std::max(0.0, static_cast<double>(def->cooldown));
                            consumed = true;
                        }
                    }
                } else {
                    switch (inst.def.effect) {
                        case ItemEffect::Heal: {
                            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                                hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + inst.def.value * hp->maxHealth);
                            }
                            consumed = true;
                            break;
                        }
                        case ItemEffect::FreezeTime:
                            freezeTimer_ = std::max(freezeTimer_, static_cast<double>(inst.def.value));
                            consumed = true;
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
                            consumed = true;
                            break;
                        }
                        case ItemEffect::Lifesteal: {
                            lifestealBuff_ = inst.def.value;
                            lifestealPercent_ = lifestealBuff_;
                            lifestealTimer_ = std::max(lifestealTimer_, 60.0);
                            consumed = true;
                            break;
                        }
                        case ItemEffect::AttackSpeed: {
                            attackSpeedBuffMul_ = std::max(attackSpeedBuffMul_, 1.0f + inst.def.value);
                            chronoTimer_ = std::max(chronoTimer_, 60.0);
                            fireInterval_ = fireIntervalBase_ / std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);
                            consumed = true;
                            break;
                        }
                        case ItemEffect::Chain: {
                            chainBonusTemp_ = std::max(chainBonusTemp_, static_cast<int>(std::round(inst.def.value)));
                            stormTimer_ = std::max(stormTimer_, 60.0);
                            chainBounces_ = chainBase_ + chainBonusTemp_;
                            consumed = true;
                            break;
                        }
                        default:
                            break;
                    }
                }
                if (consumed) {
                    inventory_.erase(inventory_.begin() + targetIdx);
                    // Clear hotbar references if they pointed at the consumed item id.
                    for (auto& slotId : hotbarItemIds_) {
                        if (slotId.has_value() && slotId.value() == consumedId) slotId.reset();
                    }
                    if (inventory_.empty()) inventorySelected_ = -1;
                    else clampInventorySelection();
                }
                return consumed;
            };

            auto tryConsumeHotbarSlot = [&](int slotIdx) {
                if (slotIdx < 0 || slotIdx >= 2) return;
                if (!hotbarItemIds_[slotIdx].has_value()) return;
                const int id = *hotbarItemIds_[slotIdx];
                int invIdx = -1;
                for (std::size_t i = 0; i < inventory_.size(); ++i) {
                    if (inventory_[i].def.id == id) { invIdx = static_cast<int>(i); break; }
                }
                if (invIdx == -1) {
                    hotbarItemIds_[slotIdx].reset();
                    return;
                }
                (void)consumeAtIndex(invIdx);
            };

            if (hotbar1Edge) tryConsumeHotbarSlot(0);
            if (hotbar2Edge) tryConsumeHotbarSlot(1);
            if (useItemEdge) {
                clampInventorySelection();
                auto isConsumable = [](const ItemDefinition& d) {
                    return !d.rpgConsumableId.empty() ||
                           d.effect == ItemEffect::Heal || d.effect == ItemEffect::FreezeTime ||
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
                    (void)consumeAtIndex(targetIdx);
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
        // Apply temporary rage multipliers to the mini-unit system.
        miniUnitSystem_->setGlobalDamageMul(miniRageTimer_ > 0.0f ? miniRageDamageMul_ : 1.0f);
        miniUnitSystem_->setGlobalAttackRateMul(miniRageTimer_ > 0.0f ? miniRageAttackRateMul_ : 1.0f);
        miniUnitSystem_->setGlobalHealMul(miniRageTimer_ > 0.0f ? miniRageHealMul_ : 1.0f);
        miniUnitSystem_->update(registry_, step);
    }
    // Decay and revert mini-unit rage buffs.
    updateMiniUnitRage(static_cast<float>(step.deltaSeconds));

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
                            if (spawnMiniUnit(miniIt->second, tf.position) != Engine::ECS::kInvalidEntity) {
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

    // Lightning dome tick damage.
    if (lightningDomeTimer_ > 0.0f) {
        lightningDomeTimer_ -= static_cast<float>(step.deltaSeconds);
        int stage = std::clamp((!abilityStates_.empty() ? (abilityStates_[0].level + 1) / 2 : 1), 1, 3);
        float radius = 140.0f;
        float dmg = projectileDamage_ * (0.35f + 0.1f * stage) * (hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f);
        Engine::Gameplay::DamageEvent dome{};
        dome.type = Engine::Gameplay::DamageType::Spell;
        dome.baseDamage = dmg * static_cast<float>(step.deltaSeconds);
        Engine::Vec2 heroPos{0.0f, 0.0f};
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = tf->position;
        float r2 = radius * radius;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) return;
                float dx = tf.position.x - heroPos.x;
                float dy = tf.position.y - heroPos.y;
                float d2 = dx * dx + dy * dy;
                if (d2 <= r2) {
                    Engine::Gameplay::BuffState domeBuff{};
                    if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                        if (st->container.isStasis()) return;
                        float armorDelta = st->container.armorDeltaTotal();
                        domeBuff.healthArmorBonus += armorDelta;
                        domeBuff.shieldArmorBonus += armorDelta;
                        domeBuff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                    }
                    (void)Game::RpgDamage::apply(registry_, hero_, e, hp, dome, domeBuff, useRpgCombat_, rpgResolverConfig_, rng_,
                                                 "aura_dome", [this](const std::string& line) { pushCombatDebugLine(line); });
                    if (auto* se = registry_.get<Game::StatusEffects>(e)) {
                        se->stunTimer = std::max(se->stunTimer, 0.2f);
                    } else {
                        registry_.emplace<Game::StatusEffects>(e, Game::StatusEffects{});
                        if (auto* se2 = registry_.get<Game::StatusEffects>(e)) se2->stunTimer = 0.2f;
                    }
                }
            });
        if (lightningDomeTimer_ < 0.0f) lightningDomeTimer_ = 0.0f;
    }
    // Flame walls tick and cleanup.
    if (!flameWalls_.empty()) {
        for (auto it = flameWalls_.begin(); it != flameWalls_.end();) {
            it->timer -= static_cast<float>(step.deltaSeconds);
            float radius = 20.0f;
            float r2 = radius * radius;
            Engine::Gameplay::DamageEvent dmg{};
            dmg.type = Engine::Gameplay::DamageType::Spell;
            float zoneDmgMul = hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f;
            dmg.baseDamage = projectileDamage_ * 0.25f * zoneDmgMul * static_cast<float>(step.deltaSeconds);
            registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity e, Engine::ECS::Transform& tf, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                    if (!hp.alive()) return;
                    float dx = tf.position.x - it->pos.x;
                    float dy = tf.position.y - it->pos.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 <= r2) {
                        Engine::Gameplay::BuffState flameBuff{};
                        if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                            if (st->container.isStasis()) return;
                            float armorDelta = st->container.armorDeltaTotal();
                            flameBuff.healthArmorBonus += armorDelta;
                            flameBuff.shieldArmorBonus += armorDelta;
                            flameBuff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                        }
                        (void)Game::RpgDamage::apply(registry_, hero_, e, hp, dmg, flameBuff, useRpgCombat_, rpgResolverConfig_, rng_,
                                                     "aura_flame", [this](const std::string& line) { pushCombatDebugLine(line); });
                        // apply brief burn
                        if (auto* se = registry_.get<Game::StatusEffects>(e)) {
                            se->burnTimer = std::max(se->burnTimer, 1.5f);
                            se->burnDps = std::max(se->burnDps, projectileDamage_ * zoneDmgMul * 0.15f);
                        } else {
                            registry_.emplace<Game::StatusEffects>(e, Game::StatusEffects{});
                            if (auto* se2 = registry_.get<Game::StatusEffects>(e)) {
                                se2->burnTimer = 1.5f;
                                se2->burnDps = projectileDamage_ * zoneDmgMul * 0.15f;
                            }
                        }
                    }
                });
            bool expired = it->timer <= 0.0f;
            if (expired) {
                if (it->visEntity != Engine::ECS::kInvalidEntity &&
                    registry_.has<Engine::ECS::Transform>(it->visEntity)) {
                    registry_.destroy(it->visEntity);
                }
                it = flameWalls_.erase(it);
            } else {
                ++it;
            }
        }
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
    // Tick generic invulnerability timers (used for freshly spawned remote players on host).
    std::vector<Engine::ECS::Entity> invToClear;
    registry_.view<Game::Invulnerable>([&](Engine::ECS::Entity e, Game::Invulnerable& inv) {
        // hero_ timer is maintained above from dash/immortal logic.
        if (e == hero_) return;
        inv.timer -= static_cast<float>(step.deltaSeconds);
        if (inv.timer <= 0.0f) {
            invToClear.push_back(e);
        }
    });
    for (auto e : invToClear) {
        if (registry_.has<Game::Invulnerable>(e)) {
            registry_.remove<Game::Invulnerable>(e);
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
                registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{dir.x * projectileSpeed_, dir.y * projectileSpeed_});
                const float halfSize = projectileHitboxSize_ * 0.5f;
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
                applyProjectileVisual(proj, 1.0f, Engine::Color{120, 220, 255, 230}, true);
                Engine::Gameplay::DamageEvent dmgEvent{};
                dmgEvent.baseDamage = projectileDamage_ * 0.8f;
                dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Light] = 1.0f;
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                   dmgEvent, projectileLifetime_, lifestealPercent_, chainBounces_, Engine::ECS::kInvalidEntity});
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
                (void)be;
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
                registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{dir.x * projectileSpeed_, dir.y * projectileSpeed_});
                const float halfSize = projectileHitboxSize_ * 0.5f;
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
                applyProjectileVisual(proj, 1.0f, Engine::Color{120, 220, 255, 230}, true);
                Engine::Gameplay::DamageEvent dmgEvent{};
                dmgEvent.baseDamage = def.damage;
                dmgEvent.type = Engine::Gameplay::DamageType::Normal;
                dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Light] = 1.0f;
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                   dmgEvent, projectileLifetime_, 0.0f, 0, be});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
                b.spawnTimer = def.attackRate;
            });
    }

    // Enemy AI (host authoritative).
    if (enemyAISystem_ && !defeated_ && !paused_ && (!multiplayerEnabled_ || (netSession_ && netSession_->isHost()))) {
        if (freezeTimer_ <= 0.0) {
            enemyAISystem_->update(registry_, hero_, step);
        }
    }

    // Hero animation row selection (idle/walk/pickup/attack/knockdown).
    if (heroSpriteStateSystem_ && (!paused_ || defeated_)) {
        heroSpriteStateSystem_->update(registry_, step, hero_);
        // Update remote player visuals the same way as the local hero.
        for (const auto& kv : remoteEntities_) {
            heroSpriteStateSystem_->update(registry_, step, kv.second);
        }
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
                auto pickItemByKind = [&](ItemKind kind) -> std::optional<ItemDefinition> {
                    // Legacy (non-RPG) drops.
                    std::vector<ItemDefinition> pool;
                    for (const auto& def : itemCatalog_) {
                        if (def.kind == kind) pool.push_back(def);
                    }
                    if (pool.empty()) return std::nullopt;
                    std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
                    return pool[pick(rng_)];
                };
                std::uniform_real_distribution<float> roll01(0.0f, 1.0f);
                auto colorForRarity = [&](ItemRarity r) {
                    Engine::Color c{235, 240, 245, 235};
                    if (r == ItemRarity::Uncommon) c = Engine::Color{135, 235, 155, 235};
                    if (r == ItemRarity::Rare) c = Engine::Color{135, 190, 255, 235};
                    if (r == ItemRarity::Epic) c = Engine::Color{185, 135, 255, 235};
                    if (r == ItemRarity::Legendary) c = Engine::Color{255, 210, 120, 240};
                    return c;
                };
                auto spawnItemDrop = [&](const ItemDefinition& def, float size, float amp, float speed) {
                    Pickup item{};
                    item.kind = Pickup::Kind::Item;
                    item.item = def;
                    spawnPickupEntity(item, scatterPos(death.pos), colorForRarity(def.rarity), size, amp, speed);
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

                    if (useRpgLoot_) {
                        for (int i = 0; i < std::max(1, rpgEquipBossCount_); ++i) {
                            if (auto loot = rollRpgEquipment(RpgLootSource::Boss, false)) {
                                spawnItemDrop(*loot, 13.0f, 4.2f, 3.0f);
                            }
                        }
                        for (int i = 0; i < std::max(0, rpgConsumableBossCount_); ++i) {
                            if (auto c = rollRpgConsumable(RpgLootSource::Boss, false)) {
                                spawnItemDrop(*c, 12.0f, 4.0f, 3.0f);
                            }
                        }
                        if (roll01(rng_) < std::clamp(rpgBagChanceBoss_, 0.0f, 1.0f)) {
                            if (auto bag = rollRpgBag(RpgLootSource::Boss, false)) {
                                spawnItemDrop(*bag, 12.0f, 4.2f, 3.0f);
                            }
                        }
                    } else {
                        if (auto unique = pickItemByKind(ItemKind::Unique)) {
                            spawnItemDrop(*unique, 14.0f, 4.5f, 3.2f);
                        }
                        if (auto support = pickItemByKind(ItemKind::Support)) {
                            spawnItemDrop(*support, 13.0f, 4.0f, 3.0f);
                        }
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

                    if (useRpgLoot_) {
                        if (roll01(rng_) < std::clamp(rpgMiniBossGemChance_, 0.0f, 1.0f)) {
                            if (auto gem = rollRpgEquipment(RpgLootSource::MiniBoss, false)) {
                                spawnItemDrop(*gem, 12.0f, 4.0f, 3.0f);
                            }
                        }
                        for (int i = 0; i < std::max(1, rpgEquipMiniBossCount_); ++i) {
                            if (auto loot = rollRpgEquipment(RpgLootSource::MiniBoss, false)) {
                                spawnItemDrop(*loot, 12.0f, 4.0f, 3.0f);
                            }
                        }
                        for (int i = 0; i < std::max(0, rpgConsumableMiniBossCount_); ++i) {
                            if (auto c = rollRpgConsumable(RpgLootSource::MiniBoss, false)) {
                                spawnItemDrop(*c, 11.0f, 3.8f, 3.0f);
                            }
                        }
                        if (roll01(rng_) < std::clamp(rpgBagChanceMiniBoss_, 0.0f, 1.0f)) {
                            if (auto bag = rollRpgBag(RpgLootSource::MiniBoss, false)) {
                                spawnItemDrop(*bag, 11.0f, 4.0f, 3.0f);
                            }
                        }
                    } else {
                        if (auto support = pickItemByKind(ItemKind::Support)) {
                            spawnItemDrop(*support, 11.0f, 3.8f, 3.0f);
                        }
                    }
                } else {
                    if (useRpgLoot_ && roll01(rng_) < std::clamp(rpgEquipChanceNormal_, 0.0f, 1.0f)) {
                        if (auto loot = rollRpgEquipment(RpgLootSource::Normal, false)) {
                            spawnItemDrop(*loot, 11.0f, 3.8f, 3.0f);
                        }
                    }
                    if (useRpgLoot_ && roll01(rng_) < std::clamp(rpgConsumableChanceNormal_, 0.0f, 1.0f)) {
                        if (auto c = rollRpgConsumable(RpgLootSource::Normal, false)) {
                            spawnItemDrop(*c, 10.5f, 3.6f, 3.0f);
                        }
                    }
                    float drop = roll01(rng_);
                    if (drop < pickupDropChance_) {
                        float powerRoll = roll01(rng_);
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
            refreshPauseState();
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
    if (!defeated_ && !userPaused_ && (!multiplayerEnabled_ || !levelChoiceOpen_)) {
    if (inCombat_) {
        double stepScale = freezeTimer_ > 0.0 ? 0.0 : 1.0;
        combatTimer_ -= step.deltaSeconds * stepScale;
        if (waveSystem_ && (!multiplayerEnabled_ || (netSession_ && netSession_->isHost()))) {
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
                        applyWaveScaling(wave_);
                        const bool isBossWave = (wave_ >= bossWave_) && (bossInterval_ > 0) &&
                                               ((wave_ - bossWave_) % bossInterval_ == 0);
                        if (isBossWave) {
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
                refreshPauseState();
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
            if (multiplayerEnabled_ && reviveNextRound_) {
                reviveLocalPlayer();
            }
            inCombat_ = true;
            combatTimer_ = combatDuration_;
            waveSpawned_ = false;
            itemShopOpen_ = false;
            abilityShopOpen_ = false;
                refreshPauseState();
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

    updateCombatDebugLines(step.deltaSeconds);
    if (hero_ != Engine::ECS::kInvalidEntity) {
        updateRpgConsumables(step.deltaSeconds);
        // Keep RPG derived stats fresh for UI/preview even if the resolver is disabled.
        updateHeroRpgStats();
    }

    // Projectiles (host authoritative to avoid desync).
    if (projectileSystem_ && !defeated_ && !paused_ && (!multiplayerEnabled_ || (netSession_ && netSession_->isHost()))) {
        projectileSystem_->update(registry_, step);
    }

    // Collision / damage (host authoritative).
    if (collisionSystem_ && !paused_ && (!multiplayerEnabled_ || (netSession_ && netSession_->isHost()))) {
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
                        int frames = std::max(1, sheets->movement->width() / std::max(1, sheets->movementFrameWidth));
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

        // Status tick + shields/health regeneration.
        if (!paused_) {
            const float dt = static_cast<float>(step.deltaSeconds);
            registry_.view<Engine::ECS::Status>(
                [dt](Engine::ECS::Entity, Engine::ECS::Status& st) { st.container.update(dt); });
            registry_.view<Engine::ECS::Health>([&](Engine::ECS::Entity e, Engine::ECS::Health& hp) {
                const auto* st = registry_.get<Engine::ECS::Status>(e);
                if (st && st->container.blocksRegen()) return;
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
            bool shouldShake = false;
            if (multiplayerEnabled_ && netSession_ && !netSession_->isHost()) {
                // Client: shake only when host-reported damage arrived this frame.
                shouldShake = pendingNetShake_;
                pendingNetShake_ = false;
                lastHeroHp_ = total;  // keep baseline aligned with latest host value.
            } else {
                // Solo or host: shake when local HP actually drops.
                shouldShake = (total < lastHeroHp_);
                lastHeroHp_ = total;
            }
            if (shouldShake && screenShake_) {
                shakeTimer_ = 0.25f;
                shakeMagnitude_ = 6.0f;
            }
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
    updateGhostState(step);
    updateRemoteCombat(step);
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
                                fogLayer_.get(), fogTileSize_, fogOriginOffsetX_, fogOriginOffsetY_,
                                /*disableEnemyFogCulling=*/multiplayerEnabled_);
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

        // Mini-map HUD overlay (top-right).
        if (miniMapEnabled_) {
            miniMapEnemyCache_.clear();
            miniMapPickupCache_.clear();
            miniMapGoldCache_.clear();
            Engine::Vec2 heroPos{0.0f, 0.0f};
            if (const auto* htf = registry_.get<Engine::ECS::Transform>(hero_)) heroPos = htf->position;
            registry_.view<Engine::ECS::Transform, Engine::ECS::EnemyTag>(
                [&](Engine::ECS::Entity ent, const Engine::ECS::Transform& tf, const Engine::ECS::EnemyTag&) {
                    if (const auto* st = registry_.get<Engine::ECS::Status>(ent)) {
                        if (st->container.isStasis() || st->container.isStealthed()) return;
                    }
                    miniMapEnemyCache_.push_back(tf.position);
                });
            registry_.view<Engine::ECS::Transform, Game::Pickup>(
                [&](Engine::ECS::Entity, const Engine::ECS::Transform& tf, const Game::Pickup& p) {
                    switch (p.kind) {
                        case Game::Pickup::Kind::Gold:
                            miniMapGoldCache_.push_back(tf.position);
                            break;
                        case Game::Pickup::Kind::Copper:
                        case Game::Pickup::Kind::Powerup:
                        case Game::Pickup::Kind::Item:
                        case Game::Pickup::Kind::Revive:
                            miniMapPickupCache_.push_back(tf.position);
                            break;
                    }
                });
            Engine::UI::MiniMapConfig cfg{};
            cfg.sizePx = miniMapSize_;
            cfg.paddingPx = miniMapPadding_;
            cfg.worldRadius = miniMapWorldRadius_;
            miniMapHud_.setConfig(cfg);
            miniMapHud_.draw(*render_, heroPos, miniMapEnemyCache_, miniMapPickupCache_, miniMapGoldCache_, viewportWidth_, viewportHeight_);
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
        const auto hud = ingameHudLayout(viewportWidth_, viewportHeight_);
        std::vector<std::pair<std::string, Engine::Color>> hudLines;
        auto pushHud = [&](const std::string& txt, Engine::Color col) {
            if (!txt.empty()) hudLines.emplace_back(txt, col);
        };

        // Debug HUD (only when combat debug overlay is enabled; keep gameplay unobscured by default).
        if (combatDebugOverlay_) {
            std::ostringstream dbg;
            dbg << "FPS ~" << std::fixed << std::setprecision(1) << fpsSmooth_;
            dbg << " | Cam " << (cameraSystem_ && cameraSystem_->followEnabled() ? "Follow" : "Free");
            dbg << " | Zoom " << std::setprecision(2) << camera_.zoom;
            dbg << " | Round " << round_ << " | Wave " << wave_ << " | Kills " << kills_;
            dbg << " | Enemies " << enemiesAlive_;
            dbg << " | Lv " << level_ << " (" << xp_ << "/" << xpToNext_ << ")";
            const float ds = std::clamp(0.86f * hud.s, 0.78f, 0.98f);
            const Engine::Vec2 dsz = measureTextUnified(dbg.str(), ds);
            drawTextTTF(dbg.str(),
                        Engine::Vec2{static_cast<float>(viewportWidth_) - hud.margin - dsz.x, hud.margin},
                        ds, Engine::Color{220, 235, 250, 180});
        }

        if (!inCombat_ && waveWarmup_ > 0.0) {
            std::ostringstream warm;
            warm << "Wave " << (wave_ + 1) << " in " << std::fixed << std::setprecision(1)
                 << std::max(0.0, waveWarmup_) << "s";
            pushHud(warm.str(), Engine::Color{255, 220, 120, 220});
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
            pushHud(eventBannerText_, Engine::Color{200, 240, 255, 230});
        }
        // Intermission overlay when we are in grace time.
        if (!inCombat_) {
            double intermissionLeft = waveWarmup_;
            std::ostringstream inter;
            inter << "Intermission: next round in " << std::fixed << std::setprecision(1) << intermissionLeft
                  << "s";
            pushHud(inter.str(), Engine::Color{140, 220, 255, 220});
            std::ostringstream wallet;
            wallet << "Copper " << copper_ << " • Gold " << gold_;
            pushHud(wallet.str(), Engine::Color{200, 230, 255, 210});
            if (abilityShopOpen_) {
                pushHud("Ability Shop OPEN (B closes)", Engine::Color{180, 255, 180, 220});
            } else {
                pushHud("Press B to open Ability Shop", Engine::Color{150, 220, 255, 200});
            }
        }
        // Wave clear prompt when banner active (even if not intermission).
        if (clearBannerTimer_ > 0.0 && waveWarmup_ <= waveInterval_) {
            std::ostringstream note;
            note << "Wave clear bonus +" << waveClearBonus_;
            pushHud(note.str(), Engine::Color{180, 240, 180, 200});
        }
            drawResourceCluster();
            // Mini-unit selection HUD (drawn after resource bars so it sits above them).
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
                const float hudY = static_cast<float>(viewportHeight_) - hudH - kHudBottomSafeMargin;
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
            // Inventory badge anchored above ability bar.
            {
                const float s = ingameHudLayout(viewportWidth_, viewportHeight_).s;
                const float badgeW = 320.0f * s;
                const float badgeH = 62.0f * s;
                const float margin = 16.0f * s;
                HUDRect abilityRect = abilityHudRect(static_cast<int>(abilities_.size()), viewportWidth_, viewportHeight_);
                // Right-align with the ability bar and place just above it.
                float x = abilityRect.x + abilityRect.w - badgeW;
                if (x < margin) x = margin;  // clamp to screen
                float y = abilityRect.y - badgeH - margin;
                render_->drawFilledRect(Engine::Vec2{x, y + 3.0f * s}, Engine::Vec2{badgeW, badgeH}, Engine::Color{0, 0, 0, 80});
                render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{badgeW, badgeH}, Engine::Color{12, 16, 24, 220});
                render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{badgeW, 2.0f}, Engine::Color{45, 70, 95, 190});
                render_->drawFilledRect(Engine::Vec2{x, y + badgeH - 2.0f}, Engine::Vec2{badgeW, 2.0f}, Engine::Color{45, 70, 95, 190});
                auto rarityCol = [](ItemRarity r) {
                    switch (r) {
                        case ItemRarity::Common: return Engine::Color{235, 240, 245, 235};
                        case ItemRarity::Uncommon: return Engine::Color{135, 235, 155, 235};
                        case ItemRarity::Rare: return Engine::Color{135, 190, 255, 235};
                        case ItemRarity::Epic: return Engine::Color{185, 135, 255, 235};
                        case ItemRarity::Legendary: return Engine::Color{255, 210, 120, 240};
                    }
                    return Engine::Color{235, 240, 245, 235};
                };
                const float icon = 34.0f * s;
                render_->drawFilledRect(Engine::Vec2{x + 10.0f * s, y + 10.0f * s}, Engine::Vec2{icon, icon}, Engine::Color{18, 26, 40, 230});
                if (inventory_.empty()) {
                    inventorySelected_ = -1;
                    drawTextUnified("Inventory empty", Engine::Vec2{x + 10.0f * s + icon + 10.0f * s, y + 10.0f * s},
                                    0.98f * s, Engine::Color{200, 220, 240, 220});
                    drawTextUnified("Tab to cycle", Engine::Vec2{x + 10.0f * s + icon + 10.0f * s, y + 30.0f * s},
                                    0.86f * s, Engine::Color{170, 195, 220, 210});
                } else {
                    clampInventorySelection();
                    const auto& held = inventory_[inventorySelected_];
                    bool usable = held.def.kind == ItemKind::Support;
                    drawItemIcon(held.def, Engine::Vec2{x + 10.0f * s, y + 10.0f * s}, icon);
                    std::ostringstream heldText;
                    heldText << "Holding: " << held.def.name;
                    drawTextUnified(ellipsizeTextUnified(heldText.str(), badgeW - (icon + 36.0f * s), 0.96f * s),
                                    Engine::Vec2{x + 10.0f * s + icon + 10.0f * s, y + 8.0f * s},
                                    0.96f * s, rarityCol(held.def.rarity));
                    std::ostringstream hint;
                    hint << "Tab to cycle   Q to " << (usable ? "use" : "use support item");
                    drawTextUnified(hint.str(), Engine::Vec2{x + 10.0f * s + icon + 10.0f * s, y + 30.0f * s},
                                    0.84f * s, Engine::Color{180, 200, 220, 210});
                }
                // Wallet badge next to inventory.
                std::ostringstream wallet;
                wallet << "Copper " << copper_ << " | Gold " << gold_;
                drawTextUnified(wallet.str(), Engine::Vec2{x + badgeW - 170.0f * s, y + badgeH - 18.0f * s},
                                0.84f * s, Engine::Color{200, 230, 255, 210});
            }
        // Active event / status lines.
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
                evMsg << "Event: Escort • " << std::fixed << std::setprecision(0) << distLeft << "u • "
                      << std::setprecision(1) << t << "s";
            } else if (ev.type == Game::EventType::Bounty) {
                evMsg << "Event: Bounty • " << ev.requiredKills << " targets • "
                      << std::fixed << std::setprecision(1) << t << "s";
            } else {
                evMsg << "Event: Spire Hunt • " << ev.requiredKills << " spires • "
                      << std::fixed << std::setprecision(1) << t << "s";
            }
            pushHud(evMsg.str(), Engine::Color{200, 240, 255, 220});
        });
        if (hotzoneSystem_) {
            const auto* zone = hotzoneSystem_->activeZone();
            if (zone) {
                std::string label = "XP Surge";
                if (hotzoneSystem_->activeBonus() == HotzoneSystem::Bonus::DangerPay) label = "Danger-Pay";
                if (hotzoneSystem_->activeBonus() == HotzoneSystem::Bonus::WarpFlux) label = "Warp Flux";
                float t = hotzoneSystem_->timeRemaining();
                std::ostringstream hz;
                hz << "Hotzone: " << label << " • " << std::fixed << std::setprecision(0) << std::max(0.0f, t)
                   << "s • " << (hotzoneSystem_->heroInsideActive() ? "IN" : "OUT");
                pushHud(hz.str(), Engine::Color{200, 220, 255, 210});
                if (hotzoneSystem_->warningActive()) {
                    std::ostringstream hw;
                    hw << "Zone shift in " << std::fixed << std::setprecision(1) << std::max(0.0f, t) << "s";
                    pushHud(hw.str(), Engine::Color{255, 200, 160, 220});
                }
            }
        }
        // Enemies remaining warning.
        if (enemiesAlive_ > 0 && enemiesAlive_ <= enemyLowThreshold_) {
            std::ostringstream warn;
            warn << enemiesAlive_ << " enemies remain";
            pushHud(warn.str(), Engine::Color{255, 220, 140, 220});
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
        // Draw a compact status card under the resource panel.
        if (!hudLines.empty()) {
            const float s = hud.s;
            const float pad = 12.0f * s;
            const float lineH = 20.0f * s;
            const float x = hud.statusX;
            const float y = hud.statusY;
            const float w = hud.statusW;
            const float h = pad * 2.0f + static_cast<float>(hudLines.size()) * lineH;
            render_->drawFilledRect(Engine::Vec2{x, y + 4.0f * s}, Engine::Vec2{w, h}, Engine::Color{0, 0, 0, 85});
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, Engine::Color{12, 16, 24, 215});
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, 2.0f}, Engine::Color{45, 70, 95, 190});
            render_->drawFilledRect(Engine::Vec2{x, y + h - 2.0f}, Engine::Vec2{w, 2.0f}, Engine::Color{45, 70, 95, 190});

            const float ts = std::clamp(0.90f * s, 0.82f, 1.05f);
            float ly = y + pad;
            for (const auto& [txtRaw, col] : hudLines) {
                const std::string txt = ellipsizeTextUnified(txtRaw, w - pad * 2.0f, ts);
                drawTextUnified(txt, Engine::Vec2{x + pad, ly}, ts, col);
                ly += lineH;
            }
        }
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
            const float barW = 150.0f;
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
            float totalW = barW * 4.0f + 18.0f * 3.0f;
            float startX = static_cast<float>(viewportWidth_) * 0.5f - totalW * 0.5f;
            Engine::Vec2 pos{startX, static_cast<float>(viewportHeight_) - 24.0f};
            render_->drawFilledRect(pos, Engine::Vec2{barW, barH}, Engine::Color{50, 50, 60, 220});
            if (shieldRatio > 0.0f) {
                render_->drawFilledRect(pos, Engine::Vec2{barW * shieldRatio, barH}, Engine::Color{80, 140, 240, 220});
            }
            if (hpRatio > 0.0f) {
                render_->drawFilledRect(pos, Engine::Vec2{barW * hpRatio, barH}, Engine::Color{120, 220, 120, 240});
            }
            // Draw simple placeholders for other bars to maintain spacing.
            render_->drawFilledRect(Engine::Vec2{pos.x + barW + 18.0f, pos.y}, Engine::Vec2{barW, barH},
                                    Engine::Color{60, 70, 90, 180});
            render_->drawFilledRect(Engine::Vec2{pos.x + (barW + 18.0f) * 2.0f, pos.y}, Engine::Vec2{barW, barH},
                                    Engine::Color{60, 70, 90, 180});
            render_->drawFilledRect(Engine::Vec2{pos.x + (barW + 18.0f) * 3.0f, pos.y}, Engine::Vec2{barW, barH},
                                    Engine::Color{60, 90, 70, 180});
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
            drawCombatDebugOverlay();
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
            const float ry = static_cast<float>(viewportHeight_) - panelH - kHudBottomSafeMargin;
            mouseOverBottomLeftUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }
        if (selectedBuilding_ != Engine::ECS::kInvalidEntity &&
            registry_.has<Game::Building>(selectedBuilding_) &&
            registry_.has<Engine::ECS::Health>(selectedBuilding_)) {
            const auto* b = registry_.get<Game::Building>(selectedBuilding_);
            const float panelW = 270.0f;
            const float panelH = (b && b->type == Game::BuildingType::Barracks) ? 170.0f : 120.0f;
            const float rx = 20.0f;
            const float ry = static_cast<float>(viewportHeight_) - panelH - kHudBottomSafeMargin;
            mouseOverBottomLeftUI |= pointInRect(lastMouseX_, lastMouseY_, rx, ry, panelW, panelH);
        }

        bool worldLeftEdge = input.isMouseButtonDown(0) && !buildPreviewActive_ && !buildingSelectPrev_;
        if (worldLeftEdge && !mouseOverBottomLeftUI && !inMenu_ && !paused_ && !characterScreenOpen_) {
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
            drawAbilityHud(input);
        }
        if (levelChoiceOpen_) {
            drawLevelChoiceOverlay();
        }
        if (characterScreenOpen_) {
            drawCharacterScreen(input);
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
        if (!paused_ && !characterScreenOpen_ && previewLeftEdge) {
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
    // Fallback revive during lobby/match transitions (clients only; host revives on round advance).
    if (multiplayerEnabled_ && netSession_ && !netSession_->isHost() && reviveNextRound_ && !inCombat_) {
        reviveLocalPlayer();
    }
}

bool GameRoot::performRangedAutoFire(const Engine::TimeStep& step, const Engine::ActionState& actions,
                                     Game::OffensiveType offenseType) {
    (void)step;
    const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
    if (!heroTf) return false;
    const bool isWizard = activeArchetype_.id == "wizard";

    Engine::Vec2 dir{};
    bool haveTarget = false;
    float visionRange = heroVisionRadiusTiles_ * static_cast<float>(fogTileSize_ > 0 ? fogTileSize_ : 32);
    float allowedRange = std::min(visionRange, autoFireBaseRange_ + autoFireRangeBonus_);
    float allowedRange2 = allowedRange * allowedRange;
    const bool allowManualTargeting = movementMode_ != MovementMode::RTS;  // StarCraft-style: no left-click attack in RTS mode.
    if (allowManualTargeting && actions.primaryFire) {
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
            int frames = std::max(1, sheets->combat->width() / std::max(1, sheets->combatFrameWidth));
            dur = std::max(0.05f, static_cast<float>(frames) * sheets->combatFrameDuration);
        }
        if (auto* atk = registry_.get<Game::HeroAttackAnim>(hero_)) {
            *atk = Game::HeroAttackAnim{dur, dir, variant};
        } else {
            registry_.emplace<Game::HeroAttackAnim>(hero_, Game::HeroAttackAnim{dur, dir, variant});
        }
    }

    float speedLocal = projectileSpeed_;
    float sizeMul = 1.0f;
    if (isWizard) {
        speedLocal *= 0.35f;  // much slower travel
        sizeMul = 2.8f;       // larger visuals
    }

    auto proj = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(proj, heroTf->position);
    registry_.emplace<Engine::ECS::Velocity>(proj, Engine::Vec2{dir.x * speedLocal, dir.y * speedLocal});
    const float halfSize = projectileHitboxSize_ * 0.5f * sizeMul;
    registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{halfSize, halfSize}});
    if (archetypeSupportsSecondaryWeapon(activeArchetype_) && !usingSecondaryWeapon_) {
        // Melee mode: no visual projectile.
    } else {
        const SpriteSheetDesc* vis = (usingSecondaryWeapon_ && archetypeSupportsSecondaryWeapon(activeArchetype_))
                                         ? &projectileVisualArrow_
                                         : nullptr;
        applyProjectileVisual(proj, sizeMul, Engine::Color{255, 230, 90, 255}, false, vis);
    }
    float zoneDmgMul = hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f;
    float dmgMul = rageDamageBuff_ * zoneDmgMul;
    Engine::Gameplay::DamageEvent dmgEvent{};
    dmgEvent.type = Engine::Gameplay::DamageType::Normal;
    dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Biological] = 1.0f;
    float baseDamage = projectileDamage_ * dmgMul;
    if (activeArchetype_.id == "wizard") {
        switch (wizardElement_) {
            case WizardElement::Fire: baseDamage *= 1.1f; break;
            case WizardElement::Ice: baseDamage *= 0.95f; break;
            case WizardElement::Dark: baseDamage *= 1.0f; break;
            case WizardElement::Earth: baseDamage *= 1.05f; break;
            case WizardElement::Lightning: baseDamage *= 1.0f; break;
            case WizardElement::Wind: baseDamage *= 0.9f; break;
            default: break;
        }
    }
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
                                               Engine::ECS::Projectile{Engine::Vec2{dir.x * speedLocal,
                                                                                     dir.y * speedLocal},
                                                                       dmgEvent, projectileLifetime_, lifestealPercent_,
                                                                       chainBounces_, hero_});
    registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
    if (activeArchetype_.id == "wizard") {
        int stage = 1;
        if (!abilityStates_.empty()) {
            stage = std::clamp((abilityStates_[0].level + 1) / 2, 1, 3);
        }
        Game::ElementType el = Game::ElementType::Fire;
        switch (wizardElement_) {
            case WizardElement::Fire: el = Game::ElementType::Fire; break;
            case WizardElement::Ice: el = Game::ElementType::Ice; break;
            case WizardElement::Dark: el = Game::ElementType::Dark; break;
            case WizardElement::Earth: el = Game::ElementType::Earth; break;
            case WizardElement::Lightning: el = Game::ElementType::Lightning; break;
            case WizardElement::Wind: el = Game::ElementType::Wind; break;
            default: break;
        }
        if (registry_.has<Game::SpellEffect>(proj)) {
            registry_.remove<Game::SpellEffect>(proj);
        }
        registry_.emplace<Game::SpellEffect>(proj, Game::SpellEffect{el, stage});

        // Override visual to elemental sheet if available.
        if (wizardElementTex_) {
            int rowBase = 0;
            switch (wizardElement_) {
                case WizardElement::Lightning: rowBase = 0; break;          // rows 1-3
                case WizardElement::Dark: rowBase = 3; break;               // rows 4-6
                case WizardElement::Fire: rowBase = 6; break;               // rows 7-9
                case WizardElement::Ice: rowBase = 9; break;                // rows 10-12 (13 reserved)
                case WizardElement::Wind: rowBase = 13; break;              // rows 14-16
                case WizardElement::Earth: rowBase = 17; break;             // rows 18-20
                default: rowBase = 0; break;
            }
            int row = rowBase + (stage - 1);
            // Cap row to available rows.
            int maxRows = std::max(1, wizardElementTex_->height() / 8);
            row = std::clamp(row, 0, maxRows - 1);
            Engine::Color col{255, 255, 255, 255};
            float visSize = 8.0f * sizeMul;
            if (registry_.has<Engine::ECS::Renderable>(proj)) registry_.remove<Engine::ECS::Renderable>(proj);
            registry_.emplace<Engine::ECS::Renderable>(proj,
                Engine::ECS::Renderable{Engine::Vec2{visSize, visSize}, col, wizardElementTex_});
            auto anim = Engine::ECS::SpriteAnimation{8, 8, std::max(1, wizardElementColumns_), wizardElementFrameDuration_};
            anim.row = row;
            if (registry_.has<Engine::ECS::SpriteAnimation>(proj)) registry_.remove<Engine::ECS::SpriteAnimation>(proj);
            registry_.emplace<Engine::ECS::SpriteAnimation>(proj, anim);
        }
    }
    float rateMul = rageRateBuff_ * frenzyRateBuff_ * (hotzoneSystem_ ? hotzoneSystem_->rateMultiplier() : 1.0f);
    fireCooldown_ = fireInterval_ / std::max(0.1f, rateMul);
    if (isWizard) {
        fireCooldown_ *= 2.4f;  // even slower wizard firing cadence
    }
    return true;
}

bool GameRoot::archetypeSupportsSecondaryWeapon(const GameRoot::ArchetypeDef& def) const {
    return def.id == "damage" || def.name == "Damage Dealer";
}

void GameRoot::refreshHeroOffenseTag() {
    Game::OffensiveType type = heroBaseOffense_;
    if (usingSecondaryWeapon_ && archetypeSupportsSecondaryWeapon(activeArchetype_)) {
        type = Game::OffensiveType::Ranged;
    }
    if (registry_.has<Game::OffensiveTypeTag>(hero_)) {
        registry_.get<Game::OffensiveTypeTag>(hero_)->type = type;
    } else if (hero_ != Engine::ECS::kInvalidEntity) {
        registry_.emplace<Game::OffensiveTypeTag>(hero_, Game::OffensiveTypeTag{type});
    }
    activeArchetype_.offensiveType = type;
    if (registry_.has<Game::SecondaryWeapon>(hero_)) {
        registry_.get<Game::SecondaryWeapon>(hero_)->active = usingSecondaryWeapon_;
    } else if (hero_ != Engine::ECS::kInvalidEntity) {
        registry_.emplace<Game::SecondaryWeapon>(hero_, Game::SecondaryWeapon{usingSecondaryWeapon_});
    }
}

void GameRoot::toggleHeroWeaponMode() {
    if (!archetypeSupportsSecondaryWeapon(activeArchetype_)) return;
    usingSecondaryWeapon_ = !usingSecondaryWeapon_;
    refreshHeroOffenseTag();
}

bool GameRoot::performMeleeAttack(const Engine::TimeStep& step, const Engine::ActionState& actions) {
    (void)step;
    const auto* heroTf = registry_.get<Engine::ECS::Transform>(hero_);
    if (!heroTf) return false;

    Engine::Vec2 dir{};
    bool haveTarget = false;
    float visionRange = heroVisionRadiusTiles_ * static_cast<float>(fogTileSize_ > 0 ? fogTileSize_ : 32);
    float meleeRange = std::min(visionRange, meleeConfig_.range);
    auto addReach = [&](float extra) { meleeRange += extra; };
    // Dragoon spear: extend melee reach by ~1.5 tiles (48 units).
    if (activeArchetype_.id == "support") {
        addReach(48.0f);
    }
    // Core melee roster extra reach (+12 units): Knight, Militia (when in melee), Assassin, Crusader, Druid beast forms.
    const std::string id = activeArchetype_.id;
    const bool isKnight = (id == "tank");
    const bool isMilitia = (id == "damage");
    const bool isAssassin = (id == "assassin");
    const bool isCrusader = (id == "special");
    const bool isDruidForm = (id == "druid" && druidForm_ != DruidForm::Human);
    const bool militiaInMelee = isMilitia && heroBaseOffense_ == Game::OffensiveType::Melee;
    if (isKnight || isAssassin || isCrusader || isDruidForm || militiaInMelee) {
        addReach(12.0f);
    }
    float meleeRange2 = meleeRange * meleeRange;
    const bool allowManualTargeting = movementMode_ != MovementMode::RTS;  // StarCraft-style: no left-click attack in RTS mode.
    if (allowManualTargeting && actions.primaryFire) {
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
            int frames = std::max(1, sheets->combat->width() / std::max(1, sheets->combatFrameWidth));
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

            Engine::Gameplay::BuffState buff{};
            if (auto* armorBuff = registry_.get<Game::ArmorBuff>(e)) {
                buff = armorBuff->state;
            }
            if (auto* st = registry_.get<Engine::ECS::Status>(e)) {
                if (st->container.isStasis()) return;
                float armorDelta = st->container.armorDeltaTotal();
                buff.healthArmorBonus += armorDelta;
                buff.shieldArmorBonus += armorDelta;
                buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
            }
            float dealt = Game::RpgDamage::apply(registry_, hero_, e, health, dmgEvent, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                                 "melee", [this](const std::string& line) { pushCombatDebugLine(line); });
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
    if (auto* ghost = registry_.get<Game::Ghost>(hero_)) {
        ghost->active = false;
    }
    heroGhostActive_ = false;
    heroGhostPendingRise_ = false;
    heroGhostRiseTimer_ = 0.0f;
    lastSnapshotHeroHp_ = -1.0f;
    pendingNetShake_ = false;
    forceHealAfterReset_ = false;
    lastSnapshotTick_ = 0;
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
    if (auto* ghost = registry_.get<Game::Ghost>(hero_)) {
        ghost->active = false;
    }
    heroGhostActive_ = false;
    heroGhostPendingRise_ = false;
    heroGhostRiseTimer_ = 0.0f;
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
        // Prefer authoritative host-side health of remote avatars.
        registry_.view<Engine::ECS::Health, Engine::ECS::HeroTag>(
            [&](Engine::ECS::Entity e, const Engine::ECS::Health& hpRemote, const Engine::ECS::HeroTag&) {
                if (e == hero_) return;
                if (hpRemote.alive()) remoteAlive = true;
            });
    }
    if (multiplayerEnabled_ && remoteAlive) {
        reviveNextRound_ = true;
        if (!heroGhostPendingRise_ && !heroGhostActive_) {
            heroGhostPendingRise_ = true;
            // Duration of knockdown animation before ghost stands back up.
            heroGhostRiseTimer_ = 0.6f;
            if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_)) {
                if (sheets->movement) {
                    int frames = std::max(1, sheets->movement->width() / std::max(1, sheets->movementFrameWidth));
                    heroGhostRiseTimer_ = std::max(0.1f, static_cast<float>(frames) * sheets->movementFrameDuration);
                }
            }
            if (auto* ghost = registry_.get<Game::Ghost>(hero_)) {
                ghost->active = false;
            } else {
                registry_.emplace<Game::Ghost>(hero_, Game::Ghost{false});
            }
            Engine::logInfo("You are down. Awaiting next round revive (teammates still alive).");
        }
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {0.0f, 0.0f};
        }
        dashTimer_ = 0.0f;
        moveTargetActive_ = false;
        return;
    }

    // Delay the defeat overlay so the knockdown animation can play.
    float delay = 0.6f;
    if (const auto* sheets = registry_.get<Game::HeroSpriteSheets>(hero_)) {
        if (sheets->movement) {
            int frames = std::max(1, sheets->movement->width() / std::max(1, sheets->movementFrameWidth));
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

void GameRoot::updateGhostState(const Engine::TimeStep& step) {
    if (!multiplayerEnabled_) return;
    auto* hp = registry_.get<Engine::ECS::Health>(hero_);
    auto* ghost = registry_.get<Game::Ghost>(hero_);
    if (!hp) return;
    if (!ghost) {
        registry_.emplace<Game::Ghost>(hero_, Game::Ghost{false});
        ghost = registry_.get<Game::Ghost>(hero_);
    }

    if (heroGhostPendingRise_) {
        heroGhostRiseTimer_ -= static_cast<float>(step.deltaSeconds);
        if (heroGhostRiseTimer_ <= 0.0f) {
            heroGhostPendingRise_ = false;
            heroGhostActive_ = true;
            ghost->active = true;
            // Clear finished flag so animation can resume idle/walk while ghost.
            if (auto* anim = registry_.get<Engine::ECS::SpriteAnimation>(hero_)) {
                anim->finished = false;
            }
        }
    }

    // If revived (hp restored elsewhere), disable ghost visuals.
    if (hp->alive() && heroGhostActive_) {
        heroGhostActive_ = false;
        heroGhostPendingRise_ = false;
        ghost->active = false;
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
    heroShieldRegenBase_ *= globalModifiers_.playerShieldRegenMult;
    heroShieldRegen_ = heroShieldRegenBase_;
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

    auto pushVision = [&](Engine::ECS::Entity ent) {
        const auto* tf = registry_.get<Engine::ECS::Transform>(ent);
        const auto* hp = registry_.get<Engine::ECS::Health>(ent);
        const bool alive = !hp || hp->currentHealth > 0.0f;
        if (tf) {
            fogUnits_.push_back(Engine::Gameplay::Unit{tf->position.x + fogOriginOffsetX_,
                                                       tf->position.y + fogOriginOffsetY_,
                                                       heroVisionRadiusTiles_, alive, 0});
        }
    };

    if (hero_ != Engine::ECS::kInvalidEntity) {
        pushVision(hero_);
    }
    // Include all remote heroes so fog is shared across the squad.
    for (const auto& kv : remoteEntities_) {
        pushVision(kv.second);
    }

    Engine::Gameplay::updateFogOfWar(*fogLayer_, fogUnits_, 0, fogTileSize_);
}

void GameRoot::spawnShopkeeper(const Engine::Vec2& aroundPos) {
    despawnShopkeeper();
    float r = std::uniform_real_distribution<float>(260.0f, 420.0f)(rng_);
    float a = std::uniform_real_distribution<float>(0.0f, 6.28318f)(rng_);
    Engine::Vec2 pos{aroundPos.x + std::cos(a) * r, aroundPos.y + std::sin(a) * r};
    // Lazy-load shop building texture once (force the dedicated trader sprite).
    if (!shopTexture_) {
        shopTexture_ = loadTextureOptional("assets/Sprites/Buildings/tradershop.png");
        if (!shopTexture_) {
            Engine::logWarn("Shop texture missing (assets/Sprites/Buildings/tradershop.png); falling back to placeholder box.");
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
            def.frameWidth = v.value("frameWidth", 16);
            def.frameHeight = v.value("frameHeight", 16);
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
    projectileVisualPlayer_ = {};
    projectileVisualTurret_ = {};
    projectileVisualArrow_ = {};
    projectileTexRed_.reset();
    projectileTexTurret_.reset();

    auto loadSheet = [&](const std::string& rel, int columns, float frameDuration) -> SpriteSheetDesc {
        SpriteSheetDesc desc{};
        std::filesystem::path path = rel;
        if (const char* home = std::getenv("HOME")) {
            std::filesystem::path homePath = std::filesystem::path(home) / rel;
            if (std::filesystem::exists(homePath)) path = homePath;
        }
        auto tex = loadTextureOptional(path.string());
        if (!tex) return desc;
        desc.texture = tex;
        desc.frameWidth = std::max(1, tex->width() / std::max(1, columns));
        desc.frameHeight = tex->height();  // single-row sheets
        desc.frameCount = std::max(1, columns);
        desc.frameDuration = frameDuration;
        return desc;
    };

    projectileVisualPlayer_ = loadSheet("assets/Sprites/Misc/Simple_Sling_Bullet.png", 4, 0.06f);
    if (!projectileVisualPlayer_.texture) {
        projectileVisualPlayer_ = loadSheet("assets/Sprites/Misc/playerprojectile.png", 1, 0.10f);
    }
    projectileVisualTurret_ = loadSheet("assets/Sprites/Misc/Sling_Bullets.png", 4, 0.08f);
    if (!projectileVisualTurret_.texture) {
        projectileVisualTurret_ = projectileVisualPlayer_;
    }
    projectileVisualArrow_ = loadSheet("assets/Sprites/Misc/Arrows.png", 4, 0.08f);
    if (!projectileVisualArrow_.texture) {
        projectileVisualArrow_ = projectileVisualPlayer_;
    }

    projectileTexRed_ = projectileVisualPlayer_.texture;
    projectileTexTurret_ = projectileVisualTurret_.texture;
    if (projectileVisualPlayer_.texture) projectileTextures_.push_back(projectileVisualPlayer_.texture);
    if (projectileVisualTurret_.texture && projectileVisualTurret_.texture != projectileVisualPlayer_.texture) {
        projectileTextures_.push_back(projectileVisualTurret_.texture);
    }
    if (projectileVisualArrow_.texture && projectileVisualArrow_.texture != projectileVisualPlayer_.texture &&
        projectileVisualArrow_.texture != projectileVisualTurret_.texture) {
        projectileTextures_.push_back(projectileVisualArrow_.texture);
    }

    // Wizard elemental projectile sheet
    wizardElementTex_ = loadTextureOptional("assets/Sprites/Spells/Elemental_Spellcasting_Effects_v1_Anti_Alias_glow_8x8.png");
    if (wizardElementTex_) {
        wizardElementColumns_ = std::max(1, wizardElementTex_->width() / 8);
    }
}

void GameRoot::loadRpgData() {
    rpgLootTable_ = Game::RPG::loadLootTable("data/rpg/loot.json");
    if (rpgLootTable_.items.empty()) {
        rpgLootTable_ = Game::RPG::defaultLootTable();
        Engine::logWarn("data/rpg/loot.json missing or empty; using default RPG loot table.");
    } else {
        Engine::logInfo("Loaded RPG loot table from data/rpg/loot.json");
    }
    rpgConsumables_ = Game::RPG::loadConsumables("data/rpg/consumables.json");
    if (rpgConsumables_.empty()) {
        rpgConsumables_ = Game::RPG::defaultConsumables();
        Engine::logWarn("data/rpg/consumables.json missing; using defaults.");
    }
    rpgTalents_ = Game::RPG::loadTalentTrees("data/rpg/talents.json");
}

Game::RPG::LootTable GameRoot::filteredRpgLootTable(RpgLootSource src) const {
    Game::RPG::LootTable out{};
    out.affixes = rpgLootTable_.affixes;
    auto hasAny = [&](const std::initializer_list<const char*>& needles, const std::string& s) {
        for (const char* n : needles) {
            if (s.find(n) != std::string::npos) return true;
        }
        return false;
    };
    auto allow = [&](const Game::RPG::ItemTemplate& t) -> bool {
        using Game::RPG::Rarity;
        if (t.slot == Game::RPG::EquipmentSlot::Bag) return false;
        if (t.slot == Game::RPG::EquipmentSlot::Consumable || !t.consumableId.empty()) return false;
        const std::string& nm = t.name;
        switch (src) {
            case RpgLootSource::Normal:
                // Regular enemies: mostly low-grade wooden/copper/basic/iron gear.
                if (t.iconSheet == "Gems.png" || t.iconSheet == "Runes.png") return false;
                if (static_cast<int>(t.rarity) > static_cast<int>(Rarity::Uncommon)) return false;
                return hasAny({"Wooden", "Copper", "Basic", "Iron", "Green"}, nm);
            case RpgLootSource::MiniBoss:
                // Mini bosses: gems are common, plus mid/high-grade equipment.
                if (t.iconSheet == "Gems.png") return true;
                return static_cast<int>(t.rarity) >= static_cast<int>(Rarity::Rare);
            case RpgLootSource::Boss:
                // Bosses: higher-grade equipment + gems/runes.
                if (t.iconSheet == "Gems.png" || t.iconSheet == "Runes.png") return true;
                return static_cast<int>(t.rarity) >= static_cast<int>(Rarity::Epic);
            case RpgLootSource::Shop:
                // Traveling shop: higher-grade variety.
                if (t.iconSheet == "Gems.png") return true;
                return static_cast<int>(t.rarity) >= static_cast<int>(Rarity::Uncommon);
        }
        return true;
    };
    for (const auto& t : rpgLootTable_.items) {
        if (allow(t)) out.items.push_back(t);
    }
    // Fallback safety.
    if (out.items.empty()) out.items = rpgLootTable_.items;
    return out;
}

Game::RPG::LootTable GameRoot::filteredRpgConsumableTable(RpgLootSource src) const {
    Game::RPG::LootTable out{};
    out.affixes = rpgLootTable_.affixes;
    auto allow = [&](const Game::RPG::ItemTemplate& t) -> bool {
        if (!(t.slot == Game::RPG::EquipmentSlot::Consumable || !t.consumableId.empty())) return false;
        using Game::RPG::Rarity;
        switch (src) {
            case RpgLootSource::Normal:
                return static_cast<int>(t.rarity) <= static_cast<int>(Rarity::Uncommon);
            case RpgLootSource::MiniBoss:
                return static_cast<int>(t.rarity) <= static_cast<int>(Rarity::Rare);
            case RpgLootSource::Boss:
            case RpgLootSource::Shop:
                return true;
        }
        return true;
    };
    for (const auto& t : rpgLootTable_.items) {
        if (allow(t)) out.items.push_back(t);
    }
    return out;
}

Game::RPG::LootTable GameRoot::filteredRpgBagTable(RpgLootSource src) const {
    Game::RPG::LootTable out{};
    out.affixes = rpgLootTable_.affixes;
    auto allow = [&](const Game::RPG::ItemTemplate& t) -> bool {
        if (t.slot != Game::RPG::EquipmentSlot::Bag) return false;
        using Game::RPG::Rarity;
        switch (src) {
            case RpgLootSource::Normal:
                return false;
            case RpgLootSource::MiniBoss:
                return static_cast<int>(t.rarity) <= static_cast<int>(Rarity::Epic);
            case RpgLootSource::Boss:
            case RpgLootSource::Shop:
                return true;
        }
        return true;
    };
    for (const auto& t : rpgLootTable_.items) {
        if (allow(t)) out.items.push_back(t);
    }
    return out;
}

ItemDefinition GameRoot::buildRpgItemDef(const Game::RPG::GeneratedItem& rolled, bool priceInGold) const {
    Game::RPG::LootContext ctx{};
    ctx.level = std::max(1, level_);
    ctx.luck = static_cast<float>(activeArchetype_.rpgAttributes.LCK) * 0.05f;

    ItemDefinition def{};
    static int rpgId = 7000;
    def.id = rpgId++;
    def.name = rolled.def.name;
    def.desc = "Equipment";
    def.kind = ItemKind::Combat;
    def.effect = ItemEffect::Damage;  // unused by RPG items, but keeps legacy defaults safe

    if (priceInGold) {
        int base = 2;
        switch (rolled.def.rarity) {
            case Game::RPG::Rarity::Common: base = 1; break;
            case Game::RPG::Rarity::Uncommon: base = 2; break;
            case Game::RPG::Rarity::Rare: base = 3; break;
            case Game::RPG::Rarity::Epic: base = 4; break;
            case Game::RPG::Rarity::Legendary: base = 6; break;
        }
        def.cost = base + std::max(0, (ctx.level - 1) / 12);
    } else {
        def.cost = (10 + static_cast<int>(rolled.def.rarity) * 10) + std::max(0, ctx.level - 1) * 2;
    }

    def.affixes.reserve(rolled.affixes.size());
    def.rpgTemplateId = rolled.def.id;
    def.rpgAffixIds.reserve(rolled.affixes.size());
    for (const auto& af : rolled.affixes) {
        def.affixes.push_back(af.name);
        def.rpgAffixIds.push_back(af.id);
    }
    def.rpgSocketsMax = std::max(0, rolled.def.socketsMax);
    def.rpgSocketed.clear();
    switch (rolled.def.rarity) {
        case Game::RPG::Rarity::Common: def.rarity = ItemRarity::Common; break;
        case Game::RPG::Rarity::Uncommon: def.rarity = ItemRarity::Uncommon; break;
        case Game::RPG::Rarity::Rare: def.rarity = ItemRarity::Rare; break;
        case Game::RPG::Rarity::Epic: def.rarity = ItemRarity::Epic; break;
        case Game::RPG::Rarity::Legendary: def.rarity = ItemRarity::Legendary; break;
    }

    auto slotLabel = [&](Game::RPG::EquipmentSlot s) -> const char* {
        switch (s) {
            case Game::RPG::EquipmentSlot::Head: return "Head";
            case Game::RPG::EquipmentSlot::Chest: return "Chest";
            case Game::RPG::EquipmentSlot::Legs: return "Legs";
            case Game::RPG::EquipmentSlot::Boots: return "Boots";
            case Game::RPG::EquipmentSlot::MainHand: return "Main Hand";
            case Game::RPG::EquipmentSlot::OffHand: return "Off Hand";
            case Game::RPG::EquipmentSlot::Ring1: return "Ring";
            case Game::RPG::EquipmentSlot::Ring2: return "Ring";
            case Game::RPG::EquipmentSlot::Amulet: return "Amulet";
            case Game::RPG::EquipmentSlot::Trinket: return "Trinket";
            case Game::RPG::EquipmentSlot::Cloak: return "Cloak";
            case Game::RPG::EquipmentSlot::Gloves: return "Gloves";
            case Game::RPG::EquipmentSlot::Belt: return "Belt";
            case Game::RPG::EquipmentSlot::Ammo: return "Ammo";
            case Game::RPG::EquipmentSlot::Bag: return "Bag";
            case Game::RPG::EquipmentSlot::Consumable: return "Consumable";
            case Game::RPG::EquipmentSlot::Count: break;
        }
        return "Equipment";
    };

    if (rolled.def.socketable) {
        def.desc = "Socketable Gem";
    } else if (!rolled.def.consumableId.empty() || rolled.def.slot == Game::RPG::EquipmentSlot::Consumable) {
        def.kind = ItemKind::Support;
        def.rpgConsumableId = rolled.def.consumableId;
        def.desc = "Consumable";
    } else if (rolled.def.slot == Game::RPG::EquipmentSlot::Bag) {
        def.desc = std::string("Equipment: Bag (+") + std::to_string(std::max(0, rolled.def.bagSlots)) + " slots)";
    } else {
        def.desc = std::string("Equipment: ") + slotLabel(rolled.def.slot);
    }

    return def;
}

std::optional<ItemDefinition> GameRoot::rollRpgEquipment(RpgLootSource src, bool priceInGold) {
    if (rpgLootTable_.items.empty()) return std::nullopt;
    Game::RPG::LootContext ctx{};
    ctx.level = std::max(1, level_);
    ctx.luck = static_cast<float>(activeArchetype_.rpgAttributes.LCK) * 0.05f;
    Game::RPG::LootTable table = filteredRpgLootTable(src);
    auto rolled = Game::RPG::generateLoot(table, ctx, rpgRng_);
    if (rolled.def.id.empty()) return std::nullopt;
    return buildRpgItemDef(rolled, priceInGold);
}

std::optional<ItemDefinition> GameRoot::rollRpgConsumable(RpgLootSource src, bool priceInGold) {
    Game::RPG::LootContext ctx{};
    ctx.level = std::max(1, level_);
    ctx.luck = static_cast<float>(activeArchetype_.rpgAttributes.LCK) * 0.05f;
    Game::RPG::LootTable table = filteredRpgConsumableTable(src);
    if (table.items.empty()) return std::nullopt;
    auto rolled = Game::RPG::generateLoot(table, ctx, rpgRng_);
    if (rolled.def.id.empty()) return std::nullopt;
    return buildRpgItemDef(rolled, priceInGold);
}

std::optional<ItemDefinition> GameRoot::rollRpgBag(RpgLootSource src, bool priceInGold) {
    Game::RPG::LootContext ctx{};
    ctx.level = std::max(1, level_);
    ctx.luck = static_cast<float>(activeArchetype_.rpgAttributes.LCK) * 0.05f;
    Game::RPG::LootTable table = filteredRpgBagTable(src);
    if (table.items.empty()) return std::nullopt;
    auto rolled = Game::RPG::generateLoot(table, ctx, rpgRng_);
    if (rolled.def.id.empty()) return std::nullopt;
    return buildRpgItemDef(rolled, priceInGold);
}

namespace {
void addContribution(Engine::Gameplay::RPG::StatContribution& dst, const Engine::Gameplay::RPG::StatContribution& src, float scalar = 1.0f) {
    auto addFlat = [&](float& a, float b) { a += b * scalar; };
    auto addMult = [&](float& a, float b) { a += b * scalar; };

    addFlat(dst.flat.attackPower, src.flat.attackPower);
    addFlat(dst.flat.spellPower, src.flat.spellPower);
    addFlat(dst.flat.attackSpeed, src.flat.attackSpeed);
    addFlat(dst.flat.moveSpeed, src.flat.moveSpeed);
    addFlat(dst.flat.accuracy, src.flat.accuracy);
    addFlat(dst.flat.critChance, src.flat.critChance);
    addFlat(dst.flat.critMult, src.flat.critMult);
    addFlat(dst.flat.armorPen, src.flat.armorPen);
    addFlat(dst.flat.evasion, src.flat.evasion);
    addFlat(dst.flat.armor, src.flat.armor);
    addFlat(dst.flat.tenacity, src.flat.tenacity);
    addFlat(dst.flat.shieldMax, src.flat.shieldMax);
    addFlat(dst.flat.shieldRegen, src.flat.shieldRegen);
    addFlat(dst.flat.healthMax, src.flat.healthMax);
    addFlat(dst.flat.healthRegen, src.flat.healthRegen);
    addFlat(dst.flat.cooldownReduction, src.flat.cooldownReduction);
    addFlat(dst.flat.resourceRegen, src.flat.resourceRegen);
    addFlat(dst.flat.goldGainMult, src.flat.goldGainMult);
    addFlat(dst.flat.rarityScore, src.flat.rarityScore);
    for (std::size_t i = 0; i < dst.flat.resists.values.size(); ++i) {
        dst.flat.resists.values[i] += src.flat.resists.values[i] * scalar;
    }

    addMult(dst.mult.attackPower, src.mult.attackPower);
    addMult(dst.mult.spellPower, src.mult.spellPower);
    addMult(dst.mult.attackSpeed, src.mult.attackSpeed);
    addMult(dst.mult.moveSpeed, src.mult.moveSpeed);
    addMult(dst.mult.accuracy, src.mult.accuracy);
    addMult(dst.mult.critChance, src.mult.critChance);
    addMult(dst.mult.critMult, src.mult.critMult);
    addMult(dst.mult.armorPen, src.mult.armorPen);
    addMult(dst.mult.evasion, src.mult.evasion);
    addMult(dst.mult.armor, src.mult.armor);
    addMult(dst.mult.tenacity, src.mult.tenacity);
    addMult(dst.mult.shieldMax, src.mult.shieldMax);
    addMult(dst.mult.shieldRegen, src.mult.shieldRegen);
    addMult(dst.mult.healthMax, src.mult.healthMax);
    addMult(dst.mult.healthRegen, src.mult.healthRegen);
    addMult(dst.mult.cooldownReduction, src.mult.cooldownReduction);
    addMult(dst.mult.resourceRegen, src.mult.resourceRegen);
    addMult(dst.mult.goldGainMult, src.mult.goldGainMult);
    addMult(dst.mult.rarityScore, src.mult.rarityScore);
    for (std::size_t i = 0; i < dst.mult.resists.values.size(); ++i) {
        dst.mult.resists.values[i] += src.mult.resists.values[i] * scalar;
    }
}
}  // namespace

void GameRoot::updateRpgTalentAllocation() {
    if (rpgTalentArchetypeCached_ != activeArchetype_.id) {
        rpgTalentRanks_.clear();
        rpgTalentPointsSpent_ = 0;
        rpgTalentLevelCached_ = 0;
        rpgTalentArchetypeCached_ = activeArchetype_.id;
    }
    if (level_ == rpgTalentLevelCached_) return;
    rpgTalentLevelCached_ = level_;

    const int points = std::max(0, level_ / 10);
    if (points <= rpgTalentPointsSpent_) return;

    auto treeIt = rpgTalents_.find(activeArchetype_.id);
    if (treeIt == rpgTalents_.end()) return;
    const auto& nodes = treeIt->second;
    int available = points - rpgTalentPointsSpent_;
    for (int i = 0; i < available; ++i) {
        bool spent = false;
        for (const auto& node : nodes) {
            int& rank = rpgTalentRanks_[node.id];
            if (rank < node.maxRank) {
                rank += 1;
                rpgTalentPointsSpent_ += 1;
                spent = true;
                break;
            }
        }
        if (!spent) break;
    }
}

void GameRoot::updateRpgConsumables(double dt) {
    if (dt <= 0.0) return;
    if (hero_ == Engine::ECS::kInvalidEntity) return;
    if (paused_ || defeated_) return;

    // Cooldowns tick down by group.
    for (auto it = rpgConsumableCooldowns_.begin(); it != rpgConsumableCooldowns_.end(); ) {
        it->second = std::max(0.0, it->second - dt);
        if (it->second <= 0.0001) it = rpgConsumableCooldowns_.erase(it);
        else ++it;
    }

    // Over-time effects apply to the Health pools.
    if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        for (auto it = rpgConsumableOverTime_.begin(); it != rpgConsumableOverTime_.end(); ) {
            it->ttl -= dt;
            const float maxPool = (it->resource == Game::RPG::ConsumableResource::Shields) ? hp->maxShields : hp->maxHealth;
            if (maxPool > 0.0f && it->fractionPerSecond > 0.0f) {
                const float delta = std::max(0.0f, it->fractionPerSecond) * maxPool * static_cast<float>(dt);
                if (it->resource == Game::RPG::ConsumableResource::Shields) {
                    hp->currentShields = std::min(hp->maxShields, hp->currentShields + delta);
                } else {
                    hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + delta);
                }
            }
            if (it->ttl <= 0.0) it = rpgConsumableOverTime_.erase(it);
            else ++it;
        }
    }

    // Timed stat buffs fold into a single RPG contribution so aggregation stays fast.
    rpgActiveBuffContribution_ = {};
    for (auto it = rpgActiveBuffs_.begin(); it != rpgActiveBuffs_.end(); ) {
        it->ttl -= dt;
        if (it->ttl <= 0.0) {
            it = rpgActiveBuffs_.erase(it);
            continue;
        }
        addContribution(rpgActiveBuffContribution_, it->contribution, 1.0f);
        ++it;
    }
}

const Game::RPG::ItemTemplate* GameRoot::findRpgTemplateById(const std::string& id) const {
    if (id.empty()) return nullptr;
    for (const auto& t : rpgLootTable_.items) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const Game::RPG::ItemTemplate* GameRoot::findRpgTemplateFor(const ItemDefinition& def) const {
    return findRpgTemplateById(def.rpgTemplateId);
}

Engine::TexturePtr GameRoot::getEquipmentIconSheet(const std::string& sheetName) {
    if (sheetName.empty() || !textureManager_) return {};
    auto it = equipmentIconSheets_.find(sheetName);
    if (it != equipmentIconSheets_.end()) return it->second;

    std::filesystem::path rel = std::filesystem::path("assets") / "Sprites" / "Equipment" / sheetName;
    std::filesystem::path path = rel;
    if (const char* home = std::getenv("HOME")) {
        std::filesystem::path hp = std::filesystem::path(home) / rel;
        if (std::filesystem::exists(hp)) path = hp;
    }
    Engine::TexturePtr tex = loadTextureOptional(path.string());
    equipmentIconSheets_[sheetName] = tex;
    return tex;
}

void GameRoot::drawItemIcon(const ItemDefinition& def, const Engine::Vec2& pos, float size) {
    if (!render_) return;
    const auto* t = findRpgTemplateFor(def);
    if (!t || t->iconSheet.empty()) return;
    Engine::TexturePtr sheet = getEquipmentIconSheet(t->iconSheet);
    if (!sheet) return;
    constexpr int kCell = 16;
    Engine::IntRect src{t->iconCol * kCell, t->iconRow * kCell, kCell, kCell};
    render_->drawTextureRegion(*sheet, pos, Engine::Vec2{size, size}, src);
}

Engine::Gameplay::RPG::StatContribution GameRoot::collectRpgEquippedContribution() const {
    Engine::Gameplay::RPG::StatContribution total{};
    auto findAffix = [&](const std::string& id) -> const Game::RPG::Affix* {
        for (const auto& a : rpgLootTable_.affixes) {
            if (a.id == id) return &a;
        }
        return nullptr;
    };
    for (const auto& opt : equipped_) {
        if (!opt.has_value()) continue;
        const auto& inst = *opt;
        const auto& def = inst.def;
        const auto* t = findRpgTemplateById(def.rpgTemplateId);
        if (t) addContribution(total, t->baseStats, static_cast<float>(std::max(1, inst.quantity)));
        for (const auto& affId : def.rpgAffixIds) {
            if (const auto* a = findAffix(affId)) {
                addContribution(total, a->stats, static_cast<float>(std::max(1, inst.quantity)));
            }
        }
        // Socketed gems (their base stats + rolled affixes).
        for (const auto& g : def.rpgSocketed) {
            const auto* gt = findRpgTemplateById(g.templateId);
            if (gt) addContribution(total, gt->baseStats, static_cast<float>(std::max(1, inst.quantity)));
            for (const auto& ga : g.affixIds) {
                if (const auto* a = findAffix(ga)) {
                    addContribution(total, a->stats, static_cast<float>(std::max(1, inst.quantity)));
                }
            }
        }
    }
    return total;
}

Engine::Gameplay::RPG::StatContribution GameRoot::collectRpgTalentContribution() const {
    Engine::Gameplay::RPG::StatContribution total{};
    auto treeIt = rpgTalents_.find(activeArchetype_.id);
    if (treeIt == rpgTalents_.end()) return total;
    const auto& nodes = treeIt->second;
    for (const auto& node : nodes) {
        auto it = rpgTalentRanks_.find(node.id);
        if (it == rpgTalentRanks_.end()) continue;
        int rank = std::max(0, it->second);
        if (rank <= 0) continue;
        addContribution(total, node.bonus, static_cast<float>(rank));
    }
    return total;
}

void GameRoot::updateHeroRpgStats() {
    if (hero_ == Engine::ECS::kInvalidEntity) return;

    updateRpgTalentAllocation();
    Engine::Gameplay::RPG::StatContribution mods = collectRpgEquippedContribution();
    addContribution(mods, collectRpgTalentContribution(), 1.0f);
    addContribution(mods, rpgActiveBuffContribution_, 1.0f);

    if (!registry_.has<Engine::ECS::RPGStats>(hero_)) {
        registry_.emplace<Engine::ECS::RPGStats>(hero_, Engine::ECS::RPGStats{});
    }
    auto* rpg = registry_.get<Engine::ECS::RPGStats>(hero_);
    if (!rpg) return;
    rpg->baseFromHealth = false;
    rpg->attributes = activeArchetype_.rpgAttributes;
    rpg->modifiers = mods;

    // Use the legacy run-tuned values as the RPG "base template" so RPG scaling stays comparable.
    rpg->base.baseHealth = heroMaxHp_;
    rpg->base.baseShields = heroShield_;
    rpg->base.baseArmor = heroHealthArmor_;
    rpg->base.baseMoveSpeed = heroMoveSpeed_;
    rpg->base.baseAttackPower = projectileDamage_;
    rpg->base.baseSpellPower = projectileDamage_;
    rpg->base.baseAttackSpeed = std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);

    Engine::Gameplay::RPG::AggregationInput input{};
    input.attributes = rpg->attributes;
    input.base = rpg->base;
    input.contributions = {rpg->modifiers};
    rpg->derived = Engine::Gameplay::RPG::aggregateDerivedStats(input);
    rpg->dirty = false;

    // Mirror key derived stats back into the Health component so mitigation/UI match the resolver.
    if (useRpgCombat_) {
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
        const float oldMaxHp = std::max(1.0f, hp->maxHealth);
        const float oldMaxSh = std::max(0.0f, hp->maxShields);
        const float hpFrac = std::clamp(hp->currentHealth / oldMaxHp, 0.0f, 1.0f);
        const float shFrac = oldMaxSh > 0.0f ? std::clamp(hp->currentShields / oldMaxSh, 0.0f, 1.0f) : 0.0f;

        hp->maxHealth = std::max(1.0f, rpg->derived.healthMax);
        hp->maxShields = std::max(0.0f, rpg->derived.shieldMax);
        hp->healthArmor = rpg->derived.armor;
        hp->currentHealth = std::min(hp->maxHealth, hpFrac * hp->maxHealth);
        hp->currentShields = std::min(hp->maxShields, shFrac * hp->maxShields);
        }
    }
}

void GameRoot::spawnPickupEntity(const Pickup& payload, const Engine::Vec2& pos, Engine::Color color,
                                 float size, float amp, float speed) {
    auto drop = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(drop, pos);
    Engine::TexturePtr tex{};
    std::optional<Engine::ECS::SpriteAnimation> anim{};
    float scale = 1.0f;
    Engine::Color finalColor = color;

    auto setSheetIcon = [&](const std::string& sheet, int row, int col, float iconScale) {
        Engine::TexturePtr sheetTex = getEquipmentIconSheet(sheet);
        if (!sheetTex) return false;
        tex = sheetTex;
        scale = iconScale;
        finalColor = Engine::Color{255, 255, 255, 255};
        const int frameCount = std::max(1, sheetTex->width() / 16);
        Engine::ECS::SpriteAnimation sa{};
        sa.frameWidth = 16;
        sa.frameHeight = 16;
        sa.frameCount = frameCount;
        sa.frameDuration = 99999.0f;
        sa.currentFrame = std::clamp(col, 0, frameCount - 1);
        sa.row = std::max(0, row);
        sa.loop = false;
        sa.holdOnLastFrame = true;
        anim = sa;
        return true;
    };

    if (payload.kind == Pickup::Kind::Copper) { tex = pickupCopperTex_; scale = 1.75f; anim = Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f}; }
    if (payload.kind == Pickup::Kind::Gold)   { tex = pickupGoldTex_;   scale = 1.3f; }
    if (payload.kind == Pickup::Kind::Powerup) {
        switch (payload.powerup) {
            case Pickup::Powerup::Heal: tex = pickupHealPowerupTex_; break;
            case Pickup::Powerup::Kaboom: tex = pickupKaboomTex_; break;
            case Pickup::Powerup::Recharge: tex = pickupRechargeTex_; scale *= 1.8f; break;
            case Pickup::Powerup::Frenzy: tex = pickupFrenzyTex_; break;
            case Pickup::Powerup::Immortal: tex = pickupImmortalTex_; anim = Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f}; break;
            case Pickup::Powerup::Freeze: tex = pickupFreezeTex_; break;
            default: break;
        }
    }
    if (payload.kind == Pickup::Kind::Item) {
        if (!payload.item.rpgTemplateId.empty()) {
            // RPG items: show a rarity container, except for gems/consumables/bags which show their own icon.
            const auto* t = findRpgTemplateFor(payload.item);
            bool showSelf = false;
            if (t) {
                showSelf = t->socketable ||
                           t->slot == Game::RPG::EquipmentSlot::Bag ||
                           t->slot == Game::RPG::EquipmentSlot::Consumable ||
                           t->iconSheet == "Gems.png" || t->iconSheet == "Runes.png" ||
                           t->iconSheet == "Foods.png" || t->iconSheet == "Potions.png" || t->iconSheet == "Bags.png";
            }
            if (t && showSelf && !t->iconSheet.empty()) {
                (void)setSheetIcon(t->iconSheet, t->iconRow, t->iconCol, 2.1f);
            } else {
                int containerCol = 0;  // Small Bag
                if (payload.item.rarity == ItemRarity::Rare || payload.item.rarity == ItemRarity::Epic) containerCol = 2;  // Small Chest
                if (payload.item.rarity == ItemRarity::Legendary) containerCol = 3;  // Chest
                (void)setSheetIcon("Containers.png", 0, containerCol, 2.2f);
            }
        } else {
            switch (payload.item.effect) {
                case ItemEffect::Turret: tex = pickupTurretTex_; break;
                case ItemEffect::Heal: tex = pickupHealTex_; break;
                case ItemEffect::FreezeTime: tex = pickupFreezeTex_; scale *= 1.8f; break;
                case ItemEffect::AttackSpeed: tex = pickupChronoTex_; anim = Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f}; break;
                case ItemEffect::Lifesteal: tex = pickupPhaseLeechTex_; anim = Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f}; break;
                case ItemEffect::Chain: tex = pickupStormCoreTex_; anim = Engine::ECS::SpriteAnimation{16, 16, 6, 0.08f}; break;
                default: break;
            }
        }
    }

    Engine::Vec2 rendSize{size, size};
    float aabbHalf = size * 0.5f;
    if (tex) {
        if (anim.has_value()) {
            rendSize = Engine::Vec2{16.0f * scale, 16.0f * scale};
        } else {
            rendSize = Engine::Vec2{static_cast<float>(tex->width()) * scale,
                                    static_cast<float>(tex->height()) * scale};
        }
        aabbHalf = std::max(rendSize.x, rendSize.y) * 0.5f;
    }
    registry_.emplace<Engine::ECS::Renderable>(drop, Engine::ECS::Renderable{rendSize, finalColor, tex});
    registry_.emplace<Engine::ECS::AABB>(drop, Engine::ECS::AABB{Engine::Vec2{aabbHalf, aabbHalf}});
    registry_.emplace<Game::Pickup>(drop, payload);
    registry_.emplace<Game::PickupBob>(drop, Game::PickupBob{pos, 0.0f, amp, speed});
    registry_.emplace<Engine::ECS::PickupTag>(drop, Engine::ECS::PickupTag{});
    if (anim.has_value() && tex) {
        registry_.emplace<Engine::ECS::SpriteAnimation>(drop, *anim);
    }
}

void GameRoot::dropInventoryOverflow(const Engine::Vec2& dropBase) {
    if (inventory_.size() <= static_cast<std::size_t>(inventoryCapacity_)) return;
    std::uniform_real_distribution<float> ang(0.0f, 6.28318f);
    std::uniform_real_distribution<float> rad(10.0f, 30.0f);
    auto scatterPos = [&]() {
        float a = ang(rng_);
        float r = rad(rng_);
        return Engine::Vec2{dropBase.x + std::cos(a) * r, dropBase.y + std::sin(a) * r};
    };
    while (inventory_.size() > static_cast<std::size_t>(inventoryCapacity_)) {
        const auto instDrop = inventory_.back();
        inventory_.pop_back();
        Pickup p{};
        p.kind = Pickup::Kind::Item;
        p.item = instDrop.def;
        spawnPickupEntity(p, scatterPos(), Engine::Color{235, 240, 245, 235}, 11.0f, 3.8f, 3.0f);
    }
    clampInventorySelection();
}

void GameRoot::refreshInventoryCapacityFromBag(bool dropOverflow) {
    int extra = 0;
    const std::size_t bagIdx = static_cast<std::size_t>(Game::RPG::EquipmentSlot::Bag);
    if (bagIdx < equipped_.size() && equipped_[bagIdx].has_value()) {
        if (const auto* bt = findRpgTemplateFor(equipped_[bagIdx]->def)) {
            extra = std::max(0, bt->bagSlots);
        }
    }
    inventoryCapacity_ = std::max(1, baseInventoryCapacity_ + extra);
    inventoryGridScroll_ = std::max(0.0f, inventoryGridScroll_);
    if (dropOverflow && inventory_.size() > static_cast<std::size_t>(inventoryCapacity_)) {
        Engine::Vec2 dropPos{};
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(hero_)) dropPos = tf->position;
        dropInventoryOverflow(dropPos);
    }
}

bool GameRoot::equipInventoryItem(std::size_t idx) {
    if (idx >= inventory_.size()) return false;
    auto& inst = inventory_[idx];
    const auto* t = findRpgTemplateFor(inst.def);
    if (!t) return false;
    if (t->socketable) return false;
    if (t->slot == Game::RPG::EquipmentSlot::Consumable) return false;

    auto slotIndex = [&](Game::RPG::EquipmentSlot slot) -> std::size_t {
        return static_cast<std::size_t>(slot);
    };

    Game::RPG::EquipmentSlot desired = t->slot;
    std::size_t target = slotIndex(desired);
    if (desired == Game::RPG::EquipmentSlot::Ring1) {
        std::size_t r1 = slotIndex(Game::RPG::EquipmentSlot::Ring1);
        std::size_t r2 = slotIndex(Game::RPG::EquipmentSlot::Ring2);
        if (!equipped_[r1].has_value()) target = r1;
        else if (!equipped_[r2].has_value()) target = r2;
        else target = r1;
    }

    if (target >= equipped_.size()) return false;
    if (equipped_[target].has_value()) {
        // Swap in-place to avoid failing when inventory is full.
        std::swap(*equipped_[target], inst);
    } else {
        equipped_[target] = inst;
        inventory_.erase(inventory_.begin() + static_cast<std::ptrdiff_t>(idx));
    }
    clampInventorySelection();
    refreshInventoryCapacityFromBag(true);
    return true;
}

bool GameRoot::unequipRpgSlot(Game::RPG::EquipmentSlot slot) {
    const std::size_t idx = static_cast<std::size_t>(slot);
    if (idx >= equipped_.size()) return false;
    if (!equipped_[idx].has_value()) return false;
    inventory_.insert(inventory_.begin(), *equipped_[idx]);
    equipped_[idx].reset();
    clampInventorySelection();
    refreshInventoryCapacityFromBag(true);
    return true;
}

void GameRoot::pushCombatDebugLine(const std::string& line) {
    if (!combatDebugOverlay_) return;
    combatDebugLines_.push_back(CombatDebugLine{line, 4.0});
    const std::size_t kMax = 12;
    if (combatDebugLines_.size() > kMax) {
        combatDebugLines_.erase(combatDebugLines_.begin(),
                                combatDebugLines_.begin() + static_cast<std::ptrdiff_t>(combatDebugLines_.size() - kMax));
    }
}

void GameRoot::updateCombatDebugLines(double dt) {
    if (!combatDebugOverlay_) {
        combatDebugLines_.clear();
        return;
    }
    for (auto& l : combatDebugLines_) l.ttl -= dt;
    combatDebugLines_.erase(std::remove_if(combatDebugLines_.begin(), combatDebugLines_.end(),
                                           [](const CombatDebugLine& l) { return l.ttl <= 0.0; }),
                            combatDebugLines_.end());
}

void GameRoot::drawCombatDebugOverlay() {
    if (!combatDebugOverlay_ || !useRpgCombat_) return;
    if (!render_ || combatDebugLines_.empty()) return;

    const float x = 12.0f;
    float y = 110.0f;
    const float w = 520.0f;
    const float lineH = 18.0f;
    const float h = 26.0f + static_cast<float>(combatDebugLines_.size()) * lineH;
    render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, Engine::Color{12, 16, 24, 210});
    drawTextUnified("RPG Combat Debug", Engine::Vec2{x + 10.0f, y + 6.0f}, 0.85f, Engine::Color{200, 230, 255, 235});
    y += 24.0f;
    for (const auto& l : combatDebugLines_) {
        drawTextUnified(l.text, Engine::Vec2{x + 10.0f, y}, 0.75f, Engine::Color{200, 210, 230, 225});
        y += lineH;
    }
}

GameRoot::HeroSpriteFiles GameRoot::heroSpriteFilesFor(const GameRoot::ArchetypeDef& def) const {
    GameRoot::HeroSpriteFiles files{};
    auto set = [&](std::string folder, std::string base) {
        files.folder = std::move(folder);
        files.movementFile = base + ".png";
        files.combatFile = base + "_Combat.png";
    };
    if (def.id == "assassin") set("Assassin", "Assassin");
    else if (def.id == "healer") set("Healer", "Healer");
    else if (def.id == "damage") set("Damage Dealer", "Dps");
    else if (def.id == "tank") set("Tank", "Tank");
    else if (def.id == "builder") set("Builder", "Builder");
    else if (def.id == "support") set("Support", "Support");
    else if (def.id == "special") set("Special", "Special");
    else if (def.id == "summoner") set("Summoner", "Summoner");
    else if (def.id == "druid") set("Druid", "Druid");
    else if (def.id == "wizard") set("Wizard", "Wizard");
    else {
        std::string folder = def.name.empty() ? def.id : def.name;
        if (folder.empty()) folder = "Damage Dealer";
        files.folder = folder;
        std::string base = folder;
        base.erase(std::remove(base.begin(), base.end(), ' '), base.end());
        files.movementFile = base + ".png";
        files.combatFile = base + "_Combat.png";
    }
    return files;
}

bool isMeleeOnlyArchetype(const std::string& idOrName) {
    static const std::unordered_set<std::string> meleeIds = {
        "tank", "assassin", "support", "special", "damage"};
    std::string key = idOrName;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // normalize spaces
    for (auto& c : key) {
        if (c == '_') c = ' ';
    }
    return meleeIds.count(key) > 0;
}

void GameRoot::applyProjectileVisual(Engine::ECS::Entity e,
                                     float sizeMul,
                                     Engine::Color color,
                                     bool turret,
                                     const SpriteSheetDesc* overrideVis) {
    const SpriteSheetDesc& vis = overrideVis ? *overrideVis : (turret ? projectileVisualTurret_ : projectileVisualPlayer_);
    Engine::TexturePtr tex = vis.texture ? vis.texture : (turret ? projectileTexTurret_ : projectileTexRed_);
    int frameW = 0;
    int frameH = 0;
    int frameCount = 1;
    float frameDuration = 0.08f;
    if (vis.texture) {
        frameW = vis.frameWidth > 0 ? vis.frameWidth : vis.texture->width();
        frameH = vis.frameHeight > 0 ? vis.frameHeight : vis.texture->height();
        frameCount = std::max(1, vis.frameCount);
        frameDuration = vis.frameDuration;
    } else if (tex) {
        frameW = tex->width();
        frameH = tex->height();
    } else {
        frameW = frameH = static_cast<int>(projectileSize_ * sizeMul);
    }

    float w = static_cast<float>(frameW) * sizeMul;
    float h = static_cast<float>(frameH) * sizeMul;
    registry_.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{Engine::Vec2{w, h}, color, tex});
    if (tex && frameCount > 1) {
        registry_.emplace<Engine::ECS::SpriteAnimation>(
            e, Engine::ECS::SpriteAnimation{frameW, frameH, frameCount, frameDuration});
    }
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

Engine::ECS::Entity GameRoot::spawnMiniUnit(const MiniUnitDef& def, const Engine::Vec2& pos) {
    if (miniUnitSupplyUsed_ >= miniUnitSupplyMax_) {
        Engine::logWarn("Mini-unit spawn blocked: supply cap reached.");
        return Engine::ECS::kInvalidEntity;
    }
    if (copper_ < def.costCopper) {
        Engine::logWarn("Mini-unit spawn blocked: not enough copper.");
        return Engine::ECS::kInvalidEntity;
    }
    miniUnitSupplyUsed_++;
    copper_ -= def.costCopper;
    auto e = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(e, pos);
    registry_.emplace<Engine::ECS::Velocity>(e, Engine::Vec2{0.0f, 0.0f});
    const int frameW = def.frameWidth > 0 ? def.frameWidth : 16;
    const int frameH = def.frameHeight > 0 ? def.frameHeight : frameW;
    Engine::TexturePtr tex{};
    if (textureManager_ && !def.texturePath.empty()) {
        tex = loadTextureOptional(def.texturePath);
    }

    const float baseWidth = tex ? static_cast<float>(std::max(frameW, 16)) : static_cast<float>(std::max(frameW, 16));
    const float aspect = frameW > 0 ? static_cast<float>(frameH) / static_cast<float>(frameW) : 1.0f;
    Engine::Vec2 renderSize{baseWidth * kMiniUnitScale, baseWidth * aspect * kMiniUnitScale};

    registry_.emplace<Engine::ECS::AABB>(e, Engine::ECS::AABB{Engine::Vec2{renderSize.x * 0.5f, renderSize.y * 0.5f}});
    registry_.emplace<Engine::ECS::Renderable>(e,
        Engine::ECS::Renderable{renderSize, Engine::Color{160, 240, 200, 255}, tex});

    if (tex) {
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
    // Apply persistent builder buffs per class.
    float classMul = 1.0f;
    switch (def.cls) {
        case Game::MiniUnitClass::Light: classMul = miniBuffLight_; break;
        case Game::MiniUnitClass::Heavy: classMul = miniBuffHeavy_; break;
        case Game::MiniUnitClass::Medic: classMul = miniBuffMedic_; break;
    }
    hp.maxHealth *= classMul;
    hp.currentHealth *= classMul;
    hp.maxShields *= classMul;
    hp.currentShields *= classMul;
    hp.healthArmor = armor * classMul;
    hp.shieldArmor = shieldArmor * classMul;
    registry_.emplace<Engine::ECS::Health>(e, hp);
    registry_.emplace<Engine::ECS::Status>(e, Engine::ECS::Status{});
    registry_.emplace<Game::MiniUnit>(e, Game::MiniUnit{def.cls, 0, false, 0.0f, 0.0f});
    registry_.emplace<Game::MiniUnitCommand>(e, Game::MiniUnitCommand{false, {}});
    // Cache stats per-unit for data-driven AI/RTS control.
    Game::MiniUnitStats stats{};
    stats.moveSpeed = (def.moveSpeed > 0.0f ? def.moveSpeed : 160.0f) * globalSpeedMul_;
    stats.damage = def.damage * classMul;
    stats.healPerSecond = def.healPerSecond * classMul;
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
    return e;
}

void GameRoot::applyMiniUnitBuff(Game::MiniUnitClass cls, float mul) {
    if (mul <= 0.0f) return;
    switch (cls) {
        case Game::MiniUnitClass::Light: miniBuffLight_ *= mul; break;
        case Game::MiniUnitClass::Heavy: miniBuffHeavy_ *= mul; break;
        case Game::MiniUnitClass::Medic: miniBuffMedic_ *= mul; break;
    }
    registry_.view<Game::MiniUnit, Game::MiniUnitStats, Engine::ECS::Health>(
        [&](Engine::ECS::Entity, Game::MiniUnit& mu, Game::MiniUnitStats& stats, Engine::ECS::Health& hp) {
            if (mu.cls != cls) return;
            hp.maxHealth *= mul;
            hp.currentHealth *= mul;
            hp.maxShields *= mul;
            hp.currentShields *= mul;
            hp.healthArmor *= mul;
            hp.shieldArmor *= mul;
            stats.damage *= mul;
            stats.healPerSecond *= mul;
        });
}

void GameRoot::activateMiniUnitRage(float duration, float dmgMul, float atkRateMul, float hpMul, float healMul) {
    miniRageTimer_ = std::max(miniRageTimer_, duration);
    miniRageDamageMul_ = dmgMul;
    miniRageAttackRateMul_ = atkRateMul;
    miniRageHealMul_ = healMul;
    if (miniUnitSystem_) {
        miniUnitSystem_->setGlobalDamageMul(miniRageDamageMul_);
        miniUnitSystem_->setGlobalAttackRateMul(miniRageAttackRateMul_);
        miniUnitSystem_->setGlobalHealMul(miniRageHealMul_);
    }
    registry_.view<Game::MiniUnit, Game::MiniUnitStats, Engine::ECS::Health>(
        [&](Engine::ECS::Entity e, Game::MiniUnit&, Game::MiniUnitStats& stats, Engine::ECS::Health& hp) {
            if (registry_.has<Game::MiniUnitRageBuff>(e)) {
                if (auto* r = registry_.get<Game::MiniUnitRageBuff>(e)) r->timer = duration;
                return;
            }
            hp.maxHealth *= hpMul;
            hp.currentHealth = std::min(hp.maxHealth, hp.currentHealth * hpMul);
            hp.maxShields *= hpMul;
            hp.currentShields = std::min(hp.maxShields, hp.currentShields * hpMul);
            hp.healthArmor *= hpMul;
            hp.shieldArmor *= hpMul;
            stats.damage *= dmgMul;
            stats.attackRate *= atkRateMul;
            stats.healPerSecond *= healMul;
            registry_.emplace<Game::MiniUnitRageBuff>(e, Game::MiniUnitRageBuff{duration, hpMul, dmgMul, atkRateMul, healMul});
        });
}

void GameRoot::updateMiniUnitRage(float dt) {
    bool anyActive = false;
    registry_.view<Game::MiniUnitRageBuff, Game::MiniUnitStats, Engine::ECS::Health>(
        [&](Engine::ECS::Entity e, Game::MiniUnitRageBuff& r, Game::MiniUnitStats& stats, Engine::ECS::Health& hp) {
            r.timer -= dt;
            if (r.timer <= 0.0f) {
                // Revert stats.
                hp.maxHealth /= r.hpMul;
                hp.currentHealth = std::min(hp.maxHealth, hp.currentHealth / r.hpMul);
                hp.maxShields /= r.hpMul;
                hp.currentShields = std::min(hp.maxShields, hp.currentShields / r.hpMul);
                hp.healthArmor /= r.hpMul;
                hp.shieldArmor /= r.hpMul;
                stats.damage /= r.dmgMul;
                stats.attackRate /= r.atkRateMul;
                stats.healPerSecond /= r.healMul;
                registry_.remove<Game::MiniUnitRageBuff>(e);
            } else {
                anyActive = true;
            }
        });
    if (miniRageTimer_ > 0.0f) {
        miniRageTimer_ = std::max(0.0f, miniRageTimer_ - dt);
        if (miniRageTimer_ <= 0.0f && miniUnitSystem_) {
            miniUnitSystem_->setGlobalDamageMul(1.0f);
            miniUnitSystem_->setGlobalAttackRateMul(1.0f);
            miniUnitSystem_->setGlobalHealMul(1.0f);
        }
    }
    // If no per-unit buff remains, ensure globals reset.
    if (!anyActive && miniRageTimer_ <= 0.0f && miniUnitSystem_) {
        miniUnitSystem_->setGlobalDamageMul(1.0f);
        miniUnitSystem_->setGlobalAttackRateMul(1.0f);
        miniUnitSystem_->setGlobalHealMul(1.0f);
    }
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
    const float vw = static_cast<float>(viewportWidth_);
    const float vh = static_cast<float>(viewportHeight_);
    const float centerX = vw * 0.5f;
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
            // Main menu layout is designed around a 1920x1080 reference and scaled to the current viewport.
            const float refW = 1920.0f;
            const float refH = 1080.0f;
            const float s = std::min(vw / refW, vh / refH);
            const float btnW = 420.0f * s;
            const float btnH = 54.0f * s;
            const float gap = 14.0f * s;
            const float titleY = 72.0f * s;
            const float titleArea = titleY + 140.0f * s;
            const float footerArea = 90.0f * s;
            const float totalH = itemCount * btnH + (itemCount - 1) * gap;
            const float availableH = std::max(0.0f, vh - titleArea - footerArea);
            const float startY = titleArea + (availableH - totalH) * 0.5f;
            const float x = centerX - btnW * 0.5f;
            for (int i = 0; i < itemCount; ++i) {
                float y = startY + i * (btnH + gap);
                if (inside(mx, my, x, y, btnW, btnH)) {
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
            // Mouse click to select entries (hover sets focus; click selects).
            const float refW = 1920.0f;
            const float refH = 1080.0f;
            const float s = std::min(vw / refW, vh / refH);
            const float textBoost = 1.18f;
            const float margin = 28.0f * s;
            const float footerH = 120.0f * s;
            const float titleYRef = 72.0f * s;
            const float contentTop = titleYRef + 144.0f * s;
            const float contentBottom = vh - footerH;
            const float contentH = std::max(120.0f, contentBottom - contentTop);

            const float listW = 440.0f * s;
            const float leftX = margin;
            const float rightX = vw - margin - listW;
            const float panelY = contentTop;
            const float panelH = contentH;
            const float headerH = 52.0f * s * textBoost;
            const float pad = 16.0f * s * textBoost;
            const float entryH = 40.0f * s * textBoost;

            auto handleListClick = [&](auto& list, float x, float y, float w, float h, float& scrollVal, int& selected, int selIndex) {
                const float innerX = x + pad;
                const float innerY = y + headerH;
                const float innerW = w - pad * 2.0f;
                const float innerH = h - headerH - pad;
                const float barW = 10.0f * s;

                const int maxVisible = std::max(1, static_cast<int>(std::floor(innerH / entryH)));
                const float maxScroll = std::max(0.0f, static_cast<float>(list.size()) - static_cast<float>(maxVisible));
                scrollVal = std::clamp(scrollVal, 0.0f, maxScroll);
                const int start = (list.size() > static_cast<std::size_t>(maxVisible))
                                      ? std::clamp(static_cast<int>(std::round(scrollVal)), 0, static_cast<int>(list.size()) - maxVisible)
                                      : 0;
                const int end = std::min(static_cast<int>(list.size()), start + maxVisible);

                for (int i = start; i < end; ++i) {
                    float rowY = innerY + static_cast<float>(i - start) * entryH;
                    bool hovered = inside(mx, my, innerX, rowY, innerW - barW, entryH);
                    if (hovered) {
                        menuSelection_ = selIndex;
                        if (clickEdge) {
                            selected = i;
                            if (selIndex == 0) {
                                localLobbyHeroId_ = list[selected].id;
                                applyArchetypePreset();
                            } else if (selIndex == 1) {
                                applyDifficultyPreset();
                            }
                        }
                    }
                }
            };

            if (!archetypes_.empty()) {
                handleListClick(archetypes_, leftX, panelY, listW, panelH, archetypeScroll_, selectedArchetype_, 0);
            }
            if (!difficulties_.empty()) {
                handleListClick(difficulties_, rightX, panelY, listW, panelH, difficultyScroll_, selectedDifficulty_, 1);
            }

            const float btnW = 240.0f * s;
            const float btnH = 56.0f * s;
            const float btnGap = 18.0f * s;
            const float btnY = vh - footerH + 28.0f * s;
            const float startX = centerX - (btnW * 2.0f + btnGap) * 0.5f;
            if (inside(mx, my, startX, btnY, btnW, btnH)) {
                menuSelection_ = 2;
                if (clickEdge) { startNewGame(); return; }
            }
            if (inside(mx, my, startX + btnW + btnGap, btnY, btnW, btnH)) {
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
    render_->clear({0, 0, 0, 255});

    const float vw = static_cast<float>(viewportWidth_);
    const float vh = static_cast<float>(viewportHeight_);
    const float centerX = vw * 0.5f;
    const float topY = 140.0f;
    int mx = lastMouseX_;
    int my = lastMouseY_;
    auto inside = [](int mx, int my, float x, float y, float w, float h) {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    };

    // Main menu layout is designed around a 1920x1080 reference and scaled to the current viewport.
    const float refW = 1920.0f;
    const float refH = 1080.0f;
    const float s = std::min(vw / refW, vh / refH);
    const float margin = 26.0f * s;
    const float titleY = 72.0f * s;

    // Animated starfield background (flat black + moving white dots).
    {
        auto hash01 = [](uint32_t x) -> float {
            // 32-bit mix -> [0,1). Deterministic across platforms.
            x ^= x >> 16;
            x *= 0x7feb352du;
            x ^= x >> 15;
            x *= 0x846ca68bu;
            x ^= x >> 16;
            return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
        };

        const float t = static_cast<float>(menuPulse_);
        const float maxR = std::hypot(vw, vh) * 0.78f;
        const int baseCount = 320;
        const float areaScale = (vw * vh) / (1920.0f * 1080.0f);
        const int starCount = std::clamp(static_cast<int>(std::round(baseCount * std::clamp(areaScale, 0.6f, 2.2f))), 220, 720);
        const float twoPi = 6.2831853f;

        for (int i = 0; i < starCount; ++i) {
            const uint32_t idx = static_cast<uint32_t>(i + 1);
            const float depth = 0.15f + 0.85f * hash01(idx * 11u + 7u);  // 0..1, higher = brighter/larger
            const float r = std::sqrt(std::max(0.0f, hash01(idx * 5u + 17u))) * maxR;
            const float theta0 = twoPi * hash01(idx * 23u + 31u);
            const float omega = 0.010f + 0.018f * depth;  // rad/sec
            const float theta = theta0 + t * omega;

            const float x = centerX + std::cos(theta) * r;
            const float y = (vh * 0.5f) + std::sin(theta) * r;
            if (x < -6.0f || x > vw + 6.0f || y < -6.0f || y > vh + 6.0f) continue;

            const float tw = 0.65f + 0.35f * std::sin(t * (0.6f + 1.4f * hash01(idx * 3u + 101u)) + twoPi * hash01(idx * 97u + 13u));
            const float px = (depth > 0.72f) ? (2.0f * std::clamp(s, 0.8f, 1.2f)) : (1.0f * std::clamp(s, 0.8f, 1.2f));
            const float a = std::clamp((0.35f + 0.65f * depth) * tw, 0.15f, 1.0f);
            const uint8_t alpha = static_cast<uint8_t>(std::clamp(30.0f + 225.0f * a, 30.0f, 255.0f));
            render_->drawFilledRect(Engine::Vec2{x - px * 0.5f, y - px * 0.5f}, Engine::Vec2{px, px}, Engine::Color{255, 255, 255, alpha});
        }
    }

    // Title (top, centered).
    const std::string title = "PROJECT AURORA ZETA";
    const float titleScale = std::clamp(2.15f * s, 1.35f, 2.35f);
    const float titleW = measureTextUnified(title, titleScale).x;
    drawTextUnified(title, Engine::Vec2{centerX - titleW * 0.5f, titleY}, titleScale, Engine::Color{190, 235, 255, 245});

    // Build info (bottom-right).
    const std::string buildStr = "Pre-Alpha | Build v0.0.137";
    const float buildScale = std::clamp(0.95f * s, 0.72f, 0.95f);
    const Engine::Vec2 buildSz = measureTextUnified(buildStr, buildScale);
    drawTextUnified(buildStr, Engine::Vec2{vw - margin - buildSz.x, vh - margin - buildSz.y}, buildScale,
                    Engine::Color{145, 195, 230, 200});

    auto drawMainMenuButton = [&](const std::string& label, int index, float x, float y, float w, float h) {
        const bool focused = (menuPage_ == MenuPage::Main && menuSelection_ == index);
        const bool hovered = inside(mx, my, x, y, w, h);
        const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(menuPulse_) * 2.0f);
        const uint8_t base = static_cast<uint8_t>(focused ? (70 + 18 * pulse) : (hovered ? 62 : 48));
        Engine::Color bg{static_cast<uint8_t>(base), static_cast<uint8_t>(base + 20), static_cast<uint8_t>(base + 34), 230};
        // Shadow + body.
        render_->drawFilledRect(Engine::Vec2{x, y + 2.0f * s}, Engine::Vec2{w, h}, Engine::Color{0, 0, 0, 75});
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, bg);
        // Top/bottom lines.
        Engine::Color edge = focused ? Engine::Color{140, 220, 255, 235} : Engine::Color{45, 70, 95, 190};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, 2.0f}, edge);
        render_->drawFilledRect(Engine::Vec2{x, y + h - 2.0f}, Engine::Vec2{w, 2.0f}, edge);
        // Left accent bar.
        Engine::Color accent = focused ? Engine::Color{120, 200, 255, 235} : (hovered ? Engine::Color{90, 150, 220, 200}
                                                                                      : Engine::Color{40, 60, 90, 160});
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{6.0f * s, h}, accent);

        Engine::Color textCol = focused ? Engine::Color{225, 255, 255, 250} : Engine::Color{200, 225, 245, 235};
        const float textScale = std::clamp(1.05f * s, 0.9f, 1.12f);
        const float textY = y + (h - measureTextUnified(label, textScale).y) * 0.5f + 1.0f * s;
        drawTextUnified(label, Engine::Vec2{x + 22.0f * s, textY}, textScale, textCol);
    };

    if (menuPage_ == MenuPage::Main) {
        const int itemCount = 7;
        const float btnW = 420.0f * s;
        const float btnH = 54.0f * s;
        const float gap = 14.0f * s;
        const float titleArea = titleY + 140.0f * s;
        const float footerArea = 90.0f * s;
        const float totalH = itemCount * btnH + (itemCount - 1) * gap;
        const float availableH = std::max(0.0f, vh - titleArea - footerArea);
        const float startY = titleArea + (availableH - totalH) * 0.5f;
        const float btnX = centerX - btnW * 0.5f;

        // Button stack container.
        const float panelPadX = 34.0f * s;
        const float panelPadY = 34.0f * s;
        const float panelW = btnW + panelPadX * 2.0f;
        const float panelH = totalH + panelPadY * 2.0f;
        const float panelX = centerX - panelW * 0.5f;
        const float panelY = startY - panelPadY;
        render_->drawFilledRect(Engine::Vec2{panelX, panelY + 4.0f * s}, Engine::Vec2{panelW, panelH}, Engine::Color{0, 0, 0, 85});
        render_->drawFilledRect(Engine::Vec2{panelX, panelY}, Engine::Vec2{panelW, panelH}, Engine::Color{14, 18, 26, 215});
        render_->drawFilledRect(Engine::Vec2{panelX, panelY}, Engine::Vec2{panelW, 2.0f}, Engine::Color{45, 70, 95, 190});
        render_->drawFilledRect(Engine::Vec2{panelX, panelY + panelH - 2.0f}, Engine::Vec2{panelW, 2.0f}, Engine::Color{45, 70, 95, 190});

        // Buttons (centered).
        drawMainMenuButton("New Game (Solo)", 0, btnX, startY + 0 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Host",            1, btnX, startY + 1 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Join",            2, btnX, startY + 2 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Upgrades",        3, btnX, startY + 3 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Stats",           4, btnX, startY + 4 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Options",         5, btnX, startY + 5 * (btnH + gap), btnW, btnH);
        drawMainMenuButton("Exit",            6, btnX, startY + 6 * (btnH + gap), btnW, btnH);

        // Footer (bottom center).
        float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(menuPulse_) * 2.0f);
        Engine::Color hint{static_cast<uint8_t>(175 + 45 * pulse), 220, 255, 220};
        const std::string credit = "A Major Bonghit Production";
        const float creditScale = std::clamp(0.98f * s, 0.78f, 0.98f);
        const float creditW = measureTextUnified(credit, creditScale).x;
        const float creditsY = vh - margin - 34.0f * s;
        drawTextUnified(credit, Engine::Vec2{centerX - creditW * 0.5f, creditsY}, creditScale, hint);
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
        const float refW = 1920.0f;
        const float refH = 1080.0f;
        const float s = std::min(vw / refW, vh / refH);
        const float textBoost = 1.18f;
        const float margin = 28.0f * s;
        const float gap = 22.0f * s;

        // Page title (under the main game title).
        {
            const std::string pageTitle = "Select Character & Difficulty";
            const float st = std::clamp(1.25f * s * textBoost, 1.15f, 1.50f);
            const float tw = measureTextUnified(pageTitle, st).x;
            drawTextUnified(pageTitle, Engine::Vec2{centerX - tw * 0.5f, titleY + 84.0f * s}, st,
                            Engine::Color{200, 230, 255, 240});
            const std::string sub = "Click to select • Scroll lists • Enter to start • Esc to go back";
            const float ss = std::clamp(0.88f * s * textBoost, 0.82f, 1.12f);
            const float sw = measureTextUnified(sub, ss).x;
            drawTextUnified(sub, Engine::Vec2{centerX - sw * 0.5f, titleY + 114.0f * s}, ss,
                            Engine::Color{150, 190, 215, 210});
        }

        const float footerH = 120.0f * s;
        const float contentTop = titleY + 144.0f * s;
        const float contentBottom = vh - footerH;
        const float contentH = std::max(120.0f, contentBottom - contentTop);

        const float listW = 440.0f * s;
        const float leftX = margin;
        const float rightX = vw - margin - listW;
        const float panelY = contentTop;
        const float panelH = contentH;
        const float centerX0 = leftX + listW + gap;
        const float centerW = std::max(320.0f * s, rightX - gap - centerX0);

        const float headerH = 52.0f * s * textBoost;
        const float pad = 16.0f * s * textBoost;
        const float entryH = 40.0f * s * textBoost;

        int mx = lastMouseX_;
        int my = lastMouseY_;
        int scrollDelta = scrollDeltaFrame_;
        bool mouseDown = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT);

        auto drawCard = [&](float x, float y, float w, float h) {
            render_->drawFilledRect(Engine::Vec2{x, y + 4.0f * s}, Engine::Vec2{w, h}, Engine::Color{0, 0, 0, 85});
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, Engine::Color{12, 16, 24, 220});
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, 2.0f}, Engine::Color{45, 70, 95, 190});
            render_->drawFilledRect(Engine::Vec2{x, y + h - 2.0f}, Engine::Vec2{w, 2.0f}, Engine::Color{45, 70, 95, 190});
        };

        auto wrapByWidth = [&](const std::string& text, float maxWidth, float scale) {
            std::vector<std::string> lines;
            std::istringstream iss(text);
            std::string word;
            std::string current;
            while (iss >> word) {
                std::string next = current.empty() ? word : (current + " " + word);
                if (measureTextUnified(next, scale).x <= maxWidth || current.empty()) {
                    current = std::move(next);
                } else {
                    lines.push_back(current);
                    current = word;
                }
            }
            if (!current.empty()) lines.push_back(current);
            return lines;
        };

        auto drawScrollableList = [&](auto& list, int selected, bool focused, float x, float y, float w, float h,
                                      const std::string& title, float& scrollVal) {
            drawCard(x, y, w, h);
            const float titleScale = std::clamp(1.05f * s * textBoost, 0.98f, 1.28f);
            drawTextUnified(title, Engine::Vec2{x + pad, y + 14.0f * s}, titleScale, Engine::Color{210, 235, 255, 240});

            const float innerX = x + pad;
            const float innerY = y + headerH;
            const float innerW = w - pad * 2.0f;
            const float innerH = h - headerH - pad;

            const float listAreaX = innerX;
            const float listAreaY = innerY;
            const float listAreaW = innerW;
            const float listAreaH = innerH;

            const bool hoverPanel = (mx >= static_cast<int>(x) && mx <= static_cast<int>(x + w) &&
                                     my >= static_cast<int>(y) && my <= static_cast<int>(y + h));

            const int maxVisible = std::max(1, static_cast<int>(std::floor(listAreaH / entryH)));
            const float maxScroll = std::max(0.0f, static_cast<float>(list.size()) - static_cast<float>(maxVisible));
            if (hoverPanel && scrollDelta != 0) {
                scrollVal = std::clamp(scrollVal - static_cast<float>(scrollDelta) * 0.65f, 0.0f, maxScroll);
            }

            float barW = 10.0f * s;
            float trackX = listAreaX + listAreaW - barW;
            float trackY = listAreaY;
            float trackH = listAreaH;
            if (hoverPanel && mouseDown && list.size() > static_cast<std::size_t>(maxVisible)) {
                if (mx >= static_cast<int>(trackX) && mx <= static_cast<int>(trackX + barW)) {
                    float t = (static_cast<float>(my) - trackY) / std::max(1.0f, trackH);
                    t = std::clamp(t, 0.0f, 1.0f);
                    scrollVal = t * maxScroll;
                }
            }

            const int start = (list.size() > static_cast<std::size_t>(maxVisible))
                                  ? std::clamp(static_cast<int>(std::round(scrollVal)), 0, static_cast<int>(list.size()) - maxVisible)
                                  : 0;
            const int end = std::min(static_cast<int>(list.size()), start + maxVisible);

            // entries
            for (int i = start; i < end; ++i) {
                float rowY = listAreaY + static_cast<float>(i - start) * entryH;
                const bool hov = (mx >= static_cast<int>(listAreaX) && mx <= static_cast<int>(listAreaX + listAreaW) &&
                                  my >= static_cast<int>(rowY) && my <= static_cast<int>(rowY + entryH));
                const bool sel = (i == selected);

                Engine::Color bg = sel ? Engine::Color{28, 44, 66, 235} : (hov ? Engine::Color{20, 30, 46, 230}
                                                                            : Engine::Color{14, 18, 26, 210});
                render_->drawFilledRect(Engine::Vec2{listAreaX, rowY}, Engine::Vec2{listAreaW, entryH - 2.0f * s}, bg);

                Engine::Color accent = sel ? Engine::Color{120, 200, 255, 235} : (hov ? Engine::Color{80, 140, 210, 200}
                                                                                      : Engine::Color{35, 55, 80, 160});
                render_->drawFilledRect(Engine::Vec2{listAreaX, rowY}, Engine::Vec2{5.0f * s, entryH - 2.0f * s}, accent);

                Engine::Color textCol = sel ? Engine::Color{235, 255, 255, 250} : Engine::Color{200, 225, 245, 235};
                const float textScale = std::clamp(0.98f * s * textBoost, 0.90f, 1.18f);
                const float maxNameW = listAreaW - 24.0f * s - barW;
                const std::string name = ellipsizeTextUnified(list[i].name, maxNameW, textScale);
                const float textY = rowY + (entryH - measureTextUnified(name, textScale).y) * 0.5f + 1.0f * s;
                drawTextUnified(name, Engine::Vec2{listAreaX + 14.0f * s, textY}, textScale, textCol);
            }

            // scrollbar visuals
            if (list.size() > static_cast<std::size_t>(maxVisible)) {
                render_->drawFilledRect(Engine::Vec2{trackX, trackY}, Engine::Vec2{barW, trackH}, Engine::Color{10, 14, 22, 170});
                const float thumbH = std::max(22.0f * s, trackH * (static_cast<float>(maxVisible) / static_cast<float>(list.size())));
                const float ratio = (maxScroll > 0.0f) ? (scrollVal / maxScroll) : 0.0f;
                const float thumbY = trackY + (trackH - thumbH) * ratio;
                render_->drawFilledRect(Engine::Vec2{trackX, thumbY}, Engine::Vec2{barW, thumbH}, Engine::Color{90, 140, 210, 220});
            }

            // focused glow
            if (focused) {
                render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, 2.0f}, Engine::Color{120, 200, 255, 210});
                render_->drawFilledRect(Engine::Vec2{x, y + h - 2.0f}, Engine::Vec2{w, 2.0f}, Engine::Color{120, 200, 255, 210});
            }
        };

        // Left/right selector lists.
        if (!archetypes_.empty()) {
            drawScrollableList(archetypes_, selectedArchetype_, menuSelection_ == 0, leftX, panelY, listW, panelH,
                               "Champions", archetypeScroll_);
        } else {
            drawCard(leftX, panelY, listW, panelH);
        }
        if (!difficulties_.empty()) {
            drawScrollableList(difficulties_, selectedDifficulty_, menuSelection_ == 1, rightX, panelY, listW, panelH,
                               "Difficulty", difficultyScroll_);
        } else {
            drawCard(rightX, panelY, listW, panelH);
        }

        // Center: animated preview + details.
        drawCard(centerX0, panelY, centerW, panelH);
        const float previewH = panelH * 0.52f;
        render_->drawFilledRect(Engine::Vec2{centerX0 + 2.0f, panelY + 2.0f}, Engine::Vec2{centerW - 4.0f, previewH - 4.0f},
                                Engine::Color{18, 26, 40, 170});

        if (!archetypes_.empty()) {
            const auto& a = archetypes_[std::clamp(selectedArchetype_, 0, static_cast<int>(archetypes_.size() - 1))];

            // Render selected archetype idle preview (animated) inside the center card.
            if (textureManager_) {
                auto files = heroSpriteFilesFor(a);
                std::filesystem::path base = std::filesystem::path("assets") / "Sprites" / "Characters" / files.folder;
                Engine::TexturePtr move = loadTextureOptional((base / files.movementFile).string());
                if (move) {
                    const int movementColumns = 4;
                    const int movementRows = (move->height() % 31 == 0) ? 31 : std::max(1, move->height() / (move->height() / std::max(1, move->width() / movementColumns)));
                    int moveFrameW = std::max(1, move->width() / movementColumns);
                    int moveFrameH = std::max(1, move->height() / movementRows);
                    int frames = std::max(1, move->width() / moveFrameW);
                    float frameDur = 0.12f;
                    int row = 2;  // idle front (0-based)
                    int frame = static_cast<int>(menuPulse_ / frameDur) % frames;
                    Engine::IntRect src{frame * moveFrameW, row * moveFrameH, moveFrameW, moveFrameH};

                    // Increase preview size by ~200% and keep it centered within the preview box (with a safety clamp).
                    const float desiredScale = 2.35f * s * 2.0f;
                    const float safePad = 18.0f * s;
                    const float maxScaleX = (centerW - safePad * 2.0f) / std::max(1.0f, static_cast<float>(moveFrameW));
                    const float maxScaleY = (previewH - safePad * 2.0f) / std::max(1.0f, static_cast<float>(moveFrameH));
                    float scale = std::min(desiredScale, std::min(maxScaleX, maxScaleY));
                    float visW = static_cast<float>(moveFrameW) * scale;
                    float visH = static_cast<float>(moveFrameH) * scale;

                    // Character Select live preview placement (manual tweak point):
                    // Adjust these offsets to move the animated hero preview around in the center preview card.
                    constexpr float kPreviewOffsetX = 0.0f;  // +right
                    constexpr float kPreviewOffsetY = 0.0f;  // +down
                    float px = centerX0 + centerW * 0.5f - visW * 0.5f + kPreviewOffsetX * s;
                    float py = panelY + previewH * 0.5f - visH * 0.5f + kPreviewOffsetY * s;
                    if (a.id == "tank" || a.id == "special") {
                        px -= 10.0f * s;
                        py += 10.0f * s;
                    }
                    render_->drawTextureRegion(*move, Engine::Vec2{px, py}, Engine::Vec2{visW, visH}, src);
                }
            }

            // Details section.
            const float detailsX = centerX0 + pad;
            float detailsY = panelY + previewH + 12.0f * s;
            const float detailsW = centerW - pad * 2.0f;

            const float nameScale = std::clamp(1.25f * s * textBoost, 1.15f, 1.55f);
            const std::string name = a.name;
            drawTextUnified(name, Engine::Vec2{detailsX, detailsY}, nameScale, Engine::Color{225, 250, 255, 245});

            // Difficulty badge (right side).
            std::string diffName{};
            std::string diffDesc{};
            if (!difficulties_.empty()) {
                const auto& d = difficulties_[std::clamp(selectedDifficulty_, 0, static_cast<int>(difficulties_.size() - 1))];
                diffName = d.name;
                diffDesc = d.description;
            }
            if (!diffName.empty()) {
                const float badgeScale = std::clamp(0.88f * s * textBoost, 0.82f, 1.10f);
                const float badgePadX = 10.0f * s;
                const float badgePadY = 6.0f * s;
                const Engine::Vec2 bsz = measureTextUnified(diffName, badgeScale);
                const float bw = bsz.x + badgePadX * 2.0f;
                const float bh = bsz.y + badgePadY * 2.0f;
                const float bx = detailsX + detailsW - bw;
                const float by = detailsY - 2.0f * s;
                render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{bw, bh}, Engine::Color{20, 30, 46, 235});
                render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{bw, 2.0f}, Engine::Color{120, 200, 255, 210});
                drawTextUnified(diffName, Engine::Vec2{bx + badgePadX, by + badgePadY}, badgeScale, Engine::Color{200, 230, 255, 235});
            }

            detailsY += 28.0f * s;
            {
                std::ostringstream stats;
                stats << "HP x" << std::fixed << std::setprecision(2) << a.hpMul
                      << "  •  Speed x" << a.speedMul << "  •  Damage x" << a.damageMul;
                drawTextUnified(stats.str(), Engine::Vec2{detailsX, detailsY}, std::clamp(0.92f * s * textBoost, 0.86f, 1.15f),
                                Engine::Color{175, 215, 240, 230});
                detailsY += 22.0f * s;
            }

            // Two-column: bio + difficulty on the left; attributes on the right.
            const float colGap = 18.0f * s;
            const float rightColW = 170.0f * s;
            const float leftColW = std::max(160.0f * s, detailsW - rightColW - colGap);
            const float leftColX = detailsX;
            const float rightColX = detailsX + leftColW + colGap;

            const float bodyScale = std::clamp(0.88f * s * textBoost, 0.82f, 1.10f);
            auto drawParagraph = [&](const std::string& text, Engine::Color col, float maxW, int maxLines) {
                auto lines = wrapByWidth(text, maxW, bodyScale);
                int drawn = 0;
                for (const auto& ln : lines) {
                    if (drawn >= maxLines) break;
                    drawTextUnified(ln, Engine::Vec2{leftColX, detailsY}, bodyScale, col);
                    detailsY += 20.0f * s;
                    ++drawn;
                }
                if (drawn > 0) detailsY += 6.0f * s;
            };

            // Description + bio
            if (!a.description.empty()) {
                drawParagraph(a.description, Engine::Color{200, 230, 255, 225}, leftColW, 2);
            }
            if (!a.biography.empty()) {
                drawParagraph(a.biography, Engine::Color{185, 210, 235, 220}, leftColW, 2);
            }
            // Difficulty description
            if (!diffDesc.empty()) {
                drawParagraph(diffDesc, Engine::Color{200, 220, 255, 215}, leftColW, 2);
            }
            // Specialties / perks as compact chips
            float chipY = detailsY;
                auto drawChips = [&](const std::vector<std::string>& chips, Engine::Color chipCol, const std::string& label) {
                    if (chips.empty()) return;
                    drawTextUnified(label, Engine::Vec2{leftColX, chipY}, bodyScale, Engine::Color{165, 195, 220, 210});
                    float cx = leftColX + measureTextUnified(label, bodyScale).x + 10.0f * s;
                    float cy = chipY - 3.0f * s;
                    float chipH = 22.0f * s;
                    for (const auto& chip : chips) {
                        const float cs = std::clamp(0.84f * s * textBoost, 0.78f, 1.02f);
                        Engine::Vec2 ts = measureTextUnified(chip, cs);
                        float cw = ts.x + 14.0f * s;
                    if (cx + cw > leftColX + leftColW) {
                        cx = leftColX;
                        cy += chipH + 8.0f * s;
                    }
                    render_->drawFilledRect(Engine::Vec2{cx, cy}, Engine::Vec2{cw, chipH}, Engine::Color{18, 26, 40, 210});
                    render_->drawFilledRect(Engine::Vec2{cx, cy}, Engine::Vec2{cw, 2.0f}, chipCol);
                    drawTextUnified(chip, Engine::Vec2{cx + 7.0f * s, cy + 4.0f * s}, cs, Engine::Color{210, 235, 255, 235});
                    cx += cw + 8.0f * s;
                }
                chipY = cy + chipH + 10.0f * s;
            };
            drawChips(a.specialties, Engine::Color{120, 200, 255, 210}, "Specialties");
            if (!a.perks.empty()) {
                std::vector<std::string> perkOne{a.perks.front()};
                drawChips(perkOne, Engine::Color{140, 230, 180, 210}, "Perk");
            }

            // Attributes box (right)
            float ax = rightColX;
            float ay = panelY + previewH + 22.0f * s;
            render_->drawFilledRect(Engine::Vec2{ax, ay}, Engine::Vec2{rightColW, 142.0f * s}, Engine::Color{10, 14, 22, 170});
            render_->drawFilledRect(Engine::Vec2{ax, ay}, Engine::Vec2{rightColW, 2.0f}, Engine::Color{45, 70, 95, 190});
            drawTextUnified("Attributes", Engine::Vec2{ax + 10.0f * s, ay + 10.0f * s}, std::clamp(0.92f * s * textBoost, 0.86f, 1.15f),
                            Engine::Color{180, 210, 235, 225});
            ay += 36.0f * s;
            auto attrLine = [&](const std::string& label, int val) {
                drawTextUnified(label, Engine::Vec2{ax + 12.0f * s, ay}, bodyScale, Engine::Color{170, 195, 220, 220});
                drawTextUnified(std::to_string(val), Engine::Vec2{ax + rightColW - 38.0f * s, ay}, bodyScale,
                                Engine::Color{220, 240, 255, 240});
                ay += 18.0f * s;
            };
            attrLine("STR", a.rpgAttributes.STR);
            attrLine("DEX", a.rpgAttributes.DEX);
            attrLine("INT", a.rpgAttributes.INT);
            attrLine("END", a.rpgAttributes.END);
            attrLine("LCK", a.rpgAttributes.LCK);
        }

        // Bottom action bar.
        const float btnW = 240.0f * s;
        const float btnH = 56.0f * s;
        const float btnGap = 18.0f * s;
        const float btnY = vh - footerH + 28.0f * s;
        auto drawAction = [&](const std::string& label, int index, float x) {
            const bool focused = (menuSelection_ == index);
            const bool hovered = inside(mx, my, x, btnY, btnW, btnH);
            Engine::Color bg = focused ? Engine::Color{32, 52, 76, 235} : (hovered ? Engine::Color{26, 40, 60, 230}
                                                                                 : Engine::Color{16, 22, 32, 220});
            render_->drawFilledRect(Engine::Vec2{x, btnY + 3.0f * s}, Engine::Vec2{btnW, btnH}, Engine::Color{0, 0, 0, 80});
            render_->drawFilledRect(Engine::Vec2{x, btnY}, Engine::Vec2{btnW, btnH}, bg);
            Engine::Color edge = focused ? Engine::Color{120, 200, 255, 230} : Engine::Color{45, 70, 95, 190};
            render_->drawFilledRect(Engine::Vec2{x, btnY}, Engine::Vec2{btnW, 2.0f}, edge);
            render_->drawFilledRect(Engine::Vec2{x, btnY + btnH - 2.0f}, Engine::Vec2{btnW, 2.0f}, edge);
            const float ts = std::clamp(1.02f * s, 0.92f, 1.02f);
            const float tw = measureTextUnified(label, ts).x;
            const float th = measureTextUnified(label, ts).y;
            drawTextUnified(label, Engine::Vec2{x + (btnW - tw) * 0.5f, btnY + (btnH - th) * 0.5f + 2.0f * s}, ts,
                            Engine::Color{220, 245, 255, 245});
        };
        const float startX = centerX - (btnW * 2.0f + btnGap) * 0.5f;
        drawAction("Start Run", 2, startX);
        drawAction("Back", 3, startX + btnW + btnGap);

        Engine::Color hint{150, 190, 215, 210};
        const std::string hintStr = "A/D switch focus • W/S change selection • Enter start • Esc back";
        const float hs = std::clamp(0.86f * s * textBoost, 0.80f, 1.08f);
        const float hw = measureTextUnified(hintStr, hs).x;
        drawTextUnified(hintStr, Engine::Vec2{centerX - hw * 0.5f, vh - 26.0f * s}, hs, hint);
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

Engine::Vec2 GameRoot::measureTextUnified(const std::string& text, float scale) const {
    if (hasTTF() && uiFont_) {
        int w = 0;
        int h = 0;
        if (TTF_SizeUTF8(uiFont_, text.c_str(), &w, &h) == 0) {
            return Engine::Vec2{static_cast<float>(w) * scale, static_cast<float>(h) * scale};
        }
    }
    if (debugText_) {
        return debugText_->measureText(text, scale);
    }
    return Engine::Vec2{0.0f, 0.0f};
}

std::string GameRoot::ellipsizeTextUnified(const std::string& text, float maxWidth, float scale) const {
    if (text.empty()) return text;
    if (maxWidth <= 0.0f) return std::string{};
    if (measureTextUnified(text, scale).x <= maxWidth) return text;
    static constexpr const char* kEllipsis = "...";
    if (measureTextUnified(kEllipsis, scale).x > maxWidth) return std::string{};

    // Binary search for the longest prefix that fits with ellipsis.
    std::size_t lo = 0;
    std::size_t hi = text.size();
    while (lo + 1 < hi) {
        std::size_t mid = (lo + hi) / 2;
        std::string candidate = text.substr(0, mid) + kEllipsis;
        if (measureTextUnified(candidate, scale).x <= maxWidth) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return text.substr(0, lo) + kEllipsis;
}

void GameRoot::loadMenuPresets() {
    archetypes_.clear();
    difficulties_.clear();
    auto bios = Game::RPG::loadArchetypeBios("data/rpg/archetypes.json");

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
                if (archetypeSupportsSecondaryWeapon(def)) {
                    def.offensiveType = Game::OffensiveType::Melee;  // base offense; bow only when swapped
                }
                if (isMeleeOnlyArchetype(def.id)) {
                    def.offensiveType = Game::OffensiveType::Melee;
                }
                auto bioIt = bios.find(def.id);
                if (bioIt != bios.end()) {
                    def.rpgAttributes = bioIt->second.attributes;
                    def.specialties = bioIt->second.specialties;
                    def.perks = bioIt->second.perks;
                    def.biography = bioIt->second.biography;
                }
                if (def.biography.empty()) {
                    def.biography = def.description;
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
                            float spd, float dmg, Engine::Color col, Game::OffensiveType offense) {
        ArchetypeDef def{id, name, desc, hp, spd, dmg, col, "", offense,
                         Engine::Gameplay::RPG::Attributes{}, {}, {}, desc};
        auto bioIt = bios.find(id);
        if (bioIt != bios.end()) {
            def.rpgAttributes = bioIt->second.attributes;
            def.specialties = bioIt->second.specialties;
            def.perks = bioIt->second.perks;
            def.biography = bioIt->second.biography.empty() ? desc : bioIt->second.biography;
        } else {
            // Reasonable defaults if bios missing.
            def.rpgAttributes = Engine::Gameplay::RPG::Attributes{3, 3, 2, 3, 1};
            def.specialties = {"Generalist"};
            def.perks = {};
            def.biography = desc;
        }
        archetypes_.push_back(def);
    };
    if (archetypes_.empty()) {
        addArchetype("tank", "Jack", "High survivability frontliner with slower stride.", 1.35f, 0.9f, 0.9f,
                     Engine::Color{110, 190, 255, 255}, Game::OffensiveType::Melee);
        addArchetype("healer", "Sally", "Supportive sustain, balanced speed, lighter offense.", 1.05f, 1.0f, 0.95f,
                     Engine::Color{170, 220, 150, 255}, Game::OffensiveType::Ranged);
        addArchetype("damage", "Robin", "Can swap between Melee and Ranged using (Alt)", 0.95f, 1.05f, 1.15f,
                     Engine::Color{255, 180, 120, 255}, Game::OffensiveType::Melee);
        addArchetype("assassin", "María", "Very quick and lethal but fragile.", 0.85f, 1.25f, 1.2f,
                     Engine::Color{255, 110, 180, 255}, Game::OffensiveType::Melee);
        addArchetype("builder", "George", "Sturdier utility specialist with slower advance.", 1.1f, 0.95f, 0.9f,
                     Engine::Color{200, 200, 120, 255}, Game::OffensiveType::Builder);
        addArchetype("support", "Gunther", "Equipped with a long-range polearm.", 1.0f, 1.0f, 1.0f,
                     Engine::Color{150, 210, 230, 255}, Game::OffensiveType::Melee);
        addArchetype("special", "Ellis", "Experimental kit with slight boosts across the board.", 1.1f, 1.05f, 1.05f,
                     Engine::Color{200, 160, 240, 255}, Game::OffensiveType::Melee);
        addArchetype("summoner", "Sage", "Calls beetles and snakes; fragile but protected by summons.", 0.95f, 1.0f, 0.9f,
                     Engine::Color{170, 140, 230, 255}, Game::OffensiveType::Ranged);
        addArchetype("druid", "Raven", "Shifts between human ranged and melee beast forms.", 1.1f, 1.0f, 1.0f,
                     Engine::Color{140, 200, 120, 255}, Game::OffensiveType::Ranged);
        addArchetype("wizard", "Jade", "Elementalist with cycling primary and high-energy spell kit.", 0.85f, 1.05f, 1.1f,
                     Engine::Color{120, 190, 255, 255}, Game::OffensiveType::Ranged);
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
                remoteFireCooldown_.erase(rid);
                it = remoteEntities_.erase(it);
            } else {
                ++it;
            }
        }
    // Fallback revive during lobby/match transitions.
    if (multiplayerEnabled_ && reviveNextRound_ && !inCombat_) {
        reviveLocalPlayer();
    }
    refreshPauseState();
}

Game::Net::SnapshotMsg GameRoot::collectNetSnapshot() {
    Game::Net::SnapshotMsg snap{};
    auto makeState = [&](uint32_t id, Engine::ECS::Entity ent, const Game::Net::PlayerNetState* src) {
        Game::Net::PlayerNetState s{};
        s.id = id;
        s.heroId = src ? src->heroId : activeArchetype_.id;
        s.level = src ? src->level : level_;
        s.wave = wave_;
        if (const auto* tf = registry_.get<Engine::ECS::Transform>(ent)) {
            s.x = tf->position.x;
            s.y = tf->position.y;
        }
        if (const auto* look = registry_.get<Game::LookDirection>(ent)) {
            s.lookDir = static_cast<uint8_t>(look->dir);
        }
        if (const auto* atk = registry_.get<Game::HeroAttackAnim>(ent)) {
            s.attacking = atk->timer > 0.0f ? 1 : 0;
            s.attackTimer = std::max(0.0f, atk->timer);
        }
        if (const auto* hp = registry_.get<Engine::ECS::Health>(ent)) {
            s.hp = hp->currentHealth;
            s.shields = hp->currentShields;
            s.alive = hp->currentHealth > 0.0f;
        } else {
            s.hp = 0.0f;
            s.shields = 0.0f;
            s.alive = false;
        }
        return s;
    };

    // Always include our own state.
    uint32_t selfId = localPlayerId_ == 0 ? 1 : localPlayerId_;
    Game::Net::PlayerNetState self = makeState(selfId, hero_, nullptr);
    self.heroId = activeArchetype_.id;
    snap.players.push_back(self);

    // Host: populate snapshot from server-authoritative remote entities.
    if (multiplayerEnabled_ && netSession_ && netSession_->isHost()) {
        for (const auto& [pid, ent] : remoteEntities_) {
            if (pid == selfId || ent == Engine::ECS::kInvalidEntity) continue;
            const auto existing = remoteStates_.find(pid);
            const Game::Net::PlayerNetState* src = existing == remoteStates_.end() ? nullptr : &existing->second;
            Game::Net::PlayerNetState s = makeState(pid, ent, src);
            // Keep remoteStates_ mirrored to authoritative values for downstream logic.
            remoteStates_[pid] = s;
            snap.players.push_back(s);
        }
    } else {
        // Client: only send our own state to host; no need to echo others back.
        // (remoteStates_ holds host snapshots; re-broadcasting them is redundant and can desync.)
    }

    // Host shares enemy state to keep clients in sync.
    if (multiplayerEnabled_ && netSession_ && netSession_->isHost()) {
        registry_.view<Engine::ECS::EnemyTag, Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::SpriteAnimation>(
            [&](Engine::ECS::Entity e, const Engine::ECS::EnemyTag&, const Engine::ECS::Transform& tf, const Engine::ECS::Health& hp, const Engine::ECS::SpriteAnimation& anim) {
                Game::Net::EnemyNetState es{};
                es.id = static_cast<uint32_t>(e);
                es.x = tf.position.x;
                es.y = tf.position.y;
                es.hp = hp.currentHealth;
                es.shields = hp.currentShields;
                es.alive = hp.alive();
                es.frameWidth = anim.frameWidth;
                es.frameHeight = anim.frameHeight;
                es.frameDuration = anim.frameDuration;
                // Attempt to include enemy definition id by matching texture pointer.
                if (auto* rend = registry_.get<Engine::ECS::Renderable>(e)) {
                    for (const auto& def : enemyDefs_) {
                        if (def.texture && rend->texture && def.texture.get() == rend->texture.get()) {
                            es.defId = def.id;
                            break;
                        }
                    }
                }
                snap.enemies.push_back(es);
            });
    }
    snap.wave = wave_;
    return snap;
}

void GameRoot::applyRemoteSnapshot(const Game::Net::SnapshotMsg& snap) {
    // Client-side: detect host restart (wave number dropped) and fast-reset local run state.
    if (!netSession_->isHost()) {
        if (snap.wave < lastSnapshotWave_) {
            Engine::logInfo("Host restarted run; resetting local state.");
            resetRun();
            forceHealAfterReset_ = true;
        }
    }

    // Detect host-reported wave advancement to trigger client-side revive scheduling.
    if (!netSession_->isHost()) {
        int hostWave = snap.wave;
        if (hostWave > lastSnapshotWave_) {
            wave_ = hostWave;
            if (reviveNextRound_) {
                reviveLocalPlayer();
            }
        }
    }

    // Synchronize enemies from host snapshots on clients.
    if (multiplayerEnabled_ && netSession_ && !netSession_->isHost()) {
        std::unordered_set<uint32_t> seen;
        for (const auto& es : snap.enemies) {
            seen.insert(es.id);
            Engine::ECS::Entity ent = Engine::ECS::kInvalidEntity;
            auto it = remoteEnemyEntities_.find(es.id);
            if (it != remoteEnemyEntities_.end()) {
                ent = it->second;
            } else {
                ent = registry_.create();
                registry_.emplace<Engine::ECS::EnemyTag>(ent, Engine::ECS::EnemyTag{});
                registry_.emplace<Engine::ECS::Transform>(ent, Engine::Vec2(es.x, es.y));
                registry_.emplace<Engine::ECS::Velocity>(ent, Engine::Vec2{0.0f, 0.0f});
                float size = waveSettingsBase_.enemySize;
                Engine::TexturePtr tex{};
                if (!es.defId.empty()) {
                    for (const auto& def : enemyDefs_) {
                        if (def.id == es.defId && def.texture) {
                            tex = def.texture;
                            size *= def.sizeMultiplier;
                            break;
                        }
                    }
                }
                registry_.emplace<Engine::ECS::Renderable>(ent,
                                                           Engine::ECS::Renderable{Engine::Vec2{size, size},
                                                                                   Engine::Color{255, 255, 255, 255},
                                                                                   tex});
                registry_.emplace<Engine::ECS::AABB>(ent, Engine::ECS::AABB{Engine::Vec2{size * 0.5f, size * 0.5f}});
                registry_.emplace<Engine::ECS::Health>(ent, Engine::ECS::Health{std::max(1.0f, es.hp), es.hp});
                Engine::ECS::SpriteAnimation anim{};
                anim.frameWidth = es.frameWidth;
                anim.frameHeight = es.frameHeight;
                anim.frameCount = std::max(1, (tex && es.frameWidth > 0) ? tex->width() / es.frameWidth : 1);
                anim.frameDuration = es.frameDuration;
                anim.row = 0;
                registry_.emplace<Engine::ECS::SpriteAnimation>(ent, anim);
                remoteEnemyEntities_[es.id] = ent;
            }
            if (auto* tf = registry_.get<Engine::ECS::Transform>(ent)) {
                tf->position = Engine::Vec2(es.x, es.y);
            }
            if (auto* hp = registry_.get<Engine::ECS::Health>(ent)) {
                hp->currentHealth = es.hp;
                hp->currentShields = es.shields;
                hp->maxHealth = std::max(hp->maxHealth, es.hp);
                hp->maxShields = std::max(hp->maxShields, es.shields);
            }
            // Approximate velocity for animation/movement direction.
            if (auto* vel = registry_.get<Engine::ECS::Velocity>(ent)) {
                Engine::Vec2 prevPos = tfPrevEnemyPos_[es.id];
                float invDt = 20.0f;  // assume 50ms snapshot spacing
                vel->value = {(es.x - prevPos.x) * invDt, (es.y - prevPos.y) * invDt};
                tfPrevEnemyPos_[es.id] = Engine::Vec2(es.x, es.y);
            }
        }
        // Remove enemies not present in latest snapshot.
        for (auto it = remoteEnemyEntities_.begin(); it != remoteEnemyEntities_.end();) {
            if (seen.find(it->first) == seen.end()) {
                registry_.destroy(it->second);
                it = remoteEnemyEntities_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto& p : snap.players) {
        const bool isLocal = (p.id == localPlayerId_);
        if (netSession_->isHost()) {
            // Host treats incoming player snapshots as movement/hero metadata only; health is authoritative server-side.
            auto& entry = remoteStates_[p.id];
            entry.id = p.id;
            entry.heroId = p.heroId;
            entry.level = p.level;
            entry.wave = wave_;
            entry.alive = p.alive;  // use reported alive for intent; will be overwritten with host health below.
            if (!isLocal) {
                entry.x = p.x;
                entry.y = p.y;
            }
        } else {
            remoteStates_[p.id] = p;
        }

        if (!netSession_->isHost() && isLocal) {
            // Client applies host-authoritative health/shield/alive state to its own hero.
            float prevHp = -1.0f;
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            prevHp = hp->currentHealth;
            hp->maxHealth = std::max(hp->maxHealth, p.hp);
            hp->currentHealth = std::max(0.0f, p.hp);
            hp->maxShields = std::max(hp->maxShields, p.shields);
            hp->currentShields = std::max(0.0f, p.shields);
        }
        // Do NOT override local player's look/attack state from host; client drives their own facing/attacks.
            if (!registry_.has<Game::Ghost>(hero_)) {
                registry_.emplace<Game::Ghost>(hero_, Game::Ghost{false});
            }
            if (auto* ghost = registry_.get<Game::Ghost>(hero_)) {
                ghost->active = !p.alive;
            }
            // Record host-reported HP to drive damage shake only on genuine host damage.
            lastSnapshotHeroHp_ = p.hp;
            if (prevHp >= 0.0f && p.hp < prevHp - 0.01f) {
                pendingNetShake_ = true;
            }
            continue;
        }
        if (isLocal) continue;
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
            const ArchetypeDef* def = findArchetypeById(p.heroId);
            // Mirror archetype-derived base stats for remote players on the host so their shields/Hp are sane.
            float hpPreset = heroMaxHpBase_ * (def ? def->hpMul : 1.0f) * globalModifiers_.playerHealthMult;
            float shieldPreset = heroShieldBase_;
            Game::OffensiveType remoteOffense = def ? def->offensiveType : Game::OffensiveType::Melee;
            if (archetypeSupportsSecondaryWeapon(def ? *def : activeArchetype_)) {
                remoteOffense = Game::OffensiveType::Melee;
            }
            if (remoteOffense == Game::OffensiveType::Melee) {
                shieldPreset += 100.0f;
                shieldPreset += meleeShieldBonus_ * (def ? def->hpMul : 1.0f) * globalModifiers_.playerShieldsMult;
            }
            shieldPreset *= globalModifiers_.playerShieldsMult;
            float spawnHp = (p.hp > 0.1f) ? p.hp : hpPreset;
            float spawnShields = (p.shields > 0.0f) ? p.shields : shieldPreset;
            registry_.emplace<Engine::ECS::Health>(ent, Engine::ECS::Health{hpPreset, std::max(1.0f, spawnHp)});
            if (auto* hp = registry_.get<Engine::ECS::Health>(ent)) {
                hp->maxShields = std::max(shieldPreset, spawnShields);
                hp->currentShields = spawnShields;
                hp->healthArmor = heroBaseStatsScaled_.baseHealthArmor;
                hp->shieldArmor = heroBaseStatsScaled_.baseShieldArmor;
                hp->shieldRegenRate = heroShieldRegenBase_;
                hp->healthRegenRate = heroHealthRegenBase_;
                hp->regenDelay = heroRegenDelayBase_;
            }
            // Brief spawn grace so they don't take contact damage before their first real position update.
            registry_.emplace<Game::Invulnerable>(ent, Game::Invulnerable{1.5f});
            registry_.emplace<Engine::ECS::SpriteAnimation>(ent, Engine::ECS::SpriteAnimation{});
            registry_.emplace<Engine::ECS::HeroTag>(ent, Engine::ECS::HeroTag{});
            registry_.emplace<Game::Facing>(ent, Game::Facing{1});
            registry_.emplace<Game::LookDirection>(ent, Game::LookDirection{Game::LookDir4::Front});
            if (!registry_.has<Game::Ghost>(ent)) {
                registry_.emplace<Game::Ghost>(ent, Game::Ghost{false});
            }
            setupHeroVisualForEntity(ent, def ? *def : activeArchetype_);
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
            if (!netSession_->isHost()) {
                hp->maxHealth = std::max(hp->maxHealth, p.hp);
                if (p.hp > 0.01f) {
                    hp->currentHealth = p.hp;
                }
                hp->maxShields = std::max(hp->maxShields, p.shields);
                hp->currentShields = p.shields;
                if (p.alive) {
                    hp->currentHealth = std::max(hp->currentHealth, 1.0f);
                }
            } else {
                // Host keeps authoritative health; mirror into remoteStates_ for UI/logic.
                auto& entry = remoteStates_[p.id];
                entry.hp = hp->currentHealth;
                entry.shields = hp->currentShields;
                entry.alive = hp->alive();
            }
        }
        if (auto* look = registry_.get<Game::LookDirection>(ent)) {
            look->dir = static_cast<Game::LookDir4>(std::clamp<int>(p.lookDir, 0, 3));
        }
        const float incomingAttackT = std::max(0.0f, p.attackTimer);
        if (incomingAttackT > 0.0f || p.attacking) {
            if (auto* atk = registry_.get<Game::HeroAttackAnim>(ent)) {
                atk->timer = std::max(atk->timer, incomingAttackT > 0.0f ? incomingAttackT : 0.2f);
            } else {
                registry_.emplace<Game::HeroAttackAnim>(ent, Game::HeroAttackAnim{
                    incomingAttackT > 0.0f ? incomingAttackT : 0.2f,
                    {0.0f, 0.0f},
                    0});
            }
        } else if (auto* atk = registry_.get<Game::HeroAttackAnim>(ent)) {
            if (atk->timer <= 0.0f) {
                atk->timer = 0.0f;
            }
        }
        // Sync ghost/knockdown visual state for remote players.
        if (!registry_.has<Game::Ghost>(ent)) {
            registry_.emplace<Game::Ghost>(ent, Game::Ghost{false});
        }
        if (auto* ghost = registry_.get<Game::Ghost>(ent)) {
            if (netSession_->isHost()) {
                const auto* hpHost = registry_.get<Engine::ECS::Health>(ent);
                ghost->active = hpHost ? !hpHost->alive() : false;
            } else {
                ghost->active = !p.alive;
            }
        }
        // Keep HP at zero for downed ghosts so revive timing is driven by host round progression.
        if (!netSession_->isHost()) {
            if (auto* hp = registry_.get<Engine::ECS::Health>(ent)) {
                if (!p.alive) {
                    hp->currentHealth = 0.0f;
                    hp->currentShields = 0.0f;
                }
            }
        }
        // Keep visuals in sync with archetype selection updates
        const ArchetypeDef* def = findArchetypeById(p.heroId);
        if (def) {
            setupHeroVisualForEntity(ent, *def);
        }
    }
    if (!netSession_->isHost()) {
        lastSnapshotWave_ = snap.wave;
        lastSnapshotTick_ = snap.tick;
    }

    // Ensure local hero is fully restored right after a detected host reset.
    if (forceHealAfterReset_ && !netSession_->isHost()) {
        if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
            hp->maxHealth = std::max(hp->maxHealth, heroMaxHpPreset_);
            hp->currentHealth = hp->maxHealth;
            hp->maxShields = std::max(hp->maxShields, heroShieldPreset_);
            hp->currentShields = hp->maxShields;
        }
        if (auto* ghost = registry_.get<Game::Ghost>(hero_)) {
            ghost->active = false;
        }
        forceHealAfterReset_ = false;
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
    contactDamageBase_ = waveSettingsBase_.contactDamage;
    contactDamage_ = contactDamageBase_;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);
    if (collisionSystem_) collisionSystem_->setThornConfig(thornConfig_.reflectPercent, thornConfig_.maxReflectPerHit);
}

float GameRoot::contactDamageCurve(int wave) const {
    const float w = static_cast<float>(std::max(1, wave));
    // Early safety: ramp from 18% at wave1 to 100% by ~wave11.
    float earlyT = std::clamp((w - 1.0f) / 10.0f, 0.0f, 1.0f);
    float earlyMul = 0.18f + earlyT * 0.82f;
    // Late pressure: from wave20 onward climb toward +120% by wave60.
    float lateT = std::clamp((w - 20.0f) / 40.0f, 0.0f, 1.0f);
    float lateMul = 1.0f + 1.2f * lateT;
    return earlyMul * lateMul;
}

float GameRoot::copperRewardCurve(int wave) const {
    const float w = static_cast<float>(std::max(1, wave));
    // Mild boost through early teens to keep upgrades flowing.
    float earlyT = std::clamp((w - 1.0f) / 12.0f, 0.0f, 1.0f);
    float earlyMul = 1.0f + 0.30f * earlyT;  // up to +30%
    // Mid/late-game acceleration to offset tanky enemies (caps ~+250% by wave60).
    float lateT = std::clamp((w - 15.0f) / 45.0f, 0.0f, 1.0f);
    float lateMul = 1.0f + 2.5f * lateT;
    return earlyMul * lateMul;
}

void GameRoot::applyWaveScaling(int wave) {
    const int w = std::max(1, wave);
    // Contact damage scaling
    contactDamage_ = contactDamageBase_ * contactDamageCurve(w);
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);

    // Economy scaling
    const float rewardMul = copperRewardCurve(w);
    auto scaleInt = [](int base, float mul) {
        return std::max(1, static_cast<int>(std::round(static_cast<float>(base) * mul)));
    };
    copperPerKill_ = scaleInt(copperPerKillBase_, rewardMul);
    copperPickupMin_ = scaleInt(copperPickupMinBase_, rewardMul);
    copperPickupMax_ = std::max(copperPickupMin_, scaleInt(copperPickupMaxBase_, rewardMul));
    bossCopperDrop_ = scaleInt(bossCopperDropBase_, rewardMul);
    miniBossCopperDrop_ = scaleInt(miniBossCopperDropBase_, rewardMul);
    // Partial scaling on wave clear bonus to avoid runaway economy.
    float clearMul = 1.0f + (rewardMul - 1.0f) * 0.65f;
    waveClearBonus_ = scaleInt(waveClearBonusBase_, clearMul);
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
    if (isMeleeOnlyArchetype(activeArchetype_.id) || isMeleeOnlyArchetype(activeArchetype_.name)) {
        activeArchetype_.offensiveType = Game::OffensiveType::Melee;
    }
    if (archetypeSupportsSecondaryWeapon(activeArchetype_)) {
        activeArchetype_.offensiveType = Game::OffensiveType::Melee;
    }
    heroBaseOffense_ = activeArchetype_.offensiveType;
    usingSecondaryWeapon_ = false;
    heroMaxHpPreset_ = heroMaxHpBase_ * activeArchetype_.hpMul * globalModifiers_.playerHealthMult;
    heroShieldPreset_ = heroShieldBase_;
    // Melee archetypes start with a substantial shield pool for survivability.
    if (activeArchetype_.offensiveType == Game::OffensiveType::Melee) {
        heroShieldPreset_ += 100.0f;
    }
    // Apply any melee shield bonus (kept additive for future tuning).
    if (activeArchetype_.offensiveType == Game::OffensiveType::Melee) {
        heroShieldPreset_ += meleeShieldBonus_ * activeArchetype_.hpMul * globalModifiers_.playerShieldsMult;
    }
    heroShieldPreset_ *= globalModifiers_.playerShieldsMult;
    heroMoveSpeedPreset_ = heroMoveSpeedBase_ * activeArchetype_.speedMul * globalModifiers_.playerSpeedMult;
    projectileDamagePreset_ = projectileDamageBase_ * activeArchetype_.damageMul * globalModifiers_.playerAttackPowerMult;
    heroColorPreset_ = activeArchetype_.color;
    heroTexturePath_ = resolveArchetypeTexture(activeArchetype_);
    buildAbilities();
}

void GameRoot::setupHeroVisualForEntity(Engine::ECS::Entity ent, const GameRoot::ArchetypeDef& def) {
    auto files = heroSpriteFilesFor(def);
    auto tryLoad = [&](const std::string& rel) -> Engine::TexturePtr {
        std::filesystem::path path = std::filesystem::path("assets") / "Sprites" / "Characters" / files.folder / rel;
        if (const char* home = std::getenv("HOME")) {
            std::filesystem::path homePath = std::filesystem::path(home) / path;
            if (std::filesystem::exists(homePath)) {
                path = homePath;
            }
        }
        if (std::filesystem::exists(path)) return loadTextureOptional(path.string());
        return {};
    };

    Engine::TexturePtr moveTex = tryLoad(files.movementFile);
    Engine::TexturePtr combatTex = tryLoad(files.combatFile);
    if (!moveTex) {
        std::string compact = files.folder;
        compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
        moveTex = tryLoad(compact + ".png");
    }
    if (!combatTex) {
        std::string compact = files.folder;
        compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
        combatTex = tryLoad(compact + "_Combat.png");
    }

    Engine::ECS::Renderable* rend = registry_.get<Engine::ECS::Renderable>(ent);
    Engine::ECS::SpriteAnimation* anim = registry_.get<Engine::ECS::SpriteAnimation>(ent);
    if (!anim) {
        registry_.emplace<Engine::ECS::SpriteAnimation>(ent, Engine::ECS::SpriteAnimation{});
        anim = registry_.get<Engine::ECS::SpriteAnimation>(ent);
    }
    if (!registry_.has<Game::HeroSpriteSheets>(ent)) {
        registry_.emplace<Game::HeroSpriteSheets>(ent);
    }
    auto* sheets = registry_.get<Game::HeroSpriteSheets>(ent);

    if (moveTex) {
        sheets->movement = moveTex;
        const int movementColumns = 4;
        const int movementRows = 31;
        sheets->movementFrameWidth = std::max(1, moveTex->width() / movementColumns);
        sheets->movementFrameHeight = (moveTex->height() % movementRows == 0)
                                          ? std::max(1, moveTex->height() / movementRows)
                                          : std::max(1, moveTex->height() / std::max(1, moveTex->height() / sheets->movementFrameWidth));
        sheets->baseRenderSize = heroSize_;
        sheets->movementFrameDuration = 0.12f;
        int moveFrames = std::max(1, moveTex->width() / sheets->movementFrameWidth);
        anim->frameWidth = sheets->movementFrameWidth;
        anim->frameHeight = sheets->movementFrameHeight;
        anim->frameCount = moveFrames;
        anim->frameDuration = sheets->movementFrameDuration;
        anim->row = sheets->rowIdleFront;
    }
    if (combatTex) {
        sheets->combat = combatTex;
        const int combatColumns = 4;
        sheets->combatFrameWidth = std::max(1, combatTex->width() / combatColumns);
        sheets->combatFrameHeight =
            (combatTex->height() % 20 == 0) ? std::max(1, combatTex->height() / 20) : sheets->combatFrameWidth;
        sheets->combatFrameDuration = 0.10f;
    }

    if (rend) {
        rend->texture = sheets->movement ? sheets->movement : rend->texture;
        float frameW = static_cast<float>(sheets->movementFrameWidth);
        float frameH = static_cast<float>(sheets->movementFrameHeight);
        float aspect = (frameW > 0.0f) ? (frameH / frameW) : 1.0f;
        rend->size = {heroSize_, heroSize_ * aspect};
        if (const auto* defColor = &def.color; defColor) {
            rend->color = *defColor;
        }
    }
    if (!registry_.has<Game::LookDirection>(ent)) {
        registry_.emplace<Game::LookDirection>(ent, Game::LookDirection{Game::LookDir4::Front});
    }
}

std::string GameRoot::resolveArchetypeTexture(const GameRoot::ArchetypeDef& def) const {
    if (!def.texturePath.empty()) return def.texturePath;
    const auto files = heroSpriteFilesFor(def);
    if (!files.folder.empty() && !files.movementFile.empty()) {
        std::filesystem::path p = std::filesystem::path("assets") / "Sprites" / "Characters" / files.folder / files.movementFile;
        return p.string();
    }
    // Fallback to manifest heroTexture (defaults to Damage Dealer path).
    const std::string fallback = "assets/Sprites/Characters/Damage Dealer/Dps.png";
    return manifest_.heroTexture.empty() ? fallback : manifest_.heroTexture;
}

void GameRoot::buildAbilities() {
    abilities_.clear();
    abilityStates_.clear();
    abilityIconIndices_.clear();
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
    // Builder-specific scalars reset each build.
    miniBuffLight_ = 1.0f;
    miniBuffHeavy_ = 1.0f;
    miniBuffMedic_ = 1.0f;
    miniRageTimer_ = 0.0f;
    miniRageDamageMul_ = 1.0f;
    miniRageAttackRateMul_ = 1.0f;
    miniRageHealMul_ = 1.0f;

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
        slot.iconIndex = def.iconIndex > 0 ? def.iconIndex : iconIndexFromName(def.name);
        abilities_.push_back(slot);

        AbilityState st{};
        st.def = def;
        st.level = 1;
        st.maxLevel = 5;
        st.cooldown = 0.0f;
        abilityStates_.push_back(st);
        abilityIconIndices_.push_back(slot.iconIndex);
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
                def.iconIndex = a.value("icon", 0);
                if (!def.name.empty()) {
                    pushAbility(def);
                }
            }
            loaded = !abilities_.empty();
        }
    }

    if (!loaded) {
        const std::string archetype = activeArchetype_.id;
    if (archetype == "builder") {
        // Custom Builder kit: three upgrade abilities plus a mini-unit overdrive ultimate.
        auto push = [&](const std::string& name, const std::string& desc, const std::string& key, float cd, int cost, const std::string& type, int icon) {
            AbilityDef d{name, desc, key, cd, cost, type, icon};
            pushAbility(d);
            abilities_.back().maxLevel = 1000000;
            abilityStates_.back().maxLevel = 1000000;
        };
        push("Upgrade Light Units", "Permanently buff Light mini-units by +2% to all stats per purchase.", "1", 0.6f, 20, "builder_up_light", 7);
        push("Upgrade Heavy Units", "Permanently buff Heavy mini-units by +2% to all stats per purchase.", "2", 0.6f, 25, "builder_up_heavy", 12);
        push("Upgrade Medic Units", "Permanently buff Medic mini-units by +2% to all stats per purchase.", "3", 0.6f, 22, "builder_up_medic", 44);
        push("Mini-Unit Overdrive", "30s rage: faster attacks, bonus health, bonus damage for all mini-units.", "4", 45.0f, 35, "builder_rage", 55);
        loaded = true;
    } else if (archetype == "damage" || archetype == "damage dealer" || archetype == "dd") {
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
    std::uniform_real_distribution<float> variance(0.9f, 1.15f);
    auto jitter = [&](float v) { return v * variance(rng_); };
    std::array<LevelChoice, 5> pool = {{
        {LevelChoiceType::Damage, jitter(levelDmgBonus_)},
        {LevelChoiceType::Health, jitter(levelHpBonus_)},
        {LevelChoiceType::Speed, jitter(levelSpeedBonus_)},
        {LevelChoiceType::Shield, 5.0f},
        {LevelChoiceType::Recharge, 0.01f},  // +1% shield regen
    }};
    std::uniform_int_distribution<int> pickType(0, static_cast<int>(pool.size()) - 1);
    for (int i = 0; i < 3; ++i) {
        levelChoices_[i] = pool[pickType(rng_)];
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
        case LevelChoiceType::Shield:
            heroShield_ += choice.amount;
            heroShieldPreset_ += choice.amount;
            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                hp->maxShields += choice.amount;
                hp->currentShields = std::min(hp->maxShields, hp->currentShields + choice.amount);
            }
            break;
        case LevelChoiceType::Recharge: {
            // Treat amount as fractional increase (e.g., 0.01 = +1%).
            float baseline = heroShieldRegen_ > 0.0f ? heroShieldRegen_ : 0.5f;
            float newRate = baseline * (1.0f + choice.amount);
            heroShieldRegen_ = newRate;
            heroShieldRegenBase_ = newRate;
            if (auto* hp = registry_.get<Engine::ECS::Health>(hero_)) {
                hp->shieldRegenRate = heroShieldRegen_;
            }
            break;
        }
    }
    levelChoiceOpen_ = false;
    refreshPauseState();
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
            case LevelChoiceType::Shield:
                title = "Ward";
                desc = "+ " + std::to_string(static_cast<int>(c.amount)) + " shields";
                tcol = Engine::Color{150, 220, 255, 240};
                break;
            case LevelChoiceType::Recharge:
                title = "Recharge";
                desc = "+ " + std::to_string(static_cast<int>(std::round(c.amount * 100.0f))) + "% shield regen";
                tcol = Engine::Color{170, 245, 255, 240};
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
    shopInventory_.clear();
    shopPool_ = travelShopUnlocked_ ? goldCatalog_ : itemCatalog_;
    std::shuffle(shopPool_.begin(), shopPool_.end(), rng_);

    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    int rpgAdded = 0;
    std::size_t legacyIdx = 0;
    const int targetSlots = 4;
    while (static_cast<int>(shopInventory_.size()) < targetSlots) {
        // Traveling shop: sometimes offer RPG equipment items (when RPG loot is enabled).
        if (travelShopUnlocked_ && useRpgLoot_ && rpgAdded < std::max(0, rpgShopEquipCount_) &&
            roll(rng_) < std::clamp(rpgShopEquipChance_, 0.0f, 1.0f)) {
            if (auto item = rollRpgEquipment(RpgLootSource::Shop, true)) {
                shopInventory_.push_back(*item);
                rpgAdded += 1;
                continue;
            }
        }
        // Legacy offers.
        if (legacyIdx >= shopPool_.size()) break;
        const auto& def = shopPool_[legacyIdx++];
        int cost = effectiveShopCost(def.effect, def.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                     shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                     shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_);
        if (cost < 0) continue;  // capped out; don't offer
        ItemDefinition priced = def;
        priced.cost = cost;
        shopInventory_.push_back(priced);
    }
    if (shopSystem_) {
        shopSystem_->setInventory(shopInventory_);
    }
}

void GameRoot::drawItemShopOverlay() {
    if (!render_ || !itemShopOpen_) return;
    const auto lay = itemShopLayout(viewportWidth_, viewportHeight_);
    const float s = lay.s;
    const float pad = 18.0f * s;
    const float innerPad = 18.0f * s;
    int mx = lastMouseX_;
    int my = lastMouseY_;

    auto insideF = [&](float x, float y, float w, float h) {
        return mx >= static_cast<int>(x) && mx <= static_cast<int>(x + w) &&
               my >= static_cast<int>(y) && my <= static_cast<int>(y + h);
    };

    auto rarityCol = [](ItemRarity r) {
        switch (r) {
            case ItemRarity::Common: return Engine::Color{235, 240, 245, 235};
            case ItemRarity::Uncommon: return Engine::Color{135, 235, 155, 235};
            case ItemRarity::Rare: return Engine::Color{135, 190, 255, 235};
            case ItemRarity::Epic: return Engine::Color{185, 135, 255, 235};
            case ItemRarity::Legendary: return Engine::Color{255, 210, 120, 240};
        }
        return Engine::Color{235, 240, 245, 235};
    };

    auto stripRaritySuffixLocal = [](const std::string& n) -> std::string {
        static constexpr const char* kSufs[] = {" [Common]", " [Uncommon]", " [Rare]", " [Epic]", " [Legendary]"};
        for (const char* suf : kSufs) {
            const std::size_t len = std::strlen(suf);
            if (n.size() >= len && n.compare(n.size() - len, len, suf) == 0) {
                return n.substr(0, n.size() - len);
            }
        }
        return n;
    };

    auto dynCostFor = [&](const ItemDefinition& item) -> int {
        return item.rpgTemplateId.empty()
                   ? effectiveShopCost(item.effect, item.cost, shopDamagePctPurchases_, shopAttackSpeedPctPurchases_,
                                       shopVitalsPctPurchases_, shopCooldownPurchases_, shopRangeVisionPurchases_,
                                       shopChargePurchases_, shopSpeedBootsPurchases_, shopVitalAusterityPurchases_)
                   : item.cost;
    };

    auto legacyDesc = [&](const ItemDefinition& item) -> std::string {
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
        return desc.str();
    };

    // Scrim + container.
    render_->drawFilledRect(Engine::Vec2{0.0f, 0.0f}, Engine::Vec2{static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_)},
                            Engine::Color{6, 8, 12, 170});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY + 6.0f * s}, Engine::Vec2{lay.panelW, lay.panelH}, Engine::Color{0, 0, 0, 95});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY}, Engine::Vec2{lay.panelW, lay.panelH}, Engine::Color{12, 16, 24, 235});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY}, Engine::Vec2{lay.panelW, 2.0f}, Engine::Color{45, 70, 95, 190});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY + lay.panelH - 2.0f}, Engine::Vec2{lay.panelW, 2.0f}, Engine::Color{45, 70, 95, 190});

    // Header.
    drawTextUnified("Traveling Shop", Engine::Vec2{lay.panelX + pad, lay.panelY + 18.0f * s}, std::clamp(1.15f * s, 0.95f, 1.25f),
                    Engine::Color{200, 255, 200, 240});
    drawTextUnified("Gold • E to close", Engine::Vec2{lay.panelX + pad, lay.panelY + 42.0f * s}, std::clamp(0.88f * s, 0.78f, 0.98f),
                    Engine::Color{160, 200, 230, 210});
    std::ostringstream wallet;
    wallet << "Gold " << gold_;
    const float ws = std::clamp(0.98f * s, 0.86f, 1.08f);
    const Engine::Vec2 wSz = measureTextUnified(wallet.str(), ws);
    drawTextUnified(wallet.str(), Engine::Vec2{lay.panelX + lay.panelW - pad - wSz.x, lay.panelY + 22.0f * s}, ws,
                    Engine::Color{200, 255, 200, 240});

    // Offers.
    drawTextUnified("Offers", Engine::Vec2{lay.offersX, lay.offersY + 10.0f * s}, std::clamp(0.95f * s, 0.82f, 1.05f),
                    Engine::Color{200, 230, 255, 220});
    const float offersStartX = lay.offersX;
    const float offersStartY = lay.offersY + 34.0f * s;
    int hoveredOffer = -1;
    for (int i = 0; i < static_cast<int>(shopInventory_.size()); ++i) {
        int c = i % 2;
        int r = i / 2;
        float x = offersStartX + static_cast<float>(c) * (lay.cardW + lay.cardGap);
        float y = offersStartY + static_cast<float>(r) * (lay.cardH + lay.cardGap);
        bool hov = insideF(x, y, lay.cardW, lay.cardH);
        if (hov) hoveredOffer = i;
        bool selected = (i == itemShopSelected_);
        const auto& item = shopInventory_[static_cast<std::size_t>(i)];

        Engine::Color bg = selected ? Engine::Color{22, 34, 52, 235} : (hov ? Engine::Color{18, 26, 40, 232}
                                                                            : Engine::Color{14, 18, 26, 220});
        render_->drawFilledRect(Engine::Vec2{x, y + 3.0f * s}, Engine::Vec2{lay.cardW, lay.cardH}, Engine::Color{0, 0, 0, 70});
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{lay.cardW, lay.cardH}, bg);
        Engine::Color edge = item.rpgTemplateId.empty() ? Engine::Color{90, 140, 210, 190} : rarityCol(item.rarity);
        if (selected) edge.a = 240;
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{lay.cardW, 2.0f}, edge);
        render_->drawFilledRect(Engine::Vec2{x, y + lay.cardH - 2.0f}, Engine::Vec2{lay.cardW, 2.0f}, edge);

        const float icon = 34.0f * s;
        render_->drawFilledRect(Engine::Vec2{x + innerPad, y + innerPad}, Engine::Vec2{icon, icon}, Engine::Color{18, 26, 40, 230});
        if (!item.rpgTemplateId.empty()) {
            drawItemIcon(item, Engine::Vec2{x + innerPad, y + innerPad}, icon);
        } else {
            drawTextUnified("★", Engine::Vec2{x + innerPad + 10.0f * s, y + innerPad + 6.0f * s}, 1.15f * s,
                            Engine::Color{200, 230, 255, 220});
        }

        const float nameScale = std::clamp(0.94f * s, 0.84f, 1.04f);
        const float descScale = std::clamp(0.82f * s, 0.76f, 0.92f);
        const float textX = x + innerPad + icon + 12.0f * s;
        const float maxNameW = lay.cardW - (textX - x) - innerPad;
        const std::string title = item.rpgTemplateId.empty() ? shopLabelForEffect(item.effect) : stripRaritySuffixLocal(item.name);
        drawTextUnified(ellipsizeTextUnified(title, maxNameW, nameScale), Engine::Vec2{textX, y + innerPad - 1.0f * s}, nameScale,
                        Engine::Color{220, 240, 255, 240});
        const std::string desc = item.rpgTemplateId.empty() ? legacyDesc(item) : item.desc;
        drawTextUnified(ellipsizeTextUnified(desc, maxNameW, descScale), Engine::Vec2{textX, y + innerPad + 18.0f * s}, descScale,
                        Engine::Color{180, 205, 230, 220});

        int dynCost = dynCostFor(item);
        const bool affordable = dynCost >= 0 && gold_ >= dynCost;
        std::ostringstream cost;
        if (dynCost < 0) cost << "CAPPED";
        else cost << dynCost << "g";
        Engine::Color costCol = affordable ? Engine::Color{180, 255, 200, 235} : Engine::Color{220, 160, 160, 220};
        drawTextUnified(cost.str(), Engine::Vec2{x + innerPad, y + lay.cardH - 28.0f * s}, std::clamp(0.88f * s, 0.78f, 0.98f), costCol);
    }
    if (hoveredOffer >= 0) itemShopSelected_ = hoveredOffer;

    // Details.
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY + 3.0f * s}, Engine::Vec2{lay.detailsW, lay.detailsH}, Engine::Color{0, 0, 0, 75});
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY}, Engine::Vec2{lay.detailsW, lay.detailsH}, Engine::Color{14, 18, 26, 220});
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY}, Engine::Vec2{lay.detailsW, 2.0f}, Engine::Color{45, 70, 95, 190});
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY + lay.detailsH - 2.0f}, Engine::Vec2{lay.detailsW, 2.0f}, Engine::Color{45, 70, 95, 190});

    if (!shopInventory_.empty()) {
        itemShopSelected_ = std::clamp(itemShopSelected_, 0, static_cast<int>(shopInventory_.size()) - 1);
        const auto& item = shopInventory_[static_cast<std::size_t>(itemShopSelected_)];
        const std::string title = item.rpgTemplateId.empty() ? shopLabelForEffect(item.effect) : stripRaritySuffixLocal(item.name);
        const float titleScale = std::clamp(1.12f * s, 0.95f, 1.25f);
        drawTextUnified(title, Engine::Vec2{lay.detailsX + pad, lay.detailsY + pad}, titleScale,
                        item.rpgTemplateId.empty() ? Engine::Color{220, 240, 255, 240} : rarityCol(item.rarity));

        const float icon = 58.0f * s;
        render_->drawFilledRect(Engine::Vec2{lay.detailsX + pad, lay.detailsY + pad + 38.0f * s}, Engine::Vec2{icon, icon}, Engine::Color{18, 26, 40, 230});
        if (!item.rpgTemplateId.empty()) drawItemIcon(item, Engine::Vec2{lay.detailsX + pad, lay.detailsY + pad + 38.0f * s}, icon);

        float infoX = lay.detailsX + pad + icon + 14.0f * s;
        float infoY = lay.detailsY + pad + 42.0f * s;
        const float infoW = lay.detailsW - (infoX - lay.detailsX) - pad;
        const float bodyScale = std::clamp(0.90f * s, 0.82f, 1.05f);
        const std::string desc = item.rpgTemplateId.empty() ? legacyDesc(item) : item.desc;
        drawTextUnified(ellipsizeTextUnified(desc, infoW, bodyScale), Engine::Vec2{infoX, infoY}, bodyScale,
                        Engine::Color{190, 210, 235, 230});
        infoY += 26.0f * s;
        if (!item.rpgTemplateId.empty()) {
            for (std::size_t a = 0; a < item.affixes.size() && a < 6; ++a) {
                drawTextUnified("• " + item.affixes[a], Engine::Vec2{infoX, infoY}, std::clamp(0.84f * s, 0.76f, 0.98f),
                                Engine::Color{185, 220, 255, 220});
                infoY += 20.0f * s;
            }
        }

        int dynCost = dynCostFor(item);
        const bool affordable = dynCost >= 0 && gold_ >= dynCost;
        std::ostringstream cost;
        if (dynCost < 0) cost << "Capped";
        else cost << dynCost << "g";
        drawTextUnified("Price", Engine::Vec2{lay.detailsX + pad, lay.buyY - 34.0f * s}, std::clamp(0.88f * s, 0.78f, 0.98f),
                        Engine::Color{160, 190, 215, 210});
        drawTextUnified(cost.str(), Engine::Vec2{lay.detailsX + pad + 58.0f * s, lay.buyY - 34.0f * s}, std::clamp(0.92f * s, 0.82f, 1.08f),
                        affordable ? Engine::Color{180, 255, 200, 235} : Engine::Color{220, 160, 160, 220});

        // Buy button (visual only).
        const bool hovBuy = insideF(lay.buyX, lay.buyY, lay.buyW, lay.buyH);
        Engine::Color btnBg = affordable ? Engine::Color{30, 70, 52, 235} : Engine::Color{60, 45, 45, 225};
        if (dynCost < 0) btnBg = Engine::Color{50, 50, 58, 220};
        if (hovBuy) {
            btnBg = Engine::Color{static_cast<uint8_t>(std::min(255, btnBg.r + 12)),
                                  static_cast<uint8_t>(std::min(255, btnBg.g + 12)),
                                  static_cast<uint8_t>(std::min(255, btnBg.b + 12)),
                                  btnBg.a};
        }
        render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY + 3.0f * s}, Engine::Vec2{lay.buyW, lay.buyH}, Engine::Color{0, 0, 0, 80});
        render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY}, Engine::Vec2{lay.buyW, lay.buyH}, btnBg);
        render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY}, Engine::Vec2{lay.buyW, 2.0f},
                                affordable ? Engine::Color{120, 220, 170, 230} : Engine::Color{120, 120, 140, 210});
        std::string btnLabel = (dynCost < 0) ? "Capped" : (affordable ? "Buy" : "Need Gold");
        const float bs = std::clamp(1.0f * s, 0.90f, 1.12f);
        Engine::Vec2 bsz = measureTextUnified(btnLabel, bs);
        drawTextUnified(btnLabel, Engine::Vec2{lay.buyX + (lay.buyW - bsz.x) * 0.5f, lay.buyY + (lay.buyH - bsz.y) * 0.5f + 1.0f * s},
                        bs, Engine::Color{220, 245, 255, 245});
        if (!affordable && shopNoFundsTimer_ > 0.0) {
            drawTextUnified("Not enough gold", Engine::Vec2{lay.buyX + pad, lay.buyY - 18.0f * s}, std::clamp(0.86f * s, 0.78f, 0.95f),
                            Engine::Color{255, 170, 170, 220});
        }
    }

    // Inventory panel (sell).
    render_->drawFilledRect(Engine::Vec2{lay.invX, lay.invY + 3.0f * s}, Engine::Vec2{lay.invW, lay.invH}, Engine::Color{0, 0, 0, 75});
    render_->drawFilledRect(Engine::Vec2{lay.invX, lay.invY}, Engine::Vec2{lay.invW, lay.invH}, Engine::Color{14, 18, 26, 220});
    render_->drawFilledRect(Engine::Vec2{lay.invX, lay.invY}, Engine::Vec2{lay.invW, 2.0f}, Engine::Color{45, 70, 95, 190});
    drawTextUnified("Inventory", Engine::Vec2{lay.invX + pad, lay.invY + 14.0f * s}, std::clamp(0.95f * s, 0.82f, 1.05f),
                    Engine::Color{200, 230, 255, 220});
    drawTextUnified("Click to sell (50%)", Engine::Vec2{lay.invX + pad, lay.invY + 34.0f * s}, std::clamp(0.82f * s, 0.76f, 0.92f),
                    Engine::Color{160, 190, 215, 210});

    const float invHeader = 50.0f * s;
    const float listX = lay.invX + innerPad;
    const float listY = lay.invY + invHeader;
    const float listW = lay.invW - innerPad * 2.0f;
    const float listH = lay.invH - invHeader - innerPad;
    const int visibleRows = std::max(1, static_cast<int>(std::floor(listH / lay.invRowH)));
    const int maxScrollRows = std::max(0, static_cast<int>(inventory_.size()) - visibleRows);
    const int scrollRow = std::clamp(static_cast<int>(std::round(shopSellScroll_)), 0, maxScrollRows);
    for (int r = 0; r < visibleRows; ++r) {
        const int idx = scrollRow + r;
        if (idx < 0 || idx >= static_cast<int>(inventory_.size())) break;
        float y = listY + static_cast<float>(r) * lay.invRowH;
        bool hov = insideF(listX, y, listW, lay.invRowH);
        Engine::Color bg = hov ? Engine::Color{18, 26, 40, 232} : Engine::Color{12, 16, 24, 210};
        render_->drawFilledRect(Engine::Vec2{listX, y}, Engine::Vec2{listW, lay.invRowH - 2.0f * s}, bg);
        render_->drawFilledRect(Engine::Vec2{listX, y}, Engine::Vec2{4.0f * s, lay.invRowH - 2.0f * s}, Engine::Color{45, 70, 95, 180});

        const auto& inst = inventory_[static_cast<std::size_t>(idx)];
        const float ts = std::clamp(0.86f * s, 0.78f, 0.98f);
        const float maxName = listW - 72.0f * s;
        drawTextUnified(ellipsizeTextUnified(stripRaritySuffixLocal(inst.def.name), maxName, ts),
                        Engine::Vec2{listX + 12.0f * s, y + 8.0f * s}, ts, rarityCol(inst.def.rarity));
        int refund = std::max(1, inst.def.cost / 2);
        std::ostringstream sell;
        sell << refund << "g";
        Engine::Vec2 ss = measureTextUnified(sell.str(), ts);
        drawTextUnified(sell.str(), Engine::Vec2{listX + listW - 10.0f * s - ss.x, y + 8.0f * s}, ts,
                        Engine::Color{200, 255, 200, 220});
    }
    if (maxScrollRows > 0) {
        const float barW = 10.0f * s;
        const float trackX = lay.invX + lay.invW - barW - 10.0f * s;
        render_->drawFilledRect(Engine::Vec2{trackX, listY}, Engine::Vec2{barW, listH}, Engine::Color{10, 14, 22, 170});
        const float thumbH = std::max(24.0f * s, listH * (static_cast<float>(visibleRows) / static_cast<float>(inventory_.size())));
        const float ratio = static_cast<float>(scrollRow) / static_cast<float>(maxScrollRows);
        const float thumbY = listY + (listH - thumbH) * ratio;
        render_->drawFilledRect(Engine::Vec2{trackX, thumbY}, Engine::Vec2{barW, thumbH}, Engine::Color{90, 140, 210, 220});
    }
}

void GameRoot::drawBuildMenuOverlay(int mouseX, int mouseY, bool leftClickEdge) {
    if (!render_ || !buildMenuOpen_ || activeArchetype_.offensiveType != Game::OffensiveType::Builder) return;
    const float panelW = 260.0f;
    const float panelH = 220.0f;
    const float panelY = static_cast<float>(viewportHeight_) - panelH - kHudBottomSafeMargin;
    Engine::Vec2 topLeft{22.0f, panelY};
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
    float rowY = topLeft.y + 60.0f;
    const float rowH = 32.0f;
    for (const auto& e : entries) {
        bool hover = mouseX >= static_cast<int>(topLeft.x + 10.0f) && mouseX <= static_cast<int>(topLeft.x + panelW - 10.0f) &&
                     mouseY >= static_cast<int>(rowY) && mouseY <= static_cast<int>(rowY + rowH);
        Engine::Color bg = hover ? Engine::Color{40, 60, 80, 230} : Engine::Color{28, 36, 48, 210};
        render_->drawFilledRect(Engine::Vec2{topLeft.x + 8.0f, rowY}, Engine::Vec2{panelW - 16.0f, rowH}, bg);
        auto it = buildingDefs_.find(buildingKey(e.type));
        int cost = (it != buildingDefs_.end()) ? it->second.costCopper : 0;
        std::ostringstream label;
        label << e.label << " - " << cost << "c";
        drawTextUnified(label.str(), Engine::Vec2{topLeft.x + 14.0f, rowY + 6.0f}, 0.9f,
                        Engine::Color{220, 240, 255, 240});
        if (hover && leftClickEdge) {
            beginBuildPreview(e.type);
            buildMenuOpen_ = false;
        }
        rowY += rowH + 6.0f;
    }
    std::ostringstream cp;
    cp << "Copper: " << copper_;
    drawTextUnified(cp.str(), Engine::Vec2{topLeft.x + 12.0f, topLeft.y + panelH - 26.0f}, 0.9f,
                    Engine::Color{200, 255, 200, 230});
}

void GameRoot::drawAbilityShopOverlay() {
    if (!render_ || !abilityShopOpen_) return;
    const auto lay = abilityShopLayout(viewportWidth_, viewportHeight_);
    const float s = lay.s;
    const float pad = lay.pad;
    int mx = lastMouseX_;
    int my = lastMouseY_;

    auto insideF = [&](float x, float y, float w, float h) {
        return mx >= static_cast<int>(x) && mx <= static_cast<int>(x + w) &&
               my >= static_cast<int>(y) && my <= static_cast<int>(y + h);
    };

    auto costForLevel = [&](int level) {
        return static_cast<int>(std::round(static_cast<float>(abilityShopBaseCost_) *
                                           std::pow(abilityShopCostGrowth_, static_cast<float>(level))));
    };

    struct UpgradeRow {
        std::string title;
        std::string desc;
        int level{0};
        int maxLevel{999};
        int cost{0};
    };

    std::array<UpgradeRow, 6> rows{};
    rows[0] = {"Weapon Damage", "Increase base damage.", abilityDamageLevel_, 999, costForLevel(abilityDamageLevel_)};
    rows[1] = {"Attack Speed", "Fire faster.", abilityAttackSpeedLevel_, 999, costForLevel(abilityAttackSpeedLevel_)};
    rows[2] = {"Weapon Range", "Increase auto-fire range.", abilityRangeLevel_, abilityRangeMaxBonus_, costForLevel(abilityRangeLevel_)};
    rows[3] = {"Sight Range", "Increase vision radius.", abilityVisionLevel_, abilityVisionMaxBonus_, costForLevel(abilityVisionLevel_)};
    rows[4] = {"Max Health", "Increase maximum HP.", abilityHealthLevel_, 999, costForLevel(abilityHealthLevel_)};
    rows[5] = {"Armor", "Reduce incoming damage.", abilityArmorLevel_, 999, costForLevel(abilityArmorLevel_)};

    if (!rows.empty()) {
        abilityShopSelected_ = std::clamp(abilityShopSelected_, 0, static_cast<int>(rows.size()) - 1);
    } else {
        abilityShopSelected_ = 0;
    }

    // Scrim + container.
    render_->drawFilledRect(Engine::Vec2{0.0f, 0.0f}, Engine::Vec2{static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_)},
                            Engine::Color{6, 8, 12, 170});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY + 6.0f * s}, Engine::Vec2{lay.panelW, lay.panelH}, Engine::Color{0, 0, 0, 95});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY}, Engine::Vec2{lay.panelW, lay.panelH}, Engine::Color{12, 16, 24, 235});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY}, Engine::Vec2{lay.panelW, 2.0f}, Engine::Color{45, 70, 95, 190});
    render_->drawFilledRect(Engine::Vec2{lay.panelX, lay.panelY + lay.panelH - 2.0f}, Engine::Vec2{lay.panelW, 2.0f}, Engine::Color{45, 70, 95, 190});

    // Header.
    drawTextUnified("Ability Shop", Engine::Vec2{lay.panelX + pad, lay.panelY + 18.0f * s}, std::clamp(1.15f * s, 0.95f, 1.25f),
                    Engine::Color{200, 255, 200, 240});
    drawTextUnified("Copper • B to close", Engine::Vec2{lay.panelX + pad, lay.panelY + 42.0f * s}, std::clamp(0.88f * s, 0.78f, 0.98f),
                    Engine::Color{160, 200, 230, 210});
    std::ostringstream wallet;
    wallet << "Copper " << copper_;
    const float ws = std::clamp(0.98f * s, 0.86f, 1.08f);
    const Engine::Vec2 wSz = measureTextUnified(wallet.str(), ws);
    drawTextUnified(wallet.str(), Engine::Vec2{lay.panelX + lay.panelW - pad - wSz.x, lay.panelY + 22.0f * s}, ws,
                    Engine::Color{200, 255, 200, 240});

    // List section.
    render_->drawFilledRect(Engine::Vec2{lay.listX, lay.listY + 3.0f * s}, Engine::Vec2{lay.listW, lay.listH}, Engine::Color{0, 0, 0, 65});
    render_->drawFilledRect(Engine::Vec2{lay.listX, lay.listY}, Engine::Vec2{lay.listW, lay.listH}, Engine::Color{14, 18, 26, 220});
    render_->drawFilledRect(Engine::Vec2{lay.listX, lay.listY}, Engine::Vec2{lay.listW, 2.0f}, Engine::Color{45, 70, 95, 190});
    drawTextUnified("Upgrades", Engine::Vec2{lay.listX + pad, lay.listY + 14.0f * s}, std::clamp(0.95f * s, 0.82f, 1.05f),
                    Engine::Color{200, 230, 255, 220});
    drawTextUnified("Hover to preview • Buy on right", Engine::Vec2{lay.listX + pad, lay.listY + 34.0f * s}, std::clamp(0.82f * s, 0.76f, 0.92f),
                    Engine::Color{160, 190, 215, 210});

    const float rowGap = 10.0f * s;
    const float rowsStartY = lay.listY + pad + 42.0f * s;
    const float maxNameW = lay.listW - pad * 2.0f - 90.0f * s;
    const float titleScale = std::clamp(0.96f * s, 0.86f, 1.08f);
    const float descScale = std::clamp(0.82f * s, 0.74f, 0.95f);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        float y = rowsStartY + static_cast<float>(i) * (lay.rowH + rowGap);
        bool hov = insideF(lay.listX + pad, y, lay.listW - pad * 2.0f, lay.rowH);
        bool sel = (i == abilityShopSelected_);
        const auto& r = rows[static_cast<std::size_t>(i)];

        Engine::Color bg = sel ? Engine::Color{22, 34, 52, 235} : (hov ? Engine::Color{18, 26, 40, 232}
                                                                       : Engine::Color{12, 16, 24, 210});
        render_->drawFilledRect(Engine::Vec2{lay.listX + pad, y + 3.0f * s}, Engine::Vec2{lay.listW - pad * 2.0f, lay.rowH}, Engine::Color{0, 0, 0, 65});
        render_->drawFilledRect(Engine::Vec2{lay.listX + pad, y}, Engine::Vec2{lay.listW - pad * 2.0f, lay.rowH}, bg);
        render_->drawFilledRect(Engine::Vec2{lay.listX + pad, y}, Engine::Vec2{4.0f * s, lay.rowH},
                                Engine::Color{45, 70, 95, static_cast<uint8_t>(sel ? 235 : 180)});

        drawTextUnified(ellipsizeTextUnified(r.title, maxNameW, titleScale),
                        Engine::Vec2{lay.listX + pad + 12.0f * s, y + 10.0f * s}, titleScale, Engine::Color{220, 240, 255, 240});
        drawTextUnified(ellipsizeTextUnified(r.desc, maxNameW, descScale),
                        Engine::Vec2{lay.listX + pad + 12.0f * s, y + 32.0f * s}, descScale, Engine::Color{180, 210, 235, 220});

        bool maxed = (r.level >= r.maxLevel);
        std::ostringstream right;
        if (maxed) {
            right << "Max";
        } else {
            right << "Lv " << r.level;
        }
        Engine::Vec2 rs = measureTextUnified(right.str(), descScale);
        drawTextUnified(right.str(), Engine::Vec2{lay.listX + lay.listW - pad - 10.0f * s - rs.x, y + 10.0f * s}, descScale,
                        maxed ? Engine::Color{170, 200, 230, 200} : Engine::Color{200, 230, 220, 220});

        std::ostringstream cost;
        cost << r.cost << "c";
        const bool affordable = (copper_ >= r.cost) && !maxed;
        Engine::Color costCol = maxed ? Engine::Color{170, 200, 230, 190}
                                      : (affordable ? Engine::Color{180, 255, 200, 235} : Engine::Color{220, 160, 160, 220});
        Engine::Vec2 cs = measureTextUnified(cost.str(), titleScale);
        drawTextUnified(cost.str(), Engine::Vec2{lay.listX + lay.listW - pad - 10.0f * s - cs.x, y + 32.0f * s}, titleScale, costCol);
    }

    // Details panel.
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY + 3.0f * s}, Engine::Vec2{lay.detailsW, lay.detailsH}, Engine::Color{0, 0, 0, 65});
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY}, Engine::Vec2{lay.detailsW, lay.detailsH}, Engine::Color{14, 18, 26, 220});
    render_->drawFilledRect(Engine::Vec2{lay.detailsX, lay.detailsY}, Engine::Vec2{lay.detailsW, 2.0f}, Engine::Color{45, 70, 95, 190});

    const auto& sel = rows[static_cast<std::size_t>(abilityShopSelected_)];
    bool selMaxed = sel.level >= sel.maxLevel;
    const bool affordable = (copper_ >= sel.cost) && !selMaxed;

    drawTextUnified("Details", Engine::Vec2{lay.detailsX + pad, lay.detailsY + 14.0f * s}, std::clamp(0.95f * s, 0.82f, 1.05f),
                    Engine::Color{200, 230, 255, 220});
    drawTextUnified(sel.title, Engine::Vec2{lay.detailsX + pad, lay.detailsY + 44.0f * s}, std::clamp(1.12f * s, 0.95f, 1.25f),
                    Engine::Color{220, 245, 255, 240});

    float infoY = lay.detailsY + 84.0f * s;
    const float bodyScale = std::clamp(0.92f * s, 0.82f, 1.05f);
    const float infoW = lay.detailsW - pad * 2.0f;
    drawTextUnified(ellipsizeTextUnified(sel.desc, infoW, bodyScale), Engine::Vec2{lay.detailsX + pad, infoY}, bodyScale,
                    Engine::Color{190, 210, 235, 230});
    infoY += 28.0f * s;

    std::ostringstream lv;
    lv << "Level " << sel.level;
    if (sel.maxLevel < 900) lv << " / " << sel.maxLevel;
    drawTextUnified(lv.str(), Engine::Vec2{lay.detailsX + pad, infoY}, std::clamp(0.90f * s, 0.78f, 1.0f),
                    Engine::Color{180, 220, 255, 220});
    infoY += 22.0f * s;

    std::ostringstream c;
    c << (selMaxed ? "Maxed" : (std::to_string(sel.cost) + "c"));
    drawTextUnified("Cost", Engine::Vec2{lay.detailsX + pad, infoY}, std::clamp(0.88f * s, 0.78f, 0.98f),
                    Engine::Color{160, 190, 215, 210});
    drawTextUnified(c.str(), Engine::Vec2{lay.detailsX + pad + 62.0f * s, infoY}, std::clamp(0.92f * s, 0.82f, 1.08f),
                    selMaxed ? Engine::Color{170, 200, 230, 200}
                             : (affordable ? Engine::Color{180, 255, 200, 235} : Engine::Color{220, 160, 160, 220}));

    // Buy button (visual only; click handled in update).
    const bool hovBuy = insideF(lay.buyX, lay.buyY, lay.buyW, lay.buyH);
    Engine::Color btnBg = affordable ? Engine::Color{30, 70, 52, 235} : Engine::Color{60, 45, 45, 225};
    if (selMaxed) btnBg = Engine::Color{50, 50, 58, 220};
    if (hovBuy) {
        btnBg = Engine::Color{static_cast<uint8_t>(std::min(255, btnBg.r + 12)),
                              static_cast<uint8_t>(std::min(255, btnBg.g + 12)),
                              static_cast<uint8_t>(std::min(255, btnBg.b + 12)),
                              btnBg.a};
    }
    render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY + 3.0f * s}, Engine::Vec2{lay.buyW, lay.buyH}, Engine::Color{0, 0, 0, 80});
    render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY}, Engine::Vec2{lay.buyW, lay.buyH}, btnBg);
    render_->drawFilledRect(Engine::Vec2{lay.buyX, lay.buyY}, Engine::Vec2{lay.buyW, 2.0f},
                            affordable ? Engine::Color{120, 220, 170, 230} : Engine::Color{120, 120, 140, 210});
    std::string btnLabel = selMaxed ? "Maxed" : (affordable ? "Buy Upgrade" : "Need Copper");
    const float bs = std::clamp(1.0f * s, 0.90f, 1.12f);
    Engine::Vec2 bsz = measureTextUnified(btnLabel, bs);
    drawTextUnified(btnLabel, Engine::Vec2{lay.buyX + (lay.buyW - bsz.x) * 0.5f, lay.buyY + (lay.buyH - bsz.y) * 0.5f + 1.0f * s},
                    bs, Engine::Color{220, 245, 255, 245});
    if (!affordable && !selMaxed && shopNoFundsTimer_ > 0.0) {
        drawTextUnified("Not enough copper", Engine::Vec2{lay.buyX + pad, lay.buyY - 18.0f * s}, std::clamp(0.86f * s, 0.78f, 0.95f),
                        Engine::Color{255, 170, 170, 220});
    }
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

void GameRoot::drawResourceCluster() {
    if (!render_) return;
    float hpFillTarget = 0.0f;
    float shieldFillTarget = 0.0f;
    float curHp = 0.0f;
    float maxHp = 0.0f;
    float curShield = 0.0f;
    float maxShield = 0.0f;
    if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
        maxHp = heroHealth->maxHealth;
        curHp = heroHealth->currentHealth;
        maxShield = heroHealth->maxShields;
        curShield = heroHealth->currentShields;
        if (maxHp > 0.0f) hpFillTarget = std::clamp(curHp / maxHp, 0.0f, 1.0f);
        if (maxShield > 0.0f) shieldFillTarget = std::clamp(curShield / maxShield, 0.0f, 1.0f);
    }
    float energyRatio = energyMax_ > 0.0f ? std::clamp(energy_ / energyMax_, 0.0f, 1.0f) : 0.0f;
    float dashRatio = dashCooldown_ > 0.0f ? 1.0f - std::clamp(dashCooldownTimer_ / dashCooldown_, 0.0f, 1.0f) : 1.0f;

    auto smooth = [](float current, float target) {
        const float k = 0.18f;
        return current + (target - current) * k;
    };
    uiHpFill_ = smooth(uiHpFill_, hpFillTarget);
    uiShieldFill_ = smooth(uiShieldFill_, shieldFillTarget);
    uiEnergyFill_ = smooth(uiEnergyFill_, energyRatio);
    uiDashFill_ = smooth(uiDashFill_, dashRatio);

    // Modern HUD layout (1080p reference-scaled), anchored top-left to avoid obscuring gameplay.
    const auto hud = ingameHudLayout(viewportWidth_, viewportHeight_);
    const float s = hud.s;
    const float pad = 12.0f * s;
    const float headerH = 20.0f * s;
    const float barW = hud.resourceW - pad * 2.0f;
    const float barH = 22.0f * s;
    const float gap = 8.0f * s;

    const float x0 = hud.resourceX;
    const float y0 = hud.resourceY;

    // Container (shadow + body + subtle top/bottom lines).
    render_->drawFilledRect(Engine::Vec2{x0, y0 + 4.0f * s}, Engine::Vec2{hud.resourceW, hud.resourceH}, Engine::Color{0, 0, 0, 85});
    render_->drawFilledRect(Engine::Vec2{x0, y0}, Engine::Vec2{hud.resourceW, hud.resourceH}, Engine::Color{12, 16, 24, 220});
    render_->drawFilledRect(Engine::Vec2{x0, y0}, Engine::Vec2{hud.resourceW, 2.0f}, Engine::Color{45, 70, 95, 190});
    render_->drawFilledRect(Engine::Vec2{x0, y0 + hud.resourceH - 2.0f}, Engine::Vec2{hud.resourceW, 2.0f}, Engine::Color{45, 70, 95, 190});

    // Title row (hero name + wave/level).
    {
        std::ostringstream t;
        t << activeArchetype_.name << " • Wave " << wave_ << " • Lv " << level_;
        drawTextUnified(t.str(), Engine::Vec2{x0 + pad, y0 + 6.0f * s}, std::clamp(0.92f * s, 0.82f, 1.05f),
                        Engine::Color{190, 225, 255, 230});
    }

    auto drawStackedBar = [&](int idx,
                              float fill,
                              Engine::Color fillCol,
                              Engine::Color accent,
                              const std::string& label,
                              const std::string& value) {
        const float bx = x0 + pad;
        const float by = y0 + headerH + pad + static_cast<float>(idx) * (barH + gap);

        // Background track.
        render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{barW, barH}, Engine::Color{8, 12, 18, 180});
        render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{barW, 2.0f}, Engine::Color{30, 40, 58, 170});
        render_->drawFilledRect(Engine::Vec2{bx, by + barH - 2.0f}, Engine::Vec2{barW, 2.0f}, Engine::Color{30, 40, 58, 170});

        // Left accent strip.
        render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{6.0f * s, barH}, accent);

        // Fill.
        const float fw = std::clamp(fill, 0.0f, 1.0f) * barW;
        if (fw > 0.0f) {
            render_->drawFilledRect(Engine::Vec2{bx, by}, Engine::Vec2{fw, barH}, fillCol);
            render_->drawFilledRect(Engine::Vec2{bx, by + barH - 3.0f}, Engine::Vec2{fw, 3.0f}, Engine::Color{255, 255, 255, 25});
        }

        // Text: label left, value right (centered vertically).
        const float ts = std::clamp(0.90f * s, 0.82f, 1.05f);
        const float ty = by + (barH - measureTextUnified(label, ts).y) * 0.5f + 1.0f * s;
        drawTextUnified(label, Engine::Vec2{bx + 12.0f * s, ty}, ts, Engine::Color{220, 240, 255, 235});
        const Engine::Vec2 vSz = measureTextUnified(value, ts);
        drawTextUnified(value, Engine::Vec2{bx + barW - 10.0f * s - vSz.x, ty}, ts, Engine::Color{235, 245, 255, 235});
    };

    std::ostringstream hpText;
    hpText << static_cast<int>(curHp) << "/" << static_cast<int>(maxHp);
    std::ostringstream shieldText;
    shieldText << static_cast<int>(curShield) << "/" << static_cast<int>(maxShield);
    std::ostringstream energyText;
    energyText << static_cast<int>(std::round(energy_)) << "/" << static_cast<int>(std::round(energyMax_));
    std::ostringstream dashText;
    if (dashCooldownTimer_ <= 0.0f) {
        dashText << "Ready";
    } else {
        dashText << std::fixed << std::setprecision(1) << dashCooldownTimer_ << "s";
    }

    // Slight warning tint when low health.
    const bool lowHp = (maxHp > 0.0f && (curHp / maxHp) < 0.25f);
    Engine::Color hpFill = lowHp ? Engine::Color{230, 90, 90, 235} : Engine::Color{200, 80, 80, 235};
    Engine::Color hpAcc = lowHp ? Engine::Color{255, 140, 140, 235} : Engine::Color{200, 120, 120, 220};

    drawStackedBar(0, uiHpFill_, hpFill, hpAcc, "Health", hpText.str());
    drawStackedBar(1, uiShieldFill_, Engine::Color{80, 150, 255, 235}, Engine::Color{120, 200, 255, 220}, "Shield", shieldText.str());
    drawStackedBar(2, uiEnergyFill_, Engine::Color{90, 200, 255, 235}, Engine::Color{120, 220, 255, 210}, "Energy", energyText.str());
    drawStackedBar(3, uiDashFill_, Engine::Color{110, 170, 130, 230}, Engine::Color{140, 220, 170, 210}, "Dash", dashText.str());
}

void GameRoot::drawAbilityHud(const Engine::InputState& input) {
    if (!render_ || abilities_.empty()) return;
    HUDRect rect = abilityHudRect(static_cast<int>(abilities_.size()), viewportWidth_, viewportHeight_);
    if (rect.w <= 0.0f) return;
    const float iconBox = 52.0f;
    const float gap = 10.0f;
    const float iconSize = 40.0f;  // scaled from 16x16
    const float inset = (iconBox - iconSize) * 0.5f;

    bool leftClick = input.isMouseButtonDown(0);
    bool leftEdge = leftClick && !abilityHudClickPrev_;
    abilityHudClickPrev_ = leftClick;

    int hovered = -1;
    int mx = input.mouseX();
    int my = input.mouseY();

    render_->drawFilledRect(Engine::Vec2{rect.x - 6.0f, rect.y - 6.0f},
                            Engine::Vec2{rect.w + 12.0f, rect.h + 12.0f}, Engine::Color{14, 18, 26, 200});

    // Wizard element indicator
    if (activeArchetype_.id == "wizard") {
        auto elementName = [&]() -> std::string {
            switch (wizardElement_) {
                case WizardElement::Lightning: return "Lightning";
                case WizardElement::Dark: return "Dark";
                case WizardElement::Fire: return "Fire";
                case WizardElement::Ice: return "Ice";
                case WizardElement::Earth: return "Earth";
                case WizardElement::Wind: return "Wind";
                default: return "Element";
            }
        };
        auto elementColor = [&]() -> Engine::Color {
            switch (wizardElement_) {
                case WizardElement::Lightning: return Engine::Color{200, 220, 255, 235};
                case WizardElement::Dark: return Engine::Color{150, 110, 200, 235};
                case WizardElement::Fire: return Engine::Color{255, 170, 120, 235};
                case WizardElement::Ice: return Engine::Color{170, 220, 255, 235};
                case WizardElement::Earth: return Engine::Color{180, 200, 140, 235};
                case WizardElement::Wind: return Engine::Color{200, 240, 230, 235};
                default: return Engine::Color{220, 220, 220, 235};
            }
        };
        int primaryLevel = (!abilityStates_.empty()) ? abilityStates_[0].level : 1;
        int stage = std::clamp((primaryLevel + 1) / 2, 1, 3);
        std::ostringstream lbl;
        lbl << "Element: " << elementName() << " (Stage " << stage << ")";
        float textX = rect.x + rect.w - 180.0f;  // shift left to avoid clipping
        float textY = rect.y - 18.0f;
        drawTextUnified(lbl.str(), Engine::Vec2{textX, textY}, 0.95f, elementColor());
    }

    auto upgradeCost = [](const AbilitySlot& slot) {
        return slot.upgradeCost + (slot.level - 1) * (slot.upgradeCost / 2);
    };

    auto attemptUpgrade = [&](int idx) {
        if (idx < 0 || idx >= static_cast<int>(abilities_.size())) return;
        auto& slot = abilities_[idx];
        auto& st = abilityStates_[idx];
        if (slot.level >= slot.maxLevel) return;
        int cost = upgradeCost(slot);
        if (copper_ < cost) {
            shopNoFundsTimer_ = 0.6;
            return;
        }
        copper_ -= cost;
        slot.level += 1;
        st.level = slot.level;
        if (slot.type == "scatter" || slot.type == "nova" || slot.type == "ultimate") {
            slot.powerScale *= 1.2f;
        } else if (slot.type == "rage") {
            slot.powerScale *= 1.1f;
            slot.cooldownMax = std::max(4.0f, slot.cooldownMax * 0.92f);
        }
    };

    for (std::size_t i = 0; i < abilities_.size(); ++i) {
        float x = rect.x + static_cast<float>(i) * (iconBox + gap);
        float y = rect.y + (rect.h - iconBox) * 0.5f;
        bool inside = mx >= x && mx <= x + iconBox && my >= y && my <= y + iconBox;
        if (inside) hovered = static_cast<int>(i);

        Engine::Color bg{26, 34, 48, static_cast<uint8_t>(inside ? 230 : 200)};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{iconBox, iconBox}, bg);

        const auto& slot = abilities_[i];
        int iconIndex = (i < abilityIconIndices_.size() ? abilityIconIndices_[i] : iconIndexFromName(slot.name));
        if (abilityIconTex_ && iconIndex >= 1 && iconIndex <= 66) {
            int zeroIndex = iconIndex - 1;
            int col = zeroIndex % 11;
            int row = zeroIndex / 11;
            Engine::IntRect src{col * 16, row * 16, 16, 16};
            render_->drawTextureRegion(*abilityIconTex_, Engine::Vec2{x + inset, y + inset},
                                       Engine::Vec2{iconSize, iconSize}, src);
        } else {
            render_->drawFilledRect(Engine::Vec2{x + inset, y + inset}, Engine::Vec2{iconSize, iconSize},
                                    Engine::Color{80, 120, 180, 230});
        }

        // Cooldown fill overlay from bottom (darker the more remaining).
        float cdRatio = (slot.cooldownMax > 0.0f) ? std::clamp(slot.cooldown / slot.cooldownMax, 0.0f, 1.0f) : 0.0f;
        if (cdRatio > 0.0f) {
            float h = iconBox * cdRatio;
            float oy = y + iconBox - h;
            Engine::Color cdCol{10, 14, 24, 180};
            render_->drawFilledRect(Engine::Vec2{x, oy}, Engine::Vec2{iconBox, h}, cdCol);
            // subtle accent near bottom to hint progression
            Engine::Color accent{80, 160, 255, 80};
            render_->drawFilledRect(Engine::Vec2{x, y + iconBox - 6.0f}, Engine::Vec2{iconBox, 6.0f}, accent);
        }

        bool maxed = slot.level >= slot.maxLevel;
        int cost = upgradeCost(slot);
        bool affordable = copper_ >= cost;
        if (maxed) {
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{iconBox, iconBox},
                                    Engine::Color{0, 0, 0, 80});
        } else if (!affordable) {
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{iconBox, iconBox},
                                    Engine::Color{80, 40, 40, 80});
        }

        // Level badge
        std::ostringstream lvl;
        lvl << slot.level;
        Engine::Color badgeBg{18, 24, 34, 220};
        Engine::Color badgeTxt{220, 240, 255, 240};
        float badgeW = 16.0f;
        float badgeH = 14.0f;
        render_->drawFilledRect(Engine::Vec2{x + iconBox - badgeW - 4.0f, y + iconBox - badgeH - 4.0f},
                                Engine::Vec2{badgeW, badgeH}, badgeBg);
        drawTextUnified(lvl.str(), Engine::Vec2{x + iconBox - badgeW - 0.5f, y + iconBox - badgeH - 6.0f}, 0.8f,
                        badgeTxt);

        if (inside && leftEdge && !maxed) {
            attemptUpgrade(static_cast<int>(i));
        }
    }

    if (hovered >= 0 && hovered < static_cast<int>(abilities_.size())) {
        abilityFocus_ = hovered;
        const auto& slot = abilities_[hovered];
        int cost = slot.level >= slot.maxLevel ? -1 : (slot.upgradeCost + (slot.level - 1) * (slot.upgradeCost / 2));
        // Tooltip anchored to cursor.
        float tipW = 260.0f;
        float tipH = 78.0f;
        float tx = static_cast<float>(mx) + 18.0f;
        float ty = static_cast<float>(my) + 18.0f;
        if (tx + tipW > viewportWidth_) tx = static_cast<float>(viewportWidth_) - tipW - 12.0f;
        if (ty + tipH > viewportHeight_) ty = static_cast<float>(viewportHeight_) - tipH - 12.0f;
        render_->drawFilledRect(Engine::Vec2{tx, ty}, Engine::Vec2{tipW, tipH}, Engine::Color{12, 16, 24, 225});
        drawTextUnified(slot.name, Engine::Vec2{tx + 10.0f, ty + 8.0f}, 1.0f, Engine::Color{210, 235, 255, 240});
        drawTextUnified(slot.description, Engine::Vec2{tx + 10.0f, ty + 26.0f}, 0.9f,
                        Engine::Color{190, 210, 235, 230});
        std::ostringstream costTxt;
        costTxt << (cost < 0 ? "MAX" : ("Upgrade " + std::to_string(cost) + "c"));
        drawTextUnified(costTxt.str(), Engine::Vec2{tx + 10.0f, ty + 48.0f}, 0.9f,
                        cost < 0 ? Engine::Color{160, 200, 180, 230}
                                 : Engine::Color{220, 240, 200, 230});
    }
}

void GameRoot::drawCharacterScreen(const Engine::InputState& input) {
    if (!render_ || !characterScreenOpen_) return;
    const float uiScale = 1.16f;  // slight bump for readability
    const bool leftClick = input.isMouseButtonDown(0);
    const bool rightClick = input.isMouseButtonDown(2);
    const bool leftWasDown = characterScreenClickPrev_;
    const bool leftEdge = leftClick && !leftWasDown;
    const bool leftRelease = !leftClick && leftWasDown;
    const bool rightWasDown = characterScreenRightPrev_;
    const bool rightEdge = rightClick && !rightWasDown;
    characterScreenClickPrev_ = leftClick;
    characterScreenRightPrev_ = rightClick;
    const int mx = input.mouseX();
    const int my = input.mouseY();
    Engine::Color scrim{10, 12, 20, 180};
    render_->drawFilledRect(Engine::Vec2{0.0f, 0.0f},
                            Engine::Vec2{static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_)},
                            scrim);
    // Slightly larger overall window so tall stat/detail sections don't clip on 720p-ish viewports.
    const float margin = 16.0f;
    float panelW = std::min(1120.0f, static_cast<float>(viewportWidth_) - margin * 2.0f);
    float panelH = std::min(820.0f, static_cast<float>(viewportHeight_) - margin * 2.0f);
    float px = static_cast<float>(viewportWidth_) * 0.5f - panelW * 0.5f;
    float py = static_cast<float>(viewportHeight_) * 0.5f - panelH * 0.5f;
    // Background + subtle inner border.
    render_->drawFilledRect(Engine::Vec2{px, py}, Engine::Vec2{panelW, panelH}, Engine::Color{14, 18, 26, 238});
    render_->drawFilledRect(Engine::Vec2{px + 1.0f, py + 1.0f}, Engine::Vec2{panelW - 2.0f, panelH - 2.0f},
                            Engine::Color{18, 24, 34, 235});

    auto inside = [&](float x, float y, float w, float h) -> bool {
        return mx >= static_cast<int>(x) && mx <= static_cast<int>(x + w) &&
               my >= static_cast<int>(y) && my <= static_cast<int>(y + h);
    };

    auto rarityCol = [&](ItemRarity r) {
        // Common - White, Uncommon - Green, Rare - Blue, Epic - Deep Purple, Legendary - Gold
        switch (r) {
            case ItemRarity::Common: return Engine::Color{235, 240, 245, 235};
            case ItemRarity::Uncommon: return Engine::Color{135, 235, 155, 235};
            case ItemRarity::Rare: return Engine::Color{135, 190, 255, 235};
            case ItemRarity::Epic: return Engine::Color{185, 135, 255, 235};
            case ItemRarity::Legendary: return Engine::Color{255, 210, 120, 240};
        }
        return Engine::Color{235, 240, 245, 235};
    };

    auto stripRaritySuffix = [](const std::string& n) -> std::string {
        // Legacy RPG display used to append " [Common]" etc. Strip it for cleaner UI.
        static constexpr const char* kSufs[] = {" [Common]", " [Uncommon]", " [Rare]", " [Epic]", " [Legendary]"};
        for (const char* suf : kSufs) {
            const std::size_t len = std::strlen(suf);
            if (n.size() >= len && n.compare(n.size() - len, len, suf) == 0) {
                return n.substr(0, n.size() - len);
            }
        }
        return n;
    };

    auto drawPanel = [&](float x, float y, float w, float h, const std::string& title, float titleYOffset = 10.0f) {
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, Engine::Color{10, 14, 22, 210});
        render_->drawFilledRect(Engine::Vec2{x + 1.0f, y + 1.0f}, Engine::Vec2{w - 2.0f, h - 2.0f}, Engine::Color{14, 18, 26, 215});
        drawTextUnified(title, Engine::Vec2{x + 12.0f, y + titleYOffset}, 1.0f * uiScale, Engine::Color{210, 235, 255, 240});
    };

    drawTextUnified("Character", Engine::Vec2{px + 18.0f, py + 14.0f}, 1.2f * uiScale, Engine::Color{220, 245, 255, 245});
    drawTextUnified(multiplayerEnabled_ ? "Multiplayer (no pause)" : "Single-player (paused)",
                    Engine::Vec2{px + 20.0f, py + 36.0f}, 0.88f * uiScale,
                    multiplayerEnabled_ ? Engine::Color{180, 220, 255, 220} : Engine::Color{190, 240, 200, 220});
    drawTextUnified("Tip: Hover for details • Click inventory to equip • Click equipped slot to unequip",
                    Engine::Vec2{px + 420.0f, py + 22.0f}, 0.78f * uiScale, Engine::Color{160, 190, 215, 210});

    // 3-column layout.
    const float pad = 14.0f;
    const float contentY = py + 60.0f;
    const float contentH = panelH - 74.0f;
    float col1W = std::floor(panelW * 0.30f);
    float col3W = std::floor(panelW * 0.28f);
    float col2W = panelW - (col1W + col3W + pad * 4.0f);
    float col1X = px + pad;
    float col2X = col1X + col1W + pad;
    float col3X = col2X + col2W + pad;

    // Hovered item for tooltip/detail.
    const ItemDefinition* hoveredDef = nullptr;
    const ItemDefinition* focusedDef = nullptr;
    const ItemInstance* focusedInst = nullptr;
    bool focusedIsEquipped = false;
    bool hoveredIsEquipped = false;
    // Defer mutations to avoid invalidating hovered pointers mid-frame.
    std::optional<std::size_t> pendingEquip{};
    std::optional<Game::RPG::EquipmentSlot> pendingUnequip{};
    std::optional<std::size_t> pendingUnsocket{};
    struct PendingSocket {
        std::size_t inventoryIdx{0};
        std::size_t equippedSlotIdx{0};
    };
    std::optional<PendingSocket> pendingSocket{};
    struct PendingSwap {
        std::size_t a{0};
        std::size_t b{0};
    };
    std::optional<PendingSwap> pendingSwap{};
    // Persist the last hovered equipped item as the socket target so the player can
    // hover a weapon/gear, then move to a gem and click to socket it.
    auto socketTarget = [&]() -> std::optional<std::size_t> { return characterScreenSocketTargetSlotIdx_; };
    auto setSocketTarget = [&](std::optional<std::size_t> v) { characterScreenSocketTargetSlotIdx_ = v; };
    std::optional<std::size_t> hoveredEquipSlotIdx{};
    std::optional<std::size_t> hoveredInvIdx{};

    // ----- Left: Equipment -----
    // Size the equipment panel to fit the slot grid (Bag slot adds another row).
    struct SlotCell { Game::RPG::EquipmentSlot slot; const char* label; };
    const std::array<SlotCell, 15> slotCells{{
        {Game::RPG::EquipmentSlot::Head,     "Head"},
        {Game::RPG::EquipmentSlot::MainHand, "Main"},
        {Game::RPG::EquipmentSlot::Chest,    "Chest"},
        {Game::RPG::EquipmentSlot::OffHand,  "Off"},
        {Game::RPG::EquipmentSlot::Legs,     "Legs"},
        {Game::RPG::EquipmentSlot::Ring1,    "Ring 1"},
        {Game::RPG::EquipmentSlot::Boots,    "Boots"},
        {Game::RPG::EquipmentSlot::Ring2,    "Ring 2"},
        {Game::RPG::EquipmentSlot::Gloves,   "Gloves"},
        {Game::RPG::EquipmentSlot::Amulet,   "Amulet"},
        {Game::RPG::EquipmentSlot::Belt,     "Belt"},
        {Game::RPG::EquipmentSlot::Cloak,    "Cloak"},
        {Game::RPG::EquipmentSlot::Ammo,     "Ammo"},
        {Game::RPG::EquipmentSlot::Bag,      "Bag"},
        {Game::RPG::EquipmentSlot::Trinket,  "Trinket"},
    }};
    const float cellPad = 10.0f;
    const float cellW = (col1W - cellPad * 3.0f) * 0.5f;
    const float cellH = 34.0f;
    const float icon = 22.0f;
    const float startY = contentY + 52.0f;
    const int equipRows = static_cast<int>((slotCells.size() + 1) / 2);
    const float requiredEquipH = (startY - contentY) + static_cast<float>(equipRows) * (cellH + 6.0f) + 10.0f;
    const float minStatsH = 160.0f;
    const float equipH = std::min(std::max(360.0f, requiredEquipH), contentH - minStatsH - pad);

    drawPanel(col1X, contentY, col1W, equipH, "Equipment");
    drawTextUnified("Slots", Engine::Vec2{col1X + 12.0f, contentY + 32.0f}, 0.82f * uiScale, Engine::Color{170, 205, 230, 215});
    for (std::size_t i = 0; i < slotCells.size(); ++i) {
        int c = static_cast<int>(i % 2);
        int r = static_cast<int>(i / 2);
        float x = col1X + cellPad + static_cast<float>(c) * (cellW + cellPad);
        float y = startY + static_cast<float>(r) * (cellH + 6.0f);
        bool hov = inside(x, y, cellW, cellH);
        Engine::Color bg = hov ? Engine::Color{20, 28, 42, 235} : Engine::Color{16, 22, 32, 225};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cellW, cellH}, bg);

        const std::size_t slotIdx = static_cast<std::size_t>(slotCells[i].slot);
        const bool hasItem = slotIdx < equipped_.size() && equipped_[slotIdx].has_value();
        Engine::Color border = Engine::Color{40, 55, 80, 200};
        if (hasItem) border = rarityCol(equipped_[slotIdx]->def.rarity);
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cellW, 1.0f}, border);
        render_->drawFilledRect(Engine::Vec2{x, y + cellH - 1.0f}, Engine::Vec2{cellW, 1.0f}, border);

        render_->drawFilledRect(Engine::Vec2{x + 8.0f, y + 6.0f}, Engine::Vec2{icon, icon}, Engine::Color{30, 40, 58, 230});
        if (hasItem) {
            drawItemIcon(equipped_[slotIdx]->def, Engine::Vec2{x + 8.0f, y + 6.0f}, icon);
        }
        drawTextUnified(slotCells[i].label, Engine::Vec2{x + 8.0f + icon + 8.0f, y + 0.0f}, 0.80f * uiScale,
                        Engine::Color{190, 210, 230, 225});
        if (hasItem) {
            const std::string raw = stripRaritySuffix(equipped_[slotIdx]->def.name);
            const float nameX = x + 8.0f + icon + 8.0f;
            const float maxNameW = cellW - (nameX - x) - 10.0f;
            const std::string nm = ellipsizeTextUnified(raw, maxNameW, 0.72f * uiScale);
            drawTextUnified(nm, Engine::Vec2{x + 8.0f + icon + 8.0f, y + 11.0f}, 0.72f * uiScale,
                            rarityCol(equipped_[slotIdx]->def.rarity));
        } else {
            drawTextUnified("Empty", Engine::Vec2{x + 8.0f + icon + 8.0f, y + 11.0f}, 0.72f * uiScale,
                            Engine::Color{125, 150, 175, 180});
        }

        if (hov) {
            hoveredIsEquipped = true;
            hoveredEquipSlotIdx = slotIdx;
            if (hasItem) {
                hoveredDef = &equipped_[slotIdx]->def;
                focusedDef = hoveredDef;
                focusedInst = &(*equipped_[slotIdx]);
                focusedIsEquipped = true;
                setSocketTarget(slotIdx);
            }
            if (leftEdge && hasItem) pendingUnequip = slotCells[i].slot;
            if (rightEdge && hasItem) {
                const auto& def = equipped_[slotIdx]->def;
                if (!def.rpgSocketed.empty()) {
                    pendingUnsocket = slotIdx;
                }
            }
        }
    }

    // ----- Left: Stats (fill space) -----
    float statsY = contentY + equipH + pad;
    float statsH = contentH - equipH - pad;
    drawPanel(col1X, statsY, col1W, statsH, "Stats");

    const auto* hp = registry_.get<Engine::ECS::Health>(hero_);
    const float maxHp = hp ? hp->maxHealth : 0.0f;
    const float maxShield = hp ? hp->maxShields : 0.0f;
    const auto* rpg = registry_.get<Engine::ECS::RPGStats>(hero_);
    const bool hasRpg = (rpg != nullptr);
    float lineY = statsY + 38.0f;
    auto statLine = [&](const std::string& k, const std::string& v, Engine::Color kc = Engine::Color{185, 205, 225, 220}) {
        drawTextUnified(k, Engine::Vec2{col1X + 12.0f, lineY}, 0.86f * uiScale, kc);
        drawTextUnified(v, Engine::Vec2{col1X + col1W - 120.0f, lineY}, 0.86f * uiScale, Engine::Color{220, 240, 255, 230});
        lineY += 18.0f * uiScale;
    };
    if (hasRpg) {
        statLine("RPG Combat", useRpgCombat_ ? "ON" : "OFF (preview)", useRpgCombat_ ? Engine::Color{170, 235, 190, 230}
                                                                                   : Engine::Color{235, 210, 150, 230});
    }
    statLine("Health", std::to_string(static_cast<int>(hasRpg ? std::round(rpg->derived.healthMax) : maxHp)));
    statLine("Shields", std::to_string(static_cast<int>(hasRpg ? std::round(rpg->derived.shieldMax) : maxShield)));
    statLine("Energy", std::to_string(static_cast<int>(energyMax_)));
    statLine("Damage", std::to_string(static_cast<int>(std::round(hasRpg ? rpg->derived.attackPower : projectileDamage_))));
    {
        std::ostringstream s;
        s.setf(std::ios::fixed);
        s << std::setprecision(2) << (hasRpg ? rpg->derived.attackSpeed : (attackSpeedMul_ * attackSpeedBuffMul_));
        statLine("Attack Speed x", s.str());
    }
    statLine("Move Speed", std::to_string(static_cast<int>(std::round(hasRpg ? rpg->derived.moveSpeed : heroMoveSpeed_))));

    if (hasRpg) {
        lineY += 10.0f;
        drawTextUnified("RPG Details", Engine::Vec2{col1X + 12.0f, lineY}, 0.86f * uiScale,
                        Engine::Color{200, 230, 255, 235});
        lineY += 18.0f;
        statLine("Armor", std::to_string(static_cast<int>(std::round(rpg->derived.armor))));
        statLine("Crit %", std::to_string(static_cast<int>(std::round(rpg->derived.critChance * 100.0f))));
        statLine("Tenacity", std::to_string(static_cast<int>(std::round(rpg->derived.tenacity))));
    }

    // ----- Middle: Inventory grid -----
    drawPanel(col2X, contentY, col2W, contentH, "Inventory", 0.0f);
    drawTextUnified("Capacity: " + std::to_string(inventory_.size()) + "/" + std::to_string(inventoryCapacity_),
                    Engine::Vec2{col2X + col2W - 190.0f, contentY + 12.0f}, 0.80f * uiScale,
                    Engine::Color{160, 190, 215, 210});

    const float gridPad = 16.0f;
    float gridX = col2X + gridPad;
    float gridY = contentY + 42.0f;
    float gridW = col2W - gridPad * 2.0f;
    float gridH = contentH - 56.0f;
    const int cols = 4;
    const float gap = 10.0f;
    const float cell = std::floor(std::clamp((gridW - (cols - 1) * gap) / cols, 54.0f, 82.0f));
    const float iconSz = cell - 14.0f;

    // Hotbar slots (R/F) live above the inventory grid.
    auto isHotbarEligible = [&](const ItemDefinition& d) {
        return !d.rpgConsumableId.empty() ||
               d.effect == ItemEffect::Heal || d.effect == ItemEffect::FreezeTime ||
               d.effect == ItemEffect::Turret || d.effect == ItemEffect::Lifesteal ||
               d.effect == ItemEffect::AttackSpeed || d.effect == ItemEffect::Chain;
    };
    auto findInventoryIndexById = [&](int id) -> std::optional<std::size_t> {
        for (std::size_t i = 0; i < inventory_.size(); ++i) {
            if (inventory_[i].def.id == id) return i;
        }
        return std::nullopt;
    };
    std::optional<int> hoveredHotbarIdx{};
    {
        const float hotH = 44.0f;
        const float slot = 42.0f;
        const float hbY = contentY + 38.0f;
        const float hbX = gridX;
        drawTextUnified("Hotbar", Engine::Vec2{hbX, hbY - 18.0f}, 0.82f * uiScale, Engine::Color{170, 205, 230, 215});
        drawTextUnified("R / F", Engine::Vec2{hbX + 80.0f, hbY - 18.0f}, 0.78f * uiScale, Engine::Color{140, 170, 195, 210});
        for (int s = 0; s < 2; ++s) {
            float x = hbX + static_cast<float>(s) * (slot + 12.0f);
            float y = hbY;
            bool hov = inside(x, y, slot, hotH);
            if (hov) hoveredHotbarIdx = s;
            Engine::Color bg = hov ? Engine::Color{24, 34, 48, 235} : Engine::Color{16, 22, 32, 220};
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{slot, hotH}, bg);
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{slot, 2.0f}, Engine::Color{40, 55, 80, 200});
            render_->drawFilledRect(Engine::Vec2{x, y + hotH - 2.0f}, Engine::Vec2{slot, 2.0f}, Engine::Color{40, 55, 80, 200});

            if (hotbarItemIds_[s].has_value()) {
                auto invIdx = findInventoryIndexById(*hotbarItemIds_[s]);
                if (invIdx.has_value() && *invIdx < inventory_.size()) {
                    const auto& inst = inventory_[*invIdx];
                    Engine::Color border = rarityCol(inst.def.rarity);
                    render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{slot, 2.0f}, border);
                    render_->drawFilledRect(Engine::Vec2{x, y + hotH - 2.0f}, Engine::Vec2{slot, 2.0f}, border);
                    drawItemIcon(inst.def, Engine::Vec2{x + 10.0f, y + 10.0f}, slot - 20.0f);
                } else {
                    hotbarItemIds_[s].reset();
                }
            }
            const char* key = (s == 0) ? "R" : "F";
            drawTextUnified(key, Engine::Vec2{x + 3.0f, y + hotH - 16.0f}, 0.75f * uiScale, Engine::Color{160, 190, 215, 200});
            if (hov && rightEdge) {
                hotbarItemIds_[s].reset();
            }
        }

        // Shift grid down to make room.
        gridY += hotH + 12.0f;
        gridH -= hotH + 12.0f;
    }

    const int totalRows = std::max(1, (inventoryCapacity_ + cols - 1) / cols);
    const int visibleRows = std::max(1, static_cast<int>(std::floor((gridH + gap) / (cell + gap))));
    const int maxScrollRows = std::max(0, totalRows - visibleRows);
    const bool gridHover = inside(gridX, gridY, gridW, gridH);
    if (gridHover && scrollDeltaFrame_ != 0) {
        inventoryGridScroll_ = std::clamp(inventoryGridScroll_ - static_cast<float>(scrollDeltaFrame_), 0.0f,
                                          static_cast<float>(maxScrollRows));
    }
    const int rowOffset = std::clamp(static_cast<int>(std::round(inventoryGridScroll_)), 0, maxScrollRows);

    // Scrollbar
    if (maxScrollRows > 0) {
        const float barW = 10.0f;
        const float trackX = gridX + gridW - barW;
        const float trackY = gridY;
        const float trackH = gridH;
        render_->drawFilledRect(Engine::Vec2{trackX, trackY}, Engine::Vec2{barW, trackH}, Engine::Color{10, 14, 22, 160});
        const float thumbH = std::max(18.0f, trackH * (static_cast<float>(visibleRows) / static_cast<float>(totalRows)));
        const float ratio = static_cast<float>(rowOffset) / static_cast<float>(maxScrollRows);
        const float thumbY = trackY + (trackH - thumbH) * ratio;
        render_->drawFilledRect(Engine::Vec2{trackX, thumbY}, Engine::Vec2{barW, thumbH}, Engine::Color{90, 140, 210, 220});
    }

    const int firstIdx = rowOffset * cols;
    const int lastIdxExclusive = std::min(inventoryCapacity_, (rowOffset + visibleRows) * cols);
    for (int i = firstIdx; i < lastIdxExclusive; ++i) {
        int c = i % cols;
        int r = (i / cols) - rowOffset;
        float x = gridX + c * (cell + gap);
        float y = gridY + r * (cell + gap);
        bool hov = inside(x, y, cell, cell);
        Engine::Color bg = hov ? Engine::Color{22, 30, 44, 235} : Engine::Color{16, 22, 32, 220};
        render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cell, cell}, bg);
        if (i == inventorySelected_) {
            Engine::Color sel{210, 255, 255, 220};
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cell, 2.0f}, sel);
            render_->drawFilledRect(Engine::Vec2{x, y + cell - 2.0f}, Engine::Vec2{cell, 2.0f}, sel);
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{2.0f, cell}, sel);
            render_->drawFilledRect(Engine::Vec2{x + cell - 2.0f, y}, Engine::Vec2{2.0f, cell}, sel);
        }

        if (i < static_cast<int>(inventory_.size())) {
            const auto& inst = inventory_[static_cast<std::size_t>(i)];
            Engine::Color border = rarityCol(inst.def.rarity);
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cell, 2.0f}, border);
            render_->drawFilledRect(Engine::Vec2{x, y + cell - 2.0f}, Engine::Vec2{cell, 2.0f}, border);
            render_->drawFilledRect(Engine::Vec2{x + 7.0f, y + 7.0f}, Engine::Vec2{iconSz, iconSz},
                                    Engine::Color{30, 40, 58, 230});
            drawItemIcon(inst.def, Engine::Vec2{x + 7.0f, y + 7.0f}, iconSz);
            if (inst.quantity > 1) {
                drawTextUnified(std::to_string(inst.quantity), Engine::Vec2{x + cell - 18.0f, y + cell - 18.0f},
                                0.75f * uiScale, Engine::Color{220, 240, 255, 220});
            }
            if (hov) {
                hoveredDef = &inst.def;
                focusedDef = hoveredDef;
                focusedInst = &inst;
                focusedIsEquipped = false;
                hoveredIsEquipped = false;
                hoveredInvIdx = static_cast<std::size_t>(i);
                if (leftEdge) {
                    inventorySelected_ = i;
                    characterDragArmed_ = true;
                    characterDragActive_ = false;
                    characterDragInvIdx_ = static_cast<std::size_t>(i);
                    characterDragStartX_ = mx;
                    characterDragStartY_ = my;
                }
            }
        } else {
            Engine::Color border = Engine::Color{30, 40, 58, 200};
            render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{cell, 1.0f}, border);
            render_->drawFilledRect(Engine::Vec2{x, y + cell - 1.0f}, Engine::Vec2{cell, 1.0f}, border);
        }
    }

    // Drag/drop state transitions:
    // - Press on an inventory item arms a drag.
    // - Move beyond a small threshold starts dragging.
    // - Release while armed acts as a click (equip/socket).
    // - Release while dragging attempts drop (swap/equip/socket).
    if (characterDragArmed_ && leftClick) {
        const int dx = mx - characterDragStartX_;
        const int dy = my - characterDragStartY_;
        if ((dx * dx + dy * dy) >= 36) {  // 6px threshold
            characterDragActive_ = true;
            characterDragArmed_ = false;
        }
    }
    if (characterDragActive_) {
        if (characterDragInvIdx_ < inventory_.size()) {
            const auto& dragInst = inventory_[characterDragInvIdx_];
            drawItemIcon(dragInst.def, Engine::Vec2{static_cast<float>(mx) - 14.0f, static_cast<float>(my) - 14.0f}, 28.0f);
            drawTextUnified(stripRaritySuffix(dragInst.def.name),
                            Engine::Vec2{static_cast<float>(mx) + 18.0f, static_cast<float>(my) - 10.0f},
                            0.78f * uiScale, rarityCol(dragInst.def.rarity));
        }
        if (leftRelease) {
            if (characterDragInvIdx_ < inventory_.size()) {
                const auto* dragT = findRpgTemplateFor(inventory_[characterDragInvIdx_].def);
                const bool dragIsGem = dragT && dragT->socketable;
                const bool dragIsConsumable = isHotbarEligible(inventory_[characterDragInvIdx_].def);
                if (hoveredHotbarIdx.has_value() && dragIsConsumable) {
                    hotbarItemIds_[*hoveredHotbarIdx] = inventory_[characterDragInvIdx_].def.id;
                } else
                if (hoveredEquipSlotIdx.has_value() && *hoveredEquipSlotIdx < equipped_.size()) {
                    if (dragIsGem && equipped_[*hoveredEquipSlotIdx].has_value()) {
                        auto& targetDef = equipped_[*hoveredEquipSlotIdx]->def;
                        if (targetDef.rpgSocketsMax > 0 &&
                            static_cast<int>(targetDef.rpgSocketed.size()) < targetDef.rpgSocketsMax) {
                            pendingSocket = PendingSocket{characterDragInvIdx_, *hoveredEquipSlotIdx};
                        }
                    } else {
                        pendingEquip = characterDragInvIdx_;
                    }
                } else if (hoveredInvIdx.has_value() && *hoveredInvIdx < inventory_.size()) {
                    if (*hoveredInvIdx != characterDragInvIdx_) {
                        pendingSwap = PendingSwap{characterDragInvIdx_, *hoveredInvIdx};
                    }
                }
            }
            characterDragActive_ = false;
        }
        if (characterDragInvIdx_ >= inventory_.size()) {
            characterDragActive_ = false;
        }
    }
    if (characterDragArmed_ && leftRelease) {
        // Click behavior (no drag): equip, or socket if gem + socket target has room.
        characterDragArmed_ = false;
        if (characterDragInvIdx_ < inventory_.size()) {
            const auto& inst = inventory_[characterDragInvIdx_];
            const auto* gemT = findRpgTemplateFor(inst.def);
            const bool isGem = gemT && gemT->socketable;
            auto tgt = socketTarget();
            if (isGem && tgt.has_value() &&
                *tgt < equipped_.size() && equipped_[*tgt].has_value()) {
                auto& targetDef = equipped_[*tgt]->def;
                if (targetDef.rpgSocketsMax > 0 &&
                    static_cast<int>(targetDef.rpgSocketed.size()) < targetDef.rpgSocketsMax) {
                    pendingSocket = PendingSocket{characterDragInvIdx_, *tgt};
                } else {
                    pendingEquip = characterDragInvIdx_;
                }
            } else {
                pendingEquip = characterDragInvIdx_;
            }
        }
    }

    // ----- Right: Run + Abilities + Details -----
    const float cardGap = 12.0f;
    const float runH = 140.0f;
    const float abilH = 160.0f;
    float detailsY = contentY + runH + cardGap + abilH + cardGap;
    float detailsH = contentH - (runH + abilH + cardGap * 2.0f);

    drawPanel(col3X, contentY, col3W, runH, "Run");
    float ry = contentY + 32.0f;
    auto runLine = [&](const std::string& k, const std::string& v) {
        drawTextUnified(k, Engine::Vec2{col3X + 12.0f, ry}, 0.84f * uiScale, Engine::Color{185, 205, 225, 220});
        drawTextUnified(v, Engine::Vec2{col3X + col3W - 100.0f, ry}, 0.84f * uiScale, Engine::Color{220, 240, 255, 230});
        ry += 18.0f * uiScale;
    };
    runLine("Wave", std::to_string(wave_ + 1));
    runLine("Kills", std::to_string(kills_));
    runLine("Copper", std::to_string(copper_));
    runLine("Gold", std::to_string(gold_));
    runLine("Level", std::to_string(level_));

    drawPanel(col3X, contentY + runH + cardGap, col3W, abilH, "Abilities");
    float ay = contentY + runH + cardGap + 36.0f;
    for (std::size_t i = 0; i < abilities_.size() && i < 6; ++i) {
        const auto& slot = abilities_[i];
        std::ostringstream s;
        s << slot.name << "  Lv " << slot.level;
        drawTextUnified(s.str(), Engine::Vec2{col3X + 12.0f, ay}, 0.80f * uiScale, Engine::Color{200, 230, 255, 230});
        ay += 18.0f * uiScale;
        if (ay > contentY + runH + cardGap + abilH - 18.0f) break;
    }

    drawPanel(col3X, detailsY, col3W, detailsH, "Details");
    const ItemDefinition* detail = focusedDef;
    const ItemInstance* detailInst = focusedInst;
    bool detailIsEquipped = focusedIsEquipped;
    if (!detail && inventorySelected_ >= 0 && inventorySelected_ < static_cast<int>(inventory_.size())) {
        detail = &inventory_[static_cast<std::size_t>(inventorySelected_)].def;
        detailInst = &inventory_[static_cast<std::size_t>(inventorySelected_)];
        detailIsEquipped = false;
    }
    float dy = detailsY + 38.0f;
    if (!detail) {
        drawTextUnified("Hover an item or slot", Engine::Vec2{col3X + 12.0f, dy}, 0.86f * uiScale,
                        Engine::Color{170, 200, 230, 220});
    } else {
        const std::string dn = stripRaritySuffix(detail->name);
        drawTextUnified(dn, Engine::Vec2{col3X + 12.0f, dy}, 0.92f * uiScale, rarityCol(detail->rarity));
        dy += 20.0f * uiScale;
        drawTextUnified(detail->desc, Engine::Vec2{col3X + 12.0f, dy}, 0.78f * uiScale,
                        Engine::Color{175, 200, 225, 220});
        dy += 18.0f * uiScale;

        int sell = std::max(1, detail->cost / 2);
        drawTextUnified("Sell: " + std::to_string(sell) + "c", Engine::Vec2{col3X + 12.0f, dy}, 0.80f * uiScale,
                        Engine::Color{180, 255, 200, 220});
        dy += 18.0f * uiScale;

        // RPG stats breakdown (base template + affixes).
        if (!detail->rpgTemplateId.empty()) {
            const auto* t = findRpgTemplateFor(*detail);
            auto findAffix = [&](const std::string& id) -> const Game::RPG::Affix* {
                for (const auto& a : rpgLootTable_.affixes) if (a.id == id) return &a;
                return nullptr;
            };
            Engine::Gameplay::RPG::StatContribution total{};
            if (t) addContribution(total, t->baseStats, 1.0f);
            for (const auto& id : detail->rpgAffixIds) {
                if (const auto* a = findAffix(id)) addContribution(total, a->stats, 1.0f);
            }
            // Socketed gems contributions.
            for (const auto& g : detail->rpgSocketed) {
                if (const auto* gt = findRpgTemplateById(g.templateId)) {
                    addContribution(total, gt->baseStats, 1.0f);
                }
                for (const auto& ga : g.affixIds) {
                    if (const auto* a = findAffix(ga)) addContribution(total, a->stats, 1.0f);
                }
            }
            auto addLine = [&](const std::string& s) {
                drawTextUnified(s, Engine::Vec2{col3X + 14.0f, dy}, 0.78f * uiScale, Engine::Color{190, 220, 255, 220});
                dy += 16.0f * uiScale;
            };
            auto pct = [&](float v) { return std::to_string(static_cast<int>(std::round(v * 100.0f))) + "%"; };
            dy += 4.0f;
            drawTextUnified("Bonuses", Engine::Vec2{col3X + 12.0f, dy}, 0.84f * uiScale, Engine::Color{200, 230, 255, 230});
            dy += 18.0f * uiScale;
            if (total.flat.attackPower != 0.0f) addLine((total.flat.attackPower > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.attackPower))) + " Attack Power");
            if (total.flat.spellPower != 0.0f) addLine((total.flat.spellPower > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.spellPower))) + " Spell Power");
            if (total.flat.armor != 0.0f) addLine((total.flat.armor > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.armor))) + " Armor");
            if (total.flat.healthMax != 0.0f) addLine((total.flat.healthMax > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.healthMax))) + " Max Health");
            if (total.flat.shieldMax != 0.0f) addLine((total.flat.shieldMax > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.shieldMax))) + " Max Shields");
            if (total.flat.critChance != 0.0f) addLine((total.flat.critChance > 0 ? "+" : "") + pct(total.flat.critChance) + " Crit Chance");
            if (total.flat.armorPen != 0.0f) addLine((total.flat.armorPen > 0 ? "+" : "") + std::to_string(static_cast<int>(std::round(total.flat.armorPen))) + " Armor Pen");
            if (total.mult.attackSpeed != 0.0f) addLine((total.mult.attackSpeed > 0 ? "+" : "") + pct(total.mult.attackSpeed) + " Attack Speed");
            if (total.mult.moveSpeed != 0.0f) addLine((total.mult.moveSpeed > 0 ? "+" : "") + pct(total.mult.moveSpeed) + " Move Speed");

            // Compare view: preview derived stat deltas if this inventory item were equipped.
            // (Equipped item hover shows its own details; inventory hover previews impact before equipping.)
            if (!detailIsEquipped && detailInst && t && !t->socketable && hero_ != Engine::ECS::kInvalidEntity) {
                auto contributionFromEquipped = [&](const auto& eq) {
                    Engine::Gameplay::RPG::StatContribution contrib{};
                    auto findAffix2 = [&](const std::string& id) -> const Game::RPG::Affix* {
                        for (const auto& a : rpgLootTable_.affixes) if (a.id == id) return &a;
                        return nullptr;
                    };
                    for (const auto& opt : eq) {
                        if (!opt.has_value()) continue;
                        const auto& inst2 = *opt;
                        const auto& def2 = inst2.def;
                        const auto* tt = findRpgTemplateById(def2.rpgTemplateId);
                        if (tt) addContribution(contrib, tt->baseStats, static_cast<float>(std::max(1, inst2.quantity)));
                        for (const auto& affId : def2.rpgAffixIds) {
                            if (const auto* a = findAffix2(affId)) addContribution(contrib, a->stats, static_cast<float>(std::max(1, inst2.quantity)));
                        }
                        for (const auto& g : def2.rpgSocketed) {
                            if (const auto* gt = findRpgTemplateById(g.templateId)) {
                                addContribution(contrib, gt->baseStats, static_cast<float>(std::max(1, inst2.quantity)));
                            }
                            for (const auto& ga : g.affixIds) {
                                if (const auto* a = findAffix2(ga)) addContribution(contrib, a->stats, static_cast<float>(std::max(1, inst2.quantity)));
                            }
                        }
                    }
                    return contrib;
                };

                auto computeDerivedForEquipped = [&](const auto& eq) {
                    Engine::Gameplay::RPG::StatContribution mods = contributionFromEquipped(eq);
                    addContribution(mods, collectRpgTalentContribution(), 1.0f);
                    Engine::Gameplay::RPG::AggregationInput input{};
                    input.attributes = activeArchetype_.rpgAttributes;
                    input.base.baseHealth = heroMaxHp_;
                    input.base.baseShields = heroShield_;
                    input.base.baseArmor = heroHealthArmor_;
                    input.base.baseMoveSpeed = heroMoveSpeed_;
                    input.base.baseAttackPower = projectileDamage_;
                    input.base.baseSpellPower = projectileDamage_;
                    input.base.baseAttackSpeed = std::max(0.1f, attackSpeedMul_ * attackSpeedBuffMul_);
                    input.contributions = {mods};
                    return Engine::Gameplay::RPG::aggregateDerivedStats(input);
                };

                auto slotIndex = [&](Game::RPG::EquipmentSlot slot) -> std::size_t {
                    return static_cast<std::size_t>(slot);
                };
                std::size_t target = slotIndex(t->slot);
                if (t->slot == Game::RPG::EquipmentSlot::Ring1) {
                    const std::size_t r1 = slotIndex(Game::RPG::EquipmentSlot::Ring1);
                    const std::size_t r2 = slotIndex(Game::RPG::EquipmentSlot::Ring2);
                    if (r1 < equipped_.size() && !equipped_[r1].has_value()) target = r1;
                    else if (r2 < equipped_.size() && !equipped_[r2].has_value()) target = r2;
                    else target = r1;
                }

                const Engine::Gameplay::RPG::DerivedStats cur = (registry_.has<Engine::ECS::RPGStats>(hero_) && registry_.get<Engine::ECS::RPGStats>(hero_))
                                                                  ? registry_.get<Engine::ECS::RPGStats>(hero_)->derived
                                                                  : computeDerivedForEquipped(equipped_);
                auto eq2 = equipped_;
                if (target < eq2.size()) {
                    eq2[target] = *detailInst;
                }
                const Engine::Gameplay::RPG::DerivedStats nxt = computeDerivedForEquipped(eq2);

                auto deltaColor = [&](float d) {
                    if (d > 0.0001f) return Engine::Color{155, 235, 175, 230};
                    if (d < -0.0001f) return Engine::Color{255, 165, 165, 230};
                    return Engine::Color{185, 210, 235, 220};
                };
                auto addDeltaLine = [&](const std::string& label, float d, const std::string& suffix = std::string{}) {
                    if (std::abs(d) < 0.0001f) return;
                    std::ostringstream s;
                    s.setf(std::ios::fixed);
                    s << label << " ";
                    if (suffix == "%") {
                        s << (d >= 0.0f ? "+" : "") << std::setprecision(0) << (d * 100.0f) << "%";
                    } else if (suffix == "x") {
                        s << (d >= 0.0f ? "+" : "") << std::setprecision(2) << d;
                    } else {
                        s << (d >= 0.0f ? "+" : "") << std::setprecision(0) << d;
                        if (!suffix.empty()) s << suffix;
                    }
                    drawTextUnified(s.str(), Engine::Vec2{col3X + 14.0f, dy}, 0.78f * uiScale, deltaColor(d));
                    dy += 16.0f * uiScale;
                };

                dy += 6.0f;
                drawTextUnified("Compare (equip)", Engine::Vec2{col3X + 12.0f, dy}, 0.84f * uiScale, Engine::Color{200, 230, 255, 230});
                dy += 18.0f * uiScale;
                addDeltaLine("Attack Power", nxt.attackPower - cur.attackPower);
                addDeltaLine("Attack Speed", nxt.attackSpeed - cur.attackSpeed, "x");
                addDeltaLine("Move Speed", nxt.moveSpeed - cur.moveSpeed);
                addDeltaLine("Max Health", nxt.healthMax - cur.healthMax);
                addDeltaLine("Max Shields", nxt.shieldMax - cur.shieldMax);
                addDeltaLine("Armor", nxt.armor - cur.armor);
                addDeltaLine("Crit", nxt.critChance - cur.critChance, "%");
            }

            if (detail->rpgSocketsMax > 0) {
                dy += 4.0f;
                std::ostringstream s;
                s << "Sockets: " << detail->rpgSocketed.size() << "/" << detail->rpgSocketsMax;
                drawTextUnified(s.str(), Engine::Vec2{col3X + 12.0f, dy}, 0.84f * uiScale, Engine::Color{200, 230, 255, 230});
                dy += 18.0f * uiScale;
                for (std::size_t i = 0; i < detail->rpgSocketed.size() && i < 3; ++i) {
                    const auto& g = detail->rpgSocketed[i];
                    std::string nm = g.templateId;
                    if (const auto* gt = findRpgTemplateById(g.templateId)) nm = gt->name;
                    addLine("• " + nm);
                }
                if (detail->rpgSocketed.size() < static_cast<std::size_t>(detail->rpgSocketsMax)) {
                    addLine("• (empty)");
                }
            }

            // Show a couple of affix name lines for flavor.
            if (!detail->affixes.empty()) {
                dy += 4.0f;
                drawTextUnified("Affixes", Engine::Vec2{col3X + 12.0f, dy}, 0.84f * uiScale, Engine::Color{200, 230, 255, 230});
                dy += 18.0f * uiScale;
                for (std::size_t i = 0; i < detail->affixes.size() && i < 4; ++i) {
                    addLine("• " + detail->affixes[i]);
                }
            }
        } else if (!detail->affixes.empty()) {
            drawTextUnified("Affixes", Engine::Vec2{col3X + 12.0f, dy}, 0.84f * uiScale, Engine::Color{200, 230, 255, 230});
            dy += 18.0f * uiScale;
            for (std::size_t i = 0; i < detail->affixes.size() && i < 6; ++i) {
                drawTextUnified("• " + detail->affixes[i], Engine::Vec2{col3X + 14.0f, dy}, 0.78f * uiScale,
                                Engine::Color{185, 220, 255, 215});
                dy += 16.0f * uiScale;
            }
        }
    }

    // Hover tooltip near mouse (lightweight; complements Details card).
    if (hoveredDef) {
        std::vector<std::string> lines;
        lines.push_back(stripRaritySuffix(hoveredDef->name));
        if (!hoveredDef->rpgTemplateId.empty()) lines.push_back(hoveredDef->desc);
        if (!hoveredDef->affixes.empty()) {
            for (std::size_t i = 0; i < hoveredDef->affixes.size() && i < 3; ++i) {
                lines.push_back("• " + hoveredDef->affixes[i]);
            }
        }
        std::string clickHint = hoveredIsEquipped ? "Click: Unequip" : "Click: Equip";
        if (!hoveredIsEquipped) {
            const auto* t = findRpgTemplateFor(*hoveredDef);
            auto tgt = socketTarget();
            if (t && t->socketable && tgt.has_value() &&
                *tgt < equipped_.size() && equipped_[*tgt].has_value()) {
                const auto& targetDef = equipped_[*tgt]->def;
                if (targetDef.rpgSocketsMax > 0 &&
                    static_cast<int>(targetDef.rpgSocketed.size()) < targetDef.rpgSocketsMax) {
                    clickHint = "Click/Drag: Socket into equipped item";
                    lines.push_back("Target: " + targetDef.name);
                }
            }
        } else {
            if (hoveredEquipSlotIdx.has_value() && *hoveredEquipSlotIdx < equipped_.size() &&
                equipped_[*hoveredEquipSlotIdx].has_value()) {
                const auto& def = equipped_[*hoveredEquipSlotIdx]->def;
                if (!def.rpgSocketed.empty()) {
                    lines.push_back("Right-click: Remove gem (destroys it)");
                }
            }
        }
        if (!hoveredIsEquipped) lines.push_back("Drag: Move / swap / equip");
        lines.push_back(clickHint);

        float tw = 360.0f;
        float th = 18.0f + static_cast<float>(lines.size()) * 16.0f;
        float tx = static_cast<float>(mx) + 18.0f;
        float ty = static_cast<float>(my) + 18.0f;
        // clamp within viewport
        tx = std::min(tx, px + panelW - tw - 10.0f);
        ty = std::min(ty, py + panelH - th - 10.0f);
        render_->drawFilledRect(Engine::Vec2{tx, ty}, Engine::Vec2{tw, th}, Engine::Color{8, 12, 18, 235});
        render_->drawFilledRect(Engine::Vec2{tx + 1.0f, ty + 1.0f}, Engine::Vec2{tw - 2.0f, th - 2.0f}, Engine::Color{16, 22, 32, 235});
        float ly = ty + 8.0f;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            Engine::Color c = (i == 0) ? rarityCol(hoveredDef->rarity) : Engine::Color{185, 210, 235, 220};
            drawTextUnified(lines[i], Engine::Vec2{tx + 10.0f, ly}, 0.78f * uiScale, c);
            ly += 16.0f;
        }
    }

    // Apply pending interactions at end of draw to avoid use-after-free on pointers above.
    bool didMutate = false;
    if (pendingUnsocket.has_value()) {
        const std::size_t eqIdx = *pendingUnsocket;
        if (eqIdx < equipped_.size() && equipped_[eqIdx].has_value()) {
            auto& def = equipped_[eqIdx]->def;
            if (!def.rpgSocketed.empty()) {
                def.rpgSocketed.pop_back();  // destroys the gem; frees a socket
                didMutate = true;
            }
        }
    } else if (pendingUnequip.has_value()) {
        (void)unequipRpgSlot(*pendingUnequip);
        didMutate = true;
        const std::size_t uneqIdx = static_cast<std::size_t>(*pendingUnequip);
        if (characterScreenSocketTargetSlotIdx_.has_value() && *characterScreenSocketTargetSlotIdx_ == uneqIdx) {
            if (uneqIdx >= equipped_.size() || !equipped_[uneqIdx].has_value()) {
                characterScreenSocketTargetSlotIdx_.reset();
            }
        }
    } else if (pendingSwap.has_value()) {
        if (pendingSwap->a < inventory_.size() && pendingSwap->b < inventory_.size()) {
            std::swap(inventory_[pendingSwap->a], inventory_[pendingSwap->b]);
            inventorySelected_ = static_cast<int>(pendingSwap->b);
            clampInventorySelection();
            didMutate = true;
        }
    } else if (pendingSocket.has_value()) {
        const std::size_t invIdx = pendingSocket->inventoryIdx;
        const std::size_t eqIdx = pendingSocket->equippedSlotIdx;
        if (invIdx < inventory_.size() && eqIdx < equipped_.size() && equipped_[eqIdx].has_value()) {
            auto& gemInst = inventory_[invIdx];
            const auto* gemT = findRpgTemplateFor(gemInst.def);
            auto& targetDef = equipped_[eqIdx]->def;
            if (gemT && gemT->socketable && targetDef.rpgSocketsMax > 0 &&
                static_cast<int>(targetDef.rpgSocketed.size()) < targetDef.rpgSocketsMax) {
                ItemDefinition::RpgSocketedGem g{};
                g.templateId = gemInst.def.rpgTemplateId;
                g.affixIds = gemInst.def.rpgAffixIds;
                targetDef.rpgSocketed.push_back(std::move(g));
                if (gemInst.quantity > 1) {
                    gemInst.quantity -= 1;
                } else {
                    inventory_.erase(inventory_.begin() + static_cast<std::ptrdiff_t>(invIdx));
                }
                clampInventorySelection();
                didMutate = true;
            }
        }
    } else if (pendingEquip.has_value()) {
        (void)equipInventoryItem(*pendingEquip);
        didMutate = true;
    }

    // Keep derived stats synced immediately for UI feedback while paused.
    if (didMutate && hero_ != Engine::ECS::kInvalidEntity) {
        updateHeroRpgStats();
    }
}



void GameRoot::drawAbilityPanel() {
    // Legacy wrapper kept for compatibility; new icon HUD is invoked directly from onUpdate.
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
    registry_.emplace<Engine::ECS::Status>(hero_, Engine::ECS::Status{});
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
    heroBaseOffense_ = activeArchetype_.offensiveType;
    usingSecondaryWeapon_ = false;
    registry_.emplace<Game::SecondaryWeapon>(hero_, Game::SecondaryWeapon{usingSecondaryWeapon_});
    refreshHeroOffenseTag();

    // Dual-sheet character support (movement + combat).
    Engine::TexturePtr moveTex{};
    Engine::TexturePtr combatTex{};
    {
        auto files = heroSpriteFilesFor(activeArchetype_);
        if (files.folder.empty()) {
            files.folder = activeArchetype_.name.empty() ? std::string("Damage Dealer") : activeArchetype_.name;
        }
        const char* home = std::getenv("HOME");
        auto tryLoad = [&](const std::string& rel) -> Engine::TexturePtr {
            if (rel.empty()) return {};
            std::filesystem::path relPath = std::filesystem::path("assets") / "Sprites" / "Characters" / files.folder / rel;
            std::filesystem::path path = relPath;
            if (home && *home) {
                std::filesystem::path homePath = std::filesystem::path(home) / relPath;
                if (std::filesystem::exists(homePath)) {
                    path = homePath;
                }
            }
            if (std::filesystem::exists(path)) return loadTextureOptional(path.string());
            return {};
        };

        moveTex = tryLoad(files.movementFile);
        combatTex = tryLoad(files.combatFile);
        // Legacy fallback to flat naming if custom archetype assets are present.
        if (!moveTex) {
            std::string legacy = files.folder;
            std::string compact = legacy;
            compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
            moveTex = tryLoad(compact + ".png");
        }
        if (!combatTex) {
            std::string legacy = files.folder;
            std::string compact = legacy;
            compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
            combatTex = tryLoad(compact + "_Combat.png");
        }
    }

    if (moveTex && combatTex) {
        const int movementColumns = 4;
        const int movementRows = 31;
        int moveFrameW = std::max(1, moveTex->width() / movementColumns);
        int moveFrameH = (moveTex->height() % movementRows == 0)
                             ? std::max(1, moveTex->height() / movementRows)
                             : std::max(1, moveTex->height() / std::max(1, moveTex->height() / moveFrameW));
        int moveFrames = std::max(1, moveTex->width() / moveFrameW);

        const int combatColumns = 4;
        int combatFrameW = std::max(1, combatTex->width() / combatColumns);
        int combatFrameH = combatFrameW;
        if (combatTex->height() % 20 == 0) {
            combatFrameH = std::max(1, combatTex->height() / 20);
        }
        float moveFrameDur = 0.12f;
        float combatFrameDur = 0.10f;

        registry_.emplace<Game::HeroSpriteSheets>(hero_,
                                                  Game::HeroSpriteSheets{moveTex,
                                                                         combatTex,
                                                                         moveFrameW,
                                                                         moveFrameH,
                                                                         combatFrameW,
                                                                         combatFrameH,
                                                                         heroSize_,
                                                                         moveFrameDur,
                                                                         combatFrameDur});
        if (!registry_.has<Game::HeroAttackCycle>(hero_)) {
            registry_.emplace<Game::HeroAttackCycle>(hero_, Game::HeroAttackCycle{0});
        }
        if (auto* rend = registry_.get<Engine::ECS::Renderable>(hero_)) {
            rend->texture = moveTex;
        }
        registry_.emplace<Engine::ECS::SpriteAnimation>(
            hero_, Engine::ECS::SpriteAnimation{moveFrameW, moveFrameH, moveFrames, moveFrameDur});
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
    heroGhostActive_ = false;
    heroGhostPendingRise_ = false;
    heroGhostRiseTimer_ = 0.0f;
    lastSnapshotHeroHp_ = -1.0f;
    pendingNetShake_ = false;
    usingSecondaryWeapon_ = false;
    swapWeaponHeld_ = false;
    remoteEntities_.clear();
    remoteStates_.clear();
    remoteTargets_.clear();
    kills_ = 0;
    copper_ = 0;
    gold_ = 0;
    miniUnitSupplyUsed_ = 0;
    miniUnitSupplyMax_ = 0;
    miniUnitSupplyCap_ = 10;
    copperPerKill_ = copperPerKillBase_;
    copperPickupMin_ = copperPickupMinBase_;
    copperPickupMax_ = copperPickupMaxBase_;
    bossCopperDrop_ = bossCopperDropBase_;
    miniBossCopperDrop_ = miniBossCopperDropBase_;
    waveClearBonus_ = waveClearBonusBase_;
    contactDamage_ = contactDamageBase_;
    if (activeArchetype_.offensiveType == Game::OffensiveType::Builder) {
        miniUnitSupplyMax_ = std::max(miniUnitSupplyMax_, 2);
    }
    if (activeArchetype_.id == "summoner") {
        miniUnitSupplyMax_ = std::max(miniUnitSupplyMax_, 6);
        miniUnitSupplyCap_ = 12;
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
    druidForm_ = DruidForm::Human;
    druidChosen_ = DruidForm::Human;
    druidChoiceMade_ = false;
    wizardElement_ = WizardElement::Fire;
    lightningDomeTimer_ = 0.0f;
    flameWalls_.clear();
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
    inventoryCapacity_ = baseInventoryCapacity_;
    inventorySelected_ = -1;
    equipped_.fill(std::nullopt);
    inventoryGridScroll_ = 0.0f;
    rpgTalentRanks_.clear();
    rpgTalentPointsSpent_ = 0;
    rpgTalentLevelCached_ = 0;
    rpgTalentArchetypeCached_.clear();
    combatDebugLines_.clear();
    inventoryScrollLeftPrev_ = false;
    inventoryScrollRightPrev_ = false;
    inventoryCyclePrev_ = false;
    shopkeeper_ = Engine::ECS::kInvalidEntity;
    interactPrev_ = false;
    useItemPrev_ = false;
    hotbarItemIds_.fill(std::nullopt);
    hotbar1Prev_ = false;
    hotbar2Prev_ = false;
    rpgConsumableCooldowns_.clear();
    rpgConsumableOverTime_.clear();
    rpgActiveBuffs_.clear();
    rpgActiveBuffContribution_ = {};
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
        waveSystem_->setBossConfig(bossWave_, bossInterval_, bossHpMultiplier_, bossSpeedMultiplier_, bossMaxSize_);
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
    characterScreenOpen_ = false;
    characterScreenPrev_ = false;
    abilityHudClickPrev_ = false;
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
    uiHpFill_ = uiShieldFill_ = uiEnergyFill_ = uiDashFill_ = 1.0f;
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

void GameRoot::updateRemoteCombat(const Engine::TimeStep& step) {
    if (!multiplayerEnabled_ || !netSession_ || !netSession_->isHost()) return;
    for (const auto& kv : remoteEntities_) {
        uint32_t pid = kv.first;
        Engine::ECS::Entity ent = kv.second;
        if (ent == Engine::ECS::kInvalidEntity) continue;
        auto* hp = registry_.get<Engine::ECS::Health>(ent);
        auto* tf = registry_.get<Engine::ECS::Transform>(ent);
        if (!hp || !tf || !hp->alive()) continue;

        double& cd = remoteFireCooldown_[pid];
        cd -= step.deltaSeconds;
        if (cd > 0.0) continue;

        const ArchetypeDef* def = findArchetypeById(remoteStates_[pid].heroId);
        Game::OffensiveType ot = def ? def->offensiveType : Game::OffensiveType::Ranged;

        Engine::Vec2 bestPos{};
        float bestD2 = 9999999.0f;
        Engine::ECS::Entity bestEnemy = Engine::ECS::kInvalidEntity;
        registry_.view<Engine::ECS::Transform, Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity enemy, const Engine::ECS::Transform& etf, const Engine::ECS::Health& ehp, Engine::ECS::EnemyTag&) {
                if (!ehp.alive()) return;
                float dx = etf.position.x - tf->position.x;
                float dy = etf.position.y - tf->position.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    bestPos = etf.position;
                    bestEnemy = enemy;
                }
            });
        if (bestD2 >= 9999998.0f) continue;

        Engine::Vec2 dir(bestPos.x - tf->position.x, bestPos.y - tf->position.y);
        float len2 = dir.x * dir.x + dir.y * dir.y;
        if (len2 < 0.0001f) continue;
        float inv = 1.0f / std::sqrt(len2);
        dir.x *= inv;
        dir.y *= inv;
        if (ot == Game::OffensiveType::Melee || ot == Game::OffensiveType::ThornTank) {
            float meleeRange = 70.0f;
            if (len2 <= meleeRange * meleeRange && bestEnemy != Engine::ECS::kInvalidEntity) {
                if (auto* ehp = registry_.get<Engine::ECS::Health>(bestEnemy)) {
                    Engine::Gameplay::DamageEvent dmg{};
                    dmg.baseDamage = projectileDamage_ * 1.1f;
                    dmg.type = Engine::Gameplay::DamageType::Normal;
                    Engine::Gameplay::BuffState buff{};
                    if (auto* st = registry_.get<Engine::ECS::Status>(bestEnemy)) {
                        if (st->container.isStasis()) return;
                        float armorDelta = st->container.armorDeltaTotal();
                        buff.healthArmorBonus += armorDelta;
                        buff.shieldArmorBonus += armorDelta;
                        buff.damageTakenMultiplier *= st->container.damageTakenMultiplier();
                    }
                    float dealt = Game::RpgDamage::apply(registry_, ent, bestEnemy, *ehp, dmg, buff, useRpgCombat_, rpgResolverConfig_, rng_,
                                                         "remote_melee", [this](const std::string& line) { pushCombatDebugLine(line); });
                    if (!ehp->alive()) {
                        hp->currentHealth = std::min(hp->maxHealth, hp->currentHealth + dealt * 0.1f);
                    }
                }
                cd = fireInterval_;
            } else {
                cd = fireInterval_ * 0.3;  // retry soon if out of range
            }
        } else {
            float speed = projectileSpeed_;
            auto p = registry_.create();
            registry_.emplace<Engine::ECS::Transform>(p, tf->position);
            registry_.emplace<Engine::ECS::Velocity>(p, dir * speed);
            float hb = projectileHitboxSize_ * 0.5f;
            registry_.emplace<Engine::ECS::AABB>(p, Engine::ECS::AABB{Engine::Vec2{hb, hb}});
            applyProjectileVisual(p, 1.0f, Engine::Color{255, 220, 140, 255}, false);
            Engine::Gameplay::DamageEvent dmg{};
            dmg.baseDamage = projectileDamage_;
            dmg.type = Engine::Gameplay::DamageType::Normal;
            registry_.emplace<Engine::ECS::Projectile>(p, Engine::ECS::Projectile{dir * speed, dmg, projectileLifetime_, lifestealPercent_, chainBounces_, ent});
            registry_.emplace<Engine::ECS::ProjectileTag>(p, Engine::ECS::ProjectileTag{});

            cd = fireInterval_;
        }

        if (auto* look = registry_.get<Game::LookDirection>(ent)) {
            look->dir = (std::abs(dir.x) > std::abs(dir.y))
                            ? (dir.x >= 0.0f ? Game::LookDir4::Right : Game::LookDir4::Left)
                            : (dir.y >= 0.0f ? Game::LookDir4::Front : Game::LookDir4::Back);
        }
    }
}

void GameRoot::refreshPauseState() {
    // In multiplayer, overlays (shop, level-up, build, inventory) do NOT pause the match.
    bool overlayPause = !multiplayerEnabled_ &&
                        (itemShopOpen_ || abilityShopOpen_ || levelChoiceOpen_ || characterScreenOpen_);
    paused_ = userPaused_ || overlayPause || defeated_;
}

}  // namespace Game
