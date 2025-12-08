// Game layer bootstrap implementing engine callbacks.
#pragma once

#include <cstddef>
#include <string>

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
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../engine/input/ActionMapper.h"
#include "../engine/input/ActionState.h"
#include "../engine/input/InputBinding.h"
#include "../engine/input/InputLoader.h"
#include "render/RenderSystem.h"
#include "systems/MovementSystem.h"
#include "systems/CameraSystem.h"
#include "systems/ProjectileSystem.h"
#include "systems/CollisionSystem.h"
#include "systems/EnemyAISystem.h"
#include "systems/WaveSystem.h"
#include "systems/HitFlashSystem.h"
#include "systems/DamageNumberSystem.h"
#include "systems/ShopSystem.h"
#include "systems/PickupSystem.h"
#include "systems/EventSystem.h"

namespace Game {

class GameRoot final : public Engine::ApplicationListener {
public:
    bool onInitialize(Engine::Application& app) override;
    void onUpdate(const Engine::TimeStep& step, const Engine::InputState& input) override;
    void onShutdown() override;

private:
    void handleHeroDeath();
    void showDefeatOverlay();
    void processDefeatInput(const Engine::ActionState& actions);
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
    void drawShopOverlay();
    void drawPauseOverlay();
    void refreshShopInventory();
    void loadMenuPresets();
    void applyDifficultyPreset();
    void applyArchetypePreset();
    void rebuildWaveSettings();
    void buildAbilities();
    void drawAbilityPanel();
    void executeAbility(int index);
    void upgradeFocusedAbility();

    enum class MenuPage { Main, Stats, Options, CharacterSelect };
    enum class LevelChoiceType { Damage, Health, Speed };
    struct LevelChoice {
        LevelChoiceType type{LevelChoiceType::Damage};
        float amount{0.0f};
    };
    struct ArchetypeDef {
        std::string id;
        std::string name;
        std::string description;
        float hpMul{1.0f};
        float speedMul{1.0f};
        float damageMul{1.0f};
        Engine::Color color{90, 200, 255, 255};
    };
    struct DifficultyDef {
        std::string id;
        std::string name;
        std::string description;
        float enemyHpMul{1.0f};
        int startWave{1};
    };
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
    int currencyPerKill_{5};
    int waveClearBonus_{20};
    int enemyLowThreshold_{5};
    double combatDuration_{90.0};
    double intermissionDuration_{30.0};
    int bountyBonus_{40};
    int bossWave_{20};
    float bossHpMultiplier_{12.0f};
    float bossSpeedMultiplier_{0.8f};
    int bossKillBonus_{60};
    int salvageReward_{20};
    // Shop options
    int shopDamageCost_{25};
    int shopHpCost_{25};
    int shopSpeedCost_{20};
    float shopDamageBonus_{5.0f};
    float shopHpBonus_{20.0f};
    float shopSpeedBonus_{20.0f};  // flat bonus to move speed
    // Shop click edge detectors (declared once)
    bool shopLeftPrev_{false};
    bool shopRightPrev_{false};
    bool shopMiddlePrev_{false};
    bool shopUIClickPrev_{false};
    double fireInterval_{0.2};
    float contactDamage_{10.0f};
    double fireCooldown_{0.0};
    float heroMoveSpeed_{200.0f};
    float heroMaxHp_{100.0f};
    float heroMoveSpeedBase_{200.0f};
    float heroMaxHpBase_{100.0f};
    float heroMoveSpeedPreset_{200.0f};
    float heroMaxHpPreset_{100.0f};
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
    int credits_{0};
    int level_{1};
    int xp_{0};
    int xpToNext_{60};
    int xpBaseToLevel_{60};
    int xpPerKill_{8};
    int xpPerWave_{5};
    float levelHpBonus_{12.0f};
    float levelDmgBonus_{2.5f};
    float levelSpeedBonus_{5.0f};
    float xpGrowth_{1.35f};
    double levelBannerTimer_{0.0};
    int wave_{0};
    int enemiesAlive_{0};
    Game::WaveSettings waveSettingsDefault_{};
    Game::WaveSettings waveSettingsBase_{};
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
    bool showDamageNumbers_{true};
    bool screenShake_{true};
    // Session stats
    int totalRuns_{0};
    int bestWave_{0};
    int totalKillsAccum_{0};
    bool runStarted_{false};
    // Level-up choice overlay
    bool levelChoiceOpen_{false};
    LevelChoice levelChoices_[3];
    bool levelChoicePrevClick_{false};
    std::vector<ShopItem> shopInventory_;
    std::vector<ShopItem> shopPool_;
    double shopNoCreditTimer_{0.0};
    bool dashPrev_{false};
    std::unique_ptr<Game::MovementSystem> movementSystem_;
    std::unique_ptr<Game::CameraSystem> cameraSystem_;
    std::unique_ptr<Game::ProjectileSystem> projectileSystem_;
    std::unique_ptr<Game::CollisionSystem> collisionSystem_;
    std::unique_ptr<Game::EnemyAISystem> enemyAISystem_;
    std::unique_ptr<Game::WaveSystem> waveSystem_;
    std::unique_ptr<Game::HitFlashSystem> hitFlashSystem_;
    std::unique_ptr<Game::DamageNumberSystem> damageNumberSystem_;
    std::unique_ptr<Game::ShopSystem> shopSystem_;
    std::unique_ptr<Game::PickupSystem> pickupSystem_;
    std::unique_ptr<Game::EventSystem> eventSystem_;
    std::unique_ptr<Engine::TextureManager> textureManager_;
    TTF_Font* uiFont_{nullptr};
    SDL_Renderer* sdlRenderer_{nullptr};
    bool restartPrev_{false};
    bool shopOpen_{false};
    bool waveClearedPending_{false};
    double shopUnavailableTimer_{0.0};
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
    // UI helpers
    double waveBannerTimer_{0.0};
    int waveBannerWave_{0};
    double clearBannerTimer_{0.0};
    // Mouse cache
    int lastMouseX_{0};
    int lastMouseY_{0};
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
    // Character/difficulty presets
    std::vector<ArchetypeDef> archetypes_;
    std::vector<DifficultyDef> difficulties_;
    int selectedArchetype_{0};
    int selectedDifficulty_{2};
    ArchetypeDef activeArchetype_{};
    DifficultyDef activeDifficulty_{};
    int startWaveBase_{1};
};

}  // namespace Game
