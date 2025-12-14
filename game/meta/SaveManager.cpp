#include "SaveManager.h"

#include <fstream>
#include <filesystem>
#include <array>
#include <cstring>

#include <nlohmann/json.hpp>

namespace Game {

namespace {
constexpr uint32_t kMagic = 0x5A455441;  // 'ZETA'
// Simple stream key; not military-grade but prevents casual editing.
constexpr std::array<uint64_t, 2> kKeySeed = {0x7a6574616c696b65ULL, 0x516f5f4e4f4f4e21ULL};

uint32_t readU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void writeU32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}
}  // namespace

SaveManager::SaveManager(std::string path) : path_(std::move(path)) {}

bool SaveManager::load(SaveData& outData) {
    std::ifstream f(path_, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> fileBuf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (fileBuf.size() < 12) return false;
    uint32_t magic = readU32(fileBuf.data());
    if (magic != kMagic) return false;
    uint32_t payloadSize = readU32(fileBuf.data() + 4);
    uint32_t storedCrc = readU32(fileBuf.data() + 8);
    if (fileBuf.size() != payloadSize + 12) return false;
    std::vector<uint8_t> payload(fileBuf.begin() + 12, fileBuf.end());
    decrypt(payload);
    if (crc32(payload) != storedCrc) return false;
    return deserialize(payload, outData);
}

bool SaveManager::save(const SaveData& data) {
    std::vector<uint8_t> payload = serialize(data);
    uint32_t c = crc32(payload);
    encrypt(payload);
    std::vector<uint8_t> fileBuf;
    fileBuf.resize(12 + payload.size());
    writeU32(fileBuf.data(), kMagic);
    writeU32(fileBuf.data() + 4, static_cast<uint32_t>(payload.size()));
    writeU32(fileBuf.data() + 8, c);
    std::memcpy(fileBuf.data() + 12, payload.data(), payload.size());

    std::filesystem::create_directories(std::filesystem::path(path_).parent_path());
    std::ofstream f(path_, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(fileBuf.data()), static_cast<std::streamsize>(fileBuf.size()));
    return f.good();
}

std::vector<uint8_t> SaveManager::serialize(const SaveData& data) const {
    nlohmann::json j;
    j["version"] = data.version;
    j["totalRuns"] = data.totalRuns;
    j["bestWave"] = data.bestWave;
    j["totalKills"] = data.totalKills;
    j["movementMode"] = data.movementMode;
    nlohmann::json vault;
    vault["gold"] = data.vaultGold;
    vault["lastMatchId"] = data.lastDepositedMatchId;
    j["vault"] = vault;
    nlohmann::json upgrades;
    upgrades["attack_power"] = data.upgrades.attackPower;
    upgrades["attack_speed"] = data.upgrades.attackSpeed;
    upgrades["health"] = data.upgrades.health;
    upgrades["speed"] = data.upgrades.speed;
    upgrades["armor"] = data.upgrades.armor;
    upgrades["shields"] = data.upgrades.shields;
    upgrades["lifesteal"] = data.upgrades.lifesteal;
    upgrades["regeneration"] = data.upgrades.regeneration;
    upgrades["lives"] = data.upgrades.lives;
    upgrades["difficulty"] = data.upgrades.difficulty;
    upgrades["mastery"] = data.upgrades.mastery;
    j["global_upgrades"] = upgrades;
    auto str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

bool SaveManager::deserialize(const std::vector<uint8_t>& bytes, SaveData& outData) const {
    try {
        std::string s(bytes.begin(), bytes.end());
        auto j = nlohmann::json::parse(s);
        outData.version = j.value("version", 1);
        outData.totalRuns = j.value("totalRuns", 0);
        outData.bestWave = j.value("bestWave", 0);
        outData.totalKills = j.value("totalKills", 0);
        outData.movementMode = j.value("movementMode", 0);
        if (j.contains("vault")) {
            auto v = j["vault"];
            outData.vaultGold = v.value("gold", static_cast<int64_t>(0));
            outData.lastDepositedMatchId = v.value("lastMatchId", std::string{});
        } else {
            outData.vaultGold = j.value("vaultGold", static_cast<int64_t>(0));
            outData.lastDepositedMatchId = j.value("lastDepositedMatchId", std::string{});
        }
        if (j.contains("global_upgrades")) {
            auto g = j["global_upgrades"];
            outData.upgrades.attackPower = g.value("attack_power", 0);
            outData.upgrades.attackSpeed = g.value("attack_speed", 0);
            outData.upgrades.health = g.value("health", 0);
            outData.upgrades.speed = g.value("speed", 0);
            outData.upgrades.armor = g.value("armor", 0);
            outData.upgrades.shields = g.value("shields", 0);
            outData.upgrades.lifesteal = g.value("lifesteal", 0);
            outData.upgrades.regeneration = g.value("regeneration", 0);
            outData.upgrades.lives = g.value("lives", 0);
            outData.upgrades.difficulty = g.value("difficulty", 0);
            outData.upgrades.mastery = g.value("mastery", 0);
        }
        for (const auto& def : Meta::upgradeDefinitions()) {
            if (int* ptr = Meta::levelPtrByKey(outData.upgrades, def.key)) {
                *ptr = Meta::clampLevel(def, *ptr);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

void SaveManager::encrypt(std::vector<uint8_t>& buffer) const {
    uint64_t s0 = kKeySeed[0];
    uint64_t s1 = kKeySeed[1];
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        uint64_t x = s0 + s1;
        s1 ^= s0;
        s0 = ((s0 << 55) | (s0 >> (64 - 55))) ^ s1 ^ (s1 << 14);
        s1 = (s1 << 36) | (s1 >> (64 - 36));
        buffer[i] ^= static_cast<uint8_t>(x & 0xFF);
    }
}

void SaveManager::decrypt(std::vector<uint8_t>& buffer) const {
    // symmetric
    encrypt(buffer);
}

uint32_t SaveManager::crc32(const std::vector<uint8_t>& data) const {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint8_t b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

}  // namespace Game
