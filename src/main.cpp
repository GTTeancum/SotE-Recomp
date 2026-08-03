#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>

#include "librecomp/game.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/rsp.hpp"
#include "frontend.hpp"
#include "graphics_menu.hpp"
#include "rt64_renderer.hpp"
#include "ultramodern/error_handling.hpp"
#include "ultramodern/events.hpp"
#include "ultramodern/input.hpp"
#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80017B60(uint8_t* rdram, recomp_context* ctx);
extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    constexpr uint32_t cart_physical_base = 0x10000000U;
    constexpr uint32_t cart_size = 0x00C00000U;
    const uint32_t device_address = static_cast<uint32_t>(ctx->r4);
    uint32_t physical_address = device_address & 0x1FFFFFFFU;
    if (physical_address < cart_physical_base) {
        // Some of SOTE's helpers carry cartridge-relative offsets while
        // others retain the usual 0xB0000000 KSEG1 cartridge address.
        physical_address += cart_physical_base;
    }
    if (physical_address < cart_physical_base ||
        physical_address >= cart_physical_base + cart_size) {
        std::fprintf(
            stderr,
            "[sote] invalid PI PIO address: device=%08X physical=%08X\n",
            device_address,
            physical_address);
        std::_Exit(EXIT_FAILURE);
    }
    recomp::do_rom_pio(rdram, ctx->r5, physical_address);
    ctx->r2 = 0;
}
void register_overlays();
RspExitReason aspMain(uint8_t* rdram, uint32_t ucode_addr);

std::atomic<int> display_list_count{0};

namespace {

std::atomic<int> vi_count{0};
std::atomic<int> thread_count{0};
std::atomic<uint64_t> game_frame_count{0};
std::atomic<int> bike_controller_last_vi{-10000};
std::atomic<uint32_t> bike_invalid_delta_count{0};
std::atomic<uint32_t> player_invalid_delta_count{0};
std::atomic<uint32_t> motion_zero_velocity_normalization_count{0};
std::atomic<uint32_t> motion_loop_guard_count{0};
std::atomic<uint32_t> audio_negative_exponent_count{0};
std::atomic<uint32_t> invalid_frame_delta_count{0};
std::atomic<uint32_t> rapid_life_loss_count{0};
std::atomic<uint32_t> duplicate_life_loss_suppression_count{0};
std::atomic<uint32_t> committed_life_loss_count{0};
std::atomic<uint16_t> current_scripted_buttons{0};
int last_life_loss_vi = -10000;
int16_t last_life_loss_event = -1;
int last_committed_life_loss_vi = -10000;
int16_t last_committed_life_loss_event = -1;
struct LifeLossEpisode {
    bool active = false;
    uint32_t source = 0;
    uint32_t object = 0;
    uint32_t controller = 0;
    uint32_t death_action = 0;
    int16_t event = -1;
    int normal_since_vi = -1;
};
LifeLossEpisode life_loss_episode;
uint8_t* game_rdram = nullptr;
bool smoke_test = false;
bool frontend_smoke_test = false;
bool muted_output = false;
bool portable_layout = false;
bool persistent_logging = false;
bool smoke_refill_lives = false;
bool smoke_expect_natural_game_over = false;
int smoke_vi_limit = 600;
int smoke_observation_start_vi = 0;
int smoke_observation_start_display_lists = 0;
uint64_t smoke_observation_start_game_frames = 0;
int smoke_expected_level_index = -1;
std::string smoke_expected_level_name;
std::filesystem::path runtime_directory;
bool graphics_fullscreen = false;
HWND graphics_window = nullptr;
WINDOWPLACEMENT windowed_placement{sizeof(WINDOWPLACEMENT)};
std::once_flag symbol_engine_once;
std::atomic<bool> stall_stack_dumped{false};
int last_display_activity_vi = 0;
int last_observed_display_lists = 0;
std::mutex game_frame_mutex;
std::condition_variable game_frame_cv;
int last_game_frame_vi = -1;

struct ScriptedInputPulse {
    int start_vi;
    int duration;
    uint16_t buttons;
    int8_t x;
    int8_t y;
};

std::vector<ScriptedInputPulse> scripted_input;

std::filesystem::path get_executable_directory() {
    std::vector<wchar_t> path_buffer(32768);
    const DWORD path_length = GetModuleFileNameW(
        nullptr,
        path_buffer.data(),
        static_cast<DWORD>(path_buffer.size()));
    if (path_length == 0 || path_length >= path_buffer.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(
        path_buffer.data(),
        path_buffer.data() + path_length).parent_path();
}

void initialize_persistent_logging() {
    const std::filesystem::path log_directory =
        runtime_directory / "logs";
    std::error_code error;
    std::filesystem::create_directories(log_directory, error);
    if (error) {
        return;
    }

    FILE* stdout_file = nullptr;
    FILE* stderr_file = nullptr;
    const std::string stdout_path =
        (log_directory / "sote-latest.log").string();
    const std::string stderr_path =
        (log_directory / "sote-latest-errors.log").string();
    if (freopen_s(
            &stdout_file, stdout_path.c_str(), "w", stdout) != 0 ||
        freopen_s(
            &stderr_file, stderr_path.c_str(), "w", stderr) != 0) {
        return;
    }

    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    persistent_logging = true;

    SYSTEMTIME local_time{};
    GetLocalTime(&local_time);
    std::printf(
        "[sote] session started %04u-%02u-%02u %02u:%02u:%02u.%03u "
        "pid=%lu\n",
        local_time.wYear,
        local_time.wMonth,
        local_time.wDay,
        local_time.wHour,
        local_time.wMinute,
        local_time.wSecond,
        local_time.wMilliseconds,
        static_cast<unsigned long>(GetCurrentProcessId()));
}

uint32_t read_guest_word(uint8_t* rdram, uint32_t address) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    return static_cast<uint32_t>(MEM_W(0, guest_address));
}

void write_guest_word(
    uint8_t* rdram,
    uint32_t address,
    uint32_t value) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    MEM_W(0, guest_address) = value;
}

uint16_t read_guest_half(uint8_t* rdram, uint32_t address) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    return static_cast<uint16_t>(MEM_H(0, guest_address));
}

void write_guest_half(
    uint8_t* rdram,
    uint32_t address,
    uint16_t value) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    MEM_H(0, guest_address) = value;
}

void write_guest_byte(
    uint8_t* rdram,
    uint32_t address,
    uint8_t value) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    MEM_B(0, guest_address) = value;
}

uint8_t read_guest_byte(uint8_t* rdram, uint32_t address) {
    const gpr guest_address = static_cast<gpr>(
        static_cast<int32_t>(address));
    return static_cast<uint8_t>(MEM_BU(0, guest_address));
}

float read_guest_float(uint8_t* rdram, uint32_t address) {
    const uint32_t bits = read_guest_word(rdram, address);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double read_guest_double(uint8_t* rdram, uint32_t address) {
    const uint64_t bits =
        (static_cast<uint64_t>(read_guest_word(rdram, address)) << 32) |
        read_guest_word(rdram, address + sizeof(uint32_t));
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

struct BikeTelemetry {
    bool valid = false;
    int32_t stage = 0;
    int32_t result = 0;
    int32_t lives = 0;
    int32_t failure_condition = 0;
    int16_t event = 0;
};

BikeTelemetry previous_bike_telemetry;
struct GeneralFailureTelemetry {
    bool valid = false;
    int32_t lives = 0;
    int32_t result = 0;
    int32_t stage = 0;
    int32_t failure_condition = 0;
    int16_t event = 0;
};

GeneralFailureTelemetry previous_general_telemetry;

int read_current_level_index(uint8_t* rdram) {
    constexpr uint32_t profile_table = 0x8018BBF8U;
    constexpr uint32_t profile_size = 0x7AU;
    constexpr uint32_t current_profile_offset = 0x5U;
    constexpr uint32_t current_level_offset = 0x8U;
    const uint8_t profile =
        read_guest_byte(rdram, profile_table + current_profile_offset);
    if (profile >= 4) {
        return -1;
    }
    return read_guest_byte(
        rdram,
        profile_table + profile * profile_size + current_level_offset);
}

bool event_matches_level(int level_index, int16_t event) {
    switch (level_index) {
        case 0: return event == 2 || event == 3;
        case 1: return event == 4;
        case 2: return event == 6;
        // Ord Mantell's active route advances through four event records.
        // The passive smoke only reached 7; synthesized actions exposed the
        // legitimate 7 -> 8 transition, and 11 begins Gall Spaceport.
        case 3:
            return event >= 7 && event <= 10;
        case 4:
            return event == 11 || event == 12 ||
                event == 13 || event == 14;
        case 5: return event == 17;
        case 6: return event == 20;
        case 7: return event == 24 || event == 25;
        case 8: return event == 26 || event == 27;
        case 9: return event == 28 || event == 29 || event == 30;
        default: return false;
    }
}

int level_index_for_event(int16_t event) {
    for (int level_index = 0; level_index < 10; ++level_index) {
        if (event_matches_level(level_index, event)) {
            return level_index;
        }
    }
    return -1;
}

const char* level_name_for_index(int level_index) {
    static constexpr const char* names[] = {
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
    };
    if (level_index < 0 ||
        level_index >= static_cast<int>(std::size(names))) {
        return "unknown";
    }
    return names[level_index];
}

void note_life_loss_cadence(
    int vi,
    int16_t event,
    int32_t previous_lives,
    int32_t current_lives,
    const char* level_name) {
    // Human Hoth telemetry captured duplicate deductions 190 and 330 VIs
    // after the preceding real death. Active fingerprint runs then exposed
    // callback storms through 658 VIs on the same player object, while the
    // next plausible independently playable death was 1,884 VIs later.
    // Treat the first thirty seconds as one death/respawn episode.
    constexpr int minimum_plausible_respawn_vis = 1800;
    const int elapsed_vis = vi - last_life_loss_vi;
    if (event == last_life_loss_event &&
        elapsed_vis >= 0 &&
        elapsed_vis < minimum_plausible_respawn_vis) {
        const uint32_t occurrence =
            rapid_life_loss_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        std::printf(
            "[sote][anomaly] RAPID LIFE LOSS level=%s VI=%d "
            "elapsed_vis=%d lives=%d->%d event=%d occurrence=%u\n",
            level_name,
            vi,
            elapsed_vis,
            previous_lives,
            current_lives,
            event,
            occurrence);
    }
    last_life_loss_vi = vi;
    last_life_loss_event = event;
}

void log_general_failure_telemetry(int vi) {
    if (game_rdram == nullptr) {
        return;
    }

    const int16_t early_event = static_cast<int16_t>(
        read_guest_half(game_rdram, 0x8013CE0EU));
    if (smoke_refill_lives &&
        event_matches_level(smoke_expected_level_index, early_event)) {
        const int32_t early_lives = static_cast<int32_t>(
            read_guest_word(game_rdram, 0x800E0EB0U));
        if (early_lives <= 0) {
            constexpr int32_t diagnostic_lives = 3;
            write_guest_word(
                game_rdram,
                0x800E0EB0U,
                static_cast<uint32_t>(diagnostic_lives));
            std::printf(
                "[sote][smoke] REFILLED PRE-OBSERVATION LIVES "
                "expected_level=%s VI=%d lives=%d event=%d\n",
                smoke_expected_level_name.c_str(),
                vi,
                diagnostic_lives,
                early_event);
        }
    }
    if (vi < smoke_observation_start_vi) {
        return;
    }

    GeneralFailureTelemetry current{};
    current.valid = true;
    current.lives = static_cast<int32_t>(
        read_guest_word(game_rdram, 0x800E0EB0U));
    current.result = static_cast<int32_t>(
        read_guest_word(game_rdram, 0x800DD2B0U));
    current.stage = static_cast<int32_t>(
        read_guest_word(game_rdram, 0x800DD340U));
    current.failure_condition = static_cast<int32_t>(
        read_guest_word(game_rdram, 0x800DD364U));
    current.event = static_cast<int16_t>(
        read_guest_half(game_rdram, 0x8013CE0EU));
    const int actual_level_index = read_current_level_index(game_rdram);
    const int event_level_index = level_index_for_event(current.event);
    const bool diagnostic_smoke = smoke_expected_level_index >= 0;

    // Human sessions use the same persistent diagnostics as headless smoke.
    // Previously only the bike controller produced level-specific output, so
    // a later Gall failure disappeared into generic VI heartbeats.
    if (!diagnostic_smoke) {
        if (event_level_index < 0) {
            return;
        }
        const char* level_name = level_name_for_index(event_level_index);
        const bool level_changed =
            !previous_general_telemetry.valid ||
            current.event != previous_general_telemetry.event;
        if (level_changed &&
            current.lives < 0 &&
            current.result != 4) {
            // The retail game stores extra lives, so 3 represents the four
            // lives shown to the player. A game-over path can leave -1 in
            // this global while returning to the level menu; carrying it
            // into the next selected level causes an immediate game over.
            constexpr int32_t restored_extra_lives = 3;
            std::printf(
                "[sote][lives] restored new-level stock level=%s VI=%d "
                "lives=%d->%d result=%d event=%d\n",
                level_name,
                vi,
                current.lives,
                restored_extra_lives,
                current.result,
                current.event);
            write_guest_word(
                game_rdram,
                0x800E0EB0U,
                static_cast<uint32_t>(restored_extra_lives));
            current.lives = restored_extra_lives;
        }
        if (level_changed) {
            std::printf(
                "[sote][gameplay] LEVEL STATE level=%s level_index=%d "
                "profile_index=%d VI=%d lives=%d result=%d stage=%d "
                "failure_condition=%d event=%d frame_delta=%.6f\n",
                level_name,
                event_level_index,
                actual_level_index,
                vi,
                current.lives,
                current.result,
                current.stage,
                current.failure_condition,
                current.event,
                read_guest_double(game_rdram, 0x8018E998U));
        } else {
            if (current.lives < previous_general_telemetry.lives) {
                note_life_loss_cadence(
                    vi,
                    current.event,
                    previous_general_telemetry.lives,
                    current.lives,
                    level_name);
                std::printf(
                    "[sote][failure] LIFE LOST level=%s VI=%d "
                    "lives=%d->%d result=%d stage=%d "
                    "failure_condition=%d event=%d frame_delta=%.6f\n",
                    level_name,
                    vi,
                    previous_general_telemetry.lives,
                    current.lives,
                    current.result,
                    current.stage,
                    current.failure_condition,
                    current.event,
                    read_guest_double(game_rdram, 0x8018E998U));
            }
            if (current.lives < 0 &&
                previous_general_telemetry.lives >= 0) {
                std::printf(
                    "[sote][failure] GAME OVER CONDITION level=%s VI=%d "
                    "lives=%d result=%d stage=%d failure_condition=%d "
                    "event=%d\n",
                    level_name,
                    vi,
                    current.lives,
                    current.result,
                    current.stage,
                    current.failure_condition,
                    current.event);
            }
            if (current.result != previous_general_telemetry.result) {
                std::printf(
                    "[sote][failure] RESULT TRANSITION level=%s VI=%d "
                    "result=%d->%d lives=%d stage=%d "
                    "failure_condition=%d event=%d\n",
                    level_name,
                    vi,
                    previous_general_telemetry.result,
                    current.result,
                    current.lives,
                    current.stage,
                    current.failure_condition,
                    current.event);
            }
        }
        if (vi % 600 == 0) {
            std::printf(
                "[sote][gameplay] LEVEL HEARTBEAT level=%s VI=%d "
                "lives=%d result=%d stage=%d failure_condition=%d "
                "event=%d frame_delta=%.6f display_lists=%d\n",
                level_name,
                vi,
                current.lives,
                current.result,
                current.stage,
                current.failure_condition,
                current.event,
                read_guest_double(game_rdram, 0x8018E998U),
                display_list_count.load(std::memory_order_relaxed));
        }
        std::fflush(stdout);
        previous_general_telemetry = current;
        return;
    }

    // The event identifies the actively executing sub-level. Some routes do
    // not commit the profile's current-level byte until a later transition,
    // so that save field is diagnostic context rather than route authority.
    const bool route_matches =
        event_matches_level(smoke_expected_level_index, current.event);
    const bool profile_still_matches = route_matches;

    if (!previous_general_telemetry.valid) {
        smoke_observation_start_display_lists =
            display_list_count.load(std::memory_order_relaxed);
        smoke_observation_start_game_frames =
            game_frame_count.load(std::memory_order_relaxed);
        std::printf(
            "[sote][smoke] LEVEL OBSERVATION START expected_index=%d "
            "actual_index=%d route_ok=%d expected_name=%s VI=%d "
            "lives=%d result=%d stage=%d "
            "failure_condition=%d event=%d\n",
            smoke_expected_level_index,
            actual_level_index,
            route_matches ? 1 : 0,
            smoke_expected_level_name.c_str(),
            vi,
            current.lives,
            current.result,
            current.stage,
            current.failure_condition,
            current.event);
        if (!route_matches) {
            std::fprintf(
                stderr,
                "[sote][smoke] LEVEL ROUTE MISMATCH expected_index=%d "
                "actual_index=%d expected_name=%s VI=%d\n",
                smoke_expected_level_index,
                actual_level_index,
                smoke_expected_level_name.c_str(),
                vi);
            std::fflush(nullptr);
            std::_Exit(EXIT_FAILURE);
        }
    } else {
        if (!profile_still_matches) {
            if (smoke_expect_natural_game_over &&
                previous_general_telemetry.lives == -1 &&
                previous_general_telemetry.result == 4) {
                std::printf(
                    "[sote][smoke] NATURAL GAME OVER COMPLETE "
                    "expected_index=%d expected_name=%s VI=%d "
                    "lives=%d result=%d event=%d\n",
                    smoke_expected_level_index,
                    smoke_expected_level_name.c_str(),
                    vi,
                    previous_general_telemetry.lives,
                    previous_general_telemetry.result,
                    previous_general_telemetry.event);
                std::fflush(nullptr);
                std::_Exit(EXIT_SUCCESS);
            }
            std::fprintf(
                stderr,
                "[sote][smoke] LEVEL EXITED expected_index=%d "
                "actual_index=%d expected_name=%s VI=%d event=%d\n",
                smoke_expected_level_index,
                actual_level_index,
                smoke_expected_level_name.c_str(),
                vi,
                current.event);
            std::fflush(nullptr);
            std::_Exit(EXIT_FAILURE);
        }
        if (current.lives < previous_general_telemetry.lives) {
            note_life_loss_cadence(
                vi,
                current.event,
                previous_general_telemetry.lives,
                current.lives,
                smoke_expected_level_name.c_str());
            std::printf(
                "[sote][failure] LIFE LOST expected_level=%s VI=%d "
                "actual_index=%d lives=%d->%d result=%d stage=%d "
                "failure_condition=%d event=%d\n",
                smoke_expected_level_name.c_str(),
                vi,
                actual_level_index,
                previous_general_telemetry.lives,
                current.lives,
                current.result,
                current.stage,
                current.failure_condition,
                current.event);
        }
        if (current.lives < 0 && previous_general_telemetry.lives >= 0) {
            std::printf(
                "[sote][failure] GAME OVER CONDITION expected_level=%s VI=%d "
                "actual_index=%d lives=%d result=%d stage=%d "
                "failure_condition=%d event=%d\n",
                smoke_expected_level_name.c_str(),
                vi,
                actual_level_index,
                current.lives,
                current.result,
                current.stage,
                current.failure_condition,
                current.event);
            if (smoke_refill_lives) {
                constexpr int32_t diagnostic_lives = 3;
                write_guest_word(
                    game_rdram,
                    0x800E0EB0U,
                    static_cast<uint32_t>(diagnostic_lives));
                current.lives = diagnostic_lives;
                std::printf(
                    "[sote][smoke] REFILLED LIVES expected_level=%s "
                    "VI=%d lives=%d\n",
                    smoke_expected_level_name.c_str(),
                    vi,
                    diagnostic_lives);
            }
        }
        if (current.result != previous_general_telemetry.result) {
            std::printf(
                "[sote][failure] RESULT TRANSITION expected_level=%s "
                "VI=%d actual_index=%d result=%d->%d lives=%d stage=%d "
                "failure_condition=%d event=%d\n",
                smoke_expected_level_name.c_str(),
                vi,
                actual_level_index,
                previous_general_telemetry.result,
                current.result,
                current.lives,
                current.stage,
                current.failure_condition,
                current.event);
        }
    }
    if (vi % 600 == 0) {
        std::printf(
            "[sote][smoke] LEVEL HEARTBEAT expected_level=%s VI=%d "
            "observed_vis=%d actual_index=%d lives=%d result=%d stage=%d "
            "failure_condition=%d event=%d display_lists=%d\n",
            smoke_expected_level_name.c_str(),
            vi,
            vi - smoke_observation_start_vi,
            actual_level_index,
            current.lives,
            current.result,
            current.stage,
            current.failure_condition,
            current.event,
            display_list_count.load(std::memory_order_relaxed));
    }
    std::fflush(stdout);
    previous_general_telemetry = current;
}

void log_bike_telemetry(int vi) {
    if (game_rdram == nullptr) {
        return;
    }

    uint8_t* rdram = game_rdram;
    if (vi - bike_controller_last_vi.load(std::memory_order_relaxed) > 2) {
        previous_bike_telemetry.valid = false;
        return;
    }

    BikeTelemetry current{};
    current.valid = true;
    current.stage = static_cast<int32_t>(
        read_guest_word(rdram, 0x800DD340U));
    current.result = static_cast<int32_t>(
        read_guest_word(rdram, 0x800DD2B0U));
    current.lives = static_cast<int32_t>(
        read_guest_word(rdram, 0x800E0EB0U));
    current.failure_condition = static_cast<int32_t>(
        read_guest_word(rdram, 0x800DD364U));
    current.event = static_cast<int16_t>(
        read_guest_half(rdram, 0x8013CE0EU));
    const float stage_timer =
        read_guest_float(rdram, 0x800DD344U);
    const float failure_timer =
        read_guest_float(rdram, 0x800DEB78U);
    const double frame_delta =
        read_guest_double(rdram, 0x8018E998U);

    const bool changed =
        !previous_bike_telemetry.valid ||
        current.stage != previous_bike_telemetry.stage ||
        current.result != previous_bike_telemetry.result ||
        current.lives != previous_bike_telemetry.lives ||
        current.failure_condition !=
            previous_bike_telemetry.failure_condition ||
        current.event != previous_bike_telemetry.event;
    if (changed || vi % 60 == 0) {
        std::printf(
            "[sote][bike] VI=%d stage=%d stage_timer=%.3f "
            "result=%d lives=%d failure_timer=%.3f "
            "failure_condition=%d event=%d frame_delta=%.6f\n",
            vi,
            current.stage,
            stage_timer,
            current.result,
            current.lives,
            failure_timer,
            current.failure_condition,
            current.event,
            frame_delta);
    }

    if (previous_bike_telemetry.valid &&
        current.lives < previous_bike_telemetry.lives) {
        std::printf(
            "[sote][bike] LIFE LOST at VI=%d: %d -> %d "
            "(stage=%d timer=%.3f result=%d)\n",
            vi,
            previous_bike_telemetry.lives,
            current.lives,
            current.stage,
            stage_timer,
            current.result);
    }
    if (previous_bike_telemetry.valid &&
        current.result != previous_bike_telemetry.result &&
        (current.result == 3 ||
         current.result == 4 ||
         current.result == 6)) {
        const char* reason =
            current.result == 4
                ? "GAME OVER / lives exhausted"
                : current.result == 6
                    ? "stage retry / mission failure"
                    : "bike failure timer expired";
        std::printf(
            "[sote][bike] FAILURE TRANSITION at VI=%d: result %d -> %d "
            "(%s, lives=%d stage=%d stage_timer=%.3f "
            "failure_timer=%.3f condition=%d event=%d)\n",
            vi,
            previous_bike_telemetry.result,
            current.result,
            reason,
            current.lives,
            current.stage,
            stage_timer,
            failure_timer,
            current.failure_condition,
            current.event);
    }
    std::fflush(stdout);
    previous_bike_telemetry = current;
}

RspExitReason run_sote_audio_rsp(uint8_t* rdram, uint32_t ucode_addr) {
    const auto start = std::chrono::steady_clock::now();
    const RspExitReason result = aspMain(rdram, ucode_addr);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    static std::atomic<uint64_t> task_count{0};
    const uint64_t count = ++task_count;
    if (std::getenv("SOTE_TRACE_AUDIO_RSP") != nullptr &&
        (count <= 10 || count % 30 == 0)) {
        std::printf(
            "[sote] audio RSP=%llu duration=%lld us\n",
            static_cast<unsigned long long>(count),
            static_cast<long long>(elapsed.count()));
        std::fflush(stdout);
    }
    return result;
}

bool parse_scripted_button(
    std::string_view name,
    uint16_t& buttons,
    int8_t& x,
    int8_t& y) {
    if (name == "a") buttons |= 0x8000;
    else if (name == "b") buttons |= 0x4000;
    else if (name == "z") buttons |= 0x2000;
    else if (name == "start") buttons |= 0x1000;
    else if (name == "up") buttons |= 0x0800;
    else if (name == "down") buttons |= 0x0400;
    else if (name == "left") buttons |= 0x0200;
    else if (name == "right") buttons |= 0x0100;
    else if (name == "l") buttons |= 0x0020;
    else if (name == "r") buttons |= 0x0010;
    else if (name == "cu") buttons |= 0x0008;
    else if (name == "cd") buttons |= 0x0004;
    else if (name == "cl") buttons |= 0x0002;
    else if (name == "cr") buttons |= 0x0001;
    else if (name == "stick_up") y = 80;
    else if (name == "stick_down") y = -80;
    else if (name == "stick_left") x = -80;
    else if (name == "stick_right") x = 80;
    else return false;
    return true;
}

void parse_scripted_input(const char* specification) {
    std::string script = specification;
    size_t cursor = 0;
    while (cursor < script.size()) {
        const size_t entry_end = script.find(',', cursor);
        const std::string_view entry{
            script.data() + cursor,
            (entry_end == std::string::npos ? script.size() : entry_end) -
                cursor};
        const size_t first_colon = entry.find(':');
        const size_t second_colon =
            first_colon == std::string_view::npos
                ? std::string_view::npos
                : entry.find(':', first_colon + 1);
        if (first_colon == std::string_view::npos ||
            second_colon == std::string_view::npos) {
            std::fprintf(
                stderr,
                "[sote] invalid scripted input entry: %.*s\n",
                static_cast<int>(entry.size()),
                entry.data());
            std::_Exit(EXIT_FAILURE);
        }
        ScriptedInputPulse pulse{
            std::atoi(std::string(entry.substr(0, first_colon)).c_str()),
            std::atoi(std::string(entry.substr(
                first_colon + 1,
                second_colon - first_colon - 1)).c_str()),
            0,
            0,
            0};
        std::string_view names = entry.substr(second_colon + 1);
        size_t name_cursor = 0;
        while (name_cursor < names.size()) {
            const size_t name_end = names.find('+', name_cursor);
            const std::string_view name = names.substr(
                name_cursor,
                (name_end == std::string_view::npos ? names.size() : name_end) -
                    name_cursor);
            if (!parse_scripted_button(
                    name, pulse.buttons, pulse.x, pulse.y)) {
                std::fprintf(
                    stderr,
                    "[sote] unknown scripted input: %.*s\n",
                    static_cast<int>(name.size()),
                    name.data());
                std::_Exit(EXIT_FAILURE);
            }
            if (name_end == std::string_view::npos) break;
            name_cursor = name_end + 1;
        }
        if (pulse.start_vi <= 0 || pulse.duration <= 0) {
            std::fprintf(stderr, "[sote] scripted VI and duration must be positive\n");
            std::_Exit(EXIT_FAILURE);
        }
        scripted_input.push_back(pulse);
        if (entry_end == std::string::npos) break;
        cursor = entry_end + 1;
    }
    std::printf(
        "[sote] loaded %zu scripted input pulse(s)\n",
        scripted_input.size());
}

LONG WINAPI report_unhandled_exception(EXCEPTION_POINTERS* exception) {
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    const DWORD64 address =
        static_cast<DWORD64>(exception->ContextRecord->Rip);
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    DWORD line_displacement = 0;
    IMAGEHLP_LINE64 source_line{};
    source_line.SizeOfStruct = sizeof(source_line);
    if (SymFromAddr(process, address, &displacement, symbol)) {
        std::fprintf(
            stderr,
            "[sote] native exception %08lX at %s+0x%llX (%p)\n",
            code,
            symbol->Name,
            static_cast<unsigned long long>(displacement),
            reinterpret_cast<void*>(address));
    } else {
        std::fprintf(
            stderr,
            "[sote] native exception %08lX at %p\n",
            code,
            reinterpret_cast<void*>(address));
    }
    if (exception->ExceptionRecord->NumberParameters >= 2) {
        std::fprintf(
            stderr,
            "[sote] access=%s address=%p\n",
            exception->ExceptionRecord->ExceptionInformation[0] == 0
                ? "read"
                : (exception->ExceptionRecord->ExceptionInformation[0] == 1
                       ? "write"
                       : "execute"),
            reinterpret_cast<void*>(
                exception->ExceptionRecord->ExceptionInformation[1]));
    }
    if (SymGetLineFromAddr64(
            process, address, &line_displacement, &source_line)) {
        std::fprintf(
            stderr,
            "[sote] source: %s:%lu\n",
            source_line.FileName,
            source_line.LineNumber);
    }
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = exception->ContextRecord->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = exception->ContextRecord->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = exception->ContextRecord->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    for (int depth = 0; depth < 16; ++depth) {
        if (!StackWalk64(
                IMAGE_FILE_MACHINE_AMD64,
                process,
                GetCurrentThread(),
                &frame,
                exception->ContextRecord,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        displacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
            std::fprintf(
                stderr,
                "[sote]   #%d %s+0x%llX\n",
                depth,
                symbol->Name,
                static_cast<unsigned long long>(displacement));
        } else {
            std::fprintf(
                stderr,
                "[sote]   #%d %p\n",
                depth,
                reinterpret_cast<void*>(frame.AddrPC.Offset));
        }
        if (SymGetLineFromAddr64(
                process,
                frame.AddrPC.Offset,
                &line_displacement,
                &source_line)) {
            std::fprintf(
                stderr,
                "[sote]      %s:%lu\n",
                source_line.FileName,
                source_line.LineNumber);
        }
    }
    std::fflush(nullptr);
    return EXCEPTION_EXECUTE_HANDLER;
}

void dump_process_thread_stacks(const char* reason) {
    std::call_once(symbol_engine_once, [] {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    });

    const DWORD process_id = GetCurrentProcessId();
    const DWORD current_thread_id = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    std::fprintf(
        stderr,
        "[sote][stall] thread stacks: %s VI=%d display_lists=%d\n",
        reason,
        vi_count.load(std::memory_order_relaxed),
        display_list_count.load(std::memory_order_relaxed));

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != process_id ||
                entry.th32ThreadID == current_thread_id) {
                continue;
            }
            HANDLE thread = OpenThread(
                THREAD_SUSPEND_RESUME |
                    THREAD_GET_CONTEXT |
                    THREAD_QUERY_INFORMATION,
                FALSE,
                entry.th32ThreadID);
            if (thread == nullptr || SuspendThread(thread) == DWORD(-1)) {
                if (thread != nullptr) CloseHandle(thread);
                continue;
            }

            CONTEXT context{};
            context.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(thread, &context)) {
                STACKFRAME64 frame{};
                frame.AddrPC.Offset = context.Rip;
                frame.AddrPC.Mode = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp;
                frame.AddrStack.Mode = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rbp;
                frame.AddrFrame.Mode = AddrModeFlat;
                std::fprintf(
                    stderr,
                    "[sote][stall] thread=%lu\n",
                    static_cast<unsigned long>(entry.th32ThreadID));
                for (int depth = 0; depth < 20; ++depth) {
                    if (depth != 0 &&
                        (!StackWalk64(
                             IMAGE_FILE_MACHINE_AMD64,
                             GetCurrentProcess(),
                             thread,
                             &frame,
                             &context,
                             nullptr,
                             SymFunctionTableAccess64,
                             SymGetModuleBase64,
                             nullptr) ||
                         frame.AddrPC.Offset == 0)) {
                        break;
                    }
                    alignas(SYMBOL_INFO) char storage[
                        sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
                    auto* symbol =
                        reinterpret_cast<SYMBOL_INFO*>(storage);
                    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    symbol->MaxNameLen = MAX_SYM_NAME;
                    DWORD64 displacement = 0;
                    if (SymFromAddr(
                            GetCurrentProcess(),
                            frame.AddrPC.Offset,
                            &displacement,
                            symbol)) {
                        std::fprintf(
                            stderr,
                            "[sote][stall]   #%d %s+0x%llX\n",
                            depth,
                            symbol->Name,
                            static_cast<unsigned long long>(displacement));
                    } else {
                        std::fprintf(
                            stderr,
                            "[sote][stall]   #%d %p\n",
                            depth,
                            reinterpret_cast<void*>(frame.AddrPC.Offset));
                    }
                }
            }
            ResumeThread(thread);
            CloseHandle(thread);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    std::fflush(stderr);
}

void retail_entrypoint(uint8_t* rdram, recomp_context* ctx) {
    constexpr size_t main_offset = 0x1EC0;
    constexpr size_t main_size = 0xEC880;
    const std::filesystem::path image_path =
        portable_layout
            ? runtime_directory / "main.bin"
            : std::filesystem::current_path() / "generated" / "main.bin";
    std::ifstream image(image_path, std::ios::binary);
    if (!image) {
        std::fprintf(
            stderr,
            "[sote] retail image is missing: %s\n",
            image_path.string().c_str());
        std::_Exit(EXIT_FAILURE);
    }
    std::vector<uint8_t> executable(main_size);
    image.read(
        reinterpret_cast<char*>(executable.data()),
        static_cast<std::streamsize>(executable.size()));
    if (image.gcount() != static_cast<std::streamsize>(main_size)) {
        std::fprintf(stderr, "[sote] generated/main.bin has the wrong size\n");
        std::_Exit(EXIT_FAILURE);
    }
    constexpr gpr main_address = static_cast<gpr>(
        static_cast<int32_t>(0x80001EC0U));
    for (size_t i = 0; i < executable.size(); ++i) {
        MEM_B(i, main_address) = executable[i];
    }

    // The retail Ogre loader leaves the entire region between the decompressed
    // image and its bootstrap stack zeroed. Explicitly clear it: the runtime's
    // backing allocation is not required to return zero-filled memory, and
    // SOTE keeps object-registry globals near 0x80112830 in this upper BSS.
    constexpr size_t retail_stack_top = 0x113440;
    const size_t main_end = main_offset + main_size;
    std::memset(rdram + main_end, 0, retail_stack_top - main_end);

    // SOTE's libultra osInitialize scales osClockRate from the 62.5 MHz
    // system clock to the 46.875 MHz CPU counter used by osGetTime. The
    // runtime replacement performs host initialization but intentionally
    // skips the guest implementation and its data write. Leaving the ROM's
    // 62.5 MHz initializer in place makes every osGetTime conversion run at
    // three quarters of real time; in particular, the 48.8 ms audio
    // scheduler fires roughly every 65 ms and repeatedly underruns a 54 ms
    // audio block.
    constexpr gpr os_clock_rate_address = static_cast<gpr>(
        static_cast<int32_t>(0x800E7C70U));
    MEM_W(0, os_clock_rate_address) = 0;
    MEM_W(4, os_clock_rate_address) = 46'875'000;

    // The cartridge bootstrap establishes this stack before decompressing the
    // retail executable. RDRAM is zero-filled by N64ModernRuntime, so loading
    // the recovered image here has the same observable state as the boot
    // decompressor while avoiding direct VI/PI MMIO.
    ctx->r29 = static_cast<int32_t>(0x80113440U);
    load_overlays(
        0x00C00000U,
        static_cast<int32_t>(0x80001EC0U),
        static_cast<uint32_t>(main_size));
    std::printf("[sote] retail executable loaded; entering 0x80017B60\n");
    std::fflush(stdout);
    func_80017B60(rdram, ctx);
}

class HeadlessRenderer final : public ultramodern::renderer::RendererContext {
public:
    HeadlessRenderer() {
        setup_result = ultramodern::renderer::SetupResult::Success;
        chosen_api = ultramodern::renderer::GraphicsApi::Auto;
    }

    bool valid() override { return true; }
    bool update_config(
        const ultramodern::renderer::GraphicsConfig&,
        const ultramodern::renderer::GraphicsConfig&) override {
        return true;
    }
    void enable_instant_present() override {}
    void send_dl(const OSTask* task) override {
        const int count = ++display_list_count;
        if (count <= 8) {
            std::printf(
                "[sote] display list %d: ucode=%08X data=%08X dl=%08X size=%u\n",
                count,
                task->t.ucode,
                task->t.ucode_data,
                task->t.data_ptr,
                task->t.data_size);
            std::fflush(stdout);
        }
    }
    void update_screen() override {}
    void shutdown() override {}
    uint32_t get_display_framerate() const override { return 60; }
    float get_resolution_scale() const override { return 1.0f; }
};

std::unique_ptr<ultramodern::renderer::RendererContext> create_renderer(
    uint8_t* rdram,
    ultramodern::renderer::WindowHandle window_handle,
    bool developer_mode) {
    if (smoke_test) {
        return std::make_unique<HeadlessRenderer>();
    }
    return create_rt64_renderer(
        rdram,
        window_handle,
        developer_mode,
        runtime_directory);
}

void toggle_graphics_fullscreen(HWND window) {
    if (window == nullptr) {
        return;
    }
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    if (!graphics_fullscreen) {
        windowed_placement.length = sizeof(windowed_placement);
        if (!GetWindowPlacement(window, &windowed_placement)) {
            return;
        }
        MONITORINFO monitor_info{sizeof(monitor_info)};
        if (!GetMonitorInfoW(
                MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                &monitor_info)) {
            return;
        }
        SetWindowLongPtrW(
            window,
            GWL_STYLE,
            (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        const RECT& monitor = monitor_info.rcMonitor;
        SetWindowPos(
            window,
            HWND_TOP,
            monitor.left,
            monitor.top,
            monitor.right - monitor.left,
            monitor.bottom - monitor.top,
            SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        graphics_fullscreen = true;
    } else {
        SetWindowLongPtrW(
            window,
            GWL_STYLE,
            (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &windowed_placement);
        SetWindowPos(
            window,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOZORDER | SWP_NOOWNERZORDER);
        graphics_fullscreen = false;
    }
    sote::graphics_menu::sync_window_mode(graphics_fullscreen);
}

void resize_graphics_window(
    HWND window,
    int client_width,
    int client_height,
    bool center_on_monitor = true) {
    if (window == nullptr || graphics_fullscreen ||
        client_width <= 0 || client_height <= 0) {
        return;
    }
    const DWORD style =
        static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const DWORD extended_style =
        static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    RECT bounds{0, 0, client_width, client_height};
    if (!AdjustWindowRectEx(&bounds, style, FALSE, extended_style)) {
        return;
    }
    const int window_width = bounds.right - bounds.left;
    const int window_height = bounds.bottom - bounds.top;
    int x = 0;
    int y = 0;
    UINT position_flags =
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
    if (center_on_monitor) {
        MONITORINFO monitor_info{sizeof(monitor_info)};
        if (!GetMonitorInfoW(
                MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                &monitor_info)) {
            return;
        }
        const RECT& work = monitor_info.rcWork;
        x = work.left +
            ((work.right - work.left) - window_width) / 2;
        y = work.top +
            ((work.bottom - work.top) - window_height) / 2;
    } else {
        position_flags |= SWP_NOMOVE;
    }
    SetWindowPos(
        window,
        nullptr,
        x,
        y,
        window_width,
        window_height,
        position_flags);
    std::printf(
        "[sote] output resolution applied: %dx%d\n",
        client_width,
        client_height);
    std::fflush(stdout);
}

LRESULT CALLBACK window_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
        case WM_ERASEBKGND:
            // RT64 owns every pixel in the client area. Letting DefWindowProc
            // paint the class brush between swap-chain presents causes visible
            // black flashes, especially on the game's static intro screens.
            return 1;
        case WM_KEYDOWN:
            if (wparam == VK_F11) {
                toggle_graphics_fullscreen(window);
                return 0;
            }
            break;
        case WM_SYSKEYDOWN:
            if (wparam == VK_RETURN &&
                (lparam & (1LL << 29)) != 0) {
                toggle_graphics_fullscreen(window);
                return 0;
            }
            break;
        case WM_CLOSE:
            ultramodern::quit();
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            if (window == graphics_window) {
                graphics_window = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

ultramodern::renderer::WindowHandle create_window(void*) {
    if (smoke_test) {
        HWND window = CreateWindowExW(
            0,
            L"STATIC",
            L"Shadows of the Empire Recompiled (headless)",
            0,
            0,
            0,
            1,
            1,
            HWND_MESSAGE,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        return {window, GetCurrentThreadId()};
    }

    constexpr wchar_t class_name[] = L"SOTERecompWindow";
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = class_name;
    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return {};
    }

    const bool diagnostic_offscreen =
        std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr;
    // Start at a true 16:9 client size. RT64's WindowIntegerScale mode then
    // renders this at 1280x720 internally, and resizing/maximizing selects the
    // next suitable N64 integer scale automatically.
    RECT bounds{0, 0, 1280, 720};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(
        diagnostic_offscreen ? WS_EX_NOACTIVATE : 0,
        class_name,
        L"Shadows of the Empire Recompiled",
        WS_OVERLAPPEDWINDOW,
        diagnostic_offscreen ? -30000 : CW_USEDEFAULT,
        diagnostic_offscreen ? -30000 : CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window != nullptr) {
        graphics_window = window;
        MONITORINFO monitor_info{sizeof(monitor_info)};
        if (GetMonitorInfoW(
                MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                &monitor_info)) {
            sote::graphics_menu::sync_display_resolution(
                monitor_info.rcMonitor.right -
                    monitor_info.rcMonitor.left,
                monitor_info.rcMonitor.bottom -
                    monitor_info.rcMonitor.top);
        }
        ShowWindow(
            window,
            diagnostic_offscreen ? SW_SHOWNOACTIVATE : SW_SHOW);
        UpdateWindow(window);
    }
    return {window, GetCurrentThreadId()};
}

void update_gfx(void*) {
    if (smoke_test) {
        return;
    }
    sote::graphics_menu::Settings requested_window{};
    if (sote::graphics_menu::take_window_request(requested_window)) {
        // This callback owns the Win32 message pump, so size and style changes
        // requested by the emulated menu are applied on the correct thread.
        const bool diagnostic_offscreen =
            std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr;
        if (diagnostic_offscreen) {
            // Keep capture windows offscreen, but exercise the real client
            // resize path so output-resolution proofs are meaningful.
            if (!requested_window.borderless) {
                resize_graphics_window(
                    graphics_window,
                    requested_window.output_width,
                    requested_window.output_height,
                    false);
            }
        } else if (requested_window.borderless) {
            if (!graphics_fullscreen) {
                resize_graphics_window(
                    graphics_window,
                    requested_window.output_width,
                    requested_window.output_height);
                toggle_graphics_fullscreen(graphics_window);
            }
        } else {
            if (graphics_fullscreen) {
                toggle_graphics_fullscreen(graphics_window);
            }
            resize_graphics_window(
                graphics_window,
                requested_window.output_width,
                requested_window.output_height);
        }
    }
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    if (task->t.type == M_AUDTASK) {
        static bool reported = false;
        if (!reported) {
            std::printf(
                "[sote] audio ucode observed: text=%08X/%u data=%08X/%u\n",
                task->t.ucode,
                task->t.ucode_size,
                task->t.ucode_data,
                task->t.ucode_data_size);
            std::fflush(stdout);
            reported = true;
        }
        return run_sote_audio_rsp;
    }
    std::fprintf(stderr, "[sote] unsupported RSP task type %u\n", task->t.type);
    return nullptr;
}

void message_box(const char* message) {
    std::fprintf(stderr, "[sote] runtime error: %s\n", message);
}

void on_init(uint8_t* rdram, recomp_context*) {
    game_rdram = rdram;
    std::printf("[sote] runtime initialized; entering 0x80000400\n");
    std::fflush(stdout);
}

void on_thread_create(uint8_t*, recomp_context*) {
    const int count = ++thread_count;
    std::printf("[sote] game thread %d created\n", count);
    std::fflush(stdout);
}

void on_vi() {
    const int count = ++vi_count;
    game_frame_cv.notify_all();
    log_bike_telemetry(count);
    log_general_failure_telemetry(count);
    const int current_display_lists =
        display_list_count.load(std::memory_order_relaxed);
    if (current_display_lists != last_observed_display_lists) {
        last_observed_display_lists = current_display_lists;
        last_display_activity_vi = count;
    } else if (std::getenv("SOTE_TRACE_STALLS") != nullptr &&
               count - last_display_activity_vi >= 120 &&
               !stall_stack_dumped.exchange(
                   true, std::memory_order_relaxed)) {
        dump_process_thread_stacks(
            "no display-list submission for 120 VIs");
    }
    if (count == 700 && game_rdram != nullptr &&
        std::getenv("SOTE_DIAGNOSTIC_UNLOCK_LEVELS") != nullptr) {
        uint8_t* rdram = game_rdram;
        constexpr gpr profile_table = static_cast<gpr>(
            static_cast<int32_t>(0x8018BBF8U));
        constexpr gpr profile_size = 0x7A;
        constexpr gpr current_level_offset = 0x8;
        constexpr gpr completion_offset = 0x60;
        constexpr int level_count = 10;
        for (int profile = 0; profile < 4; ++profile) {
            const gpr profile_address =
                profile_table + profile * profile_size;
            // Every isolated smoke run starts its Change Level cursor at the
            // first entry. This makes N scripted Down presses select level N
            // regardless of the last level stored in the portable save.
            write_guest_byte(
                rdram,
                static_cast<uint32_t>(
                    profile_address + current_level_offset),
                0);
            for (int level = 0; level < level_count; ++level) {
                MEM_H(
                    completion_offset + level * sizeof(uint16_t),
                    profile_address) = 1;
            }
        }
        std::printf("[sote] diagnostic level selection unlocked\n");
        std::fflush(stdout);
    }
    uint16_t scripted_buttons = 0;
    int8_t scripted_x = 0;
    int8_t scripted_y = 0;
    for (const ScriptedInputPulse& pulse : scripted_input) {
        if (count >= pulse.start_vi &&
            count < pulse.start_vi + pulse.duration) {
            scripted_buttons |= pulse.buttons;
            if (pulse.x != 0) scripted_x = pulse.x;
            if (pulse.y != 0) scripted_y = pulse.y;
        }
        if (count == pulse.start_vi) {
            std::printf(
                "[sote] scripted input at VI=%d: buttons=%04X stick=%d,%d\n",
                count,
                pulse.buttons,
                pulse.x,
                pulse.y);
        }
    }
    sote::frontend::set_scripted_input(
        scripted_buttons, scripted_x, scripted_y);
    current_scripted_buttons.store(
        scripted_buttons, std::memory_order_relaxed);
    if (count == 1 || count % 30 == 0 ||
        std::getenv("SOTE_TRACE_EVERY_VI") != nullptr) {
        std::printf(
            "[sote] VI=%d threads=%d display_lists=%d game_frames=%llu\n",
            count,
            thread_count.load(),
            display_list_count.load(),
            static_cast<unsigned long long>(
                game_frame_count.load(std::memory_order_relaxed)));
        std::fflush(stdout);
    }
    if ((smoke_test || frontend_smoke_test) &&
        count >= smoke_vi_limit) {
        if (smoke_expected_level_index >= 0) {
            const int observation_display_lists =
                display_list_count.load(std::memory_order_relaxed) -
                smoke_observation_start_display_lists;
            const uint64_t observation_game_frames =
                game_frame_count.load(std::memory_order_relaxed) -
                smoke_observation_start_game_frames;
            const int display_stall_vis =
                count - last_display_activity_vi;
            std::printf(
                "[sote][smoke] LEVEL OBSERVATION COMPLETE "
                "expected_index=%d expected_name=%s observed_vis=%d "
                "display_lists_delta=%d game_frames_delta=%llu "
                "display_stall_vis=%d "
                "motion_guards=%u rapid_life_losses=%u "
                "player_delta_leaks=%u bike_delta_leaks=%u\n",
                smoke_expected_level_index,
                smoke_expected_level_name.c_str(),
                count - smoke_observation_start_vi,
                observation_display_lists,
                static_cast<unsigned long long>(observation_game_frames),
                display_stall_vis,
                motion_loop_guard_count.load(std::memory_order_relaxed),
                rapid_life_loss_count.load(std::memory_order_relaxed),
                player_invalid_delta_count.load(std::memory_order_relaxed),
                bike_invalid_delta_count.load(std::memory_order_relaxed));
        }
        std::printf(
            "[sote] smoke complete: VI=%d threads=%d display_lists=%d\n",
            count,
            thread_count.load(),
            display_list_count.load());
        std::fflush(nullptr);
        std::_Exit(display_list_count.load() > 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
}

} // namespace

extern "C" void sote_wait_for_game_frame() {
    // HLE can finish two complete gameplay iterations within one 60 Hz VI,
    // which made movement and timers run far too quickly. Conversely, holding
    // each iteration for two VIs reduced player and jetpack physics to 30 Hz
    // and felt visibly slow. Allow exactly one gameplay iteration per VI.
    std::unique_lock lock(game_frame_mutex);
    int current_vi = vi_count.load(std::memory_order_relaxed);
    if (last_game_frame_vi >= 0) {
        game_frame_cv.wait(lock, [&] {
            return vi_count.load(std::memory_order_relaxed) >=
                last_game_frame_vi + 1;
        });
        current_vi = vi_count.load(std::memory_order_relaxed);
    }
    last_game_frame_vi = current_vi;
    game_frame_count.fetch_add(1, std::memory_order_relaxed);
}

extern "C" uint32_t sote_enter_bike_controller(
    uint8_t* rdram,
    uint32_t object) {
    const int vi = vi_count.load(std::memory_order_relaxed);
    bike_controller_last_vi.store(vi, std::memory_order_relaxed);

    constexpr uint32_t frame_delta_address = 0x8018E998U;
    const double frame_delta =
        read_guest_double(rdram, frame_delta_address);
    const bool timeline_traversal =
        !std::isfinite(frame_delta) ||
        frame_delta < 0.0 ||
        frame_delta > 0.1;
    if (!timeline_traversal) {
        if (std::getenv("SOTE_TRACE_BIKE_CALLS") != nullptr) {
            std::printf(
                "[sote][bike-call] VI=%d object=%08X flags=%08X "
                "stage=%d timer=%.6f delta=%.6f\n",
                vi,
                object,
                read_guest_word(rdram, object + 0xE0U),
                static_cast<int32_t>(
                    read_guest_word(rdram, 0x800DD340U)),
                read_guest_float(rdram, 0x800DD344U),
                frame_delta);
            std::fflush(stdout);
        }
        return 0;
    }

    // This is the engine's global simulation delta. A corrupted override can
    // contain a host timestamp measured in thousands of seconds. Keeping that
    // value poisons gameplay, animation, and audio state; skipping the bike
    // controller leaves scene submission unfinished. Replace it permanently
    // with one nominal 50 Hz step and continue the controller normally.
    constexpr double nominal_delta = 0.02;
    uint64_t nominal_bits = 0;
    std::memcpy(&nominal_bits, &nominal_delta, sizeof(nominal_bits));
    write_guest_word(
        rdram,
        frame_delta_address,
        static_cast<uint32_t>(nominal_bits >> 32));
    write_guest_word(
        rdram,
        frame_delta_address + sizeof(uint32_t),
        static_cast<uint32_t>(nominal_bits));

    const uint32_t occurrence =
        bike_invalid_delta_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 8 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][bike] normalized timeline controller call "
            "frame_delta=%.6f -> %.6f occurrence=%u at VI=%d\n",
            frame_delta,
            nominal_delta,
            occurrence,
            vi);
        std::fflush(stdout);
    }
    return 0;
}

extern "C" void sote_enter_player_controller(
    uint8_t* rdram,
    uint32_t object,
    uint32_t controller,
    uint32_t action) {
    // The original game can revisit normal-looking controller actions while
    // it is still unwinding a destroyed player. Human and active telemetry
    // place the first independently plausible respawn death after 1,800 VIs.
    constexpr int minimum_stable_respawn_vis = 1800;
    const int controller_vi = vi_count.load(std::memory_order_relaxed);
    if (std::getenv("SOTE_TRACE_PLAYER_STATE") != nullptr &&
        object >= 0x80000000U && object < 0x80800000U) {
        static int last_player_state_vi = -1;
        if (controller_vi != last_player_state_vi &&
            controller_vi % 15 == 0) {
            last_player_state_vi = controller_vi;
            std::printf(
                "[sote][player-state] VI=%d controller=%08X object=%08X "
                "buttons=%04X action=%u pos=%.3f,%.3f,%.3f "
                "velocity=%.3f flags=%08X event=%d\n",
                controller_vi,
                controller,
                object,
                current_scripted_buttons.load(std::memory_order_relaxed),
                action,
                read_guest_float(rdram, object + 0x50U),
                read_guest_float(rdram, object + 0x54U),
                read_guest_float(rdram, object + 0x58U),
                read_guest_float(rdram, object + 0x60U),
                read_guest_word(rdram, object + 0x74U),
                static_cast<int16_t>(
                    read_guest_half(rdram, 0x8013CE0EU)));
            std::fflush(stdout);
        }
    }
    if (life_loss_episode.active &&
        life_loss_episode.controller == controller) {
        const int vi = vi_count.load(std::memory_order_relaxed);
        const int16_t event = static_cast<int16_t>(
            read_guest_half(rdram, 0x8013CE0EU));
        const int previous_level =
            level_index_for_event(life_loss_episode.event);
        const int current_level = level_index_for_event(event);
        if (current_level < 0 || current_level != previous_level) {
            life_loss_episode.active = false;
        } else if (event != life_loss_episode.event) {
            // Multi-part levels replace their player object while advancing
            // between event records. Keep the old death episode latched so
            // the loading transition cannot spend another life.
            std::printf(
                "[sote][lives] carried death episode across level event "
                "controller=%08X object=%08X->%08X event=%d->%d VI=%d\n",
                controller,
                life_loss_episode.object,
                object,
                life_loss_episode.event,
                event,
                vi);
            std::fflush(stdout);
            life_loss_episode.object = object;
            life_loss_episode.event = event;
            life_loss_episode.normal_since_vi = -1;
        }
        if (life_loss_episode.active &&
            life_loss_episode.object == object &&
            action >= 4U && action <= 6U) {
            // Actions 4/5/6 are the observed failure transition. If normal
            // control persisted long enough before returning here, this is
            // a new death; otherwise it is another callback from the old
            // destroyed-player episode.
            if (life_loss_episode.normal_since_vi >= 0 &&
                vi - life_loss_episode.normal_since_vi >=
                    minimum_stable_respawn_vis) {
                std::printf(
                    "[sote][lives] rearmed before new failure "
                    "controller=%08X object=%08X action=%u VI=%d "
                    "normal_vis=%d event=%d\n",
                    controller,
                    object,
                    action,
                    vi,
                    vi - life_loss_episode.normal_since_vi,
                    event);
                std::fflush(stdout);
                life_loss_episode.active = false;
            } else {
                life_loss_episode.normal_since_vi = -1;
            }
        } else if (
            life_loss_episode.active &&
            life_loss_episode.object == object &&
            life_loss_episode.normal_since_vi < 0) {
            life_loss_episode.normal_since_vi = vi;
        } else if (
            life_loss_episode.active &&
            life_loss_episode.object == object &&
            vi - life_loss_episode.normal_since_vi >=
                minimum_stable_respawn_vis) {
            std::printf(
                "[sote][lives] rearmed after stable respawn "
                "controller=%08X object=%08X action=%u VI=%d "
                "normal_vis=%d event=%d\n",
                controller,
                object,
                action,
                vi,
                vi - life_loss_episode.normal_since_vi,
                event);
            std::fflush(stdout);
            life_loss_episode.active = false;
        }
    }
    constexpr uint32_t frame_delta_address = 0x8018E998U;
    const double frame_delta =
        read_guest_double(rdram, frame_delta_address);
    if (std::isfinite(frame_delta) &&
        frame_delta >= 0.0 &&
        frame_delta <= 0.1) {
        return;
    }

    // On-foot controllers can be invoked by the same scripted timeline
    // traversal that reaches the bike controller. The traversal temporarily
    // exposes an absolute timeline value through the global simulation-delta
    // slot. Consuming it here advances movement, animation, and the death
    // timer by thousands of seconds in one call.
    constexpr double nominal_delta = 0.02;
    uint64_t nominal_bits = 0;
    std::memcpy(&nominal_bits, &nominal_delta, sizeof(nominal_bits));
    write_guest_word(
        rdram,
        frame_delta_address,
        static_cast<uint32_t>(nominal_bits >> 32));
    write_guest_word(
        rdram,
        frame_delta_address + sizeof(uint32_t),
        static_cast<uint32_t>(nominal_bits));

    const uint32_t occurrence =
        player_invalid_delta_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 8 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][player] normalized timeline controller call "
            "controller=%08X object=%08X frame_delta=%.6f -> %.6f "
            "occurrence=%u at VI=%d event=%d\n",
            controller,
            object,
            frame_delta,
            nominal_delta,
            occurrence,
            vi_count.load(std::memory_order_relaxed),
            static_cast<int16_t>(
                read_guest_half(rdram, 0x8013CE0EU)));
        std::fflush(stdout);
    }
}

extern "C" double sote_sanitize_frame_delta(double delta) {
    if (std::isfinite(delta) &&
        (delta == -1.0 || (delta >= 0.0 && delta <= 0.1))) {
        return delta;
    }
    constexpr double nominal_delta = 0.02;
    const uint32_t occurrence =
        invalid_frame_delta_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 8 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][timing] rejected invalid frame delta %.6f; "
            "using %.6f occurrence=%u at VI=%d\n",
            delta,
            nominal_delta,
            occurrence,
            vi_count.load(std::memory_order_relaxed));
        std::fflush(stdout);
    }
    return nominal_delta;
}

extern "C" void sote_sanitize_global_frame_delta(uint8_t* rdram) {
    constexpr uint32_t frame_delta_address = 0x8018E998U;
    const double frame_delta =
        read_guest_double(rdram, frame_delta_address);
    const double sanitized = sote_sanitize_frame_delta(frame_delta);
    if (sanitized == frame_delta) {
        return;
    }

    uint64_t sanitized_bits = 0;
    std::memcpy(
        &sanitized_bits, &sanitized, sizeof(sanitized_bits));
    write_guest_word(
        rdram,
        frame_delta_address,
        static_cast<uint32_t>(sanitized_bits >> 32));
    write_guest_word(
        rdram,
        frame_delta_address + sizeof(uint32_t),
        static_cast<uint32_t>(sanitized_bits));
}

extern "C" uint32_t sote_allow_life_loss(
    uint8_t* rdram,
    uint32_t source,
    uint32_t object,
    uint32_t action) {
    constexpr int minimum_respawn_vis = 1800;
    const int vi = vi_count.load(std::memory_order_relaxed);
    const int16_t event = static_cast<int16_t>(
        read_guest_half(rdram, 0x8013CE0EU));
    const int32_t lives = static_cast<int32_t>(
        read_guest_word(rdram, 0x800E0EB0U));
    const bool valid_object =
        object >= 0x80000000U && object < 0x80800000U;
    const uint32_t object_field_70 =
        valid_object ? read_guest_word(rdram, object + 0x70U) : 0;
    const uint32_t object_field_e0 =
        valid_object ? read_guest_word(rdram, object + 0xE0U) : 0;
    const uint32_t object_field_230 =
        valid_object ? read_guest_word(rdram, object + 0x230U) : 0;
    const uint32_t object_field_234 =
        valid_object ? read_guest_word(rdram, object + 0x234U) : 0;
    uint32_t episode_controller = 0;
    switch (source) {
        case 0x8008C638U:
            episode_controller = 0x8008BF00U;
            break;
        case 0x8009724CU:
            episode_controller = 0x80096E14U;
            break;
        default:
            break;
    }
    const int elapsed_vis = vi - last_committed_life_loss_vi;
    const bool would_underflow = lives < 0;
    const bool repeated_death_episode =
        episode_controller != 0 &&
        life_loss_episode.active &&
        life_loss_episode.source == source &&
        life_loss_episode.object == object &&
        life_loss_episode.event == event;
    const bool duplicate_respawn_loss =
        episode_controller == 0 &&
        event == last_committed_life_loss_event &&
        elapsed_vis >= 0 &&
        elapsed_vis < minimum_respawn_vis;
    if (would_underflow ||
        repeated_death_episode ||
        duplicate_respawn_loss) {
        const uint32_t occurrence =
            duplicate_life_loss_suppression_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        if (occurrence <= 8 || occurrence % 1000 == 0) {
            std::printf(
                "[sote][lives] suppressed %s life loss source=%08X "
                "VI=%d elapsed_vis=%d event=%d lives=%d object=%08X "
                "action=%u "
                "f70=%08X fe0=%08X f230=%08X f234=%08X occurrence=%u\n",
                would_underflow
                    ? "underflow"
                    : repeated_death_episode
                        ? "episode"
                        : "duplicate",
                source,
                vi,
                elapsed_vis,
                event,
                lives,
                object,
                action,
                object_field_70,
                object_field_e0,
                object_field_230,
                object_field_234,
                occurrence);
            std::fflush(stdout);
        }
        return 0;
    }
    last_committed_life_loss_vi = vi;
    last_committed_life_loss_event = event;
    if (episode_controller != 0) {
        life_loss_episode.active = true;
        life_loss_episode.source = source;
        life_loss_episode.object = object;
        life_loss_episode.controller = episode_controller;
        life_loss_episode.death_action = action;
        life_loss_episode.event = event;
        life_loss_episode.normal_since_vi = -1;
    }
    const uint32_t occurrence =
        committed_life_loss_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 16 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][lives] committed life loss source=%08X VI=%d "
            "event=%d lives=%d object=%08X action=%u "
            "f70=%08X fe0=%08X "
            "f230=%08X f234=%08X occurrence=%u\n",
            source,
            vi,
            event,
            lives,
            object,
            action,
            object_field_70,
            object_field_e0,
            object_field_230,
            object_field_234,
            occurrence);
        std::fflush(stdout);
    }
    return 1;
}

extern "C" void sote_note_audio_negative_exponent(int32_t exponent) {
    const uint32_t occurrence =
        audio_negative_exponent_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 8 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][audio] repaired negative volume exponent=%d "
            "occurrence=%u at VI=%d\n",
            exponent,
            occurrence,
            vi_count.load(std::memory_order_relaxed));
        std::fflush(stdout);
    }
}

extern "C" void sote_note_bike_life_loss(
    uint8_t* rdram,
    uint32_t source) {
    const int32_t lives = static_cast<int32_t>(
        read_guest_word(rdram, 0x800E0EB0U));
    const int32_t stage = static_cast<int32_t>(
        read_guest_word(rdram, 0x800DD340U));
    const int32_t result = static_cast<int32_t>(
        read_guest_word(rdram, 0x800DD2B0U));
    const float stage_timer =
        read_guest_float(rdram, 0x800DD344U);
    const double frame_delta =
        read_guest_double(rdram, 0x8018E998U);
    std::printf(
        "[sote][bike] LIFE-LOSS PATH at VI=%d: source=%s "
        "lives_before=%d stage=%d stage_timer=%.3f result=%d "
        "frame_delta=%.6f\n",
        vi_count.load(std::memory_order_relaxed),
        source == 1
            ? "bike collision/out-of-bounds"
            : "mission timer expired",
        lives,
        stage,
        stage_timer,
        result,
        frame_delta);
    std::fflush(stdout);
}

extern "C" uint32_t sote_normalize_zero_velocity_motion(
    uint8_t* rdram,
    uint32_t object) {
    const int16_t state = static_cast<int16_t>(
        read_guest_half(rdram, object + 0xA0U));
    const float velocity = read_guest_float(rdram, object + 0xA8U);
    if ((state != 2 && state != 3) || velocity != 0.0F) {
        return 0;
    }

    // States 2 and 3 only leave the inner transition loop by advancing
    // toward a keyframe. A zero-velocity transition cannot make progress,
    // so normalize it before entering the loop rather than relying on the
    // emergency iteration guard to recover after the fact.
    write_guest_half(rdram, object + 0xA0U, 0);
    const uint32_t occurrence =
        motion_zero_velocity_normalization_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (occurrence <= 8 || occurrence % 1000 == 0) {
        std::printf(
            "[sote][effects] normalized zero-velocity motion before loop "
            "at VI=%d occurrence=%u object=%08X type=%d state=%d "
            "keyframe=%d position=%.6f previous=%.6f\n",
            vi_count.load(std::memory_order_relaxed),
            occurrence,
            object,
            static_cast<int16_t>(
                read_guest_half(rdram, object + 0x68U)),
            state,
            static_cast<int16_t>(
                read_guest_half(rdram, object + 0xA2U)),
            read_guest_float(rdram, object + 0xACU),
            read_guest_float(rdram, object + 0xB0U));
        std::fflush(stdout);
    }
    return 1;
}

extern "C" void sote_note_motion_loop_guard(
    uint8_t* rdram,
    uint32_t object) {
    const uint32_t occurrence =
        motion_loop_guard_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (occurrence > 8 && occurrence % 1000 != 0) {
        return;
    }
    const int16_t state = static_cast<int16_t>(
        read_guest_half(rdram, object + 0xA0U));
    const int16_t keyframe = static_cast<int16_t>(
        read_guest_half(rdram, object + 0xA2U));
    const float velocity = read_guest_float(rdram, object + 0xA8U);
    const bool repaired =
        (state == 2 || state == 3) && velocity == 0.0F;
    if (repaired) {
        // States 2 and 3 only terminate by advancing toward the next
        // keyframe. With zero velocity the original loop can never change
        // its comparison result, so return the motion object to its stable
        // idle state. A later input-driven update can activate it normally.
        write_guest_half(rdram, object + 0xA0U, 0);
    }
    std::printf(
        "[sote][effects] bounded stalled motion transition at VI=%d "
        "occurrence=%u object=%08X type=%d flags=%08X state=%d "
        "keyframe=%d velocity=%.6f position=%.6f previous=%.6f repaired=%d "
        "kf0={%.6f,%.6f,%08X} kf1={%.6f,%.6f,%08X} "
        "kf2={%.6f,%.6f,%08X}\n",
        vi_count.load(std::memory_order_relaxed),
        occurrence,
        object,
        static_cast<int16_t>(read_guest_half(rdram, object + 0x68U)),
        read_guest_word(rdram, object + 0xA4U),
        state,
        keyframe,
        velocity,
        read_guest_float(rdram, object + 0xACU),
        read_guest_float(rdram, object + 0xB0U),
        repaired ? 1 : 0,
        read_guest_float(rdram, object + 0xB8U),
        read_guest_float(rdram, object + 0xBCU),
        read_guest_word(rdram, object + 0xC0U),
        read_guest_float(rdram, object + 0xC4U),
        read_guest_float(rdram, object + 0xC8U),
        read_guest_word(rdram, object + 0xCCU),
        read_guest_float(rdram, object + 0xD0U),
        read_guest_float(rdram, object + 0xD4U),
        read_guest_word(rdram, object + 0xD8U));
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    SetUnhandledExceptionFilter(report_unhandled_exception);
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    runtime_directory = get_executable_directory();
    sote::graphics_menu::initialize(runtime_directory);
    portable_layout =
        std::filesystem::is_regular_file(runtime_directory / "main.bin");
    std::filesystem::path rom_path =
        portable_layout
            ? runtime_directory / "sote.us.v1.2.z64"
            : std::filesystem::current_path() /
                "Star Wars - Shadows of the Empire (U) (V1.2) [!].z64";
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--headless-smoke") {
            smoke_test = true;
        } else if (std::string_view{argv[i]} == "--frontend-smoke") {
            frontend_smoke_test = true;
        } else if (std::string_view{argv[i]} == "--muted") {
            muted_output = true;
        } else {
            rom_path = std::filesystem::path{argv[i]};
        }
    }
    if (portable_layout &&
        ((!smoke_test && !frontend_smoke_test) ||
         std::getenv("SOTE_FORCE_FILE_LOG") != nullptr)) {
        initialize_persistent_logging();
    }
    if (const char* limit = std::getenv("SOTE_SMOKE_VIS")) {
        const int parsed = std::atoi(limit);
        if (parsed > 0) {
            smoke_vi_limit = parsed;
        }
    }
    if (const char* observation_start =
            std::getenv("SOTE_SMOKE_OBSERVATION_START_VI")) {
        const int parsed = std::atoi(observation_start);
        if (parsed > 0) {
            smoke_observation_start_vi = parsed;
        }
    }
    if (const char* expected_index =
            std::getenv("SOTE_EXPECT_LEVEL_INDEX")) {
        smoke_expected_level_index = std::atoi(expected_index);
    }
    if (const char* expected_name =
            std::getenv("SOTE_EXPECT_LEVEL_NAME")) {
        smoke_expected_level_name = expected_name;
    }
    smoke_refill_lives =
        std::getenv("SOTE_SMOKE_REFILL_LIVES") != nullptr;
    smoke_expect_natural_game_over =
        std::getenv("SOTE_EXPECT_NATURAL_GAME_OVER") != nullptr;
    if (const char* input_script = std::getenv("SOTE_INPUT_SCRIPT")) {
        parse_scripted_input(input_script);
    }
    sote::frontend::set_physical_input_enabled(
        !smoke_test &&
        std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") == nullptr);
    sote::frontend::set_audio_enabled(!muted_output);
    if (muted_output) {
        std::printf("[sote] audio output muted\n");
        std::fflush(stdout);
    }
    if (!smoke_test && !sote::frontend::initialize()) {
        std::fprintf(
            stderr,
            "[sote] frontend devices unavailable; continuing without audio\n");
    }

    register_overlays();
    const char* diagnostic_config =
        std::getenv("SOTE_DIAGNOSTIC_CONFIG_PATH");
    const std::filesystem::path config_path =
        diagnostic_config != nullptr && diagnostic_config[0] != '\0'
            ? std::filesystem::path{diagnostic_config}
            : portable_layout
                ? runtime_directory
                : std::filesystem::current_path() / "out" / "config";
    std::filesystem::create_directories(config_path);
    recomp::register_config_path(config_path);
    if (portable_layout) {
        std::printf(
            "[sote] portable layout: %s%s\n",
            runtime_directory.string().c_str(),
            persistent_logging
                ? " (persistent logging enabled)"
                : "");
        std::fflush(stdout);
    }

    recomp::GameEntry game{};
    game.rom_hash = 0x8F6A90EF92C8B1E2ULL;
    game.internal_name = "Shadow of the Empire";
    game.game_id = u8"sote.us.v1.2";
    game.save_type = recomp::SaveType::Eep4k;
    game.is_enabled = true;
    game.has_compressed_code = false;
    game.entrypoint_address = static_cast<gpr>(
        static_cast<int32_t>(0x80000400U));
    game.entrypoint = retail_entrypoint;
    game.thread_create_callback = on_thread_create;
    game.on_init_callback = on_init;
    if (!recomp::register_game(game)) {
        std::fprintf(stderr, "[sote] failed to register game\n");
        return EXIT_FAILURE;
    }

    std::u8string game_id = game.game_id;
    const recomp::RomValidationError validation =
        recomp::select_rom(rom_path, game_id);
    if (validation != recomp::RomValidationError::Good) {
        std::fprintf(
            stderr,
            "[sote] ROM validation failed (%d): %s\n",
            static_cast<int>(validation),
            rom_path.string().c_str());
        return EXIT_FAILURE;
    }
    std::printf(
        "[sote] ROM validated: %s\n",
        rom_path.string().c_str());

    // RT64 device/swap-chain creation can outlast the first VI interval. Give
    // the VI thread time to seed its dummy mode before marking the game
    // started; otherwise it can observe a null mode in the brief interval
    // before the game's first osViSetMode call.
    const int start_delay_ms = smoke_test ? 100 : 1000;
    std::thread starter([game_id, start_delay_ms] {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(start_delay_ms));
        recomp::start_game(game_id);
    });
    starter.detach();

    if (smoke_test || frontend_smoke_test) {
        const int watchdog_seconds =
            std::max(60, smoke_vi_limit / 60 + 60);
        std::thread watchdog([watchdog_seconds] {
            std::this_thread::sleep_for(
                std::chrono::seconds(watchdog_seconds));
            std::fprintf(
                stderr,
                "[sote] smoke watchdog: VI=%d threads=%d display_lists=%d\n",
                vi_count.load(),
                thread_count.load(),
                display_list_count.load());
            std::fflush(nullptr);
            std::_Exit(EXIT_FAILURE);
        });
        watchdog.detach();
    }

    recomp::Configuration config{};
    config.project_version = {0, 1, 0, "-bringup"};
    config.rsp_callbacks.get_rsp_microcode = get_rsp_microcode;
    config.renderer_callbacks.create_render_context = create_renderer;
    config.audio_callbacks.queue_samples = sote::frontend::queue_samples;
    config.audio_callbacks.get_frames_remaining =
        sote::frontend::get_frames_remaining;
    config.audio_callbacks.set_frequency = sote::frontend::set_frequency;
    config.input_callbacks.poll_input = sote::frontend::poll_input;
    config.input_callbacks.get_input = sote::frontend::get_input;
    config.input_callbacks.set_rumble = sote::frontend::set_rumble;
    config.input_callbacks.get_connected_device_info =
        sote::frontend::get_connected_device_info;
    config.gfx_callbacks.create_window = create_window;
    config.gfx_callbacks.update_gfx = update_gfx;
    config.events_callbacks.vi_callback = on_vi;
    config.error_handling_callbacks.message_box = message_box;

    recomp::start(config);
    sote::frontend::shutdown();
    return EXIT_SUCCESS;
}
