#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace sote::hd_music {

void initialize(const std::filesystem::path& runtime_directory);
bool is_enabled();
void set_track(int track_number);
void set_slot(std::string_view slot_name);
void stop();
bool mix_into(int16_t* samples, size_t sample_count, uint32_t output_frequency);

} // namespace sote::hd_music
