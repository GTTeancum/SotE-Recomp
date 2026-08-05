// Host-side harness for the Classic/Modern control-scheme module.
//
// The bike scheme's steering math, the bike_tuning.ini reader, and the
// scheme persistence all run outside the recompiled guest, so they can be
// exercised without a ROM, a window, or a controller. tools/smoke_levels.ps1
// still covers the in-game side; this covers the parts that script cannot
// reach, because the headless smoke path feeds scripted inputs and never
// touches the SDL controller code.
//
// Built by CMake as the `controls_harness` target. Run it with no arguments;
// it exits non-zero on the first failing expectation set.

#include "controls_menu.hpp"
#include "recomp_hooks.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// The module calls this to decide whether the bike stage is live. Standing
// in for it here keeps the harness independent of the guest runtime.
int stub_bike_stage_active = 0;

int failures = 0;
int checks = 0;

void check(bool condition, const char* what) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL %s\n", what);
    }
}

void check_near(float actual, float expected, float tolerance,
                const char* what) {
    ++checks;
    if (std::fabs(actual - expected) > tolerance) {
        ++failures;
        std::printf(
            "FAIL %s (expected %.6f, got %.6f)\n", what, expected, actual);
    }
}

std::filesystem::path make_scratch_directory() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sote_controls_harness";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    return root;
}

void write_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output{path, std::ios_base::out | std::ios_base::trunc};
    output << text;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

using sote::controls_menu::BikeTuning;
using sote::controls_menu::ControlScheme;
using Slot = sote::controls_menu::SchemeSlot;
using sote::controls_menu::ModernBikeAxes;
using sote::controls_menu::ModernBikeFilterState;

// Steering is shaped by an exponent curve, then scaled down as the throttle
// rises, then low-passed. Each of those is checked separately so a
// regression names itself.
void test_steering_shape() {
    BikeTuning tuning{};
    // Isolate the curve: no falloff, no damping.
    tuning.steering_curve_exponent = 2.0f;
    tuning.high_speed_sensitivity_falloff = 0.0f;
    tuning.steering_stabilization = 0.0f;
    tuning.camera_smoothing = 0.0f;

    ModernBikeFilterState filter{};
    ModernBikeAxes axes =
        sote::controls_menu::compute_modern_bike_axes(
            0.5f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.25f, 1e-5f, "curve: 0.5^2 == 0.25");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        -0.5f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, -0.25f, 1e-5f, "curve: sign is preserved");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        1.0f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 1.0f, 1e-5f, "curve: full lock stays full lock");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        0.0f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.0f, 1e-6f, "curve: centered stick stays centered");

    // Out-of-range input must not escape the stick range; std::pow of a
    // negative base with a fractional exponent would also be a NaN source.
    filter = {};
    tuning.steering_curve_exponent = 1.6f;
    axes = sote::controls_menu::compute_modern_bike_axes(
        -4.0f, 0.0f, 0.0f, tuning, filter);
    check(!std::isnan(axes.x), "curve: over-range input is not NaN");
    check_near(axes.x, -1.0f, 1e-5f, "curve: over-range input clamps");
}

void test_high_speed_falloff() {
    BikeTuning tuning{};
    tuning.steering_curve_exponent = 1.0f;
    tuning.high_speed_sensitivity_falloff = 0.5f;
    tuning.high_speed_min_scale = 0.35f;
    tuning.steering_stabilization = 0.0f;
    tuning.camera_smoothing = 0.0f;

    ModernBikeFilterState filter{};
    ModernBikeAxes axes =
        sote::controls_menu::compute_modern_bike_axes(
            1.0f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 1.0f, 1e-5f, "falloff: idle throttle does not scale");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        1.0f, 1.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.5f, 1e-5f, "falloff: full throttle halves steering");

    // A falloff strong enough to invert or zero the steering must be held
    // at the floor instead.
    tuning.high_speed_sensitivity_falloff = 2.0f;
    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        1.0f, 1.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.35f, 1e-5f, "falloff: clamped at min_scale");
    check(axes.x > 0.0f, "falloff: steering never inverts");
}

void test_throttle_axis() {
    BikeTuning tuning{};
    tuning.camera_smoothing = 0.0f;
    tuning.steering_stabilization = 0.0f;

    ModernBikeFilterState filter{};
    ModernBikeAxes axes =
        sote::controls_menu::compute_modern_bike_axes(
            0.0f, 1.0f, 0.0f, tuning, filter);
    check_near(axes.y, 1.0f, 1e-5f, "throttle: RT drives the stick forward");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        0.0f, 0.0f, 1.0f, tuning, filter);
    check_near(axes.y, -1.0f, 1e-5f, "throttle: LT drives the stick back");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        0.0f, 1.0f, 1.0f, tuning, filter);
    check_near(axes.y, 0.0f, 1e-5f, "throttle: both triggers cancel");

    filter = {};
    axes = sote::controls_menu::compute_modern_bike_axes(
        0.0f, 0.75f, 0.25f, tuning, filter);
    check_near(axes.y, 0.5f, 1e-5f, "throttle: partial pulls subtract");
}

void test_low_pass() {
    BikeTuning tuning{};
    tuning.steering_curve_exponent = 1.0f;
    tuning.high_speed_sensitivity_falloff = 0.0f;
    tuning.steering_stabilization = 0.5f;
    tuning.camera_smoothing = 0.5f;

    ModernBikeFilterState filter{};
    ModernBikeAxes axes =
        sote::controls_menu::compute_modern_bike_axes(
            1.0f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.5f, 1e-5f, "damping: first poll moves halfway");
    axes = sote::controls_menu::compute_modern_bike_axes(
        1.0f, 0.0f, 0.0f, tuning, filter);
    check_near(axes.x, 0.75f, 1e-5f, "damping: second poll halves again");

    // Held input must converge, and a released stick must return to center
    // rather than parking at the last damped value.
    for (int poll = 0; poll < 60; ++poll) {
        axes = sote::controls_menu::compute_modern_bike_axes(
            1.0f, 0.0f, 0.0f, tuning, filter);
    }
    check_near(axes.x, 1.0f, 1e-3f, "damping: held input converges to full");
    for (int poll = 0; poll < 60; ++poll) {
        axes = sote::controls_menu::compute_modern_bike_axes(
            0.0f, 0.0f, 0.0f, tuning, filter);
    }
    check_near(axes.x, 0.0f, 1e-3f, "damping: released stick recenters");

    // A damping value outside 0..1 must not turn the filter into an
    // amplifier or run it backwards.
    tuning.steering_stabilization = 5.0f;
    filter = {};
    for (int poll = 0; poll < 10; ++poll) {
        axes = sote::controls_menu::compute_modern_bike_axes(
            1.0f, 0.0f, 0.0f, tuning, filter);
        check(axes.x >= -1.0f && axes.x <= 1.0f,
              "damping: absurd damping stays in range");
    }

    tuning.steering_stabilization = -5.0f;
    filter = {};
    for (int poll = 0; poll < 10; ++poll) {
        axes = sote::controls_menu::compute_modern_bike_axes(
            1.0f, 0.0f, 0.0f, tuning, filter);
        check(axes.x >= -1.0f && axes.x <= 1.0f,
              "damping: negative damping stays in range");
    }
}


// Throttle and brake drive the game's digital Accelerate/Brakes buttons on a
// duty cycle, so the press rate over time must track trigger depth.
void test_throttle_duty_cycle() {
    using sote::controls_menu::ModernBikeButtons;
    auto press_rate = [](float throttle, int polls) {
        ModernBikeFilterState filter{};
        int presses = 0;
        for (int poll = 0; poll < polls; ++poll) {
            const ModernBikeButtons b =
                sote::controls_menu::compute_modern_bike_buttons(
                    throttle, 0.0f, filter);
            if (b.accelerate) ++presses;
        }
        return static_cast<float>(presses) / static_cast<float>(polls);
    };
    check_near(press_rate(0.0f, 600), 0.0f, 1e-3f,
               "duty: released trigger never presses accelerate");
    check_near(press_rate(1.0f, 600), 1.0f, 1e-3f,
               "duty: full trigger presses every poll");
    check_near(press_rate(0.5f, 600), 0.5f, 0.01f,
               "duty: half trigger presses half the polls");
    check_near(press_rate(0.25f, 600), 0.25f, 0.01f,
               "duty: quarter trigger presses a quarter of the polls");
    check_near(press_rate(0.75f, 600), 0.75f, 0.01f,
               "duty: three-quarter trigger tracks proportionally");
    // Monotonic: more trigger must never mean fewer presses.
    float previous = -1.0f;
    for (int step = 0; step <= 10; ++step) {
        const float rate = press_rate(step / 10.0f, 600);
        check(rate >= previous - 1e-4f,
              "duty: press rate is monotonic in trigger depth");
        previous = rate;
    }
    // Brake runs on its own accumulator and must not steal throttle presses.
    ModernBikeFilterState filter{};
    int accel = 0, brake = 0;
    for (int poll = 0; poll < 600; ++poll) {
        const ModernBikeButtons b =
            sote::controls_menu::compute_modern_bike_buttons(
                1.0f, 0.5f, filter);
        if (b.accelerate) ++accel;
        if (b.brake) ++brake;
    }
    check(accel == 600, "duty: full throttle unaffected by braking");
    check(brake > 280 && brake < 320, "duty: half brake presses about half");
    // Out-of-range input must not run the accumulator away.
    ModernBikeFilterState wild{};
    for (int poll = 0; poll < 100; ++poll) {
        sote::controls_menu::compute_modern_bike_buttons(5.0f, -3.0f, wild);
    }
    check(wild.throttle_phase >= 0.0f && wild.throttle_phase < 1.0f,
          "duty: over-range trigger keeps the accumulator bounded");
}

void test_defaults_without_ini(const std::filesystem::path& scratch) {
    const std::filesystem::path directory = scratch / "defaults";
    std::filesystem::create_directories(directory);

    sote::controls_menu::initialize(directory);
    check(std::filesystem::exists(directory / "bike_tuning.ini"),
          "ini: a template is written when none exists");
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Classic,
          "scheme: defaults to Classic with no saved settings");

    // The written template must parse back to the same values it documents,
    // otherwise first-run players silently get different handling than a
    // fresh default.
    const BikeTuning written = sote::controls_menu::bike_tuning();
    const BikeTuning defaults{};
    check_near(written.steering_curve_exponent,
               defaults.steering_curve_exponent, 1e-4f,
               "ini: template round-trips steering_curve_exponent");
    check_near(written.high_speed_sensitivity_falloff,
               defaults.high_speed_sensitivity_falloff, 1e-4f,
               "ini: template round-trips high_speed_sensitivity_falloff");
    check_near(written.high_speed_min_scale,
               defaults.high_speed_min_scale, 1e-4f,
               "ini: template round-trips high_speed_min_scale");
    check_near(written.steering_stabilization,
               defaults.steering_stabilization, 1e-4f,
               "ini: template round-trips steering_stabilization");
    check_near(written.camera_smoothing,
               defaults.camera_smoothing, 1e-4f,
               "ini: template round-trips camera_smoothing");
    check(written.fire_button_bit == defaults.fire_button_bit,
          "ini: template round-trips fire_button");
}

void test_ini_parsing(const std::filesystem::path& scratch) {
    const std::filesystem::path directory = scratch / "parsing";
    std::filesystem::create_directories(directory);
    const std::filesystem::path ini = directory / "bike_tuning.ini";

    write_file(ini,
        "; leading comment\n"
        "  steering_curve_exponent = 2.5   # trailing comment\n"
        "high_speed_sensitivity_falloff=0.75\n"
        "\thigh_speed_min_scale\t=\t0.2\n"
        "steering_stabilization = 0.4\n"
        "camera_smoothing = 0.6\n"
        "fire_button = B\n"
        "not_a_setting = 1\n"
        "garbage line with no equals\n");
    sote::controls_menu::initialize(directory);

    BikeTuning tuning = sote::controls_menu::bike_tuning();
    check_near(tuning.steering_curve_exponent, 2.5f, 1e-5f,
               "ini: parses a value with surrounding whitespace");
    check_near(tuning.high_speed_sensitivity_falloff, 0.75f, 1e-5f,
               "ini: parses a value with no spaces around '='");
    check_near(tuning.high_speed_min_scale, 0.2f, 1e-5f,
               "ini: parses tab-separated entries");
    check_near(tuning.steering_stabilization, 0.4f, 1e-5f,
               "ini: parses steering_stabilization");
    check_near(tuning.camera_smoothing, 0.6f, 1e-5f,
               "ini: parses camera_smoothing");
    check(tuning.fire_button_bit == 0x4000,
          "ini: fire_button = B maps to the B bit");

    const struct {
        const char* token;
        uint16_t bit;
    } fire_buttons[] = {
        {"A", 0x8000}, {"B", 0x4000}, {"Z", 0x2000},
        {"L", 0x0020}, {"R", 0x0010},
        // Unknown tokens fall back to the documented default rather than
        // producing a button mask of zero, which would make RB do nothing.
        {"nonsense", 0x8000},
    };
    for (const auto& entry : fire_buttons) {
        write_file(ini, std::string{"fire_button = "} + entry.token + "\n");
        sote::controls_menu::initialize(directory);
        tuning = sote::controls_menu::bike_tuning();
        check(tuning.fire_button_bit == entry.bit,
              "ini: fire_button token maps to the expected bit");
    }

    // A file that sets only one key must leave the rest at defaults, not at
    // whatever the previously loaded file had.
    write_file(ini, "camera_smoothing = 0.9\n");
    sote::controls_menu::initialize(directory);
    tuning = sote::controls_menu::bike_tuning();
    const BikeTuning defaults{};
    check_near(tuning.camera_smoothing, 0.9f, 1e-5f,
               "ini: partial file applies its own key");
    check_near(tuning.steering_curve_exponent,
               defaults.steering_curve_exponent, 1e-5f,
               "ini: partial file resets unset keys to defaults");
    check(tuning.fire_button_bit == defaults.fire_button_bit,
          "ini: partial file resets fire_button to the default");
}

void test_ini_hot_reload(const std::filesystem::path& scratch) {
    const std::filesystem::path directory = scratch / "hot_reload";
    std::filesystem::create_directories(directory);
    const std::filesystem::path ini = directory / "bike_tuning.ini";

    write_file(ini, "steering_curve_exponent = 1.0\n");
    sote::controls_menu::initialize(directory);
    check_near(sote::controls_menu::bike_tuning().steering_curve_exponent,
               1.0f, 1e-5f, "reload: initial value is loaded");

    // Reload is mtime-driven. Filesystem timestamp granularity means a
    // rewrite within the same tick can compare equal, so move the stamp
    // explicitly rather than sleeping and hoping.
    write_file(ini, "steering_curve_exponent = 3.0\n");
    std::filesystem::last_write_time(
        ini,
        std::filesystem::last_write_time(ini) + std::chrono::seconds{5});
    check_near(sote::controls_menu::bike_tuning().steering_curve_exponent,
               3.0f, 1e-5f, "reload: an edited file is picked up");

    // An unchanged file must not be re-read on every poll.
    for (int poll = 0; poll < 5; ++poll) {
        check_near(
            sote::controls_menu::bike_tuning().steering_curve_exponent,
            3.0f, 1e-5f, "reload: repeated polls are stable");
    }

    // Deleting the file mid-session must not reset tuning or throw; the
    // last known-good values stay in effect.
    std::filesystem::remove(ini);
    check_near(sote::controls_menu::bike_tuning().steering_curve_exponent,
               3.0f, 1e-5f, "reload: a deleted file keeps the last values");
}

void test_scheme_cycle_and_persistence(
    const std::filesystem::path& scratch) {
    const std::filesystem::path directory = scratch / "scheme";
    std::filesystem::create_directories(directory);

    sote::controls_menu::initialize(directory);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Classic,
          "scheme: starts Classic");

    sote::controls_menu::cycle_scheme(Slot::Bike, 1);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Modern,
          "scheme: right cycles to Modern");
    sote::controls_menu::cycle_scheme(Slot::Bike, 1);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Classic,
          "scheme: right wraps back to Classic");
    sote::controls_menu::cycle_scheme(Slot::Bike, -1);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Modern,
          "scheme: left wraps to Modern");

    // Menu Left/Right alone must not write the file; only Apply does.
    check(!std::filesystem::exists(directory / "sote_controls.json"),
          "scheme: cycling alone does not persist");

    sote::controls_menu::persist();
    check(std::filesystem::exists(directory / "sote_controls.json"),
          "scheme: Apply writes the settings file");
    check(read_file(directory / "sote_controls.json").find("Modern") !=
              std::string::npos,
          "scheme: the written file names the selected scheme");

    sote::controls_menu::initialize(directory);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Modern,
          "scheme: a persisted Modern selection is reloaded");

    sote::controls_menu::cycle_scheme(Slot::Bike, 1);
    sote::controls_menu::persist();
    sote::controls_menu::initialize(directory);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Classic,
          "scheme: a persisted Classic selection is reloaded");

    // A truncated or hand-mangled settings file must not leave the game
    // unplayable; Classic is the safe fallback.
    write_file(directory / "sote_controls.json", "{ \"scheme\": ");
    sote::controls_menu::initialize(directory);
    check(sote::controls_menu::current_scheme(Slot::Bike) == ControlScheme::Classic,
          "scheme: a corrupt settings file falls back to Classic");
}

// The Modern bike path must engage only when the player picked Modern AND
// the recompiled bike controller is actually running. Getting either half
// wrong would rewrite the stick on foot, or in the wrong stage.
void test_modern_gating(const std::filesystem::path& scratch) {
    const std::filesystem::path directory = scratch / "gating";
    std::filesystem::create_directories(directory);
    sote::controls_menu::initialize(directory);

    stub_bike_stage_active = 0;
    check(!sote::controls_menu::bike_modern_scheme_active(),
          "gating: Classic off the bike is inactive");
    stub_bike_stage_active = 1;
    check(!sote::controls_menu::bike_modern_scheme_active(),
          "gating: Classic on the bike is inactive");

    sote::controls_menu::cycle_scheme(Slot::Bike, 1);
    stub_bike_stage_active = 0;
    check(!sote::controls_menu::bike_modern_scheme_active(),
          "gating: Modern off the bike is inactive");
    stub_bike_stage_active = 1;
    check(sote::controls_menu::bike_modern_scheme_active(),
          "gating: Modern on the bike is active");

    // Leaving the bike stage must release the override immediately.
    stub_bike_stage_active = 0;
    check(!sote::controls_menu::bike_modern_scheme_active(),
          "gating: leaving the bike releases the override");
}

} // namespace

extern "C" uint32_t sote_is_bike_stage_active(void) {
    return static_cast<uint32_t>(stub_bike_stage_active);
}

int main() {
    const std::filesystem::path scratch = make_scratch_directory();

    test_steering_shape();
    test_high_speed_falloff();
    test_throttle_axis();
    test_low_pass();
    test_throttle_duty_cycle();
    test_defaults_without_ini(scratch);
    test_ini_parsing(scratch);
    test_ini_hot_reload(scratch);
    test_scheme_cycle_and_persistence(scratch);
    test_modern_gating(scratch);

    std::error_code error;
    std::filesystem::remove_all(scratch, error);

    if (failures != 0) {
        std::printf("\ncontrols harness: %d of %d checks FAILED\n",
                    failures, checks);
        return 1;
    }
    std::printf("controls harness: all %d checks passed\n", checks);
    return 0;
}
