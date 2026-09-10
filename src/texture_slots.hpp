#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <json/json.hpp>

namespace sote::texture_slots {
// Version 1: context, source address, sampled width/height, fmt, siz, line,
// TMEM, palette, TLUT, source width, load ULS/ULT, source fmt/siz, load type, palette source address.
using Key = std::array<uint32_t, 17>;
using Map = std::map<Key, uint64_t>;

inline uint64_t alias(const Key& key) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : std::string_view("SOTE-slot-v1")) {
        hash = (hash ^ c) * 1099511628211ULL;
    }
    for (uint32_t value : key) {
        for (int i = 0; i < 4; ++i) {
            hash = (hash ^ (value & 255U)) * 1099511628211ULL;
            value >>= 8;
        }
    }
    return hash;
}

inline void load(const std::filesystem::path& path, Map& destination) {
    std::ifstream input(path);
    if (!input) return;
    const auto root = nlohmann::json::parse(input);
    if (root.at("version") != 1 || !root.at("slots").is_array())
        throw std::runtime_error("unsupported texture slot manifest");
    Map additions;
    for (const auto& item : root.at("slots")) {
        const auto& values = item.at("key");
        if (!values.is_array() || values.size() != 17)
            throw std::runtime_error("texture slot key must have 17 integers");
        Key key{};
        for (size_t i = 0; i < key.size(); ++i) {
            if (!values[i].is_number_unsigned() || values[i].get<uint64_t>() > UINT32_MAX)
                throw std::runtime_error("invalid texture slot key integer");
            key[i] = values[i].get<uint32_t>();
        }
        if (key[0] > 31 || key[1] >= 0x800000 || key[2] == 0 || key[2] > 4096 ||
            key[16] >= 0x800000 || key[3] == 0 || key[3] > 4096 || key[4] > 4 || key[5] > 3 || key[7] >= 512)
            throw std::runtime_error("texture slot key outside supported range");
        const std::string text = item.at("hash").get<std::string>();
        uint64_t hash = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), hash, 16);
        if (text.size() != 16 || parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() ||
            hash == 0 || hash != alias(key))
            throw std::runtime_error("texture slot hash does not match key");
        additions[key] = hash;
    }
    // A malformed file cannot partially activate its slots.
    destination.insert(additions.begin(), additions.end());
}
}
