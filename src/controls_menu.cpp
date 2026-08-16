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
    ModernControlsTuning tuning{};
    std::filesystem::path settings_path;
    std::filesystem::path tuning_ini_path;
    std::filesystem::path legacy_bike_tuning_ini_path;
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

uint16_t parse_c_button(const std::string& token) {
    if (token == "C_UP" || token == "CU" || token == "UP") return 0x0008;
    if (token == "C_DOWN" || token == "CD" || token == "DOWN") return 0x0004;
    if (token == "C_LEFT" || token == "CL" || token == "LEFT") return 0x0002;
    if (token == "C_RIGHT" || token == "CR" || token == "RIGHT") return 0x0001;
    return 0x0008;
}

const char* fire_button_name(uint16_t bit) {
    switch (bit) {
        case 0x8000: return "A";
        case 0x4000: return "B";
        case 0x2000: return "Z";
        case 0x0020: return "L";
        case 0x0010: return "R";
        default: return "A";
    }
}

const char* c_button_name(uint16_t bit) {
    switch (bit) {
        case 0x0008: return "C_UP";
        case 0x0004: return "C_DOWN";
        case 0x0002: return "C_LEFT";
        case 0x0001: return "C_RIGHT";
        default: return "C_UP";
    }
}

bool parse_bool(const std::string& token) {
    return token == "1" || token == "true" || token == "TRUE" ||
        token == "yes" || token == "YES" || token == "on" || token == "ON";
}

float parse_float_clamped(
    const std::string& value,
    float fallback,
    float minimum,
    float maximum) {
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end == value.c_str() || !std::isfinite(parsed)) {
        return fallback;
    }
    return std::clamp(parsed, minimum, maximum);
}

void apply_tuning_value(
    ModernControlsTuning& tuning,
    const std::string& key,
    const std::string& value) {
    const ModernControlsTuning defaults{};
    if (key == "movement_deadzone") {
        tuning.movement_deadzone = parse_float_clamped(
            value, defaults.movement_deadzone, 0.0f, 1.0f);
    } else if (key == "movement_sensitivity") {
        tuning.movement_sensitivity = parse_float_clamped(
            value, defaults.movement_sensitivity, 0.1f, 3.0f);
    } else if (key == "aim_deadzone") {
        tuning.aim_deadzone = parse_float_clamped(
            value, defaults.aim_deadzone, 0.0f, 1.0f);
    } else if (key == "aim_sensitivity") {
        tuning.aim_sensitivity = parse_float_clamped(
            value, defaults.aim_sensitivity, 0.1f, 3.0f);
    } else if (key == "trigger_deadzone") {
        tuning.trigger_deadzone = parse_float_clamped(
            value, defaults.trigger_deadzone, 0.0f, 1.0f);
    } else if (key == "trigger_sensitivity") {
        tuning.trigger_sensitivity = parse_float_clamped(
            value, defaults.trigger_sensitivity, 0.1f, 3.0f);
    } else if (key == "look_snap_back_enabled") {
        tuning.look_snap_back_enabled = parse_bool(value);
    } else if (key == "look_snap_back_delay_seconds") {
        tuning.look_snap_back_delay_seconds = parse_float_clamped(
            value, defaults.look_snap_back_delay_seconds, 0.0f, 5.0f);
    } else if (key == "look_snap_back_duration_seconds") {
        tuning.look_snap_back_duration_seconds = parse_float_clamped(
            value, defaults.look_snap_back_duration_seconds, 0.0f, 1.0f);
    } else if (key == "look_snap_back_button") {
        tuning.look_snap_back_button_bit = parse_c_button(value);
    } else if (key == "bike_steering_curve_exponent" ||
               key == "steering_curve_exponent") {
        tuning.bike.steering_curve_exponent = parse_float_clamped(
            value, defaults.bike.steering_curve_exponent, 0.25f, 4.0f);
    } else if (key == "bike_high_speed_sensitivity_falloff" ||
               key == "high_speed_sensitivity_falloff") {
        tuning.bike.high_speed_sensitivity_falloff = parse_float_clamped(
            value, defaults.bike.high_speed_sensitivity_falloff, 0.0f, 1.0f);
    } else if (key == "bike_high_speed_min_scale" ||
               key == "high_speed_min_scale") {
        tuning.bike.high_speed_min_scale = parse_float_clamped(
            value, defaults.bike.high_speed_min_scale, 0.0f, 1.0f);
    } else if (key == "bike_steering_stabilization" ||
               key == "steering_stabilization") {
        tuning.bike.steering_stabilization = parse_float_clamped(
            value, defaults.bike.steering_stabilization, 0.0f, 0.99f);
    } else if (key == "bike_camera_smoothing" ||
               key == "camera_smoothing") {
        tuning.bike.camera_smoothing = parse_float_clamped(
            value, defaults.bike.camera_smoothing, 0.0f, 0.99f);
    } else if (key == "bike_fire_button" || key == "fire_button") {
        tuning.bike.fire_button_bit = parse_fire_button(value);
    }
}

// Minimal `key = value` INI reader, one entry per line, '#' or ';' starts
// a comment. Sections are ignored so testers can group settings visually.
void load_tuning_file_into_locked(
    const std::filesystem::path& path,
    ModernControlsTuning& tuning) {
    std::ifstream input{path};
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
        apply_tuning_value(tuning, key, value);
    }
}

void load_tuning_ini_locked() {
    ModernControlsTuning defaults{};
    state.tuning = defaults;

    load_tuning_file_into_locked(state.tuning_ini_path, state.tuning);
    if (!std::filesystem::exists(state.tuning_ini_path) &&
        std::filesystem::exists(state.legacy_bike_tuning_ini_path)) {
        load_tuning_file_into_locked(
            state.legacy_bike_tuning_ini_path,
            state.tuning);
    }
}

void write_default_tuning_ini_locked() {
    std::ofstream output{state.tuning_ini_path};
    if (!output.is_open()) {
        return;
    }
    const ModernControlsTuning d = state.tuning;
    output <<
        "; Shadows of the Empire Recompiled - Modern Controls tuning.\n"
        "; This file is hot-reloaded while the game is running. Values here\n"
        "; affect only the port's Modern control helpers; the in-game Controls\n"
        "; menu still chooses Classic or Modern per control category.\n"
        "; Lines beginning with ';' or '#' are comments.\n\n"

        "[General]\n"
        "; movement_deadzone is 0.0 to 1.0. 0.0 means no left-stick deadzone;\n"
        "; 1.0 means the left stick is fully ignored. Higher values prevent\n"
        "; drift but require more stick travel before Dash or the bike moves.\n"
        "movement_deadzone = " << d.movement_deadzone << "\n\n"

        "; movement_sensitivity is 0.1 to 3.0. 1.0 is normal. Lower values\n"
        "; reduce left-stick output after the deadzone. Higher values reach\n"
        "; full N64 stick sooner and can make movement/steering more abrupt.\n"
        "movement_sensitivity = " << d.movement_sensitivity << "\n\n"

        "; aim_deadzone is 0.0 to 1.0. 0.0 means no right-stick deadzone;\n"
        "; 1.0 means the right stick never sends C-button/camera input.\n"
        "; Increase this if the camera or aiming drifts when the stick rests.\n"
        "aim_deadzone = " << d.aim_deadzone << "\n\n"

        "; aim_sensitivity is 0.1 to 3.0. 1.0 is normal. Because the N64 C\n"
        "; buttons are digital, this changes how easily right-stick motion\n"
        "; crosses the C-button activation point rather than true analog speed.\n"
        "aim_sensitivity = " << d.aim_sensitivity << "\n\n"

        "; trigger_deadzone is 0.0 to 1.0. 0.0 means LT/RT react instantly;\n"
        "; 1.0 means triggers are ignored. Higher values help worn triggers\n"
        "; stop firing, aiming, accelerating, or braking by accident.\n"
        "trigger_deadzone = " << d.trigger_deadzone << "\n\n"

        "; trigger_sensitivity is 0.1 to 3.0. 1.0 is normal. Lower values\n"
        "; require a deeper pull after the deadzone; higher values reach full\n"
        "; trigger strength sooner. Bike throttle/brake duty-cycle uses this.\n"
        "trigger_sensitivity = " << d.trigger_sensitivity << "\n\n"

        "[LookSnapBack]\n"
        "; look_snap_back_enabled is 0 or 1. 0 disables it. 1 taps a C-button\n"
        "; after the right stick returns to center. This is experimental and\n"
        "; defaults off because the original game did not have a modern stick.\n"
        "look_snap_back_enabled = 0\n\n"

        "; look_snap_back_delay_seconds is 0.0 to 5.0. It is how long the\n"
        "; right stick must stay centered before the snap-back tap starts.\n"
        "; Lower values recenter sooner; higher values wait longer.\n"
        "look_snap_back_delay_seconds = "
            << d.look_snap_back_delay_seconds << "\n\n"

        "; look_snap_back_duration_seconds is 0.0 to 1.0. It is how long the\n"
        "; snap-back C-button is held. Too short may do nothing; too long can\n"
        "; feel like an extra camera command after aiming.\n"
        "look_snap_back_duration_seconds = "
            << d.look_snap_back_duration_seconds << "\n\n"

        "; look_snap_back_button accepts C_UP, C_DOWN, C_LEFT, or C_RIGHT.\n"
        "; C_UP is the default candidate for looking forward/recentering.\n"
        "look_snap_back_button = " << c_button_name(d.look_snap_back_button_bit)
            << "\n\n"

        "[SpeederBike]\n"
        "; bike_fire_button accepts A, B, Z, L, or R. It is only used if the\n"
        "; bike sequence ever needs a separate Modern fire mapping. The current\n"
        "; Modern bike path leaves Fire/Kick unbound because the game did not\n"
        "; use them in the bike stage.\n"
        "bike_fire_button = " << fire_button_name(d.bike.fire_button_bit)
            << "\n\n"

        "; bike_steering_curve_exponent is 0.25 to 4.0. 1.0 is linear.\n"
        "; Higher values soften small steering corrections near center while\n"
        "; keeping full lock at the edge. Lower values make steering twitchier.\n"
        "bike_steering_curve_exponent = "
            << d.bike.steering_curve_exponent << "\n\n"

        "; bike_high_speed_sensitivity_falloff is 0.0 to 1.0. 0.0 keeps bike\n"
        "; steering equally strong at all speeds. Higher values reduce steering\n"
        "; as RT throttle increases, making full-speed steering calmer.\n"
        "bike_high_speed_sensitivity_falloff = "
            << d.bike.high_speed_sensitivity_falloff << "\n\n"

        "; bike_high_speed_min_scale is 0.0 to 1.0. It is the floor for the\n"
        "; high-speed steering reduction above. Lower values allow weaker\n"
        "; steering at full throttle; higher values preserve more authority.\n"
        "bike_high_speed_min_scale = " << d.bike.high_speed_min_scale << "\n\n"

        "; bike_steering_stabilization is 0.0 to 0.99. 0.0 applies steering\n"
        "; instantly. Higher values smooth steering changes over more polls,\n"
        "; reducing jitter at the cost of extra steering latency.\n"
        "bike_steering_stabilization = "
            << d.bike.steering_stabilization << "\n\n"

        "; bike_camera_smoothing is 0.0 to 0.99. 0.0 applies the trigger-based\n"
        "; throttle/brake stick axis instantly. Higher values smooth those\n"
        "; changes so the bike camera/throttle axis is less abrupt.\n"
        "bike_camera_smoothing = " << d.bike.camera_smoothing << "\n";
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
        "[sote][controls-menu] reloaded CONTROLS_MODERN.INI "
        "(move_deadzone=%.2f move_sens=%.2f aim_deadzone=%.2f "
        "aim_sens=%.2f trigger_deadzone=%.2f trigger_sens=%.2f "
        "snap=%d bike_curve=%.2f bike_falloff=%.2f "
        "bike_min_scale=%.2f bike_stabilization=%.2f "
        "bike_camera_smoothing=%.2f bike_fire_bit=%04X)\n",
        state.tuning.movement_deadzone,
        state.tuning.movement_sensitivity,
        state.tuning.aim_deadzone,
        state.tuning.aim_sensitivity,
        state.tuning.trigger_deadzone,
        state.tuning.trigger_sensitivity,
        state.tuning.look_snap_back_enabled ? 1 : 0,
        state.tuning.bike.steering_curve_exponent,
        state.tuning.bike.high_speed_sensitivity_falloff,
        state.tuning.bike.high_speed_min_scale,
        state.tuning.bike.steering_stabilization,
        state.tuning.bike.camera_smoothing,
        state.tuning.bike.fire_button_bit);
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
    state.tuning_ini_path = data_directory / "CONTROLS_MODERN.INI";
    state.legacy_bike_tuning_ini_path = data_directory / "bike_tuning.ini";
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
        state.tuning = {};
        if (std::filesystem::exists(state.legacy_bike_tuning_ini_path)) {
            load_tuning_file_into_locked(
                state.legacy_bike_tuning_ini_path,
                state.tuning);
        }
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

ModernControlsTuning modern_controls_tuning() {
    std::lock_guard lock{state.mutex};
    reload_tuning_if_changed_locked();
    return state.tuning;
}

BikeTuning bike_tuning() {
    return modern_controls_tuning().bike;
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
