#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

// Based on CtrlP1's issue #15 contribution. Guest memory is word-swapped.
namespace sote::cheats {
struct Settings {
    bool infinite_health = false;
    bool infinite_lives = false;
    bool infinite_ammo = false;
    bool infinite_missiles = false;
    bool infinite_vehicle_health = false;
};

struct State {
    static constexpr uint32_t outrider_health_offset = 0x240U;
    static constexpr int32_t outrider_health_max = 500;
    int16_t vehicle_event = -1;
    template<class T> static T read(uint8_t* ram,uint32_t address) {
        T value; std::memcpy(&value,ram+((address&0x7fffff)^(sizeof(T)==2?2:0)),sizeof(T)); return value;
    }
    template<class T> static void write(uint8_t* ram,uint32_t address,T value) {
        std::memcpy(ram+((address&0x7fffff)^(sizeof(T)==2?2:0)),&value,sizeof(T));
    }
int16_t cheat_last_event = -1;
uint32_t cheat_last_player = 0;
float cheat_health_max = 0.0f;
int16_t cheat_missiles_max = 0;
std::array<float, 6> cheat_ammo_max{};
float cheat_vehicle_max = 0.0f;

static constexpr uint32_t cheat_health_offset = 0xB4U;
static constexpr uint32_t cheat_lives_address = 0x800E0EB0U;
static constexpr uint32_t cheat_missiles_address = 0x800E1B54U;
static constexpr uint32_t cheat_player_address = 0x80112B30U;
static constexpr uint32_t cheat_ammo_offset = 0xC0U;

float cheat_read_float(uint8_t* rdram, uint32_t address) {
    const uint32_t bits = read<uint32_t>(rdram,address);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void cheat_write_float(uint8_t* rdram, uint32_t address, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write<uint32_t>(rdram,address,bits);
}

void apply(uint8_t* rdram,const Settings& settings,int level,bool on_foot,uint32_t ship,bool ship_fresh) {
    if (!rdram) return;
    if (!settings.infinite_health && !settings.infinite_lives &&
        !settings.infinite_ammo && !settings.infinite_missiles &&
        !settings.infinite_vehicle_health) {
        return;
    }

    const int16_t event =
        static_cast<int16_t>(read<uint16_t>(rdram, 0x8013CE0EU));
    // Result 2 is active gameplay. The event-to-level table only lists the
    // events the upstream diagnostics have seen (e.g. one per Echo Base), so
    // gating on it silently disabled cheats in later parts of levels.
    const int32_t result =
        static_cast<int32_t>(read<uint32_t>(rdram, 0x800DD2B0U));
    if (result != 2) {
        cheat_last_event = -1;
        return;
    }
    const uint32_t player = read<uint32_t>(rdram, cheat_player_address);
    const bool valid_player = on_foot && event != 8 &&
        player >= 0x80000000U && player < 0x80800000U - 0x200U;
    const bool new_context =
        event != cheat_last_event || player != cheat_last_player;
    cheat_last_event = event;
    cheat_last_player = player;


    if (settings.infinite_vehicle_health) {
        // Hoth (snowspeeder) and Ord Mantell keep a float health at a fixed
        // address (110 / 100 when full; the GameShark addresses). Only
        // plausible floats are touched.
        uint32_t vehicle_address = 0;
        switch (level) {
            case 0: if (event == 3) vehicle_address = 0x801EBC40U; break;
            case 3: if (event == 8) vehicle_address = 0x801A7BA8U; break;
            default: break;
        }
        // The on-foot player pointer is meaningless in ship stages and may
        // change every frame; only a new event restarts the high-water mark.
        const bool new_vehicle_context = event != vehicle_event;
        vehicle_event = event;
        // Outrider (asteroid field, Skyhook): int32 health inside the ship
        // object reported by the func_80097A40 hook.
        if (ship >= 0x80000000U && ship < 0x80800000U - 0x400U &&
            ship_fresh && (level == 2 || level == 9)) {
            const int32_t health = static_cast<int32_t>(
                read<uint32_t>(rdram, ship + outrider_health_offset));
            if (health > 0 && health < outrider_health_max) {
                write<uint32_t>(rdram, ship + outrider_health_offset,
                    static_cast<uint32_t>(outrider_health_max));
            }
        }
        if (vehicle_address != 0) {
            const float value = cheat_read_float(rdram, vehicle_address);
            if (std::isfinite(value) && value > 0.0f && value <= 1000.0f) {
                if (new_vehicle_context || value > cheat_vehicle_max) {
                    cheat_vehicle_max = value;
                } else if (value < cheat_vehicle_max) {
                    cheat_write_float(rdram, vehicle_address, cheat_vehicle_max);
                }
            }
        }
    }

    if (settings.infinite_health && valid_player) {
        // On-foot health is the player object's float at +0xB4 (100 when
        // full); the respawn path reloads it. 0x800E0EA0 is the death
        // countdown and must never be refilled, or respawn stalls.
        const uint32_t address = player + cheat_health_offset;
        const float health = cheat_read_float(rdram, address);
        if (std::isfinite(health) && health > 0.0f && health <= 1000.0f) {
            if (new_context || health > cheat_health_max) {
                cheat_health_max = health;
            } else if (health < cheat_health_max) {
                cheat_write_float(rdram, address, cheat_health_max);
            }
        }
    }

    if (settings.infinite_lives) {
        const int32_t lives =
            static_cast<int32_t>(read<uint32_t>(rdram, cheat_lives_address));
        if (lives >= 0 && lives < 3) {
            write<uint32_t>(rdram, cheat_lives_address, 3U);
        }
    }

    if (settings.infinite_ammo && valid_player) {
        for (size_t weapon = 0; weapon < cheat_ammo_max.size(); ++weapon) {
            const uint32_t address = player + cheat_ammo_offset +
                static_cast<uint32_t>(weapon * sizeof(float));
            const float ammo = cheat_read_float(rdram, address);
            if (!std::isfinite(ammo) || ammo < 0.0f || ammo > 100000.0f) {
                continue;
            }
            if (new_context || ammo > cheat_ammo_max[weapon]) {
                cheat_ammo_max[weapon] = ammo;
            } else if (ammo < cheat_ammo_max[weapon]) {
                cheat_write_float(rdram, address, cheat_ammo_max[weapon]);
            }
        }
    }

    if (settings.infinite_missiles) {
        const int16_t missiles = static_cast<int16_t>(
            read<uint16_t>(rdram, cheat_missiles_address));
        if (new_context || missiles > cheat_missiles_max) {
            cheat_missiles_max = missiles;
        } else if (missiles < cheat_missiles_max) {
            write<uint16_t>(
                rdram,
                cheat_missiles_address,
                static_cast<uint16_t>(cheat_missiles_max));
        }
    }
}

};
} // namespace sote::cheats
