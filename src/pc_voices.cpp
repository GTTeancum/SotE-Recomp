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
    uint16_t first_event = 28;
    uint16_t last_event = 30;
    bool rotate_pilots = false;
    uint64_t last_seen = 0;
    bool seen = false;
};
std::array<Line, 15> lines{{
    {"destroy the turrets at the end of each arm of the station.", "ILU16.WAV"},
    {"let's get out of here!", "ILU19.WAV"},
    {"that does it for the turrets. now fly inside and destroy the power core!", "ILU17.WAV"},
    {"the empire is attacking xizor's base and us!", "ILU13.WAV"},
    {"wait... where's dash? he must not have made it out of the skyhook before it blew...", "ILU20.WAV"},
    {"stage one rogue group: take out those imperial probe droids. be careful not to shoot our own turrets!", "ILU01.WAV", 2, 3},
    {"stage two rogue group: these at-sts should be no problem for our blasters.", "ILU04.WAV", 2, 3},
    {"stage three rogue group: try using your harpoon and tow cables on the at-at.", "ILU05.WAV", 2, 3},
    {"stage four good job, rogue group. we still need to buy more time for our transports to escape. keep at them!", "ILU07.WAV", 2, 3},
    {"return to battle!", "ILU22.WAV", 2, 3},
    {"you lost the tow cable.", "ILU31.WAV", 2, 3},
    // PC HUD ID 15 selects pilot bank 1 explicitly, independent of frnd rotation.
    {"fire tow cable!", "IR108.WAV", 2, 3},
    // PC frnd warnings cycle three anonymous pilot banks. N64 has three
    // wordings rather than PC's five; use the matching meaning in each bank.
    {"hey, i'm on your side!", "IR103.WAV", 2, 3, true},
    {"don't shoot rebel forces!", "IR101.WAV", 2, 3, true},
    {"we're on the same side!", "IR103.WAV", 2, 3, true},
}};
unsigned pilot_bank = 0;
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
        pilot_bank = 0;
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
    std::printf("[sote][pc-voice] %s %.*s event=%u frame=%llu trigger=%s\n",
        queued ? "queued" : "unavailable", static_cast<int>(file.size()),
        file.data(), event, static_cast<unsigned long long>(frame), trigger);
}
}

extern "C" void sote_pc_voice_frame(uint8_t* rdram) {
    ++frame;
    update_event(rdram);
}

extern "C" void sote_pc_voice_text(uint8_t* rdram, uint32_t pointer) {
    update_event(rdram);
    if (event != 2 && event != 3 && (event < 28 || event > 30)) return;
    const auto text = normalized_text(rdram, pointer);
    for (auto& line : lines) {
        if (event < line.first_event || event > line.last_event ||
            text != line.text) continue;
        // Repeated draws (including multiple slots) must not restart speech.
        // Rearm after a full second absent, allowing a later replay/retry.
        const bool first_draw = !line.seen || frame - line.last_seen > 60;
        line.seen = true;
        line.last_seen = frame;
        if (first_draw) {
            if (line.rotate_pilots) {
                std::string file(line.file);
                file[2] = static_cast<char>('1' + pilot_bank);
                pilot_bank = (pilot_bank + 1) % 3;
                play(file, "visible Hoth pilot warning");
            } else {
                play(line.file, event <= 3 ? "visible Hoth radio text" :
                    "visible Skyhook dialogue");
            }
        }
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
    } else if (source == 0x800866CCU) {
        file = "ILU33.WAV";
        trigger = "successful walker trip detached cable";
    }
    if (file.empty() || frame < next_harpoon_voice) return;
    next_harpoon_voice = frame + 180;
    play(file, trigger);
}
