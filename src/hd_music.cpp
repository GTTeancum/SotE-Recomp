#include "hd_music.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4245 4456 4457 4701)
#endif
#include "stb/stb_vorbis.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace sote::hd_music {
namespace {

constexpr float default_native_gain = 0.25F;
constexpr float default_hd_gain = 1.0F;

struct Track {
    std::filesystem::path path;
    std::vector<int16_t> samples;
    int sample_rate = 0;
    int channels = 0;
    bool load_attempted = false;
    bool loaded = false;
};

std::mutex music_mutex;
std::filesystem::path sdata_directory;
std::filesystem::path music_directory;
std::unordered_map<std::string, Track> slots;
bool enabled = false;
std::string active_slot;
double active_position = 0.0;
float native_gain = default_native_gain;
float hd_gain = default_hd_gain;

float read_gain_env(const char* name, float fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value || !std::isfinite(parsed)) {
        return fallback;
    }
    return std::clamp(parsed, 0.0F, 2.0F);
}

std::string trim_copy(std::string_view value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string{value.substr(begin, end - begin)};
}

std::string lower_copy(std::string_view value) {
    std::string lowered{value};
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return lowered;
}

bool load_track_locked(Track& track, std::string_view slot_name) {
    if (track.loaded) {
        return true;
    }
    if (track.load_attempted) {
        return false;
    }

    track.load_attempted = true;
    int channels = 0;
    int sample_rate = 0;
    short* decoded = nullptr;
    const std::string path = track.path.string();
    const int frames = stb_vorbis_decode_filename(
        path.c_str(),
        &channels,
        &sample_rate,
        &decoded);
    if (frames <= 0 || decoded == nullptr ||
        channels <= 0 || sample_rate <= 0) {
        std::fprintf(
            stderr,
            "[sote][hd-music] failed to decode %s\n",
            path.c_str());
        if (decoded != nullptr) {
            std::free(decoded);
        }
        return false;
    }

    track.channels = channels;
    track.sample_rate = sample_rate;
    track.samples.assign(decoded, decoded + frames * channels);
    std::free(decoded);
    track.loaded = true;
    std::printf(
        "[sote][hd-music] loaded %.*s -> %s: %.2f sec, %d Hz, %d ch\n",
        static_cast<int>(slot_name.size()),
        slot_name.data(),
        path.c_str(),
        static_cast<double>(frames) / sample_rate,
        sample_rate,
        channels);
    std::fflush(stdout);
    return true;
}

int16_t sample_at(const Track& track, size_t frame, int channel) {
    if (track.channels == 1) {
        return track.samples[frame];
    }
    return track.samples[frame * track.channels + channel];
}

int16_t resampled_sample(const Track& track, double position, int channel) {
    const size_t frame_count = track.samples.size() / track.channels;
    const size_t frame0 =
        static_cast<size_t>(position) % frame_count;
    const size_t frame1 = (frame0 + 1) % frame_count;
    const double fraction = position - std::floor(position);
    const double a = sample_at(track, frame0, channel);
    const double b = sample_at(track, frame1, channel);
    return static_cast<int16_t>(std::lround(a + (b - a) * fraction));
}

int16_t clamp_i16(float value) {
    const float clamped =
        std::clamp(value, -32768.0F, 32767.0F);
    return static_cast<int16_t>(std::lround(clamped));
}

std::filesystem::path resolve_music_file(std::string_view filename) {
    std::filesystem::path path{trim_copy(filename)};
    if (path.empty()) {
        return {};
    }
    if (path.is_absolute() && std::filesystem::is_regular_file(path)) {
        return path;
    }

    const std::array<std::filesystem::path, 3> candidates = {
        music_directory / path,
        sdata_directory / path,
        std::filesystem::current_path() / path,
    };
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool register_slot(
    std::string_view slot_name,
    const std::filesystem::path& path) {
    const std::string slot = lower_copy(trim_copy(slot_name));
    if (slot.empty() || path.empty() ||
        !std::filesystem::is_regular_file(path)) {
        return false;
    }

    Track& track = slots[slot];
    track = {};
    track.path = path;
    return true;
}

void load_slot_map(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        return;
    }

    int mapped_count = 0;
    std::string line;
    while (std::getline(input, line)) {
        std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::string slot;
        std::string file;
        const size_t equals = trimmed.find('=');
        if (equals != std::string::npos) {
            slot = trim_copy(std::string_view{trimmed}.substr(0, equals));
            file = trim_copy(std::string_view{trimmed}.substr(equals + 1));
        } else {
            const size_t split = trimmed.find_first_of(" \t");
            if (split == std::string::npos) {
                continue;
            }
            slot = trim_copy(std::string_view{trimmed}.substr(0, split));
            file = trim_copy(std::string_view{trimmed}.substr(split + 1));
        }

        if (!file.empty() && file.front() == '"' && file.back() == '"' &&
            file.size() >= 2) {
            file = file.substr(1, file.size() - 2);
        }

        const std::filesystem::path resolved = resolve_music_file(file);
        if (register_slot(slot, resolved)) {
            ++mapped_count;
        } else {
            std::printf(
                "[sote][hd-music] ignoring missing N64 music map entry "
                "%s -> %s\n",
                slot.c_str(),
                file.c_str());
            std::fflush(stdout);
        }
    }

    std::printf(
        "[sote][hd-music] loaded %d N64 music slot mappings from %s\n",
        mapped_count,
        path.string().c_str());
    std::fflush(stdout);
}

void register_direct_slots() {
    static constexpr std::string_view known_slots[] = {
        "battle_of_hoth",
        "escape_from_echo_base",
        "asteroid_field",
        "ord_mantell_junkyard",
        "gall_spaceport",
        "mos_eisley_beggars_canyon",
        "imperial_freighter",
        "xizors_palace",
        "sewers_of_imperial_city",
        "skyhook_battle",
        "game_over",
    };

    for (std::string_view slot : known_slots) {
        const std::string key = lower_copy(slot);
        if (slots.find(key) != slots.end()) {
            continue;
        }
        const std::filesystem::path nested =
            music_directory / "n64" / (std::string{slot} + ".ogg");
        if (register_slot(slot, nested)) {
            continue;
        }
        register_slot(slot, music_directory / (std::string{slot} + ".ogg"));
    }
}

void register_numbered_track_slots() {
    if (!std::filesystem::is_directory(music_directory)) {
        return;
    }

    int direct_count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator{music_directory}) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        const std::string lowered = lower_copy(filename);
        if (lowered.size() != 11 ||
            lowered.substr(0, 5) != "track" ||
            lowered.substr(7) != ".ogg" ||
            !std::isdigit(static_cast<unsigned char>(lowered[5])) ||
            !std::isdigit(static_cast<unsigned char>(lowered[6]))) {
            continue;
        }

        const int track_number =
            (lowered[5] - '0') * 10 + (lowered[6] - '0');
        char track_slot[16]{};
        std::snprintf(
            track_slot,
            sizeof(track_slot),
            "track_%02d",
            track_number);
        if (register_slot(track_slot, entry.path())) {
            ++direct_count;
        }
        std::snprintf(
            track_slot,
            sizeof(track_slot),
            "track%d",
            track_number);
        register_slot(track_slot, entry.path());
    }

    if (direct_count > 0) {
        std::printf(
            "[sote][hd-music] registered %d direct TrackNN.ogg slots\n",
            direct_count);
        std::fflush(stdout);
    }
}

} // namespace

void initialize(const std::filesystem::path& runtime_directory) {
    std::lock_guard lock{music_mutex};
    enabled = false;
    active_slot.clear();
    active_position = 0.0;
    slots.clear();
    native_gain = read_gain_env(
        "SOTE_HD_MUSIC_NATIVE_GAIN",
        default_native_gain);
    hd_gain = read_gain_env("SOTE_HD_MUSIC_GAIN", default_hd_gain);

    const std::array<std::filesystem::path, 3> candidates = {
        runtime_directory / "Sdata" / "MUSIC",
        std::filesystem::current_path() / "Sdata" / "MUSIC",
        std::filesystem::current_path() / "SotE_Recompiled" /
            "Sdata" / "MUSIC",
    };
    music_directory = candidates[0];
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::is_directory(candidate) &&
            (std::filesystem::is_regular_file(
                 candidate / "n64_music_map.tsv") ||
             std::filesystem::is_regular_file(candidate / "Track02.ogg") ||
             std::filesystem::is_directory(candidate / "n64"))) {
            music_directory = candidate;
            break;
        }
    }
    sdata_directory = music_directory.parent_path();

    load_slot_map(sdata_directory / "n64_music_map.tsv");
    load_slot_map(music_directory / "n64_music_map.tsv");
    register_numbered_track_slots();
    register_direct_slots();

    enabled = !slots.empty();
    if (enabled) {
        std::printf(
            "[sote][hd-music] found %zu external N64 music slots in %s "
            "(native_gain=%.2f hd_gain=%.2f)\n",
            slots.size(),
            music_directory.string().c_str(),
            native_gain,
            hd_gain);
        std::fflush(stdout);
    } else {
        std::printf(
            "[sote][hd-music] no external N64 music slots found in %s\n",
            music_directory.string().c_str());
        std::fflush(stdout);
    }
}

bool is_enabled() {
    std::lock_guard lock{music_mutex};
    return enabled;
}

void set_track(int track_number) {
    char slot_name[16]{};
    std::snprintf(slot_name, sizeof(slot_name), "track_%02d", track_number);
    set_slot(slot_name);
}

void set_slot(std::string_view slot_name) {
    std::lock_guard lock{music_mutex};
    const std::string slot = lower_copy(trim_copy(slot_name));
    const auto track_it = slots.find(slot);
    if (!enabled || track_it == slots.end()) {
        active_slot.clear();
        active_position = 0.0;
        return;
    }
    if (active_slot == slot) {
        return;
    }
    if (!load_track_locked(track_it->second, slot)) {
        active_slot.clear();
        active_position = 0.0;
        return;
    }
    active_slot = slot;
    active_position = 0.0;
    std::printf(
        "[sote][hd-music] playing N64 slot %s\n",
        active_slot.c_str());
    std::fflush(stdout);
}

void stop() {
    std::lock_guard lock{music_mutex};
    active_slot.clear();
    active_position = 0.0;
}

bool mix_into(
    int16_t* samples,
    size_t sample_count,
    uint32_t output_frequency) {
    if (samples == nullptr || sample_count < 2 || output_frequency == 0) {
        return false;
    }

    std::lock_guard lock{music_mutex};
    const auto track_it = slots.find(active_slot);
    if (active_slot.empty() || track_it == slots.end() ||
        !track_it->second.loaded) {
        return false;
    }

    const Track& track = track_it->second;
    const size_t track_frames = track.samples.size() / track.channels;
    if (track_frames == 0) {
        return false;
    }

    const double step =
        static_cast<double>(track.sample_rate) / output_frequency;
    const size_t output_frames = sample_count / 2;
    for (size_t frame = 0; frame < output_frames; ++frame) {
        const int16_t hd_left =
            resampled_sample(track, active_position, 0);
        const int16_t hd_right = resampled_sample(
            track,
            active_position,
            track.channels > 1 ? 1 : 0);
        const size_t left_index = frame * 2;
        const size_t right_index = left_index + 1;
        samples[left_index] = clamp_i16(
            samples[left_index] * native_gain + hd_left * hd_gain);
        samples[right_index] = clamp_i16(
            samples[right_index] * native_gain + hd_right * hd_gain);

        active_position += step;
        if (active_position >= track_frames) {
            active_position = std::fmod(active_position, track_frames);
        }
    }
    return true;
}

} // namespace sote::hd_music
