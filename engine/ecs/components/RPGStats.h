// ECS component holding RPG stat aggregation inputs + derived snapshot.
#pragma once

#include "../../gameplay/RPGStats.h"

namespace Engine::ECS {

// Game/system code is expected to populate attributes/base/modifiers and then mark dirty=true.
// Combat systems can refresh derived on demand using aggregateDerivedStats().
struct RPGStats {
    Engine::Gameplay::RPG::Attributes attributes{};
    Engine::Gameplay::RPG::BaseStatTemplate base{};
    Engine::Gameplay::RPG::StatContribution modifiers{};  // aggregated (gear + talents + buffs)
    Engine::Gameplay::RPG::DerivedStats derived{};
    bool dirty{true};

    // If true, refreshers may sync base health/armor/shields from Engine::ECS::Health before aggregating.
    bool baseFromHealth{false};
};

}  // namespace Engine::ECS

