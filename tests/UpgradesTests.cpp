#include <cassert>
#include <filesystem>
#include <limits>
#include <algorithm>

#include "../game/meta/GlobalUpgrades.h"
#include "../game/meta/SaveManager.h"

int main() {
    using namespace Game::Meta;
    {
        // Deposit guard only fires once per unique match id.
        auto first = applyDepositGuard(100, 50, "match-1", "");
        assert(first.performed);
        assert(first.deposited == 50);
        assert(first.newVault == 150);
        assert(first.newLastId == "match-1");
        auto second = applyDepositGuard(first.newVault, 80, "match-1", first.newLastId);
        assert(!second.performed);
        assert(second.newVault == first.newVault);
    }
    {
        // Mastery should amplify both player and enemy scaling.
        UpgradeLevels levels{};
        levels.attackPower = 5;
        levels.difficulty = 3;
        levels.mastery = 1;
        auto mods = computeModifiers(levels);
        assert(mods.playerAttackPowerMult > 1.05f);
        assert(mods.enemyPowerMult > 1.03f);
    }
    {
        // Max level enforcement (Lives capped at 3).
        const auto& defs = upgradeDefinitions();
        const auto& livesDef = *std::find_if(defs.begin(), defs.end(), [](const UpgradeDef& d) { return d.key == "lives"; });
        int64_t maxCost = costForNext(livesDef, livesDef.maxLevel);
        assert(maxCost == std::numeric_limits<int64_t>::max());
        assert(clampLevel(livesDef, 99) == livesDef.maxLevel);
    }
    {
        // Persistence of vault + upgrades across save/load.
        std::filesystem::path tmp = std::filesystem::temp_directory_path() / "zeta_save_test.dat";
        Game::SaveData data{};
        data.vaultGold = 1234;
        data.upgrades.attackPower = 2;
        data.lastDepositedMatchId = "mid-123";
        Game::SaveManager mgr(tmp.string());
        assert(mgr.save(data));
        Game::SaveData loaded{};
        assert(mgr.load(loaded));
        assert(loaded.vaultGold == 1234);
        assert(loaded.upgrades.attackPower == 2);
        assert(loaded.lastDepositedMatchId == "mid-123");
        std::filesystem::remove(tmp);
    }
    return 0;
}
