#include "san_movies.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <memory>
extern "C" {
#include "sandec.h"
}

namespace sote::san_movies {
namespace {

constexpr uint32_t tag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(static_cast<unsigned char>(a)) << 24) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 8) |
        static_cast<uint32_t>(static_cast<unsigned char>(d));
}

constexpr std::string_view expected_movies[] = {
    "GAMEOVER.SAN",
    "L00LOGO.SAN",
    "L01INTRO.SAN",
    "L02INTRO.SAN",
    "L03INTRO.SAN",
    "L04BOSS.SAN",
    "L04INTRO.SAN",
    "L05BOSS.SAN",
    "L05INTRO.SAN",
    "L06INTRO.SAN",
    "L07INTRO.SAN",
    "L08INTRO.SAN",
    "L09BOSS.SAN",
    "L10INTRO.SAN",
    "L11LOSE.SAN",
    "L11WIN.SAN",
    "LONGTIME.SAN",
};

std::mutex playback_mutex;
std::filesystem::path movie_runtime_root;
MovieFrame current_frame;
uint64_t frame_serial = 1;
uint64_t movie_serial = 1;
bool current_music_is_main_menu = false;
std::vector<std::string> queued_movies;
bool hold_last_frame = false;

struct StreamPlayback {
    std::ifstream input;
    void* decoder = nullptr;
    sanio io{};
    std::deque<int16_t> audio;
    std::chrono::steady_clock::time_point started{};
    uint64_t token = 0;
    uint64_t audio_frames = 0;
    uint32_t frames = 0;
    uint32_t decoded = 0;
    double frame_end = 0.0;
    double frame_duration = 1.0 / 15.0;
    bool done = false;
    bool failed = false;
    bool discard_audio = false;
    ~StreamPlayback() { sandec_exit(&decoder); }
};
std::unique_ptr<StreamPlayback> active_playback;
#ifdef SOTE_SAN_TESTING
std::chrono::steady_clock::duration test_clock_offset{};
#endif
auto playback_now() {
    auto now = std::chrono::steady_clock::now();
#ifdef SOTE_SAN_TESTING
    now += test_clock_offset;
#endif
    return now;
}

uint16_t read_le16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(data[offset]) |
        static_cast<uint16_t>(data[offset + 1] << 8);
}

int16_t read_le_s16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(read_le16(data, offset));
}

uint16_t read_be16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(data[offset] << 8) |
        static_cast<uint16_t>(data[offset + 1]);
}

uint32_t read_le32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
        (static_cast<uint32_t>(data[offset + 1]) << 8) |
        (static_cast<uint32_t>(data[offset + 2]) << 16) |
        (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint32_t read_be32(const std::vector<uint8_t>& data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
        (static_cast<uint32_t>(data[offset + 1]) << 16) |
        (static_cast<uint32_t>(data[offset + 2]) << 8) |
        static_cast<uint32_t>(data[offset + 3]);
}

bool require_size(
    const std::vector<uint8_t>& data,
    size_t offset,
    size_t bytes,
    MovieInfo& info,
    const char* what) {
    if (offset <= data.size() && bytes <= data.size() - offset) {
        return true;
    }
    info.error = what;
    return false;
}

size_t padded_end(size_t start, uint32_t size, size_t file_size) {
    uint64_t end = static_cast<uint64_t>(start) + size + (size & 1U);
    if (end > file_size) {
        return file_size + 1;
    }
    return static_cast<size_t>(end);
}

void inspect_frame_payload(
    const std::vector<uint8_t>& data,
    size_t offset,
    size_t end,
    MovieInfo& info) {
    while (offset + 8 <= end) {
        const uint32_t subtag = read_be32(data, offset);
        const uint32_t subsize = read_be32(data, offset + 4);
        const size_t payload = offset + 8;
        const size_t next = padded_end(payload, subsize, data.size());
        if (next > data.size() || next > end + 1) {
            info.error = "truncated frame subchunk";
            return;
        }
        if (subtag == tag('F', 'O', 'B', 'J')) {
            ++info.fobj_chunks;
            if (subsize >= 14 && require_size(data, payload, subsize, info, "truncated FOBJ")) {
                const uint8_t codec = data[payload];
                if (codec == 47) {
                    ++info.codec47_chunks;
                } else if (codec == 48) {
                    ++info.codec48_chunks;
                }
                const int16_t x = read_le_s16(data, payload + 2);
                const int16_t y = read_le_s16(data, payload + 4);
                const uint16_t width = read_le16(data, payload + 6);
                const uint16_t height = read_le16(data, payload + 8);
                if (info.first_width == 0 && info.first_height == 0) {
                    info.first_x = x;
                    info.first_y = y;
                    info.first_width = width;
                    info.first_height = height;
                }
                info.max_width = std::max(info.max_width, width);
                info.max_height = std::max(info.max_height, height);
            }
        } else if (subtag == tag('I', 'A', 'C', 'T')) {
            ++info.iact_chunks;
        } else if (subtag == tag('X', 'P', 'A', 'L')) {
            ++info.xpal_chunks;
        } else if (subtag == tag('B', 'l', '1', '6')) {
            ++info.bl16_chunks;
        } else if (subtag == tag('W', 'a', 'v', 'e')) {
            ++info.wave_chunks;
        }
        info.payload_bytes += subsize;
        offset = next;
    }
}

std::vector<std::filesystem::path> san_search_roots(
    const std::filesystem::path& runtime_directory) {
    return {
        runtime_directory / "Sdata",
    };
}

const char* container_name(ContainerKind kind) {
    switch (kind) {
        case ContainerKind::AnimAhdr:
            return "ANIM/AHDR";
        case ContainerKind::SanmShdr:
            return "SANM/SHDR";
        case ContainerKind::Unknown:
        default:
            return "unknown";
    }
}

std::string basename_without_extension(std::string_view name) {
    std::string value{name};
    const size_t slash = value.find_last_of("/\\");
    if (slash != std::string::npos) {
        value = value.substr(slash + 1);
    }
    const size_t dot = value.find_last_of('.');
    if (dot != std::string::npos) {
        value = value.substr(0, dot);
    }
    return value;
}

int read_stream(void* user, void* dst, uint32_t size) {
    auto& stream = *static_cast<StreamPlayback*>(user);
    stream.input.read(static_cast<char*>(dst), size);
    return stream.input.gcount() == size;
}

void receive_audio(void* user, unsigned char* data, uint32_t size) {
    auto& stream = *static_cast<StreamPlayback*>(user);
    stream.audio_frames += size / 4;
    if (stream.discard_audio) return;
    // At most four seconds of interleaved stereo PCM, independent of movie length.
    constexpr size_t limit = 22050 * 2 * 4;
    if ((size & 3) || size / 2 > limit || stream.audio.size() + size / 2 > limit) {
        stream.failed = true;
        return;
    }
    for (uint32_t i = 0; i < size; i += 2)
        stream.audio.push_back(static_cast<int16_t>(data[i] | (uint16_t(data[i + 1]) << 8)));
}

void receive_video(void* user, unsigned char* data, uint32_t size,
                   uint16_t width, uint16_t height, uint16_t pitch,
                   uint32_t* palette, uint16_t, uint32_t duration) {
    auto& stream = *static_cast<StreamPlayback*>(user);
    const size_t row_bytes = size_t(width) * (palette ? 1 : 2);
    if (!width || !height || width > 2048 || height > 2048 || pitch < row_bytes ||
        size_t(pitch) * (height - 1) + row_bytes > size) {
        stream.failed = true;
        return;
    }
    current_frame.width = width;
    current_frame.height = height;
    current_frame.rgba.resize(size_t(width) * height * 4);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            auto* dst = current_frame.rgba.data() + (y * width + x) * 4;
            if (palette) {
                const uint32_t color = palette[data[y * pitch + x]];
                // sandec stores palette entries as ABGR (little-endian RGBA bytes).
                dst[0] = uint8_t(color); dst[1] = uint8_t(color >> 8); dst[2] = uint8_t(color >> 16);
            } else {
                const auto* src = data + y * pitch + x * 2;
                const uint16_t color = src[0] | (uint16_t(src[1]) << 8);
                dst[0] = uint8_t(((color >> 11) & 31) * 255 / 31);
                dst[1] = uint8_t(((color >> 5) & 63) * 255 / 63);
                dst[2] = uint8_t((color & 31) * 255 / 31);
            }
            dst[3] = 255;
        }
    }
    current_frame.serial = frame_serial++;
    stream.frame_duration = duration ? duration / 1000000.0 : 1.0 / 15.0;
}

bool decode_frame_locked() {
    auto& stream = *active_playback;
    if (stream.done || stream.failed) return false;
    const int status = sandec_decode_next_frame(stream.decoder);
    if (status == SANDEC_DONE) { stream.done = true; return false; }
    if (status != SANDEC_OK || stream.failed) {
        std::fprintf(stderr, "[sote][san] direct decode failed: status=%d frame=%u\n", status, stream.decoded);
        stream.failed = true;
        return false;
    }
    ++stream.decoded;
    stream.frame_end += stream.frame_duration;
    return true;
}

bool start_movie_locked(std::string_view name) {
    hold_last_frame = false;
    active_playback.reset();
    current_frame = {};
    const auto path = find_movie(movie_runtime_root, name);
    if (path.empty()) return false;
    active_playback = std::make_unique<StreamPlayback>();
    auto& stream = *active_playback;
    stream.input.open(path, std::ios::binary);
    stream.io = {read_stream, receive_video, receive_audio, &stream, 0};
    if (!stream.input || sandec_init(&stream.decoder) != SANDEC_OK ||
        sandec_open(stream.decoder, &stream.io) != SANDEC_OK) {
        active_playback.reset();
        return false;
    }
    stream.frames = std::max(0, sandec_get_framecount(stream.decoder));
    // Some files begin with palette/audio-only frames. Preserve their timeline.
    while (!current_frame.valid() && decode_frame_locked()) {}
    if (!current_frame.valid() || stream.failed) {
        active_playback.reset(); current_frame = {}; return false;
    }
    stream.token = movie_serial++;
    stream.started = playback_now();
    std::printf("[sote][san] direct SAN playback: %s %ux%u frames=%u (no disk cache)\n",
        path.filename().string().c_str(), current_frame.width, current_frame.height, stream.frames);
    std::fflush(stdout);
    return true;
}

bool start_next_queued_locked() {
    while (!queued_movies.empty()) {
        const std::string next = queued_movies.front();
        queued_movies.erase(queued_movies.begin());
        if (start_movie_locked(next)) {
            return true;
        }
    }
    return false;
}

} // namespace

#ifdef SOTE_SAN_TESTING
void advance_test_clock(double seconds) {
    std::lock_guard lock{playback_mutex};
    test_clock_offset += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(seconds));
}
#endif

MovieInfo inspect_file(const std::filesystem::path& path) {
    MovieInfo info;
    std::ifstream input{path, std::ios::binary};
    if (!input.is_open()) {
        info.error = "open failed";
        return info;
    }
    std::vector<uint8_t> data{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (!require_size(data, 0, 16, info, "file too small")) {
        return info;
    }

    const uint32_t main_tag = read_be32(data, 0);
    const uint32_t header_tag = read_be32(data, 8);
    const uint32_t header_size = read_be32(data, 12);
    size_t offset = 16;
    if (main_tag == tag('A', 'N', 'I', 'M') &&
        header_tag == tag('A', 'H', 'D', 'R')) {
        info.container = ContainerKind::AnimAhdr;
        if (!require_size(data, offset, header_size, info, "truncated AHDR")) {
            return info;
        }
        if (header_size < 0x30A) {
            info.error = "AHDR too small";
            return info;
        }
        info.version = read_le16(data, offset);
        info.frame_count = read_le16(data, offset + 2);
        (void)read_be16(data, offset + 4);
        if (info.version == 2 && header_size >= 0x312) {
            info.frame_rate = read_le32(data, offset + 0x306);
            info.audio_rate = read_le32(data, offset + 0x30E);
        } else {
            info.audio_rate = 11025;
        }
        offset = padded_end(offset, header_size, data.size());
    } else if (main_tag == tag('S', 'A', 'N', 'M') &&
               header_tag == tag('S', 'H', 'D', 'R')) {
        info.container = ContainerKind::SanmShdr;
        if (!require_size(data, offset, header_size, info, "truncated SHDR")) {
            return info;
        }
        if (header_size < 18) {
            info.error = "SHDR too small";
            return info;
        }
        info.version = read_le16(data, offset);
        info.frame_count = read_le32(data, offset + 2);
        info.frame_rate = read_le32(data, offset + 14);
        offset = padded_end(offset, header_size, data.size());
    } else {
        info.error = "not a supported SMUSH SAN container";
        return info;
    }
    if (offset > data.size()) {
        info.error = "truncated header padding";
        return info;
    }

    while (offset + 8 <= data.size()) {
        uint32_t chunk_tag = read_be32(data, offset);
        uint32_t chunk_size = read_be32(data, offset + 4);
        offset += 8;
        if (chunk_tag == tag('A', 'N', 'N', 'O')) {
            const size_t next = padded_end(offset, chunk_size, data.size());
            if (next > data.size()) {
                info.error = "truncated ANNO";
                return info;
            }
            offset = next;
            continue;
        }
        const size_t next = padded_end(offset, chunk_size, data.size());
        if (next > data.size()) {
            info.error = "truncated chunk";
            return info;
        }
        if (chunk_tag == tag('F', 'R', 'M', 'E')) {
            ++info.frme_chunks;
            inspect_frame_payload(data, offset, offset + chunk_size, info);
            if (!info.error.empty()) {
                return info;
            }
        }
        offset = next;
    }
    return info;
}

int expected_movie_count() {
    return static_cast<int>(std::size(expected_movies));
}

std::string_view expected_movie_name(int index) {
    if (index < 0 || index >= expected_movie_count()) {
        return {};
    }
    return expected_movies[index];
}

std::filesystem::path find_movie(
    const std::filesystem::path& runtime_directory,
    std::string_view name) {
    if (name.empty()) {
        return {};
    }
    for (const auto& root : san_search_roots(runtime_directory)) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) {
            continue;
        }
        const std::filesystem::path candidate = root / std::filesystem::path{name};
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }
    return {};
}

bool play_movie(std::string_view name) {
    std::lock_guard lock{playback_mutex};
    queued_movies.clear();
    return start_movie_locked(name);
}

bool play_sequence(std::initializer_list<std::string_view> names) {
    std::lock_guard lock{playback_mutex};
    queued_movies.clear();
    for (const std::string_view name : names) {
        queued_movies.emplace_back(name);
    }
    return start_next_queued_locked();
}

bool play_startup_sequence() {
    std::lock_guard lock{playback_mutex};
    queued_movies = {
        "LONGTIME.SAN",
    };
    if (start_movie_locked("L00LOGO.SAN")) {
        return true;
    }
    return start_next_queued_locked();
}

bool stop_playback() {
    std::lock_guard lock{playback_mutex};
    const bool was_active = current_frame.valid();
    current_frame = {};
    active_playback = {};
    queued_movies.clear();
    hold_last_frame = false;
    if (was_active) {
        ++frame_serial;
    }
    return was_active;
}

bool finish_playback() {
    std::lock_guard lock{playback_mutex};
    if (!active_playback || !current_frame.valid()) return false;
    hold_last_frame = true;
    active_playback->discard_audio = true;
    active_playback->audio.clear();
    while (decode_frame_locked()) {}
    active_playback->audio_frames = 0;
    active_playback->started = playback_now() -
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(active_playback->frame_end));
    return !active_playback->failed;
}

bool playback_active() {
    std::lock_guard lock{playback_mutex};
    return current_frame.valid();
}

uint64_t playback_token() {
    std::lock_guard lock{playback_mutex};
    return current_frame.valid() && active_playback ? active_playback->token : 0;
}

double playback_remaining_seconds() {
    std::lock_guard lock{playback_mutex};
    if (!active_playback) return 0.0;
    const auto& stream = *active_playback;
    const double duration = std::max(stream.audio_frames / 22050.0,
        stream.frame_end + (stream.done ? 0 :
        std::max<int64_t>(0, int64_t(stream.frames) - stream.decoded) * stream.frame_duration));
    const double elapsed = std::chrono::duration<double>(
        playback_now() - stream.started).count();
    return std::max(0.0, duration - elapsed);
}

void set_hold_last_frame(bool hold) {
    std::lock_guard lock{playback_mutex};
    hold_last_frame = hold;
}

std::vector<int16_t> next_audio() {
    std::lock_guard lock{playback_mutex};
    if (!active_playback) return {};
    auto& queue = active_playback->audio;
    const size_t count = std::min<size_t>(queue.size(), 8192);
    std::vector<int16_t> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(queue.front()); queue.pop_front();
    }
    return result;
}

void note_music_command(std::string_view name) {
    std::string lower_name{name};
    std::transform(
        lower_name.begin(),
        lower_name.end(),
        lower_name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::lock_guard lock{playback_mutex};
    current_music_is_main_menu =
        lower_name.find("main") != std::string::npos;
}

bool menu_music_active() {
    std::lock_guard lock{playback_mutex};
    return current_music_is_main_menu;
}

MovieFrame latest_frame() {
    std::lock_guard lock{playback_mutex};
    if (active_playback) {
        const double elapsed = std::chrono::duration<double>(
            playback_now() - active_playback->started).count();
        while (elapsed >= active_playback->frame_end && decode_frame_locked()) {}
        if (active_playback->failed || (active_playback->done && !hold_last_frame && active_playback->audio.empty() &&
            elapsed >= std::max(active_playback->frame_end, active_playback->audio_frames / 22050.0))) {
            current_frame = {};
            active_playback.reset();
            ++frame_serial;
            (void)start_next_queued_locked();
        }
    }
    return current_frame;
}

void initialize(const std::filesystem::path& runtime_directory) {
    {
        std::lock_guard lock{playback_mutex};
            movie_runtime_root = runtime_directory;
            current_frame = {};
            active_playback = {};
            queued_movies.clear();
            hold_last_frame = false;
            current_music_is_main_menu = false;
    }
    unsigned found = 0;
    for (auto name : expected_movies) {
        if (!find_movie(runtime_directory, name).empty()) ++found;
    }
    std::printf("[sote][san] %u/%zu loose SAN files available for direct playback\n",
                found, std::size(expected_movies));
    std::fflush(stdout);
}

} // namespace sote::san_movies
