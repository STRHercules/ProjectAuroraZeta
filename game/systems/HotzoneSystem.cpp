#include "HotzoneSystem.h"

#include <cmath>

#include "../../engine/ecs/components/Transform.h"
#include "../../engine/ecs/components/Renderable.h"

namespace Game {

namespace {
Engine::Color colorForBonus(HotzoneSystem::Bonus b, bool active) {
    switch (b) {
        case HotzoneSystem::Bonus::XpSurge:
            return active ? Engine::Color{90, 200, 255, 120} : Engine::Color{90, 200, 255, 60};
        case HotzoneSystem::Bonus::DangerPay:
            return active ? Engine::Color{255, 210, 90, 120} : Engine::Color{255, 210, 90, 60};
        case HotzoneSystem::Bonus::WarpFlux:
        default:
            return active ? Engine::Color{200, 140, 255, 120} : Engine::Color{200, 140, 255, 60};
    }
}
}  // namespace

HotzoneSystem::HotzoneSystem(float rotationInterval, float xpBonus, float creditBonus,
                             float fluxDamageBonus, float fluxRateBonus,
                             float mapRadius, float minRadius, float maxRadius, float minSeparation,
                             uint32_t seed)
    : rotationInterval_(rotationInterval),
      rotationTimer_(rotationInterval),
      xpMultiplier_(xpBonus),
      creditMultiplier_(creditBonus),
      fluxDamageMultiplier_(fluxDamageBonus),
      fluxRateMultiplier_(fluxRateBonus),
      mapRadius_(mapRadius),
      minRadius_(minRadius),
      maxRadius_(maxRadius),
      minSeparation_(minSeparation),
      rng_(seed) {}

Engine::Vec2 HotzoneSystem::samplePosition() {
    // Uniform sample in circle; enforce minimum separation by rejection.
    std::uniform_real_distribution<float> angle(0.0f, 6.28318f);
    std::uniform_real_distribution<float> r01(0.0f, 1.0f);
    const int kMaxAttempts = 50;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        float a = angle(rng_);
        float r = std::sqrt(r01(rng_)) * mapRadius_;
        Engine::Vec2 candidate{std::cos(a) * r, std::sin(a) * r};
        bool farEnough = true;
        for (const auto& z : zones_) {
            float dx = candidate.x - z.center.x;
            float dy = candidate.y - z.center.y;
            if ((dx * dx + dy * dy) < (minSeparation_ * minSeparation_)) {
                farEnough = false;
                break;
            }
        }
        if (farEnough) return candidate;
    }
    // Fallback if we cannot place after attempts.
    float a = angle(rng_);
    float r = std::sqrt(r01(rng_)) * mapRadius_;
    return Engine::Vec2{std::cos(a) * r, std::sin(a) * r};
}

void HotzoneSystem::initialize(Engine::ECS::Registry& registry, int zoneCount) {
    zones_.clear();
    zones_.reserve(zoneCount);
    int idx = 0;
    for (int i = 0; i < zoneCount; ++i) {
        Zone z;
        z.center = samplePosition();
        std::uniform_real_distribution<float> rdist(minRadius_, maxRadius_);
        z.radius = rdist(rng_);
        z.bonus = static_cast<Bonus>(idx % 3);
        auto e = registry.create();
        registry.emplace<Engine::ECS::Transform>(e, z.center);
        // Visualize as softly colored disc (rectangle stand-in).
        Engine::Vec2 size{z.radius * 2.0f, z.radius * 2.0f};
        registry.emplace<Engine::ECS::Renderable>(e, Engine::ECS::Renderable{size, colorForBonus(z.bonus, idx == 0)});
        z.entity = e;
        zones_.push_back(z);
        ++idx;
    }
    activeIndex_ = 0;
    activeBonus_ = zones_.empty() ? Bonus::XpSurge : zones_.front().bonus;
    rotationTimer_ = rotationInterval_;
    fluxSurgePending_ = (activeBonus_ == Bonus::WarpFlux);
}

void HotzoneSystem::rerollZones(Engine::ECS::Registry& registry) {
    if (zones_.empty()) return;
    std::uniform_real_distribution<float> rdist(minRadius_, maxRadius_);
    for (std::size_t i = 0; i < zones_.size(); ++i) {
        auto& z = zones_[i];
        z.center = samplePosition();
        z.radius = rdist(rng_);
        if (auto* tf = registry.get<Engine::ECS::Transform>(z.entity)) {
            tf->position = z.center;
        }
        if (auto* rend = registry.get<Engine::ECS::Renderable>(z.entity)) {
            rend->size = Engine::Vec2{z.radius * 2.0f, z.radius * 2.0f};
        }
    }
}

void HotzoneSystem::rotateZone(Engine::ECS::Registry& registry) {
    if (zones_.empty()) return;
    activeIndex_ = (activeIndex_ + 1) % static_cast<int>(zones_.size());
    // Rotate bonus type sequence.
    int bonusIdx = static_cast<int>(activeBonus_);
    bonusIdx = (bonusIdx + 1) % 3;
    activeBonus_ = static_cast<Bonus>(bonusIdx);
    zones_[activeIndex_].bonus = activeBonus_;
    rerollZones(registry);
    rotationTimer_ = rotationInterval_;
    fluxSurgePending_ = (activeBonus_ == Bonus::WarpFlux);
    warningActive_ = false;
    updateVisuals(registry);
}

void HotzoneSystem::updateVisuals(Engine::ECS::Registry& registry) {
    for (std::size_t i = 0; i < zones_.size(); ++i) {
        auto& z = zones_[i];
        if (auto* rend = registry.get<Engine::ECS::Renderable>(z.entity)) {
            bool active = static_cast<int>(i) == activeIndex_;
            auto base = colorForBonus(z.bonus, active);
            if (active && warningActive_) {
                // Pulse brighter on warning.
                base.a = static_cast<uint8_t>(std::min(255, base.a + 80));
            }
            rend->color = base;
        }
    }
}

void HotzoneSystem::update(Engine::ECS::Registry& registry, const Engine::TimeStep& step, Engine::ECS::Entity hero) {
    if (zones_.empty()) return;

    rotationTimer_ -= step.deltaSeconds;
    if (!warningActive_ && rotationTimer_ <= warningWindow_) {
        warningActive_ = true;
        updateVisuals(registry);
    }
    if (rotationTimer_ <= 0.0) {
        rotateZone(registry);
    }

    heroInsideActive_ = false;
    const Zone& activeZone = zones_[activeIndex_];
    const auto* tf = registry.get<Engine::ECS::Transform>(hero);
    if (tf) {
        float dx = tf->position.x - activeZone.center.x;
        float dy = tf->position.y - activeZone.center.y;
        float dist2 = dx * dx + dy * dy;
        heroInsideActive_ = dist2 <= (activeZone.radius * activeZone.radius);
    }
}

}  // namespace Game
