#include "Game.h"

#include <iomanip>
#include <sstream>

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
#include <cmath>

namespace Game {

bool GameRoot::onInitialize(Engine::Application& app) {
    Engine::logInfo("GameRoot initialized. Spawning placeholder hero entity.");
    render_ = &app.renderer();
    viewportWidth_ = app.config().width;
    viewportHeight_ = app.config().height;
    renderSystem_ = std::make_unique<RenderSystem>(*render_);
    movementSystem_ = std::make_unique<MovementSystem>();
    cameraSystem_ = std::make_unique<CameraSystem>();
    projectileSystem_ = std::make_unique<ProjectileSystem>();
    collisionSystem_ = std::make_unique<CollisionSystem>();
    enemyAISystem_ = std::make_unique<EnemyAISystem>();
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
    double initialWarmup = waveWarmup_;
    {
        std::ifstream gp("data/gameplay.json");
        if (gp.is_open()) {
            nlohmann::json j;
            gp >> j;
            if (j.contains("enemy")) {
                waveSettings.enemyHp = j["enemy"].value("hp", waveSettings.enemyHp);
                waveSettings.enemySpeed = j["enemy"].value("speed", waveSettings.enemySpeed);
                contactDamage = j["enemy"].value("contactDamage", contactDamage);
            }
            if (j.contains("projectile")) {
                projectileSpeed = j["projectile"].value("speed", projectileSpeed);
                projectileDamage = j["projectile"].value("damage", projectileDamage);
                fireRate = j["projectile"].value("fireRate", fireRate);
            }
            if (j.contains("hero")) {
                heroHp = j["hero"].value("hp", heroHp);
                heroMoveSpeed = j["hero"].value("moveSpeed", heroMoveSpeed);
            }
            if (j.contains("spawner")) {
                waveSettings.interval = j["spawner"].value("interval", waveSettings.interval);
                waveSettings.batchSize = j["spawner"].value("batchSize", waveSettings.batchSize);
                initialWarmup = j["spawner"].value("initialDelay", initialWarmup);
            }
        } else {
            Engine::logWarn("Failed to open data/gameplay.json; using defaults.");
        }
    }
    waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettings);
    waveSettings.contactDamage = contactDamage;
    waveSettingsBase_ = waveSettings;
    projectileSpeed_ = projectileSpeed;
    projectileDamage_ = projectileDamage;
    contactDamage_ = waveSettingsBase_.contactDamage;
    heroMoveSpeed_ = heroMoveSpeed;
    heroMaxHp_ = heroHp;
    waveWarmupBase_ = initialWarmup;
    waveWarmup_ = waveWarmupBase_;
    fireInterval_ = 1.0 / fireRate;
    fireCooldown_ = 0.0;
    if (collisionSystem_) collisionSystem_->setContactDamage(contactDamage_);

    // Attempt to load a placeholder sprite for hero (optional).
    if (render_ && textureManager_) {
        if (!manifest_.gridTexture.empty()) {
            auto grid = textureManager_->getOrLoadBMP(manifest_.gridTexture);
            if (grid) gridTexture_ = *grid;
        }
        // Load debug font if present.
        if (auto font = Engine::FontLoader::load("data/font.json", *textureManager_)) {
            debugText_ = std::make_unique<Engine::BitmapTextRenderer>(*render_, *font);
            Engine::logInfo("Loaded debug bitmap font.");
        } else {
            Engine::logWarn("Debug font missing; overlay disabled.");
        }
    }
    resetRun();
    return true;
}

void GameRoot::onUpdate(const Engine::TimeStep& step, const Engine::InputState& input) {
    accumulated_ += step.deltaSeconds;
    ++tickCount_;

    // Sample high-level actions.
    Engine::ActionState actions = actionMapper_.sample(input);

    const bool restartPressed = actions.restart && !restartPrev_;
    restartPrev_ = actions.restart;
    if (restartPressed) {
        resetRun();
        return;
    }

    // Update hero velocity from actions.
    if (hero_ != Engine::ECS::kInvalidEntity) {
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {actions.moveX * heroMoveSpeed_, actions.moveY * heroMoveSpeed_};
        }
        // Primary fire spawns projectile toward mouse.
        fireCooldown_ -= step.deltaSeconds;
        if (actions.primaryFire && fireCooldown_ <= 0.0) {
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
                registry_.emplace<Engine::ECS::AABB>(proj, Engine::ECS::AABB{Engine::Vec2{4.0f, 4.0f}});
                registry_.emplace<Engine::ECS::Renderable>(proj,
                                                           Engine::ECS::Renderable{Engine::Vec2{8.0f, 8.0f},
                                                                                   Engine::Color{255, 230, 90, 255}});
                registry_.emplace<Engine::ECS::Projectile>(proj,
                                                           Engine::ECS::Projectile{Engine::Vec2{dir.x * projectileSpeed_,
                                                                                                 dir.y * projectileSpeed_},
                                                                                     projectileDamage_, 1.5f});
                registry_.emplace<Engine::ECS::ProjectileTag>(proj, Engine::ECS::ProjectileTag{});
                fireCooldown_ = fireInterval_;
            }
        }
    }

    // Movement system.
    if (movementSystem_) {
        movementSystem_->update(registry_, step);
    }

    // Camera system.
    if (cameraSystem_) {
        cameraSystem_->update(camera_, registry_, hero_, actions, input, step, viewportWidth_, viewportHeight_);
    }

    // Enemy AI.
    if (enemyAISystem_ && !defeated_) {
        enemyAISystem_->update(registry_, hero_, step);
    }

    // Cleanup dead enemies (visual cleanup can be added later).
    {
        std::vector<Engine::ECS::Entity> toDestroy;
        registry_.view<Engine::ECS::Health, Engine::ECS::EnemyTag>(
            [&](Engine::ECS::Entity e, Engine::ECS::Health& hp, Engine::ECS::EnemyTag&) {
                if (!hp.alive()) {
                    toDestroy.push_back(e);
                }
            });
        for (auto e : toDestroy) {
            registry_.destroy(e);
            kills_ += 1;
        }
    }

    // Waves with warmup before spawns begin.
    if (waveSystem_ && !defeated_) {
        if (waveWarmup_ > 0.0) {
            waveWarmup_ -= step.deltaSeconds;
        } else {
            waveSystem_->update(registry_, step, hero_, wave_);
        }
    }

    // Projectiles.
    if (projectileSystem_ && !defeated_) {
        projectileSystem_->update(registry_, step);
    }

    // Collision / damage.
    if (collisionSystem_) {
        collisionSystem_->update(registry_);
    }

    handleHeroDeath();
    processDefeatInput(actions);

    // Render all rectangles.
    if (render_) {
        render_->clear({12, 12, 18, 255});
        if (renderSystem_) {
            renderSystem_->draw(registry_, camera_, viewportWidth_, viewportHeight_,
                                gridTexture_ ? gridTexture_.get() : nullptr);
        }
        if (debugText_) {
            std::ostringstream dbg;
            dbg << "FPS ~" << std::fixed << std::setprecision(1) << fpsSmooth_;
            dbg << " | Cam " << (cameraSystem_ && cameraSystem_->followEnabled() ? "Follow" : "Free");
            dbg << " | Zoom " << std::setprecision(2) << camera_.zoom;
            if (const auto* heroHealth = registry_.get<Engine::ECS::Health>(hero_)) {
                dbg << " | HP " << std::setprecision(0) << heroHealth->current << "/" << heroHealth->max;
            }
            dbg << " | Wave " << wave_ << " | Kills " << kills_;
            debugText_->drawText(dbg.str(), Engine::Vec2{10.0f, 10.0f}, 1.0f, Engine::Color{255, 255, 255, 200});
            if (waveWarmup_ > 0.0) {
                std::ostringstream warm;
                warm << "Wave " << (wave_ + 1) << " in " << std::fixed << std::setprecision(1)
                     << waveWarmup_ << "s";
                debugText_->drawText(warm.str(), Engine::Vec2{10.0f, 28.0f}, 1.0f,
                                     Engine::Color{255, 220, 120, 220});
            }
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

void GameRoot::onShutdown() { Engine::logInfo("GameRoot shutdown."); }

void GameRoot::handleHeroDeath() {
    auto* hp = registry_.get<Engine::ECS::Health>(hero_);
    if (hp && hp->current <= 0.0f && !defeated_) {
        defeated_ = true;
        Engine::logWarn("Hero defeated. Run over.");
        if (auto* vel = registry_.get<Engine::ECS::Velocity>(hero_)) {
            vel->value = {0.0f, 0.0f};
        }
    }
}

void GameRoot::showDefeatOverlay() {
    if (!defeated_ || !debugText_) return;
    std::string msg = "DEFEATED - Press R to Restart or ESC to Quit";
    debugText_->drawText(msg, Engine::Vec2{40.0f, 60.0f}, 1.5f, Engine::Color{255, 80, 80, 220});
}

void GameRoot::processDefeatInput(const Engine::ActionState& /*actions*/) {
    // Reserved for future defeat-specific inputs (e.g., confirm exit/menu).
}

void GameRoot::spawnHero() {
    hero_ = registry_.create();
    registry_.emplace<Engine::ECS::Transform>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Engine::ECS::Velocity>(hero_, Engine::Vec2{0.0f, 0.0f});
    registry_.emplace<Engine::ECS::Renderable>(hero_, Engine::ECS::Renderable{Engine::Vec2{24.0f, 24.0f},
                                                                               Engine::Color{90, 200, 255, 255}});
    registry_.emplace<Engine::ECS::AABB>(hero_, Engine::ECS::AABB{Engine::Vec2{12.0f, 12.0f}});
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
    wave_ = 0;
    defeated_ = false;
    accumulated_ = 0.0;
    tickCount_ = 0;
    fireCooldown_ = 0.0;
    mouseWorld_ = {};
    camera_ = {};
    restartPrev_ = true;  // consume the key that triggered the reset.
    waveWarmup_ = waveWarmupBase_;

    if (waveSystem_) {
        waveSystem_ = std::make_unique<WaveSystem>(rng_, waveSettingsBase_);
        waveSystem_->setTimer(0.0);
    }
    if (collisionSystem_) {
        collisionSystem_->setContactDamage(contactDamage_);
    }

    spawnHero();
    if (cameraSystem_) cameraSystem_->resetFollow(true);
}

}  // namespace Game
