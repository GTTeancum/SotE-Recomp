#include "hd_music.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
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

struct Cue {
    std::string_view prefix; // The same four bytes tested by func_80006CB0.
    std::string_view slot;
    int sound;
    int pc_track; // Zero means an optional slot-named file, not a numbered guess.
    bool loop;
};
// Numbered defaults replace the actual N64 recordings, not a guessed CD order.
// Waveform/spectral matching against the supplied ROM and OGGs is documented
// in docs/EXTERNAL_MUSIC.md. Track02 is the theme/crawl, NOT the menu.
// A dedicated main_menu.ogg replaces native 0x62; missing files retain the
// original menu cue. Track02 must never be used as the default menu recording.
constexpr Cue cues[] = {
    {"Main", "main_menu",                  0x62,  0, true},
    {"Them", "title_theme",                0x0C,  2, false},
    {"1. B", "battle_of_hoth",             0x0D,  3, true},
    {"2a. ", "escape_from_echo_base",      0x35,  4, true},
    {"2b. ", "escape_from_echo_base",      0x35,  4, true},
    {"3. A", "asteroid_field",             0x33,  6, true},
    {"4a. ", "ord_mantell_junkyard",       0x36,  7, true},
    {"4b. ", "ord_mantell_boss",           0x37, 12, true},
    {"6a. ", "gall_spaceport",             0x38, 13, true},
    {"6b. ", "gall_spaceport",             0x38, 13, true},
    {"7. S", "mos_eisley_beggars_canyon",   0x7C,  8, true},
    {"9a. ", "imperial_freighter",          0x35,  4, true},
    {"9b. ", "imperial_freighter",          0x35,  4, true},
    {"9c. ", "imperial_freighter",          0x35,  4, true},
    {"10. ", "sewers_of_imperial_city",     0x34, 10, true},
    {"11a.", "xizors_palace",              0x7D, 11, true},
    {"11b.", "xizors_palace",              0x7D, 11, true},
    {"12. ", "skyhook_station_chase",      0x33,  6, true},
    {"13. ", "skyhook_battle",             0x7E, 14, true},
    {"Boss", "boss_battle",                0x37, 12, true},
};
struct Mapping {
    std::filesystem::path path;
    bool loop = true;
};
struct Audio {
    std::filesystem::path path;
    std::vector<int16_t> pcm;
    uint32_t frequency = 0;
    int channels = 0;
    // Source-frame [begin, end) loop interval; the intro plays only once.
    size_t loop_begin = 0;
    size_t loop_end = 0;
    size_t frames() const { return pcm.size() / static_cast<size_t>(channels); }
};
struct Voice {
    std::shared_ptr<Audio> audio;
    double position = 0.0;
    float gain = 0.0F;
    bool loop = false;
    bool finished = false;
    int sound = -1;
};
std::mutex mutex;
std::filesystem::path music_directory;
std::unordered_map<std::string, Mapping> mappings;
const Cue* selected = nullptr;
Voice background;
std::vector<Voice> stingers;
// Weak references share a decoded track while in use, without retaining every
// level's decoded music for the rest of the run.
std::unordered_map<std::string, std::weak_ptr<Audio>> decoded;
std::unordered_map<std::string, bool> failed_files;
bool enabled = false;
bool trace = false;
float master_gain = 1.0F;
uint64_t starts = 0, intercepted = 0, fallbacks = 0;

std::string trim(std::string_view text) {
    size_t a = 0, b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    return std::string(text.substr(a, b - a));
}
std::string lower(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}
bool regular(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}
float read_gain() {
    const char* value = std::getenv("SOTE_HD_MUSIC_GAIN");
    if (!value) return 1.0F;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    return end != value && *end == '\0' && std::isfinite(parsed)
        ? std::clamp(parsed, 0.0F, 2.0F) : 1.0F;
}
float gain_for(int32_t volume) {
    return static_cast<float>(std::clamp(volume, 0, 32767)) / 32767.0F * master_gain;
}
std::filesystem::path resolve(std::string filename) {
#ifndef _WIN32
    std::replace(filename.begin(), filename.end(), '\\', '/');
#endif
    const std::filesystem::path file{filename};
    if (file.is_absolute()) return file;
    for (const auto& root : {music_directory, music_directory.parent_path()}) {
        if (regular(root / file)) return root / file;
    }
    // Keep missing entries: an explicit bad/missing override must fall back to
    // native music, not silently play some other built-in recording.
    return music_directory / file;
}
void load_map(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return;
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number == 1 && line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const size_t split = line.find_first_of(" \t=");
        if (split == std::string::npos) continue;
        const std::string slot = lower(trim(std::string_view(line).substr(0, split)));
        std::string rest = trim(std::string_view(line).substr(split));
        if (rest.starts_with('=')) rest = trim(std::string_view(rest).substr(1));
        std::string filename, mode;
        if (rest.starts_with('"')) {
            const size_t quote = rest.find('"', 1);
            if (quote == std::string::npos) continue;
            filename = rest.substr(1, quote - 1);
            mode = trim(std::string_view(rest).substr(quote + 1));
        } else {
            std::istringstream fields(rest);
            fields >> filename >> mode;
        }
        if (filename.empty()) continue;
        const auto old = mappings.find(slot);
        bool loop = old != mappings.end() ? old->second.loop
            : !(slot == "title_theme" || slot == "game_over" || slot == "sound_61");
        mode = lower(trim(mode.substr(0, mode.find('#'))));
        if (!mode.empty()) {
            if (mode == "loop") loop = true;
            else if (mode == "once") loop = false;
            else {
                std::fprintf(stderr, "[sote][hd-music] invalid mode in %s:%zu\n", path.string().c_str(), line_number);
                continue;
            }
        }
        mappings[slot] = {resolve(filename), loop};
    }
    std::printf("[sote][hd-music] map: %s\n", path.string().c_str());
}
std::shared_ptr<Audio> load(const Mapping& mapping) {
    const std::string key = mapping.path.generic_string();
    if (auto cached = decoded[key].lock()) return cached;
    if (failed_files.contains(key)) return {};
    auto fail = [&](const char* reason) -> std::shared_ptr<Audio> {
        failed_files[key] = true;
        std::fprintf(stderr, "[sote][hd-music] native fallback: %s (%s)\n", key.c_str(), reason);
        return {};
    };
    try {
        // std::ifstream(path) supports Unicode Windows paths. stb's narrow
        // filename API does not; decoding from memory avoids that limitation.
        std::ifstream file(mapping.path, std::ios::binary | std::ios::ate);
        if (!file) return fail("file not found/unreadable");
        const auto bytes = file.tellg();
        constexpr size_t max_compressed_bytes = 128U * 1024U * 1024U;
        if (bytes <= 0 || static_cast<uint64_t>(bytes) > max_compressed_bytes)
            return fail("invalid/oversized OGG");
        std::vector<unsigned char> encoded(static_cast<size_t>(bytes));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size())))
            return fail("short file read");
        int channels = 0, frequency = 0;
        short* raw = nullptr;
        const int frames = stb_vorbis_decode_memory(encoded.data(), static_cast<int>(encoded.size()),
            &channels, &frequency, &raw);
        std::unique_ptr<short, decltype(&std::free)> holder(raw, &std::free);
        if (frames <= 0 || !raw || (channels != 1 && channels != 2) || frequency <= 0)
            return fail("not a valid mono/stereo Vorbis stream");
        auto audio = std::make_shared<Audio>();
        audio->path = mapping.path;
        audio->frequency = static_cast<uint32_t>(frequency);
        audio->channels = channels;
        const size_t samples = static_cast<size_t>(frames) * static_cast<size_t>(channels);
        audio->pcm.assign(raw, raw + samples);
        audio->loop_end = static_cast<size_t>(frames);
        // Optional Vorbis comments, in SOURCE frames (not output-device frames).
        // Untagged recordings retain whole-file looping. One-shots always play
        // through EOF even if the file also contains valid loop comments.
        int metadata_error = 0;
        std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> metadata(
            stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()),
                &metadata_error, nullptr), &stb_vorbis_close);
        if (!metadata) return fail("could not read Vorbis loop metadata");
        const auto comments = stb_vorbis_get_comment(metadata.get());
        bool seen_begin = false, seen_end = false;
        for (int i = 0; i < comments.comment_list_length; ++i) {
            const std::string_view comment{comments.comment_list[i]};
            const size_t split = comment.find('=');
            if (split == std::string_view::npos) continue;
            const std::string tag = lower(trim(comment.substr(0, split)));
            if (tag != "loopstart" && tag != "loopend") continue;
            bool& seen = tag == "loopstart" ? seen_begin : seen_end;
            if (seen) return fail("duplicate Vorbis loop tag");
            seen = true;
            const std::string value = trim(comment.substr(split + 1));
            size_t parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size())
                return fail("invalid Vorbis loop frame");
            if (tag == "loopstart") audio->loop_begin = parsed;
            else audio->loop_end = parsed;
        }
        if (audio->loop_begin >= audio->loop_end || audio->loop_end > audio->frames())
            return fail("Vorbis loop interval outside decoded audio");
        if (seen_begin || seen_end) {
            std::printf("[sote][hd-music] source loop [%zu, %zu): %s\n",
                audio->loop_begin, audio->loop_end, key.c_str());
        }
        decoded[key] = audio;
        std::printf("[sote][hd-music] decoded %s: %.2fs %dHz %dch\n",
            key.c_str(), static_cast<double>(frames) / frequency, frequency, channels);
        std::fflush(stdout);
        return audio;
    } catch (const std::exception& error) {
        return fail(error.what());
    }
}
void stop_background() { background = {}; }
void preload_selected() {
    if (!enabled || !selected) return;
    const auto it = mappings.find(std::string(selected->slot));
    if (it == mappings.end()) return;
    // Decode on the command/game thread, never inside mix_into(). The voice
    // remains unstarted until the native sound request actually arrives.
    background.audio = load(it->second);
    background.loop = it->second.loop;
    background.sound = -1;
}
float sample(const Voice& voice, int channel) {
    const Audio& audio = *voice.audio;
    const size_t count = audio.frames();
    const size_t end = voice.loop ? audio.loop_end : count;
    const size_t a = std::min(static_cast<size_t>(voice.position), end - 1);
    const size_t b = a + 1 < end ? a + 1 : (voice.loop ? audio.loop_begin : a);
    const size_t ch = audio.channels == 1 ? 0 : static_cast<size_t>(channel);
    const float x = audio.pcm[a * audio.channels + ch];
    const float y = audio.pcm[b * audio.channels + ch];
    return x + (y - x) * static_cast<float>(voice.position - std::floor(voice.position));
}
void advance(Voice& voice, uint32_t output_frequency) {
    voice.position += static_cast<double>(voice.audio->frequency) / output_frequency;
    const double end = static_cast<double>(voice.loop ? voice.audio->loop_end : voice.audio->frames());
    if (voice.position >= end) {
        if (voice.loop) {
            const double begin = static_cast<double>(voice.audio->loop_begin);
            // Preserve fractional phase, including steps spanning several loops.
            voice.position = begin + std::fmod(voice.position - begin, end - begin);
        } else { voice.position = end; voice.finished = true; }
    }
}
int16_t saturate(float value) {
    return static_cast<int16_t>(std::lround(std::clamp(value, -32768.0F, 32767.0F)));
}
} // namespace

void initialize(const std::filesystem::path& runtime_directory) {
    std::lock_guard lock(mutex);
    enabled = false;
    selected = nullptr;
    stop_background(); stingers.clear(); decoded.clear(); failed_files.clear(); mappings.clear();
    starts = intercepted = fallbacks = 0;
    trace = std::getenv("SOTE_TRACE_HD_MUSIC") != nullptr;
    master_gain = read_gain();
    if (std::getenv("SOTE_DISABLE_HD_MUSIC") != nullptr) {
        std::printf("[sote][hd-music] disabled; native soundtrack retained\n");
        return;
    }
    music_directory = runtime_directory / "Sdata" / "MUSIC";
    const std::array roots = {music_directory,
        std::filesystem::current_path() / "Sdata" / "MUSIC",
        std::filesystem::current_path() / "SotE_Recompiled" / "Sdata" / "MUSIC"};
    for (const auto& root : roots) {
        std::error_code ec;
        if (std::filesystem::is_directory(root, ec)) { music_directory = root; break; }
    }
    for (const auto& cue : cues) {
        std::filesystem::path filename;
        if (cue.pc_track > 0) {
            char numbered[32];
            std::snprintf(numbered, sizeof(numbered), "Track%02d.ogg", cue.pc_track);
            filename = numbered;
        } else {
            filename = std::string(cue.slot) + ".ogg";
        }
        mappings[std::string(cue.slot)] = {music_directory / filename, cue.loop};
    }
    // Optional dedicated short cues. No evidence links Track14 to game-over:
    // it matches the final Skyhook music, and must not be used as that stinger.
    mappings["game_over"] = {music_directory / "game_over.ogg", false};
    mappings["sound_61"] = {music_directory / "sound_61.ogg", false};
    for (auto& [slot, mapping] : mappings) {
        for (const auto& path : {music_directory / "n64" / (slot + ".ogg"), music_directory / (slot + ".ogg")}) {
            if (regular(path)) { mapping.path = path; break; }
        }
    }
    load_map(music_directory.parent_path() / "n64_music_map.tsv");
    load_map(music_directory / "n64_music_map.tsv");
    enabled = std::any_of(mappings.begin(), mappings.end(), [](const auto& item) { return regular(item.second.path); });
    std::printf("[sote][hd-music] native-command replacements %s: %s (gain=%.2f; effects unchanged)\n",
        enabled ? "enabled" : "unavailable", music_directory.string().c_str(), master_gain);
    if (std::getenv("SOTE_HD_MUSIC_NATIVE_GAIN"))
        std::printf("[sote][hd-music] SOTE_HD_MUSIC_NATIVE_GAIN is obsolete and ignored\n");
    std::fflush(stdout);
}
bool is_enabled() { std::lock_guard lock(mutex); return enabled; }

void command(std::string_view name) {
    std::lock_guard lock(mutex);
    // Null/empty selectors are a native no-op. Unknown nonempty selectors,
    // "Cut " and "None" all select no background music in the retail switch.
    if (name.empty()) return;
    const Cue* next = nullptr;
    if (name.size() >= 4) {
        for (const auto& cue : cues) if (name.substr(0, 4) == cue.prefix) { next = &cue; break; }
    }
    if (selected && next && selected->slot == next->slot && selected->sound == next->sound) return;
    selected = next;
    stop_background();
    if (trace) {
        std::printf("[sote][hd-music] native command '%.*s' -> %s\n",
            static_cast<int>(std::min<size_t>(name.size(), 80)), name.data(), next ? next->slot.data() : "stop");
        std::fflush(stdout);
    }
    preload_selected();
}

bool replace_background(int32_t sound_id, int32_t native_volume) {
    std::lock_guard lock(mutex);
    if (!enabled || !selected || selected->sound != sound_id || !background.audio) {
        ++fallbacks;
        background.sound = -1;
        background.position = 0.0;
        background.gain = 0.0F;
        background.finished = false;
        return false; // Do not cancel the native request before decoding succeeds.
    }
    if (background.sound < 0) {
        background.sound = sound_id;
        ++starts;
        std::printf("[sote][hd-music] REPLACE native=%02X cue=%s file=%s mode=%s\n",
            static_cast<unsigned>(sound_id), selected->slot.data(),
            background.audio->path.filename().string().c_str(), background.loop ? "loop" : "once");
        std::fflush(stdout);
    }
    // Even after a one-shot reaches EOF, consume repeated native refreshes
    // until a new command/reset. Otherwise title music would restart forever.
    background.gain = gain_for(native_volume);
    ++intercepted;
    return true;
}
bool replace_stinger(int32_t sound_id, int32_t native_volume, bool continuous) {
    std::lock_guard lock(mutex);
    if (!enabled) return false;
    const char* slot = sound_id == 0x21 ? "game_over" : sound_id == 0x61 ? "sound_61" : nullptr;
    if (!slot) return false;
    if (continuous) {
        for (auto& voice : stingers) if (voice.sound == sound_id && !voice.finished) {
            voice.gain = gain_for(native_volume); ++intercepted; return true;
        }
    }
    const auto it = mappings.find(slot);
    if (it == mappings.end()) return false;
    auto audio = load(it->second);
    if (!audio) return false;
    if (stingers.size() >= 8) stingers.erase(stingers.begin());
    stingers.push_back({std::move(audio), 0.0, gain_for(native_volume), it->second.loop, false, sound_id});
    ++intercepted;
    if (trace) std::printf("[sote][hd-music] REPLACE stinger=%02X\n", static_cast<unsigned>(sound_id));
    return true;
}
void end_background_frame(bool native_requested_music) {
    std::lock_guard lock(mutex);
    if (!native_requested_music && background.sound >= 0) {
        // No timer: this is the native updater taking its disabled/no-track
        // branch. Keep decoded data for a subsequent actual resume request.
        background.position = 0.0;
        background.sound = -1;
        background.gain = 0.0F;
        background.finished = false;
    }
}
void reset() {
    std::lock_guard lock(mutex);
    selected = nullptr;
    stop_background(); stingers.clear();
}
bool mix_into(int16_t* samples, size_t sample_count, uint32_t output_frequency) {
    if (!samples || sample_count < 2 || output_frequency == 0) return false;
    std::lock_guard lock(mutex);
    const bool playing_background = background.audio && background.sound >= 0 && !background.finished;
    if (!playing_background && stingers.empty()) return false;
    bool mixed = false;
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
        float left = samples[i], right = samples[i + 1];
        auto mix_voice = [&](Voice& voice) {
            if (!voice.audio || voice.finished || voice.sound < 0) return;
            left += sample(voice, 0) * voice.gain;
            right += sample(voice, 1) * voice.gain;
            advance(voice, output_frequency);
            mixed = true;
        };
        mix_voice(background);
        for (auto& voice : stingers) mix_voice(voice);
        // The native PCM is NOT attenuated: its effects/voices are preserved.
        // The music request was removed before entering the native sound player.
        samples[i] = saturate(left);
        samples[i + 1] = saturate(right);
    }
    std::erase_if(stingers, [](const Voice& voice) { return voice.finished; });
    return mixed;
}
Status status() {
    std::lock_guard lock(mutex);
    Status result;
    result.enabled = enabled;
    result.background_active = background.audio && background.sound >= 0 && !background.finished;
    result.background_finished = background.finished;
    if (selected) result.cue = selected->slot;
    if (background.audio) result.file = background.audio->path.filename().string();
    result.position_frames = background.position;
    result.gain = background.gain;
    result.background_starts = starts;
    result.intercepted_requests = intercepted;
    result.fallback_requests = fallbacks;
    result.stingers = stingers.size();
    return result;
}
} // namespace sote::hd_music
