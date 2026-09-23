#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sote::san_movies {

enum class ContainerKind {
    Unknown,
    AnimAhdr,
    SanmShdr,
};

struct MovieInfo {
    ContainerKind container = ContainerKind::Unknown;
    uint16_t version = 0;
    uint32_t frame_count = 0;
    uint32_t frame_rate = 0;
    uint32_t audio_rate = 0;
    uint32_t frme_chunks = 0;
    uint32_t fobj_chunks = 0;
    uint32_t iact_chunks = 0;
    uint32_t xpal_chunks = 0;
    uint32_t bl16_chunks = 0;
    uint32_t wave_chunks = 0;
    uint32_t codec47_chunks = 0;
    uint32_t codec48_chunks = 0;
    int16_t first_x = 0;
    int16_t first_y = 0;
    uint16_t first_width = 0;
    uint16_t first_height = 0;
    uint16_t max_width = 0;
    uint16_t max_height = 0;
    uint64_t payload_bytes = 0;
    std::string error;
};

struct CachedFrame {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> rgba;
    uint64_t serial = 0;
    bool valid() const {
        return width != 0 && height != 0 &&
            rgba.size() == static_cast<size_t>(width) * height * 4;
    }
};

MovieInfo inspect_file(const std::filesystem::path& path);
int expected_movie_count();
std::string_view expected_movie_name(int index);
std::filesystem::path find_movie(
    const std::filesystem::path& runtime_directory,
    std::string_view name);
bool play_cached_preview(std::string_view name);
bool play_startup_sequence();
bool stop_cached_playback();
bool cached_playback_active();
void note_music_command(std::string_view name);
bool menu_music_active();
CachedFrame latest_cached_frame();
void initialize(const std::filesystem::path& runtime_directory);

} // namespace sote::san_movies
