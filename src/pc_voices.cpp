#include "recomp_hooks.h"
#include "hd_audio.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace {
// Only the game thread calls these hooks. Never use message lookup as a
// visibility signal: the game looks up dialogue well before displaying it.
struct Line {
    std::string_view text;
    std::string_view file;
    uint64_t last_seen = 0;
    bool seen = false;
};
std::array<Line, 5> lines{{
    {"destroy the turrets at the end of each arm of the station.", "ILU16.WAV"},
    {"let's get out of here!", "ILU19.WAV"},
    {"that does it for the turrets. now fly inside and destroy the power core!", "ILU17.WAV"},
    {"the empire is attacking xizor's base and us!", "ILU13.WAV"},
    {"wait... where's dash? he must not have made it out of the skyhook before it blew...", "ILU20.WAV"},
}};
uint64_t frame = 0;
uint64_t next_harpoon_voice = 0;
uint16_t event = 0;

uint16_t half(const uint8_t* ram, uint32_t address) {
    uint16_t value;
    std::memcpy(&value, ram + ((address & 0x7FFFFFU) ^ 2U), sizeof(value));
    return value;
}
uint32_t word(const uint8_t* ram, uint32_t address) {
    uint32_t value;
    std::memcpy(&value, ram + (address & 0x7FFFFFU), sizeof(value));
    return value;
}
void update_event(const uint8_t* ram) {
    const auto current = half(ram, 0x8013CE0EU);
    if (current != event) {
        event = current;
        for (auto& line : lines) line.seen = false;
        next_harpoon_voice = 0;
    }
}
std::string normalized_text(const uint8_t* ram, uint32_t pointer) {
    if (pointer < 0x80000000U || pointer >= 0x80800000U) return {};
    std::string result;
    for (uint32_t i = 0; i < 512 && pointer + i < 0x80800000U; ++i) {
        unsigned char c = ram[((pointer + i) & 0x7FFFFFU) ^ 3U];
        if (c == 0) break;
        if (c == '~') {
            if (pointer + i + 1 >= 0x80800000U) return {};
            c = ram[((pointer + ++i) & 0x7FFFFFU) ^ 3U];
            if (c != 'n') continue;
            c = ' ';
        }
        if (std::isspace(c)) {
            if (!result.empty() && result.back() != ' ') result += ' ';
        } else {
            result += static_cast<char>(std::tolower(c));
        }
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}
void play(std::string_view file, const char* trigger) {
    const bool queued = sote::hd_audio::play_file(file);
    std::printf("[sote][pc-voice] %s %.*s event=%u trigger=%s\n",
        queued ? "queued" : "unavailable", static_cast<int>(file.size()),
        file.data(), event, trigger);
}
}

extern "C" void sote_pc_voice_frame(uint8_t* rdram) {
    ++frame;
    update_event(rdram);
}

extern "C" void sote_pc_voice_text(uint8_t* rdram, uint32_t pointer) {
    update_event(rdram);
    if (event < 28 || event > 30) return;
    const auto text = normalized_text(rdram, pointer);
    for (auto& line : lines) {
        if (text != line.text) continue;
        // Repeated draws (including multiple slots) must not restart speech.
        // Rearm after a full second absent, allowing a later replay/retry.
        const bool first_draw = !line.seen || frame - line.last_seen > 60;
        line.seen = true;
        line.last_seen = frame;
        if (first_draw) play(line.file, "visible Skyhook dialogue");
        return;
    }
}

extern "C" void sote_pc_voice_harpoon(uint8_t* rdram, uint32_t source) {
    update_event(rdram);
    if (event != 2 && event != 3) return;
    std::string_view file;
    const char* trigger = "";
    if (source == 0x80086FCCU && word(rdram, 0x800E1A84U) == 0) {
        file = "ILU32.WAV";
        trigger = "harpoon fired without target";
    } else if (source == 0x800870C8U) {
        file = "ILU29.WAV";
        trigger = "tow cable attached";
    } else if ((source == 0x800867C8U || source == 0x80086A64U) &&
               word(rdram, 0x800E1A80U) != 0) {
        // Only direction reversal and loss of the attached target. Other
        // callers clear the cable during launch validation, death or reset.
        file = "ILU31.WAV";
        trigger = "tow cable lost";
    }
    if (file.empty() || frame < next_harpoon_voice) return;
    next_harpoon_voice = frame + 180;
    play(file, trigger);
}
