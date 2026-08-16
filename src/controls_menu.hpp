#pragma once

#include <cstdint>
#include <filesystem>

namespace sote::controls_menu {

enum class ControlScheme : int {
    Classic,
    Modern,
};

// On-foot and speeder-bike schemes are chosen independently: the stages use
// different control code, so one being Modern says nothing about the other.
enum class SchemeSlot : int {
    OnFoot,
    Bike,
    Count,
};

// Speeder-bike-specific tuning. Not exposed in the in-game menu on purpose
// (per design: only Classic/Modern is player-facing). Tune by editing
// CONTROLS_MODERN.INI in the game root; hot-reloaded at runtime.
struct BikeTuning {
    // Steering: input^curve_exponent (1.0 = linear passthrough).
    float steering_curve_exponent = 1.6f;
    // Multiplies steering output by (1 - throttle * high_speed_falloff),
    // clamped to high_speed_min_scale, so full throttle doesn't make the
    // bike twitchy at full lean.
    float high_speed_sensitivity_falloff = 0.45f;
    float high_speed_min_scale = 0.35f;
    // Low-pass filter coefficient applied to the outgoing analog stick
    // value per poll (0 = no stabilization, higher = more damping).
    float steering_stabilization = 0.18f;
    // Low-pass filter coefficient applied to the C-buttons/camera-relevant
    // right-stick output derived from throttle changes, to avoid abrupt
    // camera snaps under Modern's trigger-driven throttle.
    float camera_smoothing = 0.25f;
    // Which native N64 button the recompiled bike controller reads as
    // "fire". UNVERIFIED — defaulted to A. If the original bike sequence
    // actually reads a different button for firing, override this in the
    // INI (fire_button = A|B|Z|L|R) rather than trusting this default.
    uint16_t fire_button_bit = 0x8000; // n64_a
};

struct ModernControlsTuning {
    // Left stick deadzone before any movement/steering is emitted.
    float movement_deadzone = 8000.0f / 32768.0f;
    // Left stick scalar after deadzone normalization.
    float movement_sensitivity = 1.0f;
    // Right stick deadzone before any C-button/camera input is emitted.
    float aim_deadzone = 12000.0f / 32768.0f;
    // Right stick scalar used to lower or raise the effective C-button
    // activation point.
    float aim_sensitivity = 1.0f;
    // Trigger deadzone before LT/RT are treated as pressed.
    float trigger_deadzone = 8000.0f / 32767.0f;
    // Trigger scalar after deadzone normalization.
    float trigger_sensitivity = 1.0f;
    // Optional C-button tap after the right stick returns to neutral. Off by
    // default because this is game-feel tuning, not verified original logic.
    bool look_snap_back_enabled = false;
    float look_snap_back_delay_seconds = 0.35f;
    float look_snap_back_duration_seconds = 0.08f;
    uint16_t look_snap_back_button_bit = 0x0008; // n64_cu
    BikeTuning bike{};
};

// Per-poll low-pass carry for the Modern bike scheme. Owned by the caller
// so the math below stays pure and testable.
struct ModernBikeFilterState {
    float smoothed_steering = 0.0f;
    float smoothed_throttle_axis = 0.0f;
    // Phase accumulators for the duty-cycled accelerate/brake buttons.
    float throttle_phase = 0.0f;
    float brake_phase = 0.0f;
};

// The bike's Accelerate and Brakes are digital buttons in the game's own
// control table, so a trigger cannot be passed through as a level. Pressing
// the button on a duty cycle proportional to trigger depth makes the
// *average* thrust analog: a light pull accelerates gently, a full pull
// accelerates at the game's full rate.
struct ModernBikeButtons {
    bool accelerate = false;
    bool brake = false;
};

ModernBikeButtons compute_modern_bike_buttons(
    float throttle_raw,
    float brake_raw,
    ModernBikeFilterState& filter);

// Normalized N64 stick output, -1..1 on each axis. The caller scales this
// by its own stick range.
struct ModernBikeAxes {
    float x = 0.0f;
    float y = 0.0f;
};

// Modern speeder-bike scheme: RT throttle, LT brake, left stick steers.
// This assumes Classic's bike control reads the left stick's Y axis as
// throttle-forward/brake-back and X axis as steering -- i.e. Modern's whole
// point is moving throttle/brake off the stick and onto triggers, leaving
// the stick to steering only. That assumption is NOT verified against the
// original bike controller logic; if the bike doesn't respond to speed
// changes correctly under Modern, this is the first place to check.
// steering_normalized is post-deadzone, -1..1; throttle_raw and brake_raw
// are 0..1 trigger positions.
ModernBikeAxes compute_modern_bike_axes(
    float steering_normalized,
    float throttle_raw,
    float brake_raw,
    const BikeTuning& tuning,
    ModernBikeFilterState& filter);

void initialize(const std::filesystem::path& data_directory);

ControlScheme current_scheme(SchemeSlot slot);
void cycle_scheme(SchemeSlot slot, int direction);
void persist();

// One row of the control legend: physical input on the left, what it does on
// the right. Strings are already prefixed for the guest text renderer.
struct LegendEntry {
    const char* action;
    const char* pad;
    const char* key;
};

// Fills entries with the legend for a slot's current scheme and returns the
// count written (at most max_entries).
int scheme_legend(
    SchemeSlot slot,
    const LegendEntry** entries,
    int max_entries);

ModernControlsTuning modern_controls_tuning();
BikeTuning bike_tuning();
// True only when scheme == Modern AND the recompiled bike controller has
// run within the last couple of vertical-interrupt frames (see
// sote_is_bike_stage_active in recomp_hooks.h).
bool bike_modern_scheme_active();

} // namespace sote::controls_menu
