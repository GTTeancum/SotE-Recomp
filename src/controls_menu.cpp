#include "controls_menu.hpp"

#include "recomp_hooks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

namespace sote::controls_menu {
namespace {

struct State {
    std::mutex mutex;
    ControlScheme schemes[static_cast<int>(SchemeSlot::Count)] = {
        ControlScheme::Classic,
        ControlScheme::Classic,
    };
    BikeTuning tuning{};
    std::filesystem::path settings_path;
    std::filesystem::path tuning_ini_path;
    std::filesystem::file_time_type tuning_mtime{};
    bool tuning_mtime_valid = false;
};

State state;

std::string trim(std::string value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

uint16_t parse_fire_button(const std::string& token) {
    if (token == "A") return 0x8000;
    if (token == "B") return 0x4000;
    if (token == "Z") return 0x2000;
    if (token == "L") return 0x0020;
    if (token == "R") return 0x0010;
    return 0x8000;
}

// Minimal `key = value` INI reader, one entry per line, '#' or ';' starts
// a comment. No sections needed for a single flat tuning block.
void load_tuning_ini_locked() {
    BikeTuning defaults{};
    state.tuning = defaults;

    std::ifstream input{state.tuning_ini_path};
    if (!input.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        if (value.empty()) {
            continue;
        }
        if (key == "steering_curve_exponent") {
            state.tuning.steering_curve_exponent =
                std::strtof(value.c_str(), nullptr);
        } else if (key == "high_speed_sensitivity_falloff") {
            state.tuning.high_speed_sensitivity_falloff =
                std::strtof(value.c_str(), nullptr);
        } else if (key == "high_speed_min_scale") {
            state.tuning.high_speed_min_scale =
                std::strtof(value.c_str(), nullptr);
        } else if (key == "steering_stabilization") {
            state.tuning.steering_stabilization =
                std::strtof(value.c_str(), nullptr);
        } else if (key == "camera_smoothing") {
            state.tuning.camera_smoothing =
                std::strtof(value.c_str(), nullptr);
        } else if (key == "fire_button") {
            state.tuning.fire_button_bit = parse_fire_button(value);
        }
    }
}

void write_default_tuning_ini_locked() {
    std::ofstream output{state.tuning_ini_path};
    if (!output.is_open()) {
        return;
    }
    const BikeTuning d{};
    output <<
        "; Speeder-bike Modern control scheme tuning.\n"
        "; Only read while Control Scheme = Modern. Edited values apply\n"
        "; the next time the bike stage is (re)entered; the file is also\n"
        "; polled for changes while the game is running.\n"
        "; fire_button: which physical N64 button Modern's RB maps to.\n"
        "; UNVERIFIED against the original bike sequence -- defaults to A.\n"
        "; Valid values: A, B, Z, L, R\n"
        "fire_button = A\n\n"
        "steering_curve_exponent = " << d.steering_curve_exponent << "\n"
        "high_speed_sensitivity_falloff = "
            << d.high_speed_sensitivity_falloff << "\n"
        "high_speed_min_scale = " << d.high_speed_min_scale << "\n"
        "steering_stabilization = " << d.steering_stabilization << "\n"
        "camera_smoothing = " << d.camera_smoothing << "\n";
}

void reload_tuning_if_changed_locked() {
    if (state.tuning_ini_path.empty()) {
        return;
    }
    std::error_code error;
    const auto mtime =
        std::filesystem::last_write_time(state.tuning_ini_path, error);
    if (error) {
        return;
    }
    if (state.tuning_mtime_valid && mtime == state.tuning_mtime) {
        return;
    }
    state.tuning_mtime = mtime;
    state.tuning_mtime_valid = true;
    load_tuning_ini_locked();
    std::printf(
        "[sote][controls-menu] reloaded bike_tuning.ini "
        "(curve=%.2f falloff=%.2f min_scale=%.2f "
        "stabilization=%.2f camera_smoothing=%.2f fire_bit=%04X)\n",
        state.tuning.steering_curve_exponent,
        state.tuning.high_speed_sensitivity_falloff,
        state.tuning.high_speed_min_scale,
        state.tuning.steering_stabilization,
        state.tuning.camera_smoothing,
        state.tuning.fire_button_bit);
    std::fflush(stdout);
}

float apply_low_pass(float previous, float target, float damping) {
    const float alpha = std::clamp(1.0f - damping, 0.0f, 1.0f);
    return previous + (target - previous) * alpha;
}

} // namespace

ModernBikeAxes compute_modern_bike_axes(
    float steering_normalized,
    float throttle_raw,
    float brake_raw,
    const BikeTuning& tuning,
    ModernBikeFilterState& filter) {
    const float steering =
        std::clamp(steering_normalized, -1.0f, 1.0f);
    const float throttle = std::clamp(throttle_raw, 0.0f, 1.0f);
    const float brake = std::clamp(brake_raw, 0.0f, 1.0f);

    const float steering_sign = steering < 0.0f ? -1.0f : 1.0f;
    float steering_shaped = steering_sign * std::pow(
        std::fabs(steering),
        tuning.steering_curve_exponent);

    const float high_speed_scale = std::max(
        tuning.high_speed_min_scale,
        1.0f - throttle * tuning.high_speed_sensitivity_falloff);
    steering_shaped *= high_speed_scale;

    filter.smoothed_steering = apply_low_pass(
        filter.smoothed_steering,
        steering_shaped,
        tuning.steering_stabilization);

    const float throttle_axis = std::clamp(throttle - brake, -1.0f, 1.0f);
    filter.smoothed_throttle_axis = apply_low_pass(
        filter.smoothed_throttle_axis,
        throttle_axis,
        tuning.camera_smoothing);

    ModernBikeAxes axes;
    axes.x = std::clamp(filter.smoothed_steering, -1.0f, 1.0f);
    axes.y = std::clamp(filter.smoothed_throttle_axis, -1.0f, 1.0f);
    return axes;
}

ModernBikeButtons compute_modern_bike_buttons(
    float throttle_raw,
    float brake_raw,
    ModernBikeFilterState& filter) {
    const float throttle = std::clamp(throttle_raw, 0.0f, 1.0f);
    const float brake = std::clamp(brake_raw, 0.0f, 1.0f);

    ModernBikeButtons buttons;
    // Accumulate trigger depth each poll and emit a press whenever a whole
    // unit has built up, so press rate tracks depth: 1.0 presses every poll,
    // 0.5 every other poll, 0.0 never.
    filter.throttle_phase += throttle;
    if (filter.throttle_phase >= 1.0f) {
        filter.throttle_phase -= 1.0f;
        buttons.accelerate = true;
    }
    filter.brake_phase += brake;
    if (filter.brake_phase >= 1.0f) {
        filter.brake_phase -= 1.0f;
        buttons.brake = true;
    }
    return buttons;
}

void initialize(const std::filesystem::path& data_directory) {
    std::lock_guard lock{state.mutex};
    state.settings_path = data_directory / "sote_controls.json";
    // Game root is the executable's directory, the same anchor
    // graphics_menu uses for sote_options.json. Deriving this from the
    // process working directory instead would put the file somewhere else
    // whenever the game is launched from another folder.
    state.tuning_ini_path = data_directory / "bike_tuning.ini";
    // The cached timestamp describes whatever file the previous path
    // pointed at. Drop it so the file named above is always read, rather
    // than being skipped because the two happen to share a timestamp.
    state.tuning_mtime_valid = false;

    std::ifstream input{state.settings_path};
    if (input.is_open()) {
        const std::string contents{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        auto slot_is_modern = [&contents](const char* key) {
            const std::string needle =
                std::string{"\""} + key + "\": \"Modern\"";
            return contents.find(needle) != std::string::npos;
        };
        state.schemes[static_cast<int>(SchemeSlot::OnFoot)] =
            slot_is_modern("on_foot") ? ControlScheme::Modern
                                      : ControlScheme::Classic;
        // Settings written before the split carried a single "scheme" key,
        // which only ever governed the bike.
        state.schemes[static_cast<int>(SchemeSlot::Bike)] =
            (slot_is_modern("bike") || slot_is_modern("scheme"))
                ? ControlScheme::Modern
                : ControlScheme::Classic;
    }

    if (!std::filesystem::exists(state.tuning_ini_path)) {
        write_default_tuning_ini_locked();
    }
    reload_tuning_if_changed_locked();
}

ControlScheme current_scheme(SchemeSlot slot) {
    std::lock_guard lock{state.mutex};
    return state.schemes[static_cast<int>(slot)];
}

void cycle_scheme(SchemeSlot slot, int direction) {
    std::lock_guard lock{state.mutex};
    ControlScheme& scheme = state.schemes[static_cast<int>(slot)];
    const int next = (static_cast<int>(scheme) + direction + 2) % 2;
    scheme = static_cast<ControlScheme>(next);
}

int scheme_legend(
    SchemeSlot slot,
    const LegendEntry** entries,
    int max_entries) {
    const ControlScheme scheme = current_scheme(slot);
    // Classic rows describe the pad-to-N64 mapping this port performs, which
    // is verifiable from frontend.cpp. The game assigns actions to N64
    // buttons through its own Controls preset, so those names are only used
    // where this port defines them (the Modern schemes) or where the action
    // is fixed by the port itself.
    // Keyboard bindings are fixed by frontend.cpp's keyboard-to-N64 map, so
    // they are the same in both schemes; only the pad column changes.
    static const LegendEntry on_foot_classic[] = {
        {"Move", "LS", "WASD"},
        {"Jump", "A", "Z"},
        {"Fire", "RT", "X"},
        {"Camera", "D-Up", "Q"},
        {"Doors", "X", "E"},
        {"Strafe", "LB", "E"},
        {"Aim", "LT", "C"},
        {"Jetpack", "Y", "J"},
        {"Weapons", "RB", "I"},
        {"Crouch", "B", "K"},
        {"Pause", "Start", "Enter"},
    };
    static const LegendEntry on_foot_modern[] = {
        {"Move", "LS", "WASD"},
        {"Jump", "A", "Z"},
        {"Fire", "RT", "X"},
        {"Camera", "D-Up", "Q"},
        {"Doors", "X", "E"},
        {"Strafe", "X", "E"},
        {"Aim", "RS", "C"},
        {"Jetpack", "Y", "J"},
        {"Weapons", "LB/RB", "I"},
        {"Crouch", "B", "K"},
        {"Pause", "Start", "Enter"},
    };
    // Bike bindings come from the game's own controller diagram for this
    // stage. Its Fire and Kick actions were never used, so nothing binds to
    // them in either scheme.
    static const LegendEntry bike_classic[] = {
        {"Steer", "LS", "WASD"},
        {"Accel", "A", "Z"},
        {"Brakes", "X", "X"},
        {"Camera", "RS", "L"},
        {"Pause", "Start", "Enter"},
    };
    static const LegendEntry bike_modern[] = {
        {"Accel", "RT", "Z"},
        {"Brakes", "LT", "X"},
        {"Steer", "LS", "WASD"},
        {"Camera", "RS", "L"},
        {"Pause", "Start", "Enter"},
    };
    const LegendEntry* source = nullptr;
    int count = 0;
    if (slot == SchemeSlot::OnFoot) {
        source = scheme == ControlScheme::Modern ? on_foot_modern
                                                 : on_foot_classic;
        count = 11;
    } else {
        source = scheme == ControlScheme::Modern ? bike_modern : bike_classic;
        count = 5;
    }
    if (count > max_entries) {
        count = max_entries;
    }
    for (int index = 0; index < count; ++index) {
        entries[index] = &source[index];
    }
    return count;
}

void persist() {
    std::lock_guard lock{state.mutex};
    if (state.settings_path.empty()) {
        return;
    }
    std::ofstream output{
        state.settings_path,
        std::ios_base::out | std::ios_base::trunc};
    if (!output.is_open()) {
        return;
    }
    auto name = [](ControlScheme scheme) {
        return scheme == ControlScheme::Modern ? "Modern" : "Classic";
    };
    output << "{\n  \"on_foot\": \""
           << name(state.schemes[static_cast<int>(SchemeSlot::OnFoot)])
           << "\",\n  \"bike\": \""
           << name(state.schemes[static_cast<int>(SchemeSlot::Bike)])
           << "\"\n}\n";
}

BikeTuning bike_tuning() {
    std::lock_guard lock{state.mutex};
    reload_tuning_if_changed_locked();
    return state.tuning;
}

bool bike_modern_scheme_active() {
    {
        std::lock_guard lock{state.mutex};
        if (state.schemes[static_cast<int>(SchemeSlot::Bike)] !=
            ControlScheme::Modern) {
            return false;
        }
    }
    return sote_is_bike_stage_active() != 0;
}

} // namespace sote::controls_menu
