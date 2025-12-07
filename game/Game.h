// Game layer bootstrap implementing engine callbacks.
#pragma once

#include <cstddef>
#include <string>

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
    double fireInterval_{0.2};
    float contactDamage_{10.0f};
    double fireCooldown_{0.0};
    float heroMoveSpeed_{200.0f};
    float heroMaxHp_{100.0f};
    int kills_{0};
    int wave_{0};
    Game::WaveSettings waveSettingsBase_{};
    double waveWarmup_{1.5};
    double waveWarmupBase_{1.5};
    bool defeated_{false};
    float fpsSmooth_{60.0f};
    double accumulated_{0.0};
    std::size_t tickCount_{0};
    std::unique_ptr<Game::MovementSystem> movementSystem_;
    std::unique_ptr<Game::CameraSystem> cameraSystem_;
    std::unique_ptr<Game::ProjectileSystem> projectileSystem_;
    std::unique_ptr<Game::CollisionSystem> collisionSystem_;
    std::unique_ptr<Game::EnemyAISystem> enemyAISystem_;
    std::unique_ptr<Game::WaveSystem> waveSystem_;
    std::unique_ptr<Engine::TextureManager> textureManager_;
    bool restartPrev_{false};
};

}  // namespace Game
