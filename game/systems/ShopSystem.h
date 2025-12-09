// Holds in-run shop inventory and purchase helpers (placeholder).
#pragma once

#include <vector>
#include "../meta/ItemDefs.h"

namespace Game {

class ShopSystem {
public:
    void setInventory(std::vector<ItemDefinition> items) { items_ = std::move(items); }
    const std::vector<ItemDefinition>& items() const { return items_; }

    // Attempt purchase; returns true if bought and outputs purchased item.
    bool tryPurchase(int index, int& gold, float& projDamage, float& heroHp, float& heroMove,
                     float& heroHpCurrent, ItemDefinition& purchasedOut);

private:
    std::vector<ItemDefinition> items_;
};

}  // namespace Game
