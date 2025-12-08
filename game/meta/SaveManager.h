// Binary, obfuscated save file for persistent progression.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Game {

struct SaveData {
    int version{1};
    int totalRuns{0};
    int bestWave{0};
    int totalKills{0};
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
