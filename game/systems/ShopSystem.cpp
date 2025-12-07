#include "ShopSystem.h"

namespace Game {

bool ShopSystem::tryPurchase(int index, int& credits, float& projDamage, float& heroHpMax, float& heroMove,
                             float& heroHpCurrent) {
    if (index < 0 || index >= static_cast<int>(items_.size())) return false;
    const auto& item = items_[static_cast<std::size_t>(index)];
    if (credits < item.cost) return false;
    credits -= item.cost;
    switch (item.type) {
        case ShopItem::Type::Damage:
            projDamage += item.value;
            break;
        case ShopItem::Type::Health:
            heroHpMax += item.value;
            heroHpCurrent = std::min(heroHpMax, heroHpCurrent + item.value);
            break;
        case ShopItem::Type::Speed:
            heroMove += item.value;
            break;
    }
    return true;
}

}  // namespace Game
