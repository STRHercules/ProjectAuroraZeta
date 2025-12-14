// Game layer bootstrap implementing engine callbacks.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>
#include "../engine/core/ApplicationListener.h"
#include "../engine/ecs/Registry.h"
#include "../engine/render/Camera2D.h"
#include "../engine/render/RenderDevice.h"
#include "../engine/gameplay/InputHelpers.h"
#include "../engine/assets/TextureLoader.h"
#include "../engine/assets/TextureManager.h"
#include "../engine/assets/AssetManifest.h"
#include "../engine/assets/FontLoader.h"
#include "../engine/render/BitmapTextRenderer.h"
#include "../engine/gameplay/Combat.h"
#include <random>
#include <fstream>
#include <array>
#include <deque>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../engine/input/ActionMapper.h"
#include "../engine/input/ActionState.h"
#include "../engine/input/InputBinding.h"
#include "../engine/input/InputLoader.h"
#include "render/RenderSystem.h"
#include "meta/ItemDefs.h"
#include "systems/MovementSystem.h"
#include "systems/CameraSystem.h"
#include "systems/ProjectileSystem.h"
#include "systems/CollisionSystem.h"
#include "systems/EnemyAISystem.h"
#include "systems/WaveSystem.h"
#include "systems/EnemySpriteStateSystem.h"
#include "systems/HeroSpriteStateSystem.h"
#include "systems/AnimationSystem.h"
#include "systems/HitFlashSystem.h"
#include "systems/DamageNumberSystem.h"
#include "systems/ShopSystem.h"
#include "systems/PickupSystem.h"
#include "systems/EventSystem.h"
#include "systems/HotzoneSystem.h"
#include "systems/BuffSystem.h"
#include "systems/MiniUnitSystem.h"
#include "meta/SaveManager.h"
#include "EnemyDefinition.h"
#include "components/OffensiveType.h"
#include "components/Building.h"
#include "components/MiniUnit.h"
#include "components/MiniUnitCommand.h"
#include "components/MiniUnitStats.h"
#include "components/TauntTarget.h"
#include "components/LookDirection.h"
#include "components/Dying.h"
#include "components/HeroSpriteSheets.h"
#include "components/HeroAttackAnim.h"
#include "components/HeroPickupAnim.h"
#include "../engine/gameplay/FogOfWar.h"
#include "../engine/render/FogOfWarRenderer.h"
#include "net/NetSession.h"
#include "net/LobbyState.h"
#include "net/SessionConfig.h"
#include "net/NetMessages.h"

namespace Game {

class GameRoot final : public Engine::ApplicationListener {
public:
    bool onInitialize(Engine::Application& app) override;
    void onUpdate(const Engine::TimeStep& step, const Engine::InputState& input) override;
    void onShutdown() override;

private:
    struct MiniUnitDef;
    struct BuildingDef;
    void handleHeroDeath(const Engine::TimeStep& step);
    void showDefeatOverlay();
    void processDefeatInput(const Engine::ActionState& actions, const Engine::InputState& input);
    void resetRun();
    void spawnHero();
    void startNewGame();
    void levelUp();
    void renderMenu();
    void updateMenuInput(const Engine::ActionState& actions, const Engine::InputState& input, double dt);
    void drawTextTTF(const std::string& text, const Engine::Vec2& pos, float scale, Engine::Color color);
    void drawTextUnified(const std::string& text, const Engine::Vec2& pos, float scale, Engine::Color color);
    bool hasTTF() const { return uiFont_ != nullptr && sdlRenderer_ != nullptr; }
    void rollLevelChoices();
    void applyLevelChoice(int index);
    void drawLevelChoiceOverlay();
    void drawItemShopOverlay();
    void drawAbilityShopOverlay();
    void drawInventoryOverlay();
    void drawPauseOverlay();
    void refreshShopInventory();
    bool addItemToInventory(const ItemDefinition& def);
    bool sellItemFromInventory(std::size_t idx, int& copperOut);
    void clampInventorySelection();
    void loadMenuPresets();
    void reviveLocalPlayer();
    void buildNetSession();
    void connectHost();
    void connectDirect();
    void connectDirectAddress(const std::string& address);
    void updateNetwork(double dt);
    void detectLocalIp();
    std::vector<Game::Net::PlayerNetState> collectNetSnapshot();
    void applyRemoteSnapshot(const Game::Net::SnapshotMsg& snap);
    void leaveNetworkSession(bool isHostQuit);
    void applyLocalHeroFromLobby();
    int currentPlayerCount() const;
    void applyDifficultyPreset();
    void applyArchetypePreset();
    void loadProgress();
    void saveProgress();
    void rebuildWaveSettings();
    void buildAbilities();
    void drawAbilityPanel();
    void executeAbility(int index);
    void upgradeFocusedAbility();
    void applyPowerupPickup(Pickup::Powerup type);
    void loadGridTextures();
    void loadEnemyDefinitions();
    void loadUnitDefinitions();
    void loadPickupTextures();
    void loadProjectileTextures();
    Engine::TexturePtr loadTextureOptional(const std::string& path);
    void drawBuildMenuOverlay(int mouseX, int mouseY, bool leftClickEdge);
    void beginBuildPreview(Game::BuildingType type);
    void clearBuildPreview();
    void selectBuildingAt(const Engine::Vec2& worldPos);
    void drawSelectedBuildingPanel(int mouseX, int mouseY, bool leftClickEdge);
    void spawnShopkeeper(const Engine::Vec2& aroundPos);
    void despawnShopkeeper();
    void rebuildFogLayer();
    void updateFogVision();
    bool performRangedAutoFire(const Engine::TimeStep& step, const Engine::ActionState& actions,
                               Game::OffensiveType offenseType);
    bool performMeleeAttack(const Engine::TimeStep& step, const Engine::ActionState& actions);

    enum class MenuPage { Main, Stats, Options, CharacterSelect, HostConfig, JoinSelect, Lobby, ServerBrowser };
    enum class MovementMode { Modern, RTS };
    enum class LevelChoiceType { Damage, Health, Speed };
    struct LevelChoice {
        LevelChoiceType type{LevelChoiceType::Damage};
        float amount{0.0f};
    };
    struct ItemInstance {
        ItemDefinition def;
        int quantity{1};
    };
    struct ArchetypeDef {
        std::string id;
        std::string name;
        std::string description;
        float hpMul{1.0f};
        float speedMul{1.0f};
        float damageMul{1.0f};
        Engine::Color color{90, 200, 255, 255};
        std::string texturePath;
        Game::OffensiveType offensiveType{Game::OffensiveType::Ranged};
    };
    struct DifficultyDef {
        std::string id;
        std::string name;
        std::string description;
        float enemyHpMul{1.0f};
        int startWave{1};
    };
    struct MiniUnitDef {
        std::string id;
        Game::MiniUnitClass cls{Game::MiniUnitClass::Light};
        float hp{0.0f};
        float shields{0.0f};
        float moveSpeed{0.0f};
        float damage{0.0f};
        float healPerSecond{0.0f};
        float armor{0.0f};
        float shieldArmor{0.0f};
        float attackRange{0.0f};
        float attackRate{0.0f};
        float preferredDistance{0.0f};
        float tauntRadius{0.0f};
        float healRange{0.0f};
        float frameDuration{0.0f};
        Game::OffensiveType offensiveType{Game::OffensiveType::Ranged};
        int costCopper{0};
        std::string texturePath{};
    };
    struct BuildingDef {
        std::string id;
        Game::BuildingType type{Game::BuildingType::Turret};
        float hp{0.0f};
        float shields{0.0f};
        float armor{0.0f};
        float attackRange{0.0f};
        float attackRate{0.0f};
        float damage{0.0f};
        float spawnInterval{0.0f};
        bool autoSpawn{false};
        int maxQueue{0};
        int capacity{0};
        float damageMultiplierPerUnit{0.0f};
        int supplyProvided{0};
        int costCopper{0};
        std::string texturePath{};
    };
    std::string resolveArchetypeTexture(const ArchetypeDef& def) const;
    const ArchetypeDef* findArchetypeById(const std::string& id) const;
    struct AbilitySlot {
        std::string name;
        std::string description;
        std::string keyHint;
        int level{1};
        int maxLevel{5};
        int upgradeCost{25};
        float cooldown{0.0f};
        float cooldownMax{8.0f};
        bool triggered{false};
        float powerScale{1.0f};
        std::string type;  // semantic type used for behavior
        float energyCost{0.0f};
    };
    struct AbilityDef {
        std::string name;
        std::string description;
        std::string keyHint;
        float baseCooldown{8.0f};
        int baseCost{25};
        std::string type;
    };
    struct AbilityState {
        AbilityDef def;
        int level{1};
        int maxLevel{5};
        float cooldown{0.0f};
    };
    void placeBuilding(const Engine::Vec2& pos);
    bool spawnMiniUnit(const MiniUnitDef& def, const Engine::Vec2& pos);

    Engine::ECS::Registry registry_{};
    Engine::ECS::Entity hero_{Engine::ECS::kInvalidEntity};
    Engine::RenderDevice* render_{nullptr};
    Engine::Application* app_{nullptr};
    std::unique_ptr<RenderSystem> renderSystem_;
    Engine::Camera2D camera_{};
    int viewportWidth_{1280};
    int viewportHeight_{720};
    Engine::Vec2 mouseWorld_{};
    Engine::TexturePtr gridTexture_{};
    std::vector<Engine::TexturePtr> gridTileTexturesWeighted_;
    std::vector<Engine::TexturePtr> gridTileTextures_{};
    Engine::TexturePtr shopTexture_{};
    // Pickup textures
    Engine::TexturePtr pickupCopperTex_{};
    Engine::TexturePtr pickupGoldTex_{};
    Engine::TexturePtr pickupHealTex_{};         // Field med kit (item)
    Engine::TexturePtr pickupHealPowerupTex_{};  // Instant heal powerup orb
    Engine::TexturePtr pickupKaboomTex_{};
    Engine::TexturePtr pickupRechargeTex_{};
    Engine::TexturePtr pickupFrenzyTex_{};
    Engine::TexturePtr pickupImmortalTex_{};
    Engine::TexturePtr pickupFreezeTex_{};
    Engine::TexturePtr pickupTurretTex_{};
    std::vector<Engine::TexturePtr> projectileTextures_;
    Engine::TexturePtr projectileTexRed_;
    Engine::TexturePtr projectileTexTurret_;
    SDL_Cursor* customCursor_{nullptr};
    std::string heroTexturePath_{};
    std::unique_ptr<Engine::BitmapTextRenderer> debugText_;
    std::mt19937 rng_{std::random_device{}()};
    Engine::InputBindings bindings_{};
    Engine::ActionMapper actionMapper_{};
    Engine::AssetManifest manifest_{};
    float projectileSpeed_{400.0f};
    float projectileDamage_{15.0f};
    float projectileDamageBase_{15.0f};
    float projectileSize_{8.0f};
    float projectileHitboxSize_{8.0f};
    float projectileLifetime_{1.5f};
    double waveInterval_{2.5};
    double graceDuration_{1.0};
    int copperPerKill_{5};
    int waveClearBonus_{20};
    int enemyLowThreshold_{5};
    double combatDuration_{90.0};
    double intermissionDuration_{30.0};
    int bountyGoldBonus_{40};
    int eventGoldReward_{1};
    int bossWave_{20};
    float bossHpMultiplier_{12.0f};
    float bossSpeedMultiplier_{0.8f};
    int bossGoldBonus_{60};
    int bossCopperDrop_{160};
    int miniBossCopperDrop_{80};
    float pickupDropChance_{0.25f};
    float pickupPowerupShare_{0.35f};
    int copperPickupMin_{4};
    int copperPickupMax_{10};
    int salvageReward_{20};
    // Hotzones
    float hotzoneMapRadius_{900.0f};
    float hotzoneRadiusMin_{140.0f};
    float hotzoneRadiusMax_{220.0f};
    float hotzoneMinSeparation_{700.0f};
    float hotzoneSpawnClearance_{320.0f};
    double hotzoneRotation_{30.0};
    // Shop options
    int shopDamageCost_{25};
    int shopHpCost_{25};
    int shopSpeedCost_{20};
    float shopDamageBonus_{5.0f};
    float shopHpBonus_{20.0f};
    float shopSpeedBonus_{20.0f};  // flat bonus to move speed
    int abilityShopBaseCost_{8};
    float abilityShopCostGrowth_{1.18f};
    float abilityDamagePerLevel_{1.0f};
    float abilityAttackSpeedPerLevel_{0.05f};
    float abilityRangePerLevel_{60.0f};  // world units of extra range per level
    float abilityVisionPerLevel_{1.0f};
    float abilityHealthPerLevel_{5.0f};
    float abilityArmorPerLevel_{1.0f};
    int abilityRangeMaxBonus_{5};
    int abilityVisionMaxBonus_{5};
    int abilityDamageLevel_{0};
    int abilityAttackSpeedLevel_{0};
    int abilityRangeLevel_{0};
    int abilityVisionLevel_{0};
    int abilityHealthLevel_{0};
    int abilityArmorLevel_{0};
    // Shop click edge detectors (declared once)
    bool shopLeftPrev_{false};
    bool shopRightPrev_{false};
    bool shopMiddlePrev_{false};
    bool shopUIClickPrev_{false};
    double fireInterval_{0.2};
    double fireIntervalBase_{0.2};
    float autoFireRangeBonus_{0.0f};
    float autoFireBaseRange_{320.0f};
    bool autoAttackEnabled_{true};
    bool defeatDelayActive_{false};
    float defeatDelayTimer_{0.0f};
    struct OffensiveTypeModifier {
        float healthArmorBonus{0.0f};
        float shieldArmorBonus{0.0f};
        float healthRegenBonus{0.0f};
        float shieldRegenBonus{0.0f};
    };
    std::array<OffensiveTypeModifier, 5> offensiveTypeModifiers_{};
    struct MeleeConfig {
        float range{40.0f};
        float arcDegrees{70.0f};
        float damageMultiplier{1.3f};
    };
    struct PlasmaConfig {
        float shieldDamageMultiplier{1.5f};
        float healthDamageMultiplier{0.75f};
    };
    struct ThornConfig {
        float reflectPercent{0.25f};
        float maxReflectPerHit{40.0f};
    };
    MeleeConfig meleeConfig_{};
    PlasmaConfig plasmaConfig_{};
    ThornConfig thornConfig_{};
    float contactDamage_{10.0f};
    double fireCooldown_{0.0};
    float heroMoveSpeed_{200.0f};
    float heroMaxHp_{100.0f};
    float heroShield_{0.0f};
    float heroHealthArmor_{0.0f};
    float heroShieldArmor_{0.0f};
    float heroShieldRegen_{0.0f};
    float heroHealthRegen_{0.0f};
    float heroRegenDelay_{0.0f};
    float heroMoveSpeedBase_{200.0f};
    float heroMaxHpBase_{100.0f};
    float heroShieldBase_{0.0f};
    float heroMoveSpeedPreset_{200.0f};
    float heroMaxHpPreset_{100.0f};
    float heroShieldPreset_{0.0f};
    Engine::Gameplay::UpgradeState heroUpgrades_{};
    Engine::Gameplay::BaseStats heroBaseStats_{};
    float projectileDamagePreset_{15.0f};
    Engine::Color heroColorPreset_{90, 200, 255, 255};
    float heroSize_{24.0f};
    float dashSpeedMul_{3.5f};
    float dashDuration_{0.18f};
    float dashCooldown_{2.5f};
    float dashInvulnFraction_{1.0f};
    float dashTimer_{0.0f};
    float dashCooldownTimer_{0.0f};
    float dashInvulnTimer_{0.0f};
    Engine::Vec2 dashDir_{};
    std::deque<std::pair<Engine::Vec2, float>> dashTrail_;
    int kills_{0};
    int copper_{0};
    int gold_{0};
    int miniUnitSupplyUsed_{0};
    int miniUnitSupplyMax_{0};
    float globalSpeedMul_{1.0f};
    int miniUnitSupplyCap_{10};
    int level_{1};
    int xp_{0};
    int xpToNext_{60};
    int xpBaseToLevel_{60};
    int xpPerKill_{8};
    int xpPerWave_{5};
    float xpPerDamageDealt_{0.0f};
    float xpPerDamageTaken_{0.0f};
    int xpPerEvent_{0};
    float levelHpBonus_{12.0f};
    float levelDmgBonus_{2.5f};
    float levelSpeedBonus_{5.0f};
    float xpGrowth_{1.35f};
    double levelBannerTimer_{0.0};
    int wave_{0};
    int round_{1};
    int wavesTargetThisRound_{2};
    int wavesClearedThisRound_{0};
    int wavesPerRoundBase_{2};
    int wavesPerRoundGrowthInterval_{2};
    int spawnBatchRoundInterval_{5};
    int waveBatchBase_{2};
    int enemiesAlive_{0};
    Game::WaveSettings waveSettingsDefault_{};
    Game::WaveSettings waveSettingsBase_{};
    Engine::Gameplay::UpgradeState enemyUpgrades_{};
    double waveWarmup_{1.5};
    double waveWarmupBase_{1.5};
    bool defeated_{false};
    float fpsSmooth_{60.0f};
    double accumulated_{0.0};
    std::size_t tickCount_{0};
    float shakeTimer_{0.0f};
    float shakeMagnitude_{0.0f};
    float lastHeroHp_{-1.0f};
    // Menu/UI state
    bool inMenu_{true};
    MenuPage menuPage_{MenuPage::Main};
    int menuSelection_{0};
    bool menuUpPrev_{false};
    bool menuDownPrev_{false};
    bool menuLeftPrev_{false};
    bool menuRightPrev_{false};
    bool menuConfirmPrev_{false};
    bool menuPausePrev_{false};
    bool menuBackPrev_{false};
    bool menuClickPrev_{false};
    bool pauseClickPrev_{false};
    double menuPulse_{0.0};
    MovementMode movementMode_{MovementMode::Modern};
    bool moveCommandPrev_{false};
    bool moveTargetActive_{false};
    Engine::Vec2 moveTarget_{};
    float moveArriveRadius_{10.0f};
    float moveMarkerTimer_{0.0f};
    bool showDamageNumbers_{true};
    bool screenShake_{true};
    // Session stats
    int totalRuns_{0};
    int bestWave_{0};
    int totalKillsAccum_{0};
    // Multiplayer state
    std::unique_ptr<Game::Net::NetSession> netSession_;
    Game::Net::SessionConfig hostConfig_{};
    Game::Net::LobbyState lobbyCache_{};
    bool multiplayerEnabled_{false};
    bool isHosting_{false};
    uint32_t localPlayerId_{0};
    int hostLobbyNameIndex_{0};
    std::string hostLocalIp_{"0.0.0.0"};
    std::unordered_map<uint32_t, Engine::ECS::Entity> remoteEntities_;
    std::unordered_map<uint32_t, Game::Net::PlayerNetState> remoteStates_;
    std::unordered_map<uint32_t, Engine::Vec2> remoteTargets_;
    std::vector<uint32_t> remoteToRemove_;
    std::array<int, 4> joinIpSegments_{127, 0, 0, 1};
    uint16_t joinPort_{37015};
    int joinIpCursor_{3};
    bool directConnectPopup_{false};
    bool editingDirectIp_{false};
    std::string directConnectIp_{"127.0.0.1"};
    // Text input state
    bool editingLobbyName_{false};
    bool editingLobbyPassword_{false};
    bool editingJoinPassword_{false};
    bool editingHostPlayerName_{false};
    bool editingJoinPlayerName_{false};
    std::string hostPlayerName_{"Host"};
    std::string joinPlayerName_{"Player"};
    std::string joinPassword_{};
    std::string localLobbyHeroId_{};
    // Persistence
    std::string savePath_{"saves/profile.dat"};
    Game::SaveData saveData_{};
    bool runStarted_{false};
    bool reviveNextRound_{false};
    // Level-up choice overlay
    bool levelChoiceOpen_{false};
    LevelChoice levelChoices_[3];
    bool levelChoicePrevClick_{false};
    bool defeatClickPrev_{false};
    std::vector<ItemDefinition> shopInventory_;
    std::vector<ItemDefinition> shopPool_;
    double shopNoFundsTimer_{0.0};
    bool dashPrev_{false};
    std::unique_ptr<Game::MovementSystem> movementSystem_;
    std::unique_ptr<Game::CameraSystem> cameraSystem_;
    std::unique_ptr<Game::ProjectileSystem> projectileSystem_;
    std::unique_ptr<Game::CollisionSystem> collisionSystem_;
    std::unique_ptr<Game::MiniUnitSystem> miniUnitSystem_;
    std::unique_ptr<Game::EnemyAISystem> enemyAISystem_;
    std::unique_ptr<Game::WaveSystem> waveSystem_;
    std::unique_ptr<Game::EnemySpriteStateSystem> enemySpriteStateSystem_;
    std::unique_ptr<Game::HeroSpriteStateSystem> heroSpriteStateSystem_;
    std::unique_ptr<Game::AnimationSystem> animationSystem_;
    std::unique_ptr<Game::HitFlashSystem> hitFlashSystem_;
    std::unique_ptr<Game::DamageNumberSystem> damageNumberSystem_;
    std::unique_ptr<Game::BuffSystem> buffSystem_;
    std::unique_ptr<Game::ShopSystem> shopSystem_;
    std::unique_ptr<Game::PickupSystem> pickupSystem_;
    std::unique_ptr<Game::EventSystem> eventSystem_;
    std::unique_ptr<Game::HotzoneSystem> hotzoneSystem_;
    std::unique_ptr<Engine::TextureManager> textureManager_;
    TTF_Font* uiFont_{nullptr};
    SDL_Renderer* sdlRenderer_{nullptr};
    bool restartPrev_{false};
    bool itemShopOpen_{false};
    bool abilityShopOpen_{false};
    bool buildMenuOpen_{false};
    bool buildMenuPrev_{false};
    bool buildMenuClickPrev_{false};
    bool buildPreviewActive_{false};
    bool buildPreviewClickPrev_{false};
    bool buildPreviewRightPrev_{false};
    bool travelShopUnlocked_{false};
    bool waveClearedPending_{false};
    bool paused_{false};
    bool userPaused_{false};
    bool pauseTogglePrev_{false};
    bool shopTogglePrev_{false};
    double pauseMenuBlink_{0.0};
    bool inCombat_{true};
    bool waveSpawned_{false};
    double combatTimer_{0.0};
    double intermissionTimer_{0.0};
    double bossBannerTimer_{0.0};
    double eventBannerTimer_{0.0};
    std::string eventBannerText_;
    double freezeTimer_{0.0};
    float attackSpeedMul_{1.0f};
    float lifestealPercent_{0.0f};
    int chainBounces_{0};
    // Energy
    float energy_{0.0f};
    float energyMax_{100.0f};
    float energyRegen_{14.0f};
    float energyRegenIntermission_{26.0f};
    double energyWarningTimer_{0.0};
    // UI helpers
    double waveBannerTimer_{0.0};
    int waveBannerWave_{0};
    double roundBannerTimer_{0.0};
    int roundBanner_{1};
    double clearBannerTimer_{0.0};
    // Mouse cache
    int lastMouseX_{0};
    int lastMouseY_{0};
    std::vector<Engine::ECS::Entity> selectedMiniUnits_;
    bool selectingMiniUnits_{false};
    bool miniSelectMousePrev_{false};
    bool miniRightClickPrev_{false};
    bool miniHudClickPrev_{false};
    double lastMiniSelectClickTime_{-1.0};
    Engine::Vec2 selectionStart_{};
    Engine::Vec2 selectionEnd_{};
    Engine::ECS::Entity buildPreviewEntity_{Engine::ECS::kInvalidEntity};
    Game::BuildingType buildPreviewType_{Game::BuildingType::House};
    Engine::TexturePtr buildPreviewTex_{};
    Engine::ECS::Entity selectedBuilding_{Engine::ECS::kInvalidEntity};
    bool buildingPanelClickPrev_{false};
    bool buildingSelectPrev_{false};
    bool buildingJustSelected_{false};
    // Abilities
    std::vector<AbilitySlot> abilities_;
    std::vector<AbilityState> abilityStates_;
    int abilityFocus_{0};
    bool ability1Prev_{false};
    bool ability2Prev_{false};
    bool ability3Prev_{false};
    bool abilityUltPrev_{false};
    bool abilityUpgradePrev_{false};
    float rageTimer_{0.0f};
    float rageDamageBuff_{1.0f};
    float rageRateBuff_{1.0f};
    float frenzyTimer_{0.0f};
    float frenzyRateBuff_{1.0f};
    float immortalTimer_{0.0f};
    int reviveCharges_{0};
    float abilityCooldownMul_{1.0f};
    float abilityVitalCostMul_{1.0f};
    int abilityChargesBonus_{0};
    // Character/difficulty presets
    std::vector<ArchetypeDef> archetypes_;
    std::vector<DifficultyDef> difficulties_;
    int selectedArchetype_{0};
    int selectedDifficulty_{2};
    ArchetypeDef activeArchetype_{};
    DifficultyDef activeDifficulty_{};
    int startWaveBase_{1};
    std::vector<EnemyDefinition> enemyDefs_{};
    std::unordered_map<std::string, MiniUnitDef> miniUnitDefs_;
    std::unordered_map<std::string, BuildingDef> buildingDefs_;
    std::vector<ItemDefinition> itemCatalog_;
    std::vector<ItemDefinition> goldCatalog_;
    // Inventory
    std::vector<ItemInstance> inventory_;
    int inventoryCapacity_{16};
    int inventorySelected_{-1};
    bool inventoryScrollLeftPrev_{false};
    bool inventoryScrollRightPrev_{false};
    bool inventoryCyclePrev_{false};
    Engine::ECS::Entity shopkeeper_{Engine::ECS::kInvalidEntity};
    bool interactPrev_{false};
    bool useItemPrev_{false};

    struct TurretInstance {
        Engine::Vec2 pos;
        float timer{0.0f};
        float fireCooldown{0.0f};
        Engine::ECS::Entity visEntity{Engine::ECS::kInvalidEntity};
    };
    std::vector<TurretInstance> turrets_;

    // Fog of war state
    std::unique_ptr<Engine::Gameplay::FogOfWarLayer> fogLayer_{};
    std::vector<Engine::Gameplay::Unit> fogUnits_{};
    int fogTileSize_{32};
    int fogWidthTiles_{512};
    int fogHeightTiles_{512};
    float fogOriginOffsetX_{0.0f};
    float fogOriginOffsetY_{0.0f};
    float heroVisionRadiusBaseTiles_{8.0f};  // increased default vision (+2 tiles)
    float heroVisionRadiusTiles_{8.0f};
    SDL_Texture* fogTexture_{nullptr};
};

}  // namespace Game
