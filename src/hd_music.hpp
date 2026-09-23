#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace sote::hd_music {

// Called before starting the game. No additional audio device is opened.
void initialize(const std::filesystem::path& runtime_directory);
bool is_enabled();

// These are driven by native music commands, not by the VI/level timer.
void command(std::string_view native_name);
bool replace_background(int32_t sound_id, int32_t native_volume);
bool replace_stinger(int32_t sound_id, int32_t native_volume, bool continuous);
void end_background_frame(bool native_requested_music);
void reset();
bool mix_into(int16_t* samples, size_t sample_count, uint32_t output_frequency);

// Read-only diagnostics also used by the host regression harness.
struct Status {
    bool enabled = false;
    bool background_active = false;
    bool background_finished = false;
    std::string cue;
    std::string file;
    double position_frames = 0.0;
    float gain = 0.0F;
    uint64_t background_starts = 0;
    uint64_t intercepted_requests = 0;
    uint64_t fallback_requests = 0;
    size_t stingers = 0;
};
Status status();

} // namespace sote::hd_music
