#include "hd_music.hpp"
#include "recomp_hooks.h"
#include "san_movies.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace {
// The recomp uses word-swapped guest RDRAM. These hooks run on the game/audio
// scheduling thread, not from the SDL audio mixer.
constexpr uint32_t desired_table = 0x80110FE8U;
constexpr uint32_t slot_size = 0x20U;
constexpr uint32_t memory_size = 8U * 1024U * 1024U;
thread_local bool background_scope = false;
thread_local bool requested = false;
thread_local int32_t fallback_sound = -1;
struct NativeSlot { bool owned = false; int32_t sound = -1; };
std::array<NativeSlot, 8> native_slots{};

bool valid(uint32_t address, uint32_t size) {
    return address >= 0x80000000U && address < 0x80800000U &&
        (address & 0x1FFFFFFFU) <= memory_size - size;
}
int32_t word(uint8_t* rdram, uint32_t address) {
    return *reinterpret_cast<int32_t*>(rdram + (address & 0x1FFFFFFFU));
}
void put(uint8_t* rdram, uint32_t address, int32_t value) {
    *reinterpret_cast<int32_t*>(rdram + (address & 0x1FFFFFFFU)) = value;
}
void retire_native_background(uint8_t* rdram) {
    // Retire only entries previously submitted by the background updater.
    // Do not cancel sounds just because they happen to share a bank/sample ID.
    for (size_t i = 0; i < native_slots.size(); ++i) {
        auto& tag = native_slots[i];
        if (!tag.owned) continue;
        const uint32_t slot = desired_table + static_cast<uint32_t>(i) * slot_size;
        if (word(rdram, slot + 4) == tag.sound && word(rdram, slot + 0x10) == 7 && word(rdram, slot + 8) != 0) {
            put(rdram, slot, -1);
            put(rdram, slot + 4, -1);
            put(rdram, slot + 8, 0);
            put(rdram, slot + 0xC, -1);
            put(rdram, slot + 0x18, 0);
            // func_80006918 owns the live handle: leave it intact so its
            // normal stop/deallocate path releases the AL voice safely.
        }
        tag = {};
    }
}
} // namespace

extern "C" void sote_music_command(uint8_t* rdram, uint32_t name_address) {
    if (!rdram || !valid(name_address, 1)) return;
    std::string name;
    for (uint32_t i = 0; i < 80 && valid(name_address + i, 1); ++i) {
        const char ch = static_cast<char>(rdram[((name_address + i) & 0x1FFFFFFFU) ^ 3U]);
        if (!ch) break;
        name += ch;
    }
    sote::san_movies::note_music_command(name);
    sote::hd_music::command(name);
}
extern "C" void sote_music_frame_begin(uint8_t*) {
    background_scope = true;
    requested = false;
    fallback_sound = -1;
}
extern "C" uint32_t sote_music_native_request(uint8_t* rdram, int32_t sound, int32_t volume, int32_t continuous) {
    if (!rdram) return 0;
    if (!background_scope)
        return sote::hd_music::replace_stinger(sound, volume, continuous != 0) ? 1U : 0U;
    requested = true;
    if (!sote::hd_music::replace_background(sound, volume)) {
        fallback_sound = sound;
        return 0;
    }
    retire_native_background(rdram);
    return 1;
}
extern "C" void sote_music_frame_end(uint8_t* rdram) {
    if (rdram && fallback_sound >= 0) {
        for (size_t i = 0; i < native_slots.size(); ++i) {
            const uint32_t slot = desired_table + static_cast<uint32_t>(i) * slot_size;
            if (word(rdram, slot + 4) == fallback_sound && word(rdram, slot + 8) != 0 && word(rdram, slot + 0x10) == 7)
                native_slots[i] = {true, fallback_sound};
        }
    }
    sote::hd_music::end_background_frame(requested);
    background_scope = false;
}
extern "C" void sote_music_reset(uint8_t*) {
    sote::hd_music::reset();
    native_slots = {};
    background_scope = false;
    requested = false;
    fallback_sound = -1;
}
