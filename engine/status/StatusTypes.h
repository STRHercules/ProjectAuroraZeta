#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../ecs/Entity.h"

namespace Engine::Status {

// Enumerations describing supported status effects and semantic tags.
enum class EStatusId {
    ArmorReduction,
    Blindness,
    Cauterize,
    Feared,
    Silenced,
    Stasis,
    Cloaking,
    Unstoppable
};

enum class EStatusTag {
    CrowdControl,
    Vision,
    Armor,
    RegenBlock,
    Stealth,
    Immunity,
    CastingLock,
    MovementLock,
    SpeedImpair
};

// Generic payload carried by a status instance. Fields are optional by convention:
// use 1.0f for neutral multipliers and 0.0f for "unused" additive values.
struct StatusMagnitude {
    float armorDelta{0.0f};          // Negative to reduce armor.
    float visionMultiplier{1.0f};    // 0..1 visibility or detection scaling.
    float moveSpeedMultiplier{1.0f}; // Movement scalar applied while effect active.
    float damageTakenMultiplier{1.0f};
    bool blocksRegen{false};
    bool blocksHealingOverTime{false};
};

struct StatusSpec {
    EStatusId id{EStatusId::ArmorReduction};
    std::vector<EStatusTag> tags;
    float duration{0.0f};          // <= 0 means infinite.
    int maxStacks{1};
    bool refreshOnReapply{true};
    bool uniquePerSource{false};
    bool dispellable{true};
    bool isDebuff{true};
    StatusMagnitude magnitude{};
};

struct StatusInstance {
    StatusSpec spec;
    Engine::ECS::Entity source{Engine::ECS::kInvalidEntity};
    float remaining{0.0f};
    int stacks{1};

    bool infinite() const { return spec.duration <= 0.0f; }
};

}  // namespace Engine::Status
