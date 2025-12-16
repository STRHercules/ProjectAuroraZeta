#include "StatusContainer.h"

#include <algorithm>
#include <cmath>

namespace Engine::Status {

namespace {
bool specHasTag(const StatusSpec& spec, EStatusTag tag) {
    return std::find(spec.tags.begin(), spec.tags.end(), tag) != spec.tags.end();
}

bool blocksCrowdControl(const StatusSpec& spec) {
    return specHasTag(spec, EStatusTag::CrowdControl) || specHasTag(spec, EStatusTag::SpeedImpair);
}
}  // namespace

bool StatusContainer::apply(const StatusSpec& spec, ECS::Entity source) {
    // If target already has Unstoppable, reject CC/slow incoming statuses.
    const bool unstoppableActive = has(EStatusId::Unstoppable);
    if (unstoppableActive && blocksCrowdControl(spec) && spec.id != EStatusId::Unstoppable) {
        return false;
    }
    // Stasis rejects new debuffs (except itself).
    if (spec.isDebuff && has(EStatusId::Stasis) && spec.id != EStatusId::Stasis) {
        return false;
    }

    // Unique-per-source handling.
    for (auto& inst : statuses_) {
        if (inst.spec.id == spec.id) {
            if (spec.uniquePerSource && inst.source != source) continue;
            inst.source = source;
            if (spec.refreshOnReapply) {
                inst.remaining = spec.duration;
            }
            if (inst.spec.maxStacks > 1 || spec.maxStacks > 1) {
                const int maxStacks = std::max(inst.spec.maxStacks, spec.maxStacks);
                inst.stacks = std::clamp(inst.stacks + 1, 1, maxStacks);
            } else {
                inst.stacks = 1;
            }
            inst.spec = spec;
            return true;
        }
    }

    StatusInstance inst;
    inst.spec = spec;
    inst.source = source;
    inst.remaining = spec.duration;
    inst.stacks = 1;
    statuses_.push_back(inst);

    // Applying Unstoppable purges existing CC.
    if (spec.id == EStatusId::Unstoppable) {
        purgeIf([](const StatusInstance& s) {
            if (s.spec.id == EStatusId::Unstoppable) return false;
            return blocksCrowdControl(s.spec);
        });
    }
    return true;
}

void StatusContainer::remove(EStatusId id, ECS::Entity source) {
    statuses_.erase(
        std::remove_if(statuses_.begin(), statuses_.end(),
                       [&](const StatusInstance& inst) {
                           if (inst.spec.id != id) return false;
                           if (source != ECS::kInvalidEntity && inst.source != source) return false;
                           return true;
                       }),
        statuses_.end());
}

void StatusContainer::clear(bool includePermanent) {
    statuses_.erase(
        std::remove_if(statuses_.begin(), statuses_.end(),
                       [includePermanent](const StatusInstance& inst) {
                           if (!includePermanent && inst.infinite()) return false;
                           return true;
                       }),
        statuses_.end());
}

void StatusContainer::update(float dt) {
    for (auto& inst : statuses_) {
        if (inst.infinite()) continue;
        inst.remaining -= dt;
    }
    statuses_.erase(std::remove_if(statuses_.begin(), statuses_.end(),
                                   [&](const StatusInstance& inst) {
                                       if (inst.infinite()) return false;
                                       return inst.remaining <= 0.0f;
                                   }),
                    statuses_.end());
}

bool StatusContainer::has(EStatusId id) const {
    for (const auto& inst : statuses_) {
        if (inst.spec.id == id) return true;
    }
    return false;
}

bool StatusContainer::hasTag(EStatusTag tag) const {
    for (const auto& inst : statuses_) {
        if (specHasTag(inst.spec, tag)) return true;
    }
    return false;
}

int StatusContainer::stacks(EStatusId id) const {
    for (const auto& inst : statuses_) {
        if (inst.spec.id == id) return inst.stacks;
    }
    return 0;
}

StatusMagnitude StatusContainer::magnitudeFor(EStatusId id) const {
    for (const auto& inst : statuses_) {
        if (inst.spec.id == id) {
            StatusMagnitude m = inst.spec.magnitude;
            // Apply stack scaling linearly.
            m.armorDelta *= static_cast<float>(inst.stacks);
            m.visionMultiplier = std::pow(m.visionMultiplier, static_cast<float>(inst.stacks));
            m.moveSpeedMultiplier = std::pow(m.moveSpeedMultiplier, static_cast<float>(inst.stacks));
            m.damageTakenMultiplier = std::pow(m.damageTakenMultiplier, static_cast<float>(inst.stacks));
            return m;
        }
    }
    return {};
}

float StatusContainer::armorDeltaTotal() const {
    float delta = 0.0f;
    for (const auto& inst : statuses_) {
        if (inst.spec.magnitude.armorDelta != 0.0f) {
            delta += inst.spec.magnitude.armorDelta * static_cast<float>(inst.stacks);
        }
    }
    return delta;
}

float StatusContainer::visionMultiplier() const {
    float mul = 1.0f;
    for (const auto& inst : statuses_) {
        mul *= inst.spec.magnitude.visionMultiplier;
    }
    return mul;
}

float StatusContainer::moveSpeedMultiplier() const {
    float mul = 1.0f;
    for (const auto& inst : statuses_) {
        mul *= inst.spec.magnitude.moveSpeedMultiplier;
    }
    return mul;
}

float StatusContainer::damageTakenMultiplier() const {
    float mul = 1.0f;
    for (const auto& inst : statuses_) {
        mul *= inst.spec.magnitude.damageTakenMultiplier;
    }
    return mul;
}

bool StatusContainer::blocksRegen() const {
    for (const auto& inst : statuses_) {
        if (inst.spec.magnitude.blocksRegen) return true;
    }
    return false;
}

bool StatusContainer::blocksCasting() const {
    return has(EStatusId::Stasis) || has(EStatusId::Silenced) || has(EStatusId::Feared) ||
           hasTag(EStatusTag::CastingLock);
}

bool StatusContainer::blocksMovement() const {
    return has(EStatusId::Stasis) || hasTag(EStatusTag::MovementLock);
}

bool StatusContainer::isFeared() const {
    return has(EStatusId::Feared);
}

bool StatusContainer::isStealthed() const {
    return has(EStatusId::Cloaking);
}

bool StatusContainer::isStasis() const {
    return has(EStatusId::Stasis);
}

void StatusContainer::purgeIf(const std::function<bool(const StatusInstance&)>& pred) {
    statuses_.erase(std::remove_if(statuses_.begin(), statuses_.end(),
                                   [&](const StatusInstance& inst) { return pred(inst); }),
                    statuses_.end());
}

}  // namespace Engine::Status
