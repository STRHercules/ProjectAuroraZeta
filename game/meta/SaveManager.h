// Binary, obfuscated save file for persistent progression.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "GlobalUpgrades.h"

namespace Game {

struct SaveData {
    int version{5};
    int totalRuns{0};
    int bestWave{0};
    int totalKills{0};  // Lifetime enemy kills (includes bosses).
    int64_t totalBossesKilled{0};
    int64_t totalBountyTargetsKilled{0};
    int64_t totalPickupsCollected{0};
    int64_t totalItemsCollected{0};
    int64_t totalPowerupsCollected{0};
    int64_t totalRevivesCollected{0};
    int64_t totalRevivesUsed{0};
    int64_t totalCopperPickedUp{0};
    int64_t totalGoldPickedUp{0};
    int64_t totalEscortsTransported{0};
    int64_t totalAssassinsThwarted{0};  // Assassin spires destroyed.
    int64_t totalBountyEventsCompleted{0};
    int64_t totalSpawnerEventsCompleted{0};
    int64_t totalSecondsPlayed{0};
    int movementMode{0};  // 0 = Modern, 1 = RTS
    int64_t vaultGold{0};
    std::string lastDepositedMatchId{};
    Meta::UpgradeLevels upgrades{};

    // Options (persisted).
    float musicVolume{0.65f};  // 0..1
    float sfxVolume{0.75f};    // 0..1
    bool backgroundAudio{true};
    bool showDamageNumbers{true};
    bool screenShake{true};
};

class SaveManager {
public:
    explicit SaveManager(std::string path);

    bool load(SaveData& outData);
    bool save(const SaveData& data);

private:
    std::string path_;
    std::vector<uint8_t> serialize(const SaveData& data) const;
    bool deserialize(const std::vector<uint8_t>& bytes, SaveData& outData) const;
    void encrypt(std::vector<uint8_t>& buffer) const;
    void decrypt(std::vector<uint8_t>& buffer) const;
    uint32_t crc32(const std::vector<uint8_t>& data) const;
};

}  // namespace Game
