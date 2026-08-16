#include "hd_audio.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sote::hd_audio {
namespace {

constexpr float default_gain = 1.0F;
constexpr uint32_t fnv_offset = 2166136261U;
constexpr uint32_t fnv_prime = 16777619U;

struct Wave {
    std::filesystem::path path;
    std::vector<int16_t> samples;
    int sample_rate = 0;
    int channels = 0;
    bool load_attempted = false;
    bool loaded = false;
};

struct Mapping {
    std::string filename;
    float gain = default_gain;
    bool one_shot = false;
};

struct Voice {
    std::shared_ptr<Wave> wave;
    double position = 0.0;
    float gain = default_gain;
};

std::mutex audio_mutex;
std::filesystem::path sdata_directory;
bool enabled = false;
bool trace_audio = false;
float global_gain = default_gain;
std::unordered_map<std::string, std::shared_ptr<Wave>> waves;
std::unordered_map<int32_t, Mapping> sound_map;
std::unordered_map<uint32_t, Mapping> voice_map;
std::unordered_set<int32_t> traced_sound_ids;
std::unordered_set<uint32_t> traced_message_hashes;
std::unordered_set<uint32_t> played_one_shot_voice_hashes;
std::unordered_map<uint32_t, std::chrono::steady_clock::time_point>
    voice_cooldowns;
std::vector<Voice> active_voices;

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

uint32_t read_u32_le(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) |
        (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t read_u16_le(const uint8_t* bytes) {
    return static_cast<uint16_t>(
        static_cast<uint16_t>(bytes[0]) |
        (static_cast<uint16_t>(bytes[1]) << 8));
}

std::string lowercase(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string trim(std::string_view view) {
    while (!view.empty() &&
           std::isspace(static_cast<unsigned char>(view.front()))) {
        view.remove_prefix(1);
    }
    while (!view.empty() &&
           std::isspace(static_cast<unsigned char>(view.back()))) {
        view.remove_suffix(1);
    }
    return std::string{view};
}

std::vector<std::string> split_fields(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    for (char c : line) {
        if (c == '\t' || c == ',' || c == ' ') {
            if (!current.empty()) {
                fields.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        fields.push_back(current);
    }
    return fields;
}

bool parse_i32(std::string_view value, int32_t& parsed) {
    const char* begin = value.data();
    const char* end = begin + value.size();
    int base = 10;
    if (value.size() > 2 && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        begin += 2;
        base = 16;
    }
    const std::from_chars_result result =
        std::from_chars(begin, end, parsed, base);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse_u32(std::string_view value, uint32_t& parsed) {
    const char* begin = value.data();
    const char* end = begin + value.size();
    int base = 10;
    if (value.size() > 2 && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        begin += 2;
        base = 16;
    }
    const std::from_chars_result result =
        std::from_chars(begin, end, parsed, base);
    return result.ec == std::errc{} && result.ptr == end;
}

float parse_gain(const std::vector<std::string>& fields, size_t index) {
    if (fields.size() <= index) {
        return default_gain;
    }
    char* end = nullptr;
    const float parsed = std::strtof(fields[index].c_str(), &end);
    if (end == fields[index].c_str() || !std::isfinite(parsed)) {
        return default_gain;
    }
    return std::clamp(parsed, 0.0F, 2.0F);
}

std::string normalize_message(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    bool in_control = false;
    bool last_space = true;
    for (char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte == 0) {
            break;
        }
        if (c == '~') {
            in_control = true;
            continue;
        }
        if (in_control) {
            if ((c == 'n' || c == 'N' || c == 'r' || c == 'R' ||
                    c == 't' || c == 'T') &&
                !last_space) {
                normalized.push_back(' ');
                last_space = true;
            }
            in_control = false;
            continue;
        }
        if (std::isspace(byte)) {
            if (!last_space) {
                normalized.push_back(' ');
                last_space = true;
            }
            continue;
        }
        if (byte >= 0x20 && byte < 0x7F) {
            normalized.push_back(static_cast<char>(std::tolower(byte)));
            last_space = false;
        }
    }
    if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    return normalized;
}

uint32_t hash_normalized_message(std::string_view text) {
    const std::string normalized = normalize_message(text);
    uint32_t hash = fnv_offset;
    for (unsigned char c : normalized) {
        hash ^= c;
        hash *= fnv_prime;
    }
    return hash;
}

int16_t clamp_i16(float value) {
    return static_cast<int16_t>(
        std::lround(std::clamp(value, -32768.0F, 32767.0F)));
}

bool load_wave_locked(Wave& wave) {
    if (wave.loaded) {
        return true;
    }
    if (wave.load_attempted) {
        return false;
    }
    wave.load_attempted = true;

    std::ifstream file{wave.path, std::ios::binary};
    if (!file) {
        return false;
    }
    std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}};
    if (bytes.size() < 44 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        std::fprintf(stderr, "[sote][hd-audio] invalid WAV: %s\n",
            wave.path.string().c_str());
        return false;
    }

    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    const uint8_t* data = nullptr;
    size_t data_size = 0;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const char* chunk_id =
            reinterpret_cast<const char*>(bytes.data() + offset);
        const uint32_t chunk_size = read_u32_le(bytes.data() + offset + 4);
        offset += 8;
        if (offset + chunk_size > bytes.size()) {
            break;
        }
        if (std::memcmp(chunk_id, "fmt ", 4) == 0 && chunk_size >= 16) {
            const uint8_t* fmt = bytes.data() + offset;
            format = read_u16_le(fmt);
            channels = read_u16_le(fmt + 2);
            sample_rate = read_u32_le(fmt + 4);
            bits_per_sample = read_u16_le(fmt + 14);
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            data = bytes.data() + offset;
            data_size = chunk_size;
        }
        offset += chunk_size + (chunk_size & 1U);
    }

    if (format != 1 || (channels != 1 && channels != 2) ||
        sample_rate == 0 || (bits_per_sample != 8 && bits_per_sample != 16) ||
        data == nullptr || data_size == 0) {
        std::fprintf(stderr, "[sote][hd-audio] unsupported WAV: %s\n",
            wave.path.string().c_str());
        return false;
    }

    const size_t sample_count =
        bits_per_sample == 16 ? data_size / 2 : data_size;
    wave.samples.resize(sample_count);
    if (bits_per_sample == 16) {
        for (size_t i = 0; i < sample_count; ++i) {
            wave.samples[i] = static_cast<int16_t>(
                static_cast<uint16_t>(data[i * 2]) |
                (static_cast<uint16_t>(data[i * 2 + 1]) << 8));
        }
    } else {
        for (size_t i = 0; i < sample_count; ++i) {
            wave.samples[i] =
                static_cast<int16_t>((static_cast<int>(data[i]) - 128) << 8);
        }
    }
    wave.channels = channels;
    wave.sample_rate = static_cast<int>(sample_rate);
    wave.loaded = !wave.samples.empty();
    if (trace_audio || wave.loaded) {
        std::printf(
            "[sote][hd-audio] loaded %s: %.2f sec, %d Hz, %d ch\n",
            wave.path.filename().string().c_str(),
            static_cast<double>(wave.samples.size()) /
                (wave.channels * wave.sample_rate),
            wave.sample_rate,
            wave.channels);
        std::fflush(stdout);
    }
    return wave.loaded;
}

std::shared_ptr<Wave> wave_for_filename_locked(std::string filename) {
    if (filename.empty()) {
        return nullptr;
    }
    const std::string key = lowercase(filename);
    auto existing = waves.find(key);
    if (existing != waves.end()) {
        return existing->second;
    }

    auto wave = std::make_shared<Wave>();
    wave->path = sdata_directory / filename;
    if (!std::filesystem::is_regular_file(wave->path)) {
        for (const auto& entry :
             std::filesystem::directory_iterator{sdata_directory}) {
            if (lowercase(entry.path().filename().string()) == key) {
                wave->path = entry.path();
                break;
            }
        }
    }
    waves.emplace(key, wave);
    return wave;
}

bool enqueue_file_locked(const Mapping& mapping) {
    std::shared_ptr<Wave> wave = wave_for_filename_locked(mapping.filename);
    if (wave == nullptr || !load_wave_locked(*wave)) {
        return false;
    }
    active_voices.push_back(Voice{wave, 0.0, mapping.gain * global_gain});
    return true;
}

void load_sound_map_locked(const std::filesystem::path& path) {
    std::ifstream file{path};
    if (!file) {
        return;
    }
    std::string line;
    int loaded_sounds = 0;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> fields = split_fields(line);
        if (fields.size() < 2) {
            continue;
        }
        size_t id_index = 0;
        size_t filename_index = 1;
        const std::string type = lowercase(fields[0]);
        if (type == "sound" || type == "sfx") {
            id_index = 1;
            filename_index = 2;
        }
        if (fields.size() <= filename_index) {
            continue;
        }
        int32_t sound_id = 0;
        if (parse_i32(fields[id_index], sound_id)) {
            sound_map[sound_id] =
                Mapping{fields[filename_index],
                    parse_gain(fields, filename_index + 1)};
            ++loaded_sounds;
        }
    }
    std::printf(
        "[sote][hd-audio] loaded %d sound mappings from %s\n",
        loaded_sounds,
        path.string().c_str());
    std::fflush(stdout);
}

void add_builtin_voice_locked(
    std::string_view text,
    std::string filename,
    float gain = default_gain,
    bool one_shot = false) {
    voice_map[hash_normalized_message(text)] =
        Mapping{std::move(filename), gain, one_shot};
}

void load_voice_map_locked(const std::filesystem::path& path) {
    std::ifstream file{path};
    if (!file) {
        return;
    }
    std::string line;
    int loaded_voices = 0;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> fields = split_fields(line);
        if (fields.size() < 2) {
            continue;
        }
        size_t hash_index = 0;
        size_t filename_index = 1;
        const std::string type = lowercase(fields[0]);
        if (type == "voice" || type == "text" || type == "message") {
            hash_index = 1;
            filename_index = 2;
        }
        if (fields.size() <= filename_index) {
            continue;
        }
        uint32_t hash = 0;
        if (parse_u32(fields[hash_index], hash)) {
            voice_map[hash] =
                Mapping{fields[filename_index],
                    parse_gain(fields, filename_index + 1)};
            ++loaded_voices;
        }
    }
    std::printf(
        "[sote][hd-audio] loaded %d voice mappings from %s\n",
        loaded_voices,
        path.string().c_str());
    std::fflush(stdout);
}

int16_t sample_at(const Wave& wave, size_t frame, int channel) {
    if (wave.channels == 1) {
        return wave.samples[frame];
    }
    return wave.samples[frame * wave.channels + channel];
}

int16_t resampled_sample(const Wave& wave, double position, int channel) {
    const size_t frames = wave.samples.size() / wave.channels;
    if (frames == 0) {
        return 0;
    }
    const size_t frame0 = static_cast<size_t>(position);
    const size_t frame1 = std::min(frame0 + 1, frames - 1);
    const double fraction = position - std::floor(position);
    const double a = sample_at(wave, frame0, channel);
    const double b = sample_at(wave, frame1, channel);
    return static_cast<int16_t>(std::lround(a + (b - a) * fraction));
}

} // namespace

void initialize(const std::filesystem::path& runtime_directory) {
    std::lock_guard lock{audio_mutex};
    enabled = false;
    sdata_directory.clear();
    waves.clear();
    sound_map.clear();
    voice_map.clear();
    traced_sound_ids.clear();
    traced_message_hashes.clear();
    played_one_shot_voice_hashes.clear();
    voice_cooldowns.clear();
    active_voices.clear();
    trace_audio = std::getenv("SOTE_TRACE_HD_AUDIO") != nullptr;
    global_gain = read_gain_env("SOTE_HD_AUDIO_GAIN", default_gain);
    add_builtin_voice_locked(
        "~h~oI'll watch the ship. Get out there and find Boba Fett!",
        "ILB11.WAV",
        default_gain,
        true);
    add_builtin_voice_locked("Dfob", "ILB11.WAV", default_gain, true);

    const std::vector<std::filesystem::path> candidates = {
        runtime_directory / "Sdata",
        std::filesystem::current_path() / "Sdata",
        std::filesystem::current_path() / "SotE_Recompiled" / "Sdata",
        std::filesystem::path{
            "C:/Games/Star Wars Shadows of the Empire/Sdata"},
    };
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate / "BEEPS.WAV") ||
            std::filesystem::is_regular_file(candidate / "ILB01.WAV")) {
            sdata_directory = candidate;
            break;
        }
    }
    if (sdata_directory.empty()) {
        return;
    }

    int wav_count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator{sdata_directory}) {
        if (entry.is_regular_file() &&
            lowercase(entry.path().extension().string()) == ".wav") {
            ++wav_count;
        }
    }
    enabled = wav_count > 0;
    if (!enabled) {
        return;
    }

    load_sound_map_locked(sdata_directory / "hd_sound_map.tsv");
    load_sound_map_locked(sdata_directory / "hd_sound_map.txt");
    load_voice_map_locked(sdata_directory / "hd_voice_map.tsv");
    load_voice_map_locked(sdata_directory / "hd_voice_map.txt");
    if (const char* self_test = std::getenv("SOTE_HD_AUDIO_TEST_WAV")) {
        if (self_test[0] != '\0') {
            const bool queued =
                enqueue_file_locked(Mapping{self_test, default_gain});
            std::printf(
                "[sote][hd-audio] self-test %s -> %s\n",
                self_test,
                queued ? "queued" : "failed");
            std::fflush(stdout);
        }
    }
    std::printf(
        "[sote][hd-audio] found %d external WAV files in %s "
        "(sound_maps=%zu voice_maps=%zu gain=%.2f trace=%s)\n",
        wav_count,
        sdata_directory.string().c_str(),
        sound_map.size(),
        voice_map.size(),
        global_gain,
        trace_audio ? "on" : "off");
    std::fflush(stdout);
}

bool is_enabled() {
    std::lock_guard lock{audio_mutex};
    return enabled;
}

uint32_t play_sound_id(int32_t sound_id) {
    std::lock_guard lock{audio_mutex};
    if (!enabled) {
        return 0;
    }
    if (trace_audio && traced_sound_ids.insert(sound_id).second) {
        std::printf("[sote][hd-audio] sound id %d requested\n", sound_id);
        std::fflush(stdout);
    }
    const auto mapping = sound_map.find(sound_id);
    if (mapping == sound_map.end()) {
        return 0;
    }
    const bool queued = enqueue_file_locked(mapping->second);
    if (trace_audio || queued) {
        std::printf(
            "[sote][hd-audio] %s sound id %d -> %s\n",
            queued ? "queued" : "failed",
            sound_id,
            mapping->second.filename.c_str());
        std::fflush(stdout);
    }
    return queued ? 1U : 0U;
}

bool play_file(std::string_view filename, float gain) {
    std::lock_guard lock{audio_mutex};
    if (!enabled || filename.empty()) {
        return false;
    }

    const bool queued =
        enqueue_file_locked(Mapping{std::string{filename}, gain});
    if (trace_audio || queued) {
        std::printf(
            "[sote][hd-audio] %s file -> %.*s\n",
            queued ? "queued" : "failed",
            static_cast<int>(filename.size()),
            filename.data());
        std::fflush(stdout);
    }
    return queued;
}

bool play_voice_for_text(std::string_view text) {
    const std::string normalized = normalize_message(text);
    if (normalized.size() < 3) {
        return false;
    }

    const uint32_t hash = hash_normalized_message(text);
    std::lock_guard lock{audio_mutex};
    if (!enabled) {
        return false;
    }

    const auto mapping = voice_map.find(hash);
    if (mapping == voice_map.end()) {
        if (trace_audio && traced_message_hashes.insert(hash).second) {
            std::printf(
                "[sote][hd-audio] no voice mapping hash=0x%08X "
                "text=\"%.96s\"\n",
                hash,
                normalized.c_str());
            std::fflush(stdout);
        }
        return false;
    }

    if (mapping->second.one_shot &&
        played_one_shot_voice_hashes.find(hash) !=
            played_one_shot_voice_hashes.end()) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    auto& last_play = voice_cooldowns[hash];
    if (!mapping->second.one_shot &&
        last_play.time_since_epoch().count() != 0 &&
        now - last_play < std::chrono::seconds{2}) {
        return false;
    }

    const bool queued = enqueue_file_locked(mapping->second);
    if (queued) {
        last_play = now;
        if (mapping->second.one_shot) {
            played_one_shot_voice_hashes.insert(hash);
        }
    }
    if (trace_audio || queued) {
        std::printf(
            "[sote][hd-audio] %s voice hash=0x%08X -> %s "
            "text=\"%.96s\"\n",
            queued ? "queued" : "failed",
            hash,
            mapping->second.filename.c_str(),
            normalized.c_str());
        std::fflush(stdout);
    }
    return queued;
}

void note_message_text(
    uint32_t message_key,
    std::string_view text,
    uint32_t caller) {
    if (text.empty()) {
        return;
    }
    const std::string normalized = normalize_message(text);
    if (normalized.size() < 3) {
        return;
    }
    const uint32_t hash = hash_normalized_message(text);
    std::lock_guard lock{audio_mutex};
    if (!enabled) {
        return;
    }
    const bool force_trace_voice = (hash == 0x157031E6U);
    if ((trace_audio || force_trace_voice) &&
        traced_message_hashes.insert(hash).second) {
        std::printf(
            "[sote][hd-audio] message key=%u caller=%08X hash=0x%08X "
            "text=\"%.96s\"\n",
            message_key,
            caller,
            hash,
            normalized.c_str());
        std::fflush(stdout);
    }

    // Message table lookups happen while text is prepared, not necessarily
    // when the overlay is visible. VO playback is owned by display hooks.
}

bool mix_into(
    int16_t* samples,
    size_t sample_count,
    uint32_t output_frequency) {
    if (samples == nullptr || sample_count < 2 || output_frequency == 0) {
        return false;
    }

    std::lock_guard lock{audio_mutex};
    if (active_voices.empty()) {
        return false;
    }

    const size_t output_frames = sample_count / 2;
    size_t write_voice = 0;
    for (size_t voice_index = 0;
         voice_index < active_voices.size();
         ++voice_index) {
        Voice voice = active_voices[voice_index];
        const Wave& wave = *voice.wave;
        const size_t frames = wave.samples.size() / wave.channels;
        if (frames == 0) {
            continue;
        }
        const double step =
            static_cast<double>(wave.sample_rate) / output_frequency;
        for (size_t frame = 0; frame < output_frames; ++frame) {
            if (voice.position >= frames) {
                break;
            }
            const int16_t left = resampled_sample(wave, voice.position, 0);
            const int16_t right = resampled_sample(
                wave,
                voice.position,
                wave.channels > 1 ? 1 : 0);
            const size_t left_index = frame * 2;
            const size_t right_index = left_index + 1;
            samples[left_index] = clamp_i16(
                samples[left_index] + left * voice.gain);
            samples[right_index] = clamp_i16(
                samples[right_index] + right * voice.gain);
            voice.position += step;
        }
        if (voice.position < frames) {
            active_voices[write_voice++] = voice;
        }
    }
    active_voices.resize(write_voice);
    return true;
}

} // namespace sote::hd_audio
