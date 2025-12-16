#pragma once

#include <vector>
#include <optional>
#include <functional>

#include "StatusTypes.h"

namespace Engine::Status {

// Container owned per-entity, manages application, stacking, and expiry.
class StatusContainer {
public:
    bool apply(const StatusSpec& spec, ECS::Entity source);
    void remove(EStatusId id, ECS::Entity source = ECS::kInvalidEntity);
    void clear(bool includePermanent = false);
    void update(float dt);

    bool has(EStatusId id) const;
    bool hasTag(EStatusTag tag) const;
    int stacks(EStatusId id) const;
    StatusMagnitude magnitudeFor(EStatusId id) const;
    float armorDeltaTotal() const;
    float visionMultiplier() const;
    float moveSpeedMultiplier() const;
    float damageTakenMultiplier() const;
    bool blocksRegen() const;
    bool blocksCasting() const;
    bool blocksMovement() const;
    bool isFeared() const;
    bool isStealthed() const;
    bool isStasis() const;

    // Utility hook to purge all statuses matching predicate (used by Unstoppable).
    void purgeIf(const std::function<bool(const StatusInstance&)>& pred);

    const std::vector<StatusInstance>& all() const { return statuses_; }

private:
    std::vector<StatusInstance> statuses_;
};

}  // namespace Engine::Status
