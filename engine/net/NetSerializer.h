// Simple little-endian serializer helpers for network messages.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Net {

class NetWriter {
public:
    void writeU8(uint8_t v) { data_.push_back(v); }
    void writeU16(uint16_t v) { writeIntegral(v); }
    void writeU32(uint32_t v) { writeIntegral(v); }
    void writeI32(int32_t v) { writeIntegral(static_cast<uint32_t>(v)); }
    void writeF32(float v) {
        static_assert(sizeof(float) == sizeof(uint32_t));
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(float));
        writeIntegral(bits);
    }
    void writeString(std::string_view s) {
        writeU16(static_cast<uint16_t>(std::min<std::size_t>(s.size(), 0xFFFE)));
        data_.insert(data_.end(), s.begin(), s.begin() + std::min<std::size_t>(s.size(), 0xFFFE));
    }
    const std::vector<uint8_t>& buffer() const { return data_; }
    std::vector<uint8_t>& buffer() { return data_; }

private:
    template <typename T>
    void writeIntegral(T v) {
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            data_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    std::vector<uint8_t> data_;
};

class NetReader {
public:
    explicit NetReader(const std::vector<uint8_t>& buf) : data_(buf) {}

    bool readU8(uint8_t& out) { return readIntegral(out); }
    bool readU16(uint16_t& out) { return readIntegral(out); }
    bool readU32(uint32_t& out) { return readIntegral(out); }
    bool readI32(int32_t& out) {
        uint32_t tmp{};
        if (!readIntegral(tmp)) return false;
        out = static_cast<int32_t>(tmp);
        return true;
    }
    bool readF32(float& out) {
        uint32_t bits{};
        if (!readIntegral(bits)) return false;
        std::memcpy(&out, &bits, sizeof(float));
        return true;
    }
    bool readString(std::string& out) {
        uint16_t len{};
        if (!readU16(len)) return false;
        if (offset_ + len > data_.size()) return false;
        out.assign(reinterpret_cast<const char*>(data_.data()) + offset_, len);
        offset_ += len;
        return true;
    }
    bool eof() const { return offset_ >= data_.size(); }

private:
    template <typename T>
    bool readIntegral(T& out) {
        if (offset_ + sizeof(T) > data_.size()) return false;
        T v{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            v |= static_cast<T>(data_[offset_ + i]) << (i * 8);
        }
        offset_ += sizeof(T);
        out = v;
        return true;
    }

    const std::vector<uint8_t>& data_;
    std::size_t offset_{0};
};

}  // namespace Engine::Net
