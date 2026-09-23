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

std::mutex cache_mutex;
std::filesystem::path cache_root;
CachedFrame current_frame;
uint64_t frame_serial = 1;
bool current_music_is_main_menu = false;
std::vector<std::string> queued_movies;

struct CacheMetadata {
    unsigned width = 0;
    unsigned height = 0;
    uint32_t frames = 1;
    uint32_t decoded_frames = 0;
    double fps = 15.0;
};

struct CachePlayback {
    std::filesystem::path frames_path;
    CacheMetadata metadata;
    std::chrono::steady_clock::time_point started{};
    uint32_t frame_index = UINT32_MAX;
};

CachePlayback active_playback;

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
        std::filesystem::current_path() / "Sdata",
        std::filesystem::current_path() / "SotE_Recompiled" / "Sdata",
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

std::string trim(std::string value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool read_metadata(
    const std::filesystem::path& path,
    CacheMetadata& metadata) {
    std::ifstream input{path};
    if (!input.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, tab));
        const std::string value = trim(line.substr(tab + 1));
        char* end = nullptr;
        if (key == "width") {
            const unsigned parsed = static_cast<unsigned>(
                std::strtoul(value.c_str(), &end, 10));
            if (end != value.c_str()) {
                metadata.width = parsed;
            }
        } else if (key == "height") {
            const unsigned parsed = static_cast<unsigned>(
                std::strtoul(value.c_str(), &end, 10));
            if (end != value.c_str()) {
                metadata.height = parsed;
            }
        } else if (key == "frames") {
            const unsigned parsed = static_cast<unsigned>(
                std::strtoul(value.c_str(), &end, 10));
            if (end != value.c_str() && parsed != 0) {
                metadata.frames = parsed;
            }
        } else if (key == "fps") {
            const size_t slash = value.find('/');
            double parsed = 0.0;
            if (slash != std::string::npos) {
                const std::string numerator_text = value.substr(0, slash);
                const std::string denominator_text = value.substr(slash + 1);
                const double numerator = std::strtod(numerator_text.c_str(), nullptr);
                const double denominator = std::strtod(denominator_text.c_str(), nullptr);
                if (numerator > 0.0 && denominator > 0.0) {
                    parsed = numerator / denominator;
                }
            } else {
                parsed = std::strtod(value.c_str(), &end);
            }
            if (parsed > 0.0 && parsed < 240.0) {
                metadata.fps = parsed;
            }
        }
    }
    return metadata.width != 0 && metadata.height != 0;
}

bool read_cache_frame_locked(uint32_t frame_index) {
    if (active_playback.frames_path.empty() ||
        active_playback.metadata.width == 0 ||
        active_playback.metadata.height == 0) {
        return false;
    }
    const size_t frame_bytes =
        static_cast<size_t>(active_playback.metadata.width) *
        active_playback.metadata.height * 4;
    std::ifstream frames{active_playback.frames_path, std::ios::binary};
    if (!frames.is_open()) {
        return false;
    }
    frames.seekg(
        static_cast<std::streamoff>(
            static_cast<uint64_t>(frame_bytes) * frame_index),
        std::ios::beg);
    if (!frames.good()) {
        return false;
    }
    CachedFrame next;
    next.width = active_playback.metadata.width;
    next.height = active_playback.metadata.height;
    next.rgba.resize(frame_bytes);
    frames.read(
        reinterpret_cast<char*>(next.rgba.data()),
        static_cast<std::streamsize>(next.rgba.size()));
    if (frames.gcount() != static_cast<std::streamsize>(next.rgba.size())) {
        return false;
    }
    next.serial = frame_serial++;
    current_frame = std::move(next);
    active_playback.frame_index = frame_index;
    return true;
}

uint32_t count_decoded_frames(
    const std::filesystem::path& frames_path,
    const CacheMetadata& metadata) {
    if (metadata.width == 0 || metadata.height == 0) {
        return 0;
    }
    const uint64_t frame_bytes =
        static_cast<uint64_t>(metadata.width) * metadata.height * 4;
    if (frame_bytes == 0) {
        return 0;
    }
    std::error_code error;
    const uint64_t bytes = std::filesystem::file_size(frames_path, error);
    if (error || bytes < frame_bytes) {
        return 0;
    }
    return static_cast<uint32_t>(
        std::min<uint64_t>(bytes / frame_bytes, UINT32_MAX));
}

bool start_cached_preview_locked(std::string_view name) {
    if (cache_root.empty()) {
        return false;
    }
    const std::string base = basename_without_extension(name);
    if (base.empty()) {
        return false;
    }
    const std::filesystem::path movie_cache = cache_root / base;
    CacheMetadata metadata;
    if (!read_metadata(movie_cache / "metadata.tsv", metadata)) {
        std::printf(
            "[sote][san] cache unavailable for %s (missing metadata)\n",
            base.c_str());
        std::fflush(stdout);
        return false;
    }
    const std::filesystem::path frames_path = movie_cache / "frames.rgba";
    std::ifstream frames{frames_path, std::ios::binary};
    if (!frames.is_open()) {
        std::printf(
            "[sote][san] cache unavailable for %s (missing frames.rgba)\n",
            base.c_str());
        std::fflush(stdout);
        return false;
    }
    metadata.decoded_frames = count_decoded_frames(frames_path, metadata);
    if (metadata.decoded_frames == 0) {
        std::printf(
            "[sote][san] cache unavailable for %s (empty frames.rgba)\n",
            base.c_str());
        std::fflush(stdout);
        return false;
    }
    if (metadata.decoded_frames < metadata.frames) {
        std::printf(
            "[sote][san] cache for %s has %u decoded frame(s), metadata listed %u; clamping playback\n",
            base.c_str(),
            metadata.decoded_frames,
            metadata.frames);
        metadata.frames = metadata.decoded_frames;
    }
    active_playback.frames_path = frames_path;
    active_playback.metadata = metadata;
    active_playback.started = std::chrono::steady_clock::now();
    active_playback.frame_index = UINT32_MAX;
    if (!read_cache_frame_locked(0)) {
        std::printf(
            "[sote][san] cache unavailable for %s (truncated first frame)\n",
            base.c_str());
        std::fflush(stdout);
        active_playback = {};
        return false;
    }
    std::printf(
        "[sote][san] cached preview active: %s %ux%u frames=%u fps=%.3f\n",
        base.c_str(),
        metadata.width,
        metadata.height,
        metadata.frames,
        metadata.fps);
    std::fflush(stdout);
    return true;
}

bool start_next_queued_locked() {
    while (!queued_movies.empty()) {
        const std::string next = queued_movies.front();
        queued_movies.erase(queued_movies.begin());
        if (start_cached_preview_locked(next)) {
            return true;
        }
    }
    return false;
}

} // namespace

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

bool play_cached_preview(std::string_view name) {
    std::lock_guard lock{cache_mutex};
    queued_movies.clear();
    return start_cached_preview_locked(name);
}

bool play_startup_sequence() {
    std::lock_guard lock{cache_mutex};
    queued_movies = {
        "LONGTIME.SAN",
        "L01INTRO.SAN",
    };
    if (start_cached_preview_locked("L00LOGO.SAN")) {
        return true;
    }
    return start_next_queued_locked();
}

bool stop_cached_playback() {
    std::lock_guard lock{cache_mutex};
    const bool was_active = current_frame.valid();
    current_frame = {};
    active_playback = {};
    queued_movies.clear();
    if (was_active) {
        ++frame_serial;
    }
    return was_active;
}

bool cached_playback_active() {
    std::lock_guard lock{cache_mutex};
    return current_frame.valid();
}

void note_music_command(std::string_view name) {
    std::string lower_name{name};
    std::transform(
        lower_name.begin(),
        lower_name.end(),
        lower_name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::lock_guard lock{cache_mutex};
    current_music_is_main_menu =
        lower_name.find("main") != std::string::npos;
}

bool menu_music_active() {
    std::lock_guard lock{cache_mutex};
    return current_music_is_main_menu;
}

CachedFrame latest_cached_frame() {
    std::lock_guard lock{cache_mutex};
    if (current_frame.valid() && active_playback.metadata.frames > 0 &&
        active_playback.metadata.fps > 0.0) {
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - active_playback.started).count();
        const double exact_frame = elapsed * active_playback.metadata.fps;
        if (exact_frame >= active_playback.metadata.frames) {
            current_frame = {};
            active_playback = {};
            ++frame_serial;
            (void)start_next_queued_locked();
            return current_frame;
        }
        const auto desired = static_cast<uint32_t>(std::floor(exact_frame));
        if (desired != active_playback.frame_index) {
            (void)read_cache_frame_locked(desired);
        }
    }
    return current_frame;
}

void initialize(const std::filesystem::path& runtime_directory) {
    {
        std::lock_guard lock{cache_mutex};
            cache_root = runtime_directory / "Sdata" / "SAN_CACHE";
            current_frame = {};
            active_playback = {};
            queued_movies.clear();
            current_music_is_main_menu = false;
    }
    uint32_t found = 0;
    uint32_t anim = 0;
    uint32_t sanm = 0;
    std::array<std::filesystem::path, 3> examples{};
    for (const auto& root : san_search_roots(runtime_directory)) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
            if (error || !entry.is_regular_file(error)) {
                continue;
            }
            auto extension = entry.path().extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".san") {
                continue;
            }
            const MovieInfo info = inspect_file(entry.path());
            if (found < examples.size()) {
                examples[found] = entry.path().filename();
            }
            ++found;
            if (info.container == ContainerKind::AnimAhdr) {
                ++anim;
            } else if (info.container == ContainerKind::SanmShdr) {
                ++sanm;
            }
        }
        if (found != 0) {
            uint32_t expected_present = 0;
            for (const auto& name : expected_movies) {
                std::error_code expected_error;
                if (std::filesystem::is_regular_file(
                        root / std::filesystem::path{name},
                        expected_error)) {
                    ++expected_present;
                }
            }
            std::printf(
                "[sote][san] found %u loose SAN file(s) in %s "
                "(ANIM/AHDR=%u SANM/SHDR=%u expected=%u/%d; "
                "cache playback available)\n",
                found,
                root.string().c_str(),
                anim,
                sanm,
                expected_present,
                expected_movie_count());
            for (uint32_t index = 0; index < found && index < examples.size(); ++index) {
                const MovieInfo info = inspect_file(root / examples[index]);
                std::printf(
                    "[sote][san]   %s: %s frames=%u size=%ux%u max=%ux%u "
                    "FRME=%u FOBJ=%u c47=%u c48=%u IACT=%u XPAL=%u Bl16=%u Wave=%u\n",
                    examples[index].string().c_str(),
                    container_name(info.container),
                    info.frame_count,
                    info.first_width,
                    info.first_height,
                    info.max_width,
                    info.max_height,
                    info.frme_chunks,
                    info.fobj_chunks,
                    info.codec47_chunks,
                    info.codec48_chunks,
                    info.iact_chunks,
                    info.xpal_chunks,
                    info.bl16_chunks,
                    info.wave_chunks);
            }
            std::fflush(stdout);
            if (const char* preview = std::getenv("SOTE_SAN_PREVIEW")) {
                if (preview[0] != '\0') {
                    play_cached_preview(preview);
                }
            }
            return;
        }
    }
}

} // namespace sote::san_movies
