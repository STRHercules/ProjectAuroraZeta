#include "ZetaStatusFactory.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <optional>

namespace {
using Engine::Status::EStatusId;
using Engine::Status::EStatusTag;
using Engine::Status::StatusMagnitude;
using Engine::Status::StatusSpec;

std::optional<EStatusId> idFromString(const std::string& s) {
    static const std::unordered_map<std::string, EStatusId> map{
        {"ArmorReduction", EStatusId::ArmorReduction},
        {"Blindness", EStatusId::Blindness},
        {"Cauterize", EStatusId::Cauterize},
        {"Feared", EStatusId::Feared},
        {"Silenced", EStatusId::Silenced},
        {"Stasis", EStatusId::Stasis},
        {"Cloaking", EStatusId::Cloaking},
        {"Unstoppable", EStatusId::Unstoppable},
    };
    auto it = map.find(s);
    if (it != map.end()) return it->second;
    return std::nullopt;
}

std::optional<EStatusTag> tagFromString(const std::string& s) {
    static const std::unordered_map<std::string, EStatusTag> map{
        {"CrowdControl", EStatusTag::CrowdControl},
        {"Vision", EStatusTag::Vision},
        {"Armor", EStatusTag::Armor},
        {"RegenBlock", EStatusTag::RegenBlock},
        {"Stealth", EStatusTag::Stealth},
        {"Immunity", EStatusTag::Immunity},
        {"CastingLock", EStatusTag::CastingLock},
        {"MovementLock", EStatusTag::MovementLock},
        {"SpeedImpair", EStatusTag::SpeedImpair},
    };
    auto it = map.find(s);
    if (it != map.end()) return it->second;
    return std::nullopt;
}

StatusSpec defaultSpec(EStatusId id) {
    StatusSpec spec{};
    spec.id = id;
    spec.duration = 3.0f;
    spec.maxStacks = 1;
    spec.refreshOnReapply = true;
    spec.isDebuff = true;
    return spec;
}

}  // namespace

namespace Game {

bool ZetaStatusFactory::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        // Leave specs empty; make() will fall back to defaults.
        return false;
    }
    nlohmann::json j;
    f >> j;

    for (const auto& [key, value] : j.items()) {
        if (!value.is_object()) continue;
        const std::string idStr = value.value("id", key);
        auto idOpt = idFromString(idStr);
        if (!idOpt) continue;
        StatusSpec spec = defaultSpec(*idOpt);
        spec.duration = value.value("duration", spec.duration);
        spec.maxStacks = value.value("maxStacks", spec.maxStacks);
        spec.refreshOnReapply = value.value("refreshOnReapply", spec.refreshOnReapply);
        spec.uniquePerSource = value.value("uniquePerSource", spec.uniquePerSource);
        spec.dispellable = value.value("dispellable", spec.dispellable);
        spec.isDebuff = value.value("isDebuff", spec.isDebuff);
        spec.tags.clear();
        if (value.contains("tags") && value["tags"].is_array()) {
            for (const auto& t : value["tags"]) {
                if (!t.is_string()) continue;
                auto tagOpt = tagFromString(t.get<std::string>());
                if (tagOpt) spec.tags.push_back(*tagOpt);
            }
        }
        if (value.contains("magnitude") && value["magnitude"].is_object()) {
            const auto& mag = value["magnitude"];
            spec.magnitude.armorDelta = mag.value("armorDelta", spec.magnitude.armorDelta);
            spec.magnitude.visionMultiplier = mag.value("visionMultiplier", spec.magnitude.visionMultiplier);
            spec.magnitude.moveSpeedMultiplier = mag.value("moveSpeedMultiplier", spec.magnitude.moveSpeedMultiplier);
            spec.magnitude.damageTakenMultiplier = mag.value("damageTakenMultiplier", spec.magnitude.damageTakenMultiplier);
            spec.magnitude.blocksRegen = mag.value("blocksRegen", spec.magnitude.blocksRegen);
            spec.magnitude.blocksHealingOverTime = mag.value("blocksHealingOverTime", spec.magnitude.blocksHealingOverTime);
        }
        specs_[*idOpt] = spec;
    }
    return true;
}

Engine::Status::StatusSpec ZetaStatusFactory::make(Engine::Status::EStatusId id) const {
    auto it = specs_.find(id);
    if (it != specs_.end()) {
        return it->second;
    }
    // Provide sane defaults if config missing.
    return defaultSpec(id);
}

}  // namespace Game
