#include "ShopSystem.h"

namespace Game {

bool ShopSystem::tryPurchase(int index, int& credits, float& projDamage, float& heroHpMax, float& heroMove,
                             float& heroHpCurrent, ItemDefinition& purchasedOut) {
    if (index < 0 || index >= static_cast<int>(items_.size())) return false;
    const auto& item = items_[static_cast<std::size_t>(index)];
    if (credits < item.cost) return false;
    credits -= item.cost;
    switch (item.effect) {
        case ItemEffect::Damage:
            projDamage += item.value;
            break;
        case ItemEffect::Health:
            heroHpMax += item.value;
            heroHpCurrent = std::min(heroHpMax, heroHpCurrent + item.value);
            break;
        case ItemEffect::Speed:
            heroMove += item.value;
            break;
        case ItemEffect::Heal:
            heroHpCurrent = std::min(heroHpMax, heroHpCurrent + item.value * heroHpMax);
            break;
        default:
            // Unique/support effects are handled when consumed/used; just grant the item.
            break;
    }
    purchasedOut = item;
    return true;
}

}  // namespace Game
