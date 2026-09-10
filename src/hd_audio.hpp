#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace sote::hd_audio {

void initialize(const std::filesystem::path& runtime_directory);
bool is_enabled();
uint32_t play_sound_id(int32_t sound_id);
bool play_file(std::string_view filename, float gain = 1.0F);
bool play_voice_for_text(std::string_view text);
bool has_voice_for_text(std::string_view text);
// Call only for text actually drawn, with the current VI and level event.
bool play_visible_voice(std::string_view text, uint64_t vi, int event);
void note_message_text(
    uint32_t message_key,
    std::string_view text,
    uint32_t caller = 0);
bool mix_into(int16_t* samples, size_t sample_count, uint32_t output_frequency);

} // namespace sote::hd_audio
