#define SDL_MAIN_HANDLED

#include "frontend.hpp"
#include "controls_menu.hpp"
#include "recomp_hooks.h"
#include "graphics_menu.hpp"
#include "menu_skin.hpp"
#include "control_bindings.hpp"
#include "hd_audio.hpp"
#include "hd_music.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <Windows.h>
#include <SDL.h>

namespace sote::frontend {
namespace {

constexpr uint16_t n64_a = 0x8000;
constexpr uint16_t n64_b = 0x4000;
constexpr uint16_t n64_z = 0x2000;
constexpr uint16_t n64_start = 0x1000;
constexpr uint16_t n64_du = 0x0800;
constexpr uint16_t n64_dd = 0x0400;
constexpr uint16_t n64_dl = 0x0200;
constexpr uint16_t n64_dr = 0x0100;
constexpr uint16_t n64_l = 0x0020;
constexpr uint16_t n64_r = 0x0010;
constexpr uint16_t n64_cu = 0x0008;
constexpr uint16_t n64_cd = 0x0004;
constexpr uint16_t n64_cl = 0x0002;
constexpr uint16_t n64_cr = 0x0001;

constexpr float n64_stick_scale = 80.0f / 127.0f;

std::atomic<uint32_t> input_snapshot{0};
std::atomic<uint32_t> scripted_input_snapshot{0};
std::atomic<int> diagnostic_vi{0};
std::atomic<bool> physical_input_enabled{true};
std::atomic<bool> audio_output_enabled{true};
std::atomic<uint64_t> movie_playback_token{0};
std::atomic<bool> movie_guest_advance{false};
std::atomic<bool> movie_guest_start{false};
std::atomic<bool> swallow_movie_skip{false};
SDL_GameController* controller = nullptr;
bool initialized = false;

struct DiagnosticPadPulse {
    int start_vi = 0;
    int duration = 0;
    unsigned buttons = 0;
    int lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
std::vector<DiagnosticPadPulse> diagnostic_pad_pulses;
bool diagnostic_pad_loaded = false;

struct DiagnosticMousePulse {
    int start_vi = 0;
    int duration = 0;
    unsigned buttons = 0;
    int dx = 0, dy = 0;
};
std::vector<DiagnosticMousePulse> diagnostic_mouse_pulses;
bool diagnostic_mouse_loaded = false;

struct DiagnosticKeyPulse {
    int start_vi = 0;
    int duration = 0;
    int virtual_key = 0;
};
std::vector<DiagnosticKeyPulse> diagnostic_key_pulses;
bool diagnostic_keys_loaded = false;

bool diagnostic_keys_enabled() {
    return std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr &&
        std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_KEYS") != nullptr;
}

void load_diagnostic_key_pulses() {
    diagnostic_keys_loaded = true;
    diagnostic_key_pulses.clear();
    const char* specification = std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_KEYS");
    if (!diagnostic_keys_enabled() || specification == nullptr) return;
    // startVI:duration:Win32-virtual-key, comma separated. This injects only
    // a process-local snapshot; no host keyboard event is sent.
    const std::string script(specification);
    size_t begin = 0;
    while (begin < script.size()) {
        const size_t end = script.find(',', begin);
        const std::string item = script.substr(begin, end - begin);
        DiagnosticKeyPulse pulse;
        if (std::sscanf(item.c_str(), "%d:%d:%d", &pulse.start_vi,
                &pulse.duration, &pulse.virtual_key) == 3 &&
            pulse.start_vi >= 0 && pulse.duration > 0 &&
            pulse.virtual_key >= 0 && pulse.virtual_key < 256) {
            diagnostic_key_pulses.push_back(pulse);
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    std::printf("[sote][diagnostic-keys] loaded %zu in-process pulses\n",
        diagnostic_key_pulses.size());
    std::fflush(stdout);
}

void diagnostic_keys_at(int vi, control_bindings::PhysicalInput& raw) {
    for (const auto& pulse : diagnostic_key_pulses) {
        if (vi < pulse.start_vi || vi >= pulse.start_vi + pulse.duration)
            continue;
        if (pulse.virtual_key != 0) raw.keys[pulse.virtual_key] = 1;
        if (vi == pulse.start_vi) {
            static int last_reported_vi = -1;
            if (last_reported_vi != vi) {
                std::printf("[sote][diagnostic-keys] VI=%d key=%d\n",
                    vi, pulse.virtual_key);
                std::fflush(stdout);
                last_reported_vi = vi;
            }
        }
    }
}

bool diagnostic_mouse_enabled() {
    return std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr &&
        std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_MOUSE") != nullptr;
}

void load_diagnostic_mouse_pulses() {
    diagnostic_mouse_loaded = true;
    diagnostic_mouse_pulses.clear();
    const char* specification = std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_MOUSE");
    if (!diagnostic_mouse_enabled() || specification == nullptr) return;
    // startVI:duration:mouse-button-mask:relative-X:relative-Y, comma separated.
    // The snapshots stay inside this game process and never move the host cursor.
    const std::string script(specification);
    size_t begin = 0;
    while (begin < script.size()) {
        const size_t end = script.find(',', begin);
        const std::string item = script.substr(begin, end - begin);
        DiagnosticMousePulse pulse;
        if (std::sscanf(item.c_str(), "%d:%d:%x:%d:%d",
                &pulse.start_vi, &pulse.duration, &pulse.buttons,
                &pulse.dx, &pulse.dy) == 5 &&
            pulse.start_vi >= 0 && pulse.duration > 0) {
            diagnostic_mouse_pulses.push_back(pulse);
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    std::printf("[sote][diagnostic-mouse] loaded %zu in-process pulses\n",
        diagnostic_mouse_pulses.size());
    std::fflush(stdout);
}

void diagnostic_mouse_at(int vi, control_bindings::PhysicalInput& raw) {
    static int last_delta_vi = -1;
    for (const auto& pulse : diagnostic_mouse_pulses) {
        if (vi < pulse.start_vi || vi >= pulse.start_vi + pulse.duration)
            continue;
        if (pulse.buttons & 1U) raw.keys[VK_LBUTTON] = 1;
        if (pulse.buttons & 2U) raw.keys[VK_RBUTTON] = 1;
        if (pulse.buttons & 4U) raw.keys[VK_MBUTTON] = 1;
        if (vi != last_delta_vi) {
            sote::modern_controls::add_mouse_delta(pulse.dx, pulse.dy);
            if (vi == pulse.start_vi) {
                std::printf("[sote][diagnostic-mouse] VI=%d buttons=%X "
                    "delta=%d,%d\n", vi, pulse.buttons, pulse.dx, pulse.dy);
                std::fflush(stdout);
            }
        }
    }
    last_delta_vi = vi;
}

bool diagnostic_pad_enabled() {
    return std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr &&
        std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_PAD") != nullptr;
}

void load_diagnostic_pad_pulses() {
    diagnostic_pad_loaded = true;
    diagnostic_pad_pulses.clear();
    const char* specification = std::getenv("SOTE_DIAGNOSTIC_PHYSICAL_PAD");
    if (!diagnostic_pad_enabled() || specification == nullptr) return;
    // startVI:duration:SDL-button-mask:LX:LY:RX:RY:LT:RT, repeated with commas.
    // This is an in-process input snapshot; no host device is created.
    const std::string script(specification);
    size_t begin = 0;
    while (begin < script.size()) {
        const size_t end = script.find(',', begin);
        const std::string item = script.substr(begin, end - begin);
        DiagnosticPadPulse pulse;
        if (std::sscanf(item.c_str(), "%d:%d:%x:%d:%d:%d:%d:%d:%d",
                &pulse.start_vi, &pulse.duration, &pulse.buttons,
                &pulse.lx, &pulse.ly, &pulse.rx, &pulse.ry,
                &pulse.lt, &pulse.rt) == 9 &&
            pulse.start_vi >= 0 && pulse.duration > 0) {
            diagnostic_pad_pulses.push_back(pulse);
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    std::printf("[sote][diagnostic-pad] loaded %zu in-process pulses\n",
        diagnostic_pad_pulses.size());
    std::fflush(stdout);
}

control_bindings::PhysicalInput diagnostic_pad_at(int vi) {
    control_bindings::PhysicalInput raw;
    raw.connected = true;
    raw.instance = 0x534f5445;
    static int last_reported_vi = -1;
    auto axis = [](int value) {
        return static_cast<Sint16>(std::clamp(value, -32768, 32767));
    };
    for (const auto& pulse : diagnostic_pad_pulses) {
        if (vi < pulse.start_vi || vi >= pulse.start_vi + pulse.duration)
            continue;
        if (vi == pulse.start_vi && vi != last_reported_vi) {
            last_reported_vi = vi;
            std::printf("[sote][diagnostic-pad] VI=%d buttons=%08X "
                "axes=%d,%d,%d,%d,%d,%d\n", vi, pulse.buttons,
                pulse.lx, pulse.ly, pulse.rx, pulse.ry, pulse.lt, pulse.rt);
            std::fflush(stdout);
        }
        for (size_t button = 0; button < raw.buttons.size() && button < 32;
             ++button) {
            if (pulse.buttons & (1U << button)) raw.buttons[button] = 1;
        }
        raw.axes[SDL_CONTROLLER_AXIS_LEFTX] = axis(pulse.lx);
        raw.axes[SDL_CONTROLLER_AXIS_LEFTY] = axis(pulse.ly);
        raw.axes[SDL_CONTROLLER_AXIS_RIGHTX] = axis(pulse.rx);
        raw.axes[SDL_CONTROLLER_AXIS_RIGHTY] = axis(pulse.ry);
        raw.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = axis(pulse.lt);
        raw.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = axis(pulse.rt);
    }
    return raw;
}

std::mutex audio_mutex;
SDL_AudioDeviceID audio_device = 0;
SDL_AudioSpec obtained_audio{};
uint32_t source_frequency = 0;
SDL_AudioStream* audio_stream = nullptr;
double muted_queued_frames = 0.0;
std::chrono::steady_clock::time_point muted_audio_clock{};
bool muted_audio_clock_initialized = false;
std::vector<int16_t> swapped_samples;
std::vector<uint8_t> converted_samples;
std::atomic<bool> first_audio_buffer{false};
std::atomic<bool> first_non_silent_buffer{false};
uint64_t audio_buffer_count = 0;
uint64_t audio_sample_count = 0;
ULONGLONG first_audio_host_tick = 0;
FILE* audio_dump_file = nullptr;
bool audio_dump_checked = false;

// Only poll_input writes this sampled state. Rebinding transforms a copy,
// so original and replacement sources can never feed back into one another.
control_bindings::PhysicalInput mapped_input;
bool key_down(int virtual_key) {
    return virtual_key > 0 && virtual_key < 256 && mapped_input.keys[virtual_key] != 0;
}
Sint16 mapped_axis(SDL_GameController*, SDL_GameControllerAxis axis) {
    return mapped_input.axes[static_cast<size_t>(axis)];
}
Uint8 mapped_button(SDL_GameController*, SDL_GameControllerButton button) {
    return mapped_input.buttons[static_cast<size_t>(button)];
}

void update_muted_audio_clock_locked() {
    if (audio_output_enabled.load(std::memory_order_relaxed) ||
        source_frequency == 0 || !muted_audio_clock_initialized) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - muted_audio_clock;
    muted_queued_frames = std::max(
        0.0,
        muted_queued_frames - elapsed.count() * source_frequency);
    muted_audio_clock = now;
}

// Modern speeder-bike scheme: RT throttle, LT brake, left stick steering,
// and LB/RB for the native left/right ram actions.
// The curve/falloff/stabilization math itself lives in controls_menu so it
// can be exercised without SDL by tools/controls_harness.
sote::controls_menu::ModernBikeFilterState modern_bike_filter;
bool right_stick_was_active = false;
bool look_snap_back_queued = false;
bool look_snap_back_active = false;
std::chrono::steady_clock::time_point right_stick_release_time{};
std::chrono::steady_clock::time_point look_snap_back_end_time{};

float raw_axis_to_unit(Sint16 value) {
    const float normalized =
        value >= 0 ? value / 32767.0f : value / 32768.0f;
    return std::clamp(normalized, -1.0f, 1.0f);
}

float apply_deadzone_and_sensitivity(
    float normalized,
    float deadzone,
    float sensitivity) {
    if (deadzone >= 1.0f) {
        return 0.0f;
    }
    const float clamped_deadzone = std::clamp(deadzone, 0.0f, 0.999f);
    const float clamped_sensitivity = std::max(0.1f, sensitivity);
    const float magnitude = std::fabs(normalized);
    if (magnitude <= clamped_deadzone) {
        return 0.0f;
    }

    const float sign = normalized < 0.0f ? -1.0f : 1.0f;
    const float scaled =
        ((magnitude - clamped_deadzone) / (1.0f - clamped_deadzone)) *
        clamped_sensitivity;
    return sign * std::clamp(scaled, 0.0f, 1.0f);
}

float normalize_trigger(
    Sint16 value,
    const sote::controls_menu::ModernControlsTuning& tuning) {
    const float normalized = std::max(0.0f, value / 32767.0f);
    return std::max(
        0.0f,
        apply_deadzone_and_sensitivity(
            normalized,
            tuning.trigger_deadzone,
            tuning.trigger_sensitivity));
}

bool aim_direction_pressed(
    Sint16 value,
    float deadzone,
    float sensitivity,
    bool positive) {
    if (deadzone >= 1.0f) {
        return false;
    }
    float effective_deadzone = std::clamp(deadzone, 0.0f, 1.0f);
    effective_deadzone = std::clamp(
        effective_deadzone / std::max(0.1f, sensitivity),
        0.0f,
        0.999f);
    const float normalized = raw_axis_to_unit(value);
    return positive ? normalized > effective_deadzone
                    : normalized < -effective_deadzone;
}

void apply_modern_bike_scheme(
    SDL_GameController* pad,
    uint16_t& buttons,
    float& x,
    float& y,
    const sote::controls_menu::ModernControlsTuning& controls) {
    const sote::controls_menu::BikeTuning& tuning = controls.bike;

    float throttle_raw = normalize_trigger(
        mapped_axis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
        controls);
    float brake_raw = normalize_trigger(
        mapped_axis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT),
        controls);

    // Triggers are unreachable from the scripted-input diagnostics, so allow
    // a forced level for testing the Modern path without a physical pad.
    if (const char* forced = std::getenv("SOTE_FORCE_BIKE_THROTTLE")) {
        throttle_raw = std::clamp(
            static_cast<float>(std::atof(forced)), 0.0f, 1.0f);
    }
    if (const char* forced = std::getenv("SOTE_FORCE_BIKE_BRAKE")) {
        brake_raw = std::clamp(
            static_cast<float>(std::atof(forced)), 0.0f, 1.0f);
    }

    const Sint16 raw_steer_x =
        mapped_axis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    const float steering = apply_deadzone_and_sensitivity(
        raw_axis_to_unit(raw_steer_x),
        controls.movement_deadzone,
        controls.movement_sensitivity);

    const sote::controls_menu::ModernBikeAxes axes =
        sote::controls_menu::compute_modern_bike_axes(
            steering,
            throttle_raw,
            brake_raw,
            tuning,
            modern_bike_filter);

    // Steering stays on the stick, which the bike genuinely reads as an
    // analog value. Throttle and brake drive the game's actual Accelerate
    // and Brakes buttons from the current native preset on a duty cycle,
    // since those inputs are digital and cannot take a level.
    x = axes.x * n64_stick_scale;
    // The native bike camera also reads vertical stick movement. Feed it
    // the filtered throttle/brake axis without relying on that axis for speed.
    y = axes.y * n64_stick_scale;

    const sote::controls_menu::ModernBikeButtons bike_buttons =
        sote::controls_menu::compute_modern_bike_buttons(
            throttle_raw, brake_raw, modern_bike_filter);
    if (bike_buttons.accelerate) {
        buttons |= control_bindings::bike_button(true);
    }
    if (bike_buttons.brake) {
        buttons |= control_bindings::bike_button(false);
    }

    // LB/RB retain the native L/R ram actions from the common pad mapping.
    // The bike has no blaster fire action.
}

bool process_owns_foreground_window() {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }
    DWORD foreground_process = 0;
    GetWindowThreadProcessId(foreground, &foreground_process);
    return foreground_process == GetCurrentProcessId();
}

float normalize_axis(
    Sint16 value,
    const sote::controls_menu::ModernControlsTuning& tuning) {
    return apply_deadzone_and_sensitivity(
        raw_axis_to_unit(value),
        tuning.movement_deadzone,
        tuning.movement_sensitivity) * n64_stick_scale;
}

void find_controller() {
    if (controller != nullptr &&
        SDL_GameControllerGetAttached(controller) == SDL_TRUE) {
        return;
    }
    if (controller != nullptr) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller != nullptr) {
                std::printf(
                    "[sote] controller connected: %s\n",
                    SDL_GameControllerName(controller));
                std::fflush(stdout);
                return;
            }
        }
    }
}

void close_audio_locked() {
    if (audio_stream != nullptr) {
        SDL_FreeAudioStream(audio_stream);
        audio_stream = nullptr;
    }
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    obtained_audio = {};
}

bool open_audio_locked(uint32_t frequency) {
    close_audio_locked();
    if (!initialized || frequency == 0 ||
        !audio_output_enabled.load(std::memory_order_relaxed)) {
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(frequency);
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 1024;

    audio_device = SDL_OpenAudioDevice(
        nullptr,
        0,
        &desired,
        &obtained_audio,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
            SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (audio_device == 0) {
        std::fprintf(
            stderr,
            "[sote] SDL audio open failed: %s\n",
            SDL_GetError());
        return false;
    }

    if (obtained_audio.format != AUDIO_S16LSB ||
        obtained_audio.channels != 2) {
        std::fprintf(
            stderr,
            "[sote] unsupported audio device format=%u channels=%u\n",
            obtained_audio.format,
            obtained_audio.channels);
        close_audio_locked();
        return false;
    }

    if (obtained_audio.freq != static_cast<int>(frequency)) {
        audio_stream = SDL_NewAudioStream(
            AUDIO_S16LSB,
            2,
            static_cast<int>(frequency),
            obtained_audio.format,
            obtained_audio.channels,
            obtained_audio.freq);
        if (audio_stream == nullptr) {
            std::fprintf(
                stderr,
                "[sote] SDL resampler creation failed: %s\n",
                SDL_GetError());
            close_audio_locked();
            return false;
        }
    }

    audio_buffer_count = 0;
    SDL_PauseAudioDevice(audio_device, 0);
    std::printf(
        "[sote] audio device opened: source=%u output=%d Hz\n",
        frequency,
        obtained_audio.freq);
    std::fflush(stdout);
    return true;
}

} // namespace

bool initialize() {
    if (initialized) {
        return true;
    }
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(
            stderr,
            "[sote] SDL frontend initialization failed: %s\n",
            SDL_GetError());
        return false;
    }
    initialized = true;
    SDL_GameControllerEventState(SDL_ENABLE);
    load_diagnostic_pad_pulses();
    if (!diagnostic_pad_enabled() && !diagnostic_mouse_enabled() &&
        !diagnostic_keys_enabled())
        find_controller();
    return true;
}

void shutdown() {
    {
        std::lock_guard lock{audio_mutex};
        close_audio_locked();
        if (audio_dump_file != nullptr) {
            std::fclose(audio_dump_file);
            audio_dump_file = nullptr;
        }
    }
    if (controller != nullptr) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
    if (initialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
        initialized = false;
    }
}

void set_audio_enabled(bool enabled) {
    audio_output_enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        std::lock_guard lock{audio_mutex};
        close_audio_locked();
    }
}

void set_movie_playback(uint64_t token) {
    if (movie_playback_token.load(std::memory_order_relaxed) == token) {
        return;
    }
    std::lock_guard lock{audio_mutex};
    if (movie_playback_token.load(std::memory_order_relaxed) == token) {
        return;
    }
    input_snapshot.store(0, std::memory_order_relaxed);
    sote::modern_controls::publish({});
    movie_playback_token.store(token, std::memory_order_relaxed);
    (void)open_audio_locked(token != 0 ? 22050 : source_frequency);
    std::printf("[sote][san] audio handoff: %s\n",
        token != 0 ? "movie" : "game");
    std::fflush(stdout);
}

void queue_movie_audio(const int16_t* samples, size_t sample_count) {
    if (samples == nullptr || sample_count == 0) {
        return;
    }
    std::lock_guard lock{audio_mutex};
    if (audio_device == 0 ||
        movie_playback_token.load(std::memory_order_relaxed) == 0) {
        return;
    }
    static uint64_t logged_audio_token = 0;
    const uint64_t token = movie_playback_token.load(std::memory_order_relaxed);
    if (logged_audio_token != token &&
        std::any_of(samples, samples + sample_count,
            [](int16_t sample) { return sample != 0; })) {
        logged_audio_token = token;
        std::printf("[sote][san] queued non-silent movie PCM (%zu samples)\n",
            sample_count);
        std::fflush(stdout);
    }
    const int bytes = static_cast<int>(sample_count * sizeof(int16_t));
    if (audio_stream == nullptr) {
        SDL_QueueAudio(audio_device, samples, bytes);
    } else if (SDL_AudioStreamPut(audio_stream, samples, bytes) == 0) {
        const int available = SDL_AudioStreamAvailable(audio_stream);
        if (available > 0) {
            std::vector<uint8_t> converted(static_cast<size_t>(available));
            const int received = SDL_AudioStreamGet(audio_stream,
                converted.data(), available);
            if (received > 0) {
                SDL_QueueAudio(audio_device, converted.data(), received);
            }
        }
    }
}

bool skip_controls_down() {
    bool pressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    if (initialized) {
        SDL_GameControllerUpdate();
        find_controller();
        if (controller != nullptr) {
            pressed = pressed ||
                SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START) ||
                SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) ||
                SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
        }
    }
    return pressed;
}

bool movie_skip_pressed(uint64_t token, int vi) {
    static uint64_t observed_token = 0;
    static bool armed = false;
    if (observed_token != token) {
        observed_token = token;
        armed = false;
    }
    if (token == 0) {
        return false;
    }
    const char* test_skip_vi = std::getenv("SOTE_SAN_TEST_SKIP_VI");
    const bool focused = process_owns_foreground_window();
    if (!focused && test_skip_vi == nullptr) {
        armed = false;
        return false;
    }
    // An offscreen diagnostic must not see unrelated host keyboard or pad
    // state. Only its exact, process-local skip VI may advance the movie.
    bool pressed = focused && skip_controls_down();
    for (const char* entry = test_skip_vi; entry != nullptr && *entry != '\0';) {
        char* end = nullptr;
        const long requested_vi = std::strtol(entry, &end, 10);
        if (end == entry) break;
        if (requested_vi == vi) pressed = true;
        entry = *end == ',' ? end + 1 : nullptr;
    }
    if (!pressed) {
        armed = true;
    }
    if (armed && pressed) {
        swallow_movie_skip.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void poll_input() {
    if (movie_playback_token.load(std::memory_order_relaxed) != 0) {
        input_snapshot.store(0, std::memory_order_relaxed);
        sote::modern_controls::publish({});
        return;
    }
    if (swallow_movie_skip.exchange(false, std::memory_order_relaxed) &&
        process_owns_foreground_window() && skip_controls_down()) {
        swallow_movie_skip.store(true, std::memory_order_relaxed);
        input_snapshot.store(0, std::memory_order_relaxed);
        sote::modern_controls::publish({});
        return;
    }
    const bool input_enabled =
        physical_input_enabled.load(std::memory_order_relaxed);
    const bool diagnostic_pad = diagnostic_pad_enabled();
    const bool diagnostic_mouse = diagnostic_mouse_enabled();
    const bool diagnostic_keys = diagnostic_keys_enabled();
    if (diagnostic_pad && !diagnostic_pad_loaded)
        load_diagnostic_pad_pulses();
    if (diagnostic_mouse && !diagnostic_mouse_loaded)
        load_diagnostic_mouse_pulses();
    if (diagnostic_keys && !diagnostic_keys_loaded)
        load_diagnostic_key_pulses();
    const bool focused = !diagnostic_pad && !diagnostic_mouse &&
        !diagnostic_keys &&
        process_owns_foreground_window();
    if (!input_enabled ||
        (!focused && !diagnostic_pad && !diagnostic_mouse && !diagnostic_keys)) {
        // The process-local binding probe has no foreground window. Its
        // scripted pad still exercises the editor through get_input().
        if (std::getenv("SOTE_DIAGNOSTIC_BINDING_ROUTE") == nullptr)
            control_bindings::focus_lost();
        sote::modern_controls::publish({});
        input_snapshot.store(0, std::memory_order_relaxed);
        static bool reported_ignored_input = false;
        if (std::getenv("SOTE_TRACE_INPUT") != nullptr &&
            !reported_ignored_input) {
            std::printf(
                "[sote] physical input ignored: enabled=%d focused=%d\n",
                input_enabled ? 1 : 0,
                focused ? 1 : 0);
            std::fflush(stdout);
            reported_ignored_input = true;
        }
        return;
    }

    if (initialized && !diagnostic_pad && !diagnostic_mouse && !diagnostic_keys) {
        SDL_GameControllerUpdate();
        find_controller();
    }

    control_bindings::PhysicalInput raw_input;
    if (diagnostic_pad) {
        raw_input = diagnostic_pad_at(diagnostic_vi.load(std::memory_order_relaxed));
    } else if (!diagnostic_mouse && !diagnostic_keys) {
        for (int vk = 1; vk < 256; ++vk)
            raw_input.keys[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
        raw_input.connected = controller != nullptr;
        if (controller != nullptr) {
            raw_input.instance = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
            for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b)
                raw_input.buttons[b] = SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(b));
            for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
                raw_input.axes[a] = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(a));
        }
    }
    if (diagnostic_mouse)
        diagnostic_mouse_at(diagnostic_vi.load(std::memory_order_relaxed), raw_input);
    if (diagnostic_keys)
        diagnostic_keys_at(diagnostic_vi.load(std::memory_order_relaxed), raw_input);
    const auto menu = sote::menu_skin::latest();
    const bool page_keys = menu &&
        (menu->screen == sote::menu_skin::Screen::Profiles ||
         menu->screen == sote::menu_skin::Screen::Options ||
         menu->screen == sote::menu_skin::Screen::Graphics ||
         menu->screen == sote::menu_skin::Screen::Schemes);
    const bool binding_screen = menu && menu->screen == sote::menu_skin::Screen::Controls && menu->rebinding;
    const auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    const int menu_action = control_bindings::handle_menu(raw_input, binding_screen, now_ms, bool(menu));
    if (menu_action != 0) {
        sote::modern_controls::publish({});
        input_snapshot.store(menu_action == 2 ? n64_b : menu_action == 3 ? n64_start : 0, std::memory_order_relaxed);
        return;
    }
    mapped_input = control_bindings::remap(raw_input, bool(menu));
    const sote::controls_menu::ModernControlsTuning controls =
        sote::controls_menu::modern_controls_tuning();
    uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;
    Sint16 raw_lx = 0;
    Sint16 raw_ly = 0;
    Sint16 raw_rx = 0;
    Sint16 raw_ry = 0;
    if (raw_input.connected) {
        const bool classic_foot_pad = !menu &&
            control_bindings::context_active(control_bindings::Context::OnFoot) &&
            sote::controls_menu::current_scheme(
                sote::controls_menu::SchemeSlot::OnFoot) ==
                sote::controls_menu::ControlScheme::Classic;
        const bool classic_snow_pad = !menu &&
            control_bindings::context_active(control_bindings::Context::Snowspeeder);
        const bool classic_turret_pad = !menu &&
            control_bindings::context_active(control_bindings::Context::Turret);
        const bool classic_bike_pad = !menu &&
            control_bindings::context_active(control_bindings::Context::Bike) &&
            sote::controls_menu::current_scheme(
                sote::controls_menu::SchemeSlot::Bike) ==
                sote::controls_menu::ControlScheme::Classic;
        const bool classic_outrider_pad = !menu &&
            control_bindings::context_active(control_bindings::Context::Outrider);
        auto pressed = [](SDL_GameControllerButton button) {
            return mapped_button(controller, button) != 0;
        };
        if (classic_foot_pad) {
            // PC joystick buttons 1–4: Fire, Jump, Duck, Strafe/Activate.
            // Resolve their actions through the active N64 preset; Ord's
            // Fire/Jump bits differ from the Escape opening's bits.
            uint16_t face = 0;
            if (pressed(SDL_CONTROLLER_BUTTON_A)) face |= n64_b;
            if (pressed(SDL_CONTROLLER_BUTTON_B)) face |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_X)) face |= n64_cd;
            if (pressed(SDL_CONTROLLER_BUTTON_Y)) face |= n64_r;
            buttons |= control_bindings::map_on_foot_buttons(face);
        } else if (classic_snow_pad) {
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_b;
            if (pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_X)) buttons |= n64_z;
            if (pressed(SDL_CONTROLLER_BUTTON_Y)) buttons |= n64_l | n64_r;
        } else if (classic_turret_pad) {
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_b;
            if (pressed(SDL_CONTROLLER_BUTTON_B) ||
                pressed(SDL_CONTROLLER_BUTTON_X)) buttons |= n64_z;
        } else if (classic_bike_pad) {
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_b;
        } else if (classic_outrider_pad) {
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_b;
            if (pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_X)) buttons |= n64_z;
            if (pressed(SDL_CONTROLLER_BUTTON_Y)) buttons |= n64_r;
        } else {
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_X) ||
                pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_b;
            // C-left toggles Dash's jetpack. Keep the original right-stick
            // direction and provide an accessible digital Xbox-button alias.
            if (pressed(SDL_CONTROLLER_BUTTON_Y)) buttons |= n64_cl;
        }
        if (pressed(SDL_CONTROLLER_BUTTON_START)) buttons |= n64_start;
        if (pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) buttons |= n64_l;
        if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= n64_r;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= n64_du;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= n64_dd;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= n64_dl;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= n64_dr;
        if (normalize_trigger(
                mapped_axis(
                    controller,
                    SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                controls) > 0.0f) {
            buttons |= page_keys ? n64_l : n64_z;
        }
        if (page_keys && normalize_trigger(
                mapped_axis(
                    controller,
                    SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
                controls) > 0.0f) {
            buttons |= n64_r;
        }

        raw_rx = mapped_axis(
            controller,
            SDL_CONTROLLER_AXIS_RIGHTX);
        raw_ry = mapped_axis(
            controller,
            SDL_CONTROLLER_AXIS_RIGHTY);
        const bool aim_right = aim_direction_pressed(
            raw_rx, controls.aim_deadzone, controls.aim_sensitivity, true);
        const bool aim_left = aim_direction_pressed(
            raw_rx, controls.aim_deadzone, controls.aim_sensitivity, false);
        const bool aim_down = aim_direction_pressed(
            raw_ry, controls.aim_deadzone, controls.aim_sensitivity, true);
        const bool aim_up = aim_direction_pressed(
            raw_ry, controls.aim_deadzone, controls.aim_sensitivity, false);
        if (aim_right) buttons |= n64_cr;
        if (aim_left) buttons |= n64_cl;
        if (aim_down) buttons |= n64_cd;
        if (aim_up) buttons |= n64_cu;

        raw_lx = mapped_axis(
            controller,
            SDL_CONTROLLER_AXIS_LEFTX);
        raw_ly = mapped_axis(
            controller,
            SDL_CONTROLLER_AXIS_LEFTY);
        x = normalize_axis(raw_lx, controls);
        y = -normalize_axis(raw_ly, controls);

        const bool on_foot_modern = !menu &&
            (sote::modern_controls::on_foot_active() ||
             control_bindings::context_active(control_bindings::Context::OnFoot)) &&
            sote::controls_menu::current_scheme(
                sote::controls_menu::SchemeSlot::OnFoot) ==
                sote::controls_menu::ControlScheme::Modern;
        if (on_foot_modern) {
            // The right stick travels separately to the guest's analog hooks.
            // Never turn look input into jetpack/crouch/weapon C-button events.
            buttons &= static_cast<uint16_t>(~(n64_a | n64_b | n64_z |
                n64_l | n64_r | n64_cu | n64_cd | n64_cl | n64_cr));
            if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_a;
            if (pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_cd;
            if (pressed(SDL_CONTROLLER_BUTTON_X)) buttons |= n64_r;
            if (pressed(SDL_CONTROLLER_BUTTON_Y)) buttons |= n64_cl;
            if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ||
                pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) buttons |= n64_cu;
            if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= n64_l;
            if (mapped_axis(controller,
                SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 3276) buttons |= n64_b;
            buttons = sote::modern_controls::on_foot_active() ?
                sote::modern_controls::map_buttons(buttons) :
                control_bindings::map_on_foot_buttons(buttons);
            const auto movement = sote::modern_controls::shape_stick(
                {raw_axis_to_unit(raw_lx), -raw_axis_to_unit(raw_ly)},
                controls.on_foot.movement_deadzone, 1.0f);
            x = movement.x * n64_stick_scale;
            y = movement.y * n64_stick_scale;
        }

        const bool bike_modern_active =
            !menu && sote::controls_menu::bike_modern_scheme_active();
        if (bike_modern_active) {
            // Modern redefines A and B as Accelerate/Brakes driven by the
            // triggers, and LT no longer means Z, so drop those Classic
            // bits before the Modern mapping sets them.
            buttons &=
                static_cast<uint16_t>(~(n64_a | n64_b | n64_z));
            apply_modern_bike_scheme(controller, buttons, x, y, controls);
        } else {
            // Leaving the bike stage clears the damping carry, so the next
            // bike section starts from center instead of easing out of the
            // previous one's held throttle and lean.
            modern_bike_filter = {};

            const bool right_stick_active =
                aim_right || aim_left || aim_down || aim_up;
            const auto now = std::chrono::steady_clock::now();
            if (right_stick_active) {
                right_stick_was_active = true;
                look_snap_back_queued = false;
                look_snap_back_active = false;
            } else if (right_stick_was_active) {
                right_stick_was_active = false;
                look_snap_back_queued = !on_foot_modern && controls.look_snap_back_enabled;
                right_stick_release_time = now;
            }

            if (look_snap_back_queued &&
                now - right_stick_release_time >=
                    std::chrono::duration<float>(
                        controls.look_snap_back_delay_seconds)) {
                look_snap_back_queued = false;
                look_snap_back_active =
                    controls.look_snap_back_duration_seconds > 0.0f;
                look_snap_back_end_time =
                    now + std::chrono::duration_cast<
                        std::chrono::steady_clock::duration>(
                        std::chrono::duration<float>(
                            controls.look_snap_back_duration_seconds));
            }

            if (on_foot_modern) {
                look_snap_back_active = false;
                look_snap_back_queued = false;
            }
            if (look_snap_back_active) {
                if (now < look_snap_back_end_time) {
                    buttons |= controls.look_snap_back_button_bit;
                } else {
                    look_snap_back_active = false;
                }
            }
        }
    }

    // Keyboard and mouse use the same canonical Modern action mapping as the
    // pad. Keep them separate from already-translated pad bits to avoid
    // translating a native binding twice.
    uint16_t keyboard_buttons = 0;
    const bool classic_foot_keyboard = !menu &&
        control_bindings::context_active(control_bindings::Context::OnFoot) &&
        sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot) ==
            sote::controls_menu::ControlScheme::Classic;
    const bool classic_snow_keyboard = !menu &&
        control_bindings::context_active(control_bindings::Context::Snowspeeder);
    const bool classic_turret_keyboard = !menu &&
        control_bindings::context_active(control_bindings::Context::Turret);
    const bool classic_bike_keyboard = !menu &&
        control_bindings::context_active(control_bindings::Context::Bike) &&
        sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::Bike) ==
            sote::controls_menu::ControlScheme::Classic;
    const bool classic_outrider_keyboard = !menu &&
        control_bindings::context_active(control_bindings::Context::Outrider);
    if (classic_foot_keyboard) {
        // Installed PC release, first built-in control set. Movement uses
        // arrow/numpad keys below; these are its non-movement actions.
        if (key_down('Z') || key_down(VK_RBUTTON)) keyboard_buttons |= n64_a;
        if (key_down('X') || key_down(VK_LBUTTON)) keyboard_buttons |= n64_b;
        if (key_down(VK_SPACE)) keyboard_buttons |= n64_z;
        if (key_down(VK_RETURN) || key_down(VK_F1)) keyboard_buttons |= n64_start;
        if (key_down('I')) keyboard_buttons |= n64_du;
        if (key_down('K')) keyboard_buttons |= n64_dd;
        if (key_down('J')) keyboard_buttons |= n64_dl;
        if (key_down('L')) keyboard_buttons |= n64_dr;
        if (key_down(VK_TAB)) keyboard_buttons |= n64_l | n64_cr;
        if (key_down('A')) keyboard_buttons |= n64_r;
        if (key_down('W')) keyboard_buttons |= n64_cu;
        if (key_down('C')) keyboard_buttons |= n64_cd;
        if (key_down('Q')) keyboard_buttons |= n64_cl;
    } else if (classic_snow_keyboard) {
        if (key_down('Z') || key_down(VK_RBUTTON)) keyboard_buttons |= n64_a;
        if (key_down('X') || key_down(VK_LBUTTON)) keyboard_buttons |= n64_b;
        if (key_down(VK_SPACE) || key_down('C')) keyboard_buttons |= n64_z;
        if (key_down('A')) keyboard_buttons |= n64_l | n64_r;
        if (key_down(VK_TAB)) keyboard_buttons |= n64_cr;
        if (key_down(VK_RETURN) || key_down(VK_F1)) keyboard_buttons |= n64_start;
        if (key_down('I')) keyboard_buttons |= n64_du;
        if (key_down('K')) keyboard_buttons |= n64_dd;
        if (key_down('J')) keyboard_buttons |= n64_dl;
        if (key_down('L')) keyboard_buttons |= n64_dr;
    } else if (classic_turret_keyboard) {
        if (key_down('X') || key_down(VK_LBUTTON)) keyboard_buttons |= n64_b;
        if (key_down('C') || key_down(VK_RBUTTON) ||
            key_down(control_bindings::turret_skyhook_active() ? 'Z' : VK_SPACE))
            keyboard_buttons |= n64_z;
        if (key_down(VK_TAB)) keyboard_buttons |= n64_cr;
        if (key_down(VK_RETURN) || key_down(VK_F1)) keyboard_buttons |= n64_start;
        if (key_down('I')) keyboard_buttons |= n64_du;
        if (key_down('K')) keyboard_buttons |= n64_dd;
        if (key_down('J')) keyboard_buttons |= n64_dl;
        if (key_down('L')) keyboard_buttons |= n64_dr;
    } else if (classic_bike_keyboard) {
        if (key_down('Z') || key_down(VK_LBUTTON)) keyboard_buttons |= n64_a;
        if (key_down('A') || key_down(VK_RBUTTON)) keyboard_buttons |= n64_b;
        if (key_down('S')) keyboard_buttons |= n64_l;
        if (key_down('D')) keyboard_buttons |= n64_r;
        if (key_down(VK_TAB)) keyboard_buttons |= n64_cr;
        if (key_down(VK_RETURN) || key_down(VK_F1)) keyboard_buttons |= n64_start;
        if (key_down('I')) keyboard_buttons |= n64_du;
        if (key_down('K')) keyboard_buttons |= n64_dd;
        if (key_down('J')) keyboard_buttons |= n64_dl;
        if (key_down('L')) keyboard_buttons |= n64_dr;
    } else if (classic_outrider_keyboard) {
        if (key_down('Z') || key_down(VK_RBUTTON)) keyboard_buttons |= n64_a;
        if (key_down('X') || key_down(VK_LBUTTON)) keyboard_buttons |= n64_b;
        if (key_down(VK_SPACE) || key_down('C')) keyboard_buttons |= n64_z;
        if (key_down('A')) keyboard_buttons |= n64_r;
        if (key_down('Q')) keyboard_buttons |= n64_cl;
        if (key_down(VK_TAB)) keyboard_buttons |= n64_cr;
        if (key_down(VK_RETURN) || key_down(VK_F1)) keyboard_buttons |= n64_start;
        if (key_down('I')) keyboard_buttons |= n64_du;
        if (key_down('K')) keyboard_buttons |= n64_dd;
        if (key_down('J')) keyboard_buttons |= n64_dl;
        if (key_down('L')) keyboard_buttons |= n64_dr;
    } else {
        if (key_down('Z') || key_down(VK_SPACE)) keyboard_buttons |= n64_a;
        if (key_down('X')) keyboard_buttons |= n64_b;
        if (key_down('C')) keyboard_buttons |= n64_z;
        if (key_down(VK_RETURN)) keyboard_buttons |= n64_start;
        if (key_down('Q')) keyboard_buttons |= page_keys ? n64_dl : n64_l;
        if (key_down('E')) keyboard_buttons |= page_keys ? n64_dr : n64_r;
        if (key_down(VK_UP)) keyboard_buttons |= n64_du;
        if (key_down(VK_DOWN)) keyboard_buttons |= n64_dd;
        if (key_down(VK_LEFT)) keyboard_buttons |= page_keys ? n64_l : n64_dl;
        if (key_down(VK_RIGHT)) keyboard_buttons |= page_keys ? n64_r : n64_dr;
        if (key_down('I')) keyboard_buttons |= n64_cu;
        if (key_down('K')) keyboard_buttons |= n64_cd;
        if (key_down('J')) keyboard_buttons |= n64_cl;
        if (key_down('L')) keyboard_buttons |= n64_cr;
    }
    // Mouse 1 is the default Modern on-foot Fire alias. It is sampled through
    // the rebinding layer, so assigning the button elsewhere removes this alias.
    if (!menu && (sote::modern_controls::on_foot_active() ||
                  control_bindings::context_active(control_bindings::Context::OnFoot)) &&
        sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot) ==
            sote::controls_menu::ControlScheme::Modern && key_down(VK_LBUTTON))
        keyboard_buttons |= n64_b;
    const bool modern_keyboard = !menu &&
        (sote::modern_controls::on_foot_active() ||
         control_bindings::context_active(control_bindings::Context::OnFoot)) &&
        sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot) ==
            sote::controls_menu::ControlScheme::Modern;
    // Classic PC keys name actions, not fixed N64 buttons. Preset 6 (used on
    // the Ord Mantell train) swaps Jump and Fire, so resolve both schemes
    // through the live on-foot action table.
    buttons |= modern_keyboard ?
        (sote::modern_controls::on_foot_active() ?
            sote::modern_controls::map_buttons(keyboard_buttons) :
            control_bindings::map_on_foot_buttons(keyboard_buttons)) :
        classic_foot_keyboard ?
            control_bindings::map_on_foot_buttons(keyboard_buttons) :
            keyboard_buttons;

    float keyboard_x = 0.0f;
    float keyboard_y = 0.0f;
    if (classic_foot_keyboard || classic_snow_keyboard || classic_turret_keyboard ||
        classic_bike_keyboard || classic_outrider_keyboard) {
        if (key_down(VK_LEFT) || key_down(VK_NUMPAD4)) keyboard_x -= n64_stick_scale;
        if (key_down(VK_RIGHT) || key_down(VK_NUMPAD6)) keyboard_x += n64_stick_scale;
        if (key_down(VK_UP) || key_down(VK_NUMPAD8)) keyboard_y += n64_stick_scale;
        if (key_down(VK_DOWN) || key_down(VK_NUMPAD2)) keyboard_y -= n64_stick_scale;
    } else {
        if (key_down('A')) keyboard_x -= n64_stick_scale;
        if (key_down('D')) keyboard_x += n64_stick_scale;
        if (key_down('W')) keyboard_y += n64_stick_scale;
        if (key_down('S')) keyboard_y -= n64_stick_scale;
    }
    // Feed keyboard movement and relative mouse look to the same Modern guest
    // hooks used by the controller, including when no gamepad is connected.
    const sote::modern_controls::Stick modern_move =
        keyboard_x != 0 || keyboard_y != 0
            ? sote::modern_controls::Stick{
                keyboard_x / n64_stick_scale, keyboard_y / n64_stick_scale}
            : sote::modern_controls::Stick{
                raw_axis_to_unit(raw_lx), -raw_axis_to_unit(raw_ly)};
    sote::modern_controls::publish({modern_move,
        {raw_axis_to_unit(raw_rx), raw_axis_to_unit(raw_ry)}, !menu});
    if (x == 0.0f) x = keyboard_x;
    if (y == 0.0f) y = keyboard_y;

    const int8_t sx = static_cast<int8_t>(
        std::clamp(x, -1.0f, 1.0f) * 127.0f);
    const int8_t sy = static_cast<int8_t>(
        std::clamp(y, -1.0f, 1.0f) * 127.0f);
    static uint64_t input_poll_count = 0;
    ++input_poll_count;
    if (std::getenv("SOTE_TRACE_INPUT") != nullptr &&
        input_poll_count % 30 == 0) {
        std::printf(
            "[sote] input poll=%llu buttons=%04X stick=%d,%d "
            "raw_l=%d,%d raw_r=%d,%d\n",
            static_cast<unsigned long long>(input_poll_count),
            buttons,
            sx,
            sy,
            raw_lx,
            raw_ly,
            raw_rx,
            raw_ry);
        std::fflush(stdout);
    }
    input_snapshot.store(
        buttons |
            (static_cast<uint32_t>(static_cast<uint8_t>(sx)) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(sy)) << 24),
        std::memory_order_relaxed);
}

void add_mouse_delta(int x, int y) {
    sote::modern_controls::add_mouse_delta(x, y);
}

void set_physical_input_enabled(bool enabled) {
    physical_input_enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        sote::modern_controls::publish({});
        input_snapshot.store(0, std::memory_order_relaxed);
    }
}

bool get_input(int port, uint16_t* buttons, float* x, float* y) {
    if (movie_playback_token.load(std::memory_order_relaxed) != 0) {
        // The PC ending advances hidden native story cards with A. Game Over
        // advances the hidden native return-to-title scene with Start. Other
        // films keep guest input frozen.
        *buttons = movie_guest_start.load(std::memory_order_relaxed) ?
            n64_start : movie_guest_advance.load(std::memory_order_relaxed) ?
            n64_a : 0;
        *x = 0.0f;
        *y = 0.0f;
        return true;
    }
    if (port != 0) {
        return false;
    }
    const uint32_t snapshot = input_snapshot.load(std::memory_order_relaxed);
    const uint32_t scripted =
        scripted_input_snapshot.load(std::memory_order_relaxed);
    // In an offscreen game-process diagnostic, route scripted N64 pulses
    // through the same binding editor state machine as a physical pad. The
    // normal SDL poll is intentionally inactive without window focus.
    if (std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") != nullptr &&
        std::getenv("SOTE_DIAGNOSTIC_BINDING_ROUTE") != nullptr) {
        const auto menu = sote::menu_skin::latest();
        if (menu && menu->screen == sote::menu_skin::Screen::Controls &&
            menu->rebinding) {
            control_bindings::PhysicalInput virtual_pad{};
            virtual_pad.connected = true;
            virtual_pad.instance = 1;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_A] =
                (scripted & n64_a) != 0;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_B] =
                (scripted & n64_b) != 0;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_DPAD_UP] =
                (scripted & n64_du) != 0;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN] =
                (scripted & n64_dd) != 0;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT] =
                (scripted & n64_dl) != 0;
            virtual_pad.buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] =
                (scripted & n64_dr) != 0;
            const auto now = std::chrono::steady_clock::now();
            const auto now_ms = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count());
            if ((scripted & n64_a) != 0) {
                auto released = virtual_pad;
                released.buttons[SDL_CONTROLLER_BUTTON_A] = 0;
                (void)control_bindings::handle_menu(
                    released, true, now_ms - 1, true);
            }
            const int action = control_bindings::handle_menu(
                virtual_pad, true, now_ms, true);
            if (action != 0) {
                *buttons = action == 2 ? n64_b :
                    action == 3 ? n64_start : 0;
                *x = 0.0f;
                *y = 0.0f;
                return true;
            }
        }
    }
    *buttons = static_cast<uint16_t>(snapshot | scripted);
    const int8_t scripted_x = static_cast<int8_t>(scripted >> 16);
    const int8_t scripted_y = static_cast<int8_t>(scripted >> 24);
    *x = (scripted_x != 0 ? scripted_x : static_cast<int8_t>(snapshot >> 16)) /
        127.0f;
    *y = (scripted_y != 0 ? scripted_y : static_cast<int8_t>(snapshot >> 24)) /
        127.0f;
    // Diagnostic path for exercising Modern's analog throttle without a
    // physical pad: poll_input is skipped entirely when the window is not
    // focused, which is always the case for offscreen captures.
    if (const char* forced = std::getenv("SOTE_FORCE_BIKE_THROTTLE")) {
        if (sote_is_bike_stage_active() != 0) {
            float throttle = std::clamp(
                static_cast<float>(std::atof(forced)), 0.0f, 1.0f);
            // "sweep" walks 0, 25, 50, 75, 100 percent, holding each for a
            // few seconds, so a live run visibly shows speed tracking the
            // trigger level without needing a physical pad.
            if (std::strcmp(forced, "sweep") == 0) {
                static const auto start =
                    std::chrono::steady_clock::now();
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - start).count();
                const int step = static_cast<int>((elapsed / 5) % 5);
                throttle = static_cast<float>(step) * 0.25f;
                static int last_step = -1;
                if (step != last_step) {
                    last_step = step;
                    std::printf(
                        "[sote][throttle] sweep -> %d%%\n", step * 25);
                    std::fflush(stdout);
                }
            }
            float brake = 0.0f;
            if (const char* forced_brake =
                    std::getenv("SOTE_FORCE_BIKE_BRAKE")) {
                brake = std::clamp(
                    static_cast<float>(std::atof(forced_brake)), 0.0f, 1.0f);
            }
            const sote::controls_menu::ModernBikeButtons bike_buttons =
                sote::controls_menu::compute_modern_bike_buttons(
                    throttle, brake, modern_bike_filter);
            if (bike_buttons.accelerate) {
                *buttons |= n64_a;
            }
            if (bike_buttons.brake) {
                *buttons |= n64_b;
            }
        }
    }
    sote::graphics_menu::filter_input(buttons, x, y);
    return true;
}

void set_movie_guest_advance(bool pressed) {
    movie_guest_advance.store(pressed, std::memory_order_relaxed);
}

void set_movie_guest_start(bool pressed) {
    movie_guest_start.store(pressed, std::memory_order_relaxed);
}

void set_scripted_input(int vi, uint16_t buttons, int8_t x, int8_t y) {
    diagnostic_vi.store(vi, std::memory_order_relaxed);
    scripted_input_snapshot.store(
        buttons |
            (static_cast<uint32_t>(static_cast<uint8_t>(x)) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(y)) << 24),
        std::memory_order_relaxed);
}

ultramodern::input::connected_device_info_t get_connected_device_info(
    int port) {
    using ultramodern::input::Device;
    using ultramodern::input::Pak;
    if (port == 0) {
        return {Device::Controller, Pak::RumblePak};
    }
    return {Device::None, Pak::None};
}

void set_rumble(int port, bool enabled) {
    if (port == 0 && controller != nullptr) {
        const Uint16 strength = enabled ? 0xFFFF : 0;
        SDL_GameControllerRumble(
            controller,
            strength,
            strength,
            enabled ? 250 : 0);
    }
}

void queue_samples(int16_t* samples, size_t sample_count) {
    if (samples == nullptr || sample_count == 0 ||
        sample_count > (256 * 1024) / sizeof(int16_t) ||
        movie_playback_token.load(std::memory_order_relaxed) != 0) {
        return;
    }

    if (!first_non_silent_buffer.load(std::memory_order_relaxed)) {
        for (size_t i = 0; i < sample_count; ++i) {
            if (samples[i] != 0) {
                if (!first_non_silent_buffer.exchange(true)) {
                    std::printf(
                        "[sote] first non-silent audio buffer: sample[%zu]=%d\n",
                        i,
                        samples[i]);
                    std::fflush(stdout);
                }
                break;
            }
        }
    }

    std::lock_guard lock{audio_mutex};
    if (movie_playback_token.load(std::memory_order_relaxed) != 0) {
        return;
    }
    // Guest RDRAM is word-swizzled: each pair of 16-bit stereo samples is
    // reversed in the host view.
    swapped_samples.resize(sample_count);
    size_t i = 0;
    for (; i + 1 < sample_count; i += 2) {
        swapped_samples[i] = samples[i + 1];
        swapped_samples[i + 1] = samples[i];
    }
    if (i < sample_count) {
        swapped_samples[i] = samples[i];
    }

    sote::hd_music::mix_into(
        swapped_samples.data(),
        swapped_samples.size(),
        source_frequency);
    sote::hd_audio::mix_into(
        swapped_samples.data(),
        swapped_samples.size(),
        source_frequency);

    if (!audio_dump_checked) {
        audio_dump_checked = true;
        if (const char* path = std::getenv("SOTE_AUDIO_DUMP_PATH")) {
            if (path[0] != '\0') {
                if (fopen_s(&audio_dump_file, path, "wb") == 0 &&
                    audio_dump_file != nullptr) {
                    // Smoke tests terminate with _Exit, so keep the diagnostic
                    // stream unbuffered rather than relying on process cleanup.
                    std::setvbuf(audio_dump_file, nullptr, _IONBF, 0);
                    std::printf("[sote] dumping host-order PCM to %s\n", path);
                    std::fflush(stdout);
                } else {
                    std::fprintf(
                        stderr,
                        "[sote] failed to open audio dump: %s\n",
                        path);
                }
            }
        }
    }
    if (audio_dump_file != nullptr) {
        std::fwrite(
            swapped_samples.data(),
            sizeof(int16_t),
            sample_count,
            audio_dump_file);
    }

    if (audio_device == 0) {
        update_muted_audio_clock_locked();
    }
    const Uint32 queued_before =
        audio_device != 0
            ? SDL_GetQueuedAudioSize(audio_device)
            : static_cast<Uint32>(
                  std::min(
                      muted_queued_frames * sizeof(int16_t) * 2.0,
                      static_cast<double>(UINT32_MAX)));
    const int byte_count =
        static_cast<int>(sample_count * sizeof(int16_t));
    if (audio_device == 0) {
        // Muted/headless mode still reaches this point so the generated PCM
        // and its timing can be validated without opening an audio device.
        muted_queued_frames += sample_count / 2.0;
    } else if (audio_stream == nullptr) {
        SDL_QueueAudio(
            audio_device,
            swapped_samples.data(),
            static_cast<Uint32>(byte_count));
    } else if (SDL_AudioStreamPut(
                   audio_stream,
                   swapped_samples.data(),
                   byte_count) == 0) {
        const int available = SDL_AudioStreamAvailable(audio_stream);
        if (available > 0) {
            converted_samples.resize(static_cast<size_t>(available));
            const int received = SDL_AudioStreamGet(
                audio_stream,
                converted_samples.data(),
                available);
            if (received > 0) {
                SDL_QueueAudio(
                    audio_device,
                    converted_samples.data(),
                    static_cast<Uint32>(received));
            }
        }
    }

    if (!first_audio_buffer.exchange(true)) {
        std::printf(
            "[sote] first audio buffer queued: %zu stereo frames\n",
            sample_count / 2);
        std::fflush(stdout);
    }
    ++audio_buffer_count;
    audio_sample_count += sample_count;
    const ULONGLONG host_tick = GetTickCount64();
    if (first_audio_host_tick == 0) {
        first_audio_host_tick = host_tick;
    }
    if (std::getenv("SOTE_TRACE_AUDIO") != nullptr &&
        (audio_buffer_count <= 10 || audio_buffer_count % 30 == 0 ||
         (audio_device != 0 && queued_before == 0))) {
        const Uint32 queued_after =
            audio_device != 0
                ? SDL_GetQueuedAudioSize(audio_device)
                : static_cast<Uint32>(
                      std::min(
                          muted_queued_frames * sizeof(int16_t) * 2.0,
                          static_cast<double>(UINT32_MAX)));
        std::printf(
            "[sote] audio queue=%llu samples=%zu total=%llu elapsed=%llu "
            "before=%u after=%u bytes\n",
            static_cast<unsigned long long>(audio_buffer_count),
            sample_count,
            static_cast<unsigned long long>(audio_sample_count),
            static_cast<unsigned long long>(
                host_tick - first_audio_host_tick),
            queued_before,
            queued_after);
        std::fflush(stdout);
    }
}

size_t get_frames_remaining() {
    std::lock_guard lock{audio_mutex};
    if (movie_playback_token.load(std::memory_order_relaxed) != 0) {
        return 0;
    }
    if (audio_device == 0) {
        update_muted_audio_clock_locked();
        if (!audio_output_enabled.load(std::memory_order_relaxed)) {
            return static_cast<size_t>(muted_queued_frames);
        }
        return 0;
    }
    // SDL counts device-rate bytes; ultramodern expects source-rate frames.
    // The rates can differ when SDL_NewAudioStream is performing resampling.
    if (obtained_audio.freq <= 0 || source_frequency == 0) {
        return 0;
    }
    const uint64_t device_frames =
        SDL_GetQueuedAudioSize(audio_device) / (sizeof(int16_t) * 2);
    return static_cast<size_t>(
        device_frames * source_frequency /
        static_cast<uint32_t>(obtained_audio.freq));
}

void set_frequency(uint32_t frequency) {
    std::printf("[sote] audio frequency: %u Hz\n", frequency);
    std::fflush(stdout);
    std::lock_guard lock{audio_mutex};
    if (source_frequency == frequency && audio_device != 0) {
        return;
    }
    source_frequency = frequency;
    if (movie_playback_token.load(std::memory_order_relaxed) != 0) {
        return;
    }
    muted_queued_frames = 0.0;
    muted_audio_clock = std::chrono::steady_clock::now();
    muted_audio_clock_initialized = true;
    open_audio_locked(frequency);
}

} // namespace sote::frontend
