// Holds in-run shop inventory and purchase helpers (placeholder).
#pragma once

#include <vector>
#include "../components/ShopItem.h"

namespace Game {

class ShopSystem {
public:
    void setInventory(std::vector<ShopItem> items) { items_ = std::move(items); }
    const std::vector<ShopItem>& items() const { return items_; }

    // Attempt purchase; returns true if bought.
    bool tryPurchase(int index, int& credits, float& projDamage, float& heroHp, float& heroMove,
                     float& heroHpCurrent);

private:
    std::vector<ShopItem> items_;
};

}  // namespace Game
