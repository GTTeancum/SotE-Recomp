#define SDL_MAIN_HANDLED

#include "frontend.hpp"
#include "graphics_menu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
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
constexpr Sint16 stick_deadzone = 8000;
constexpr Sint16 c_deadzone = 12000;
constexpr Sint16 trigger_threshold = 8000;

std::atomic<uint32_t> input_snapshot{0};
std::atomic<uint32_t> scripted_input_snapshot{0};
std::atomic<bool> physical_input_enabled{true};
std::atomic<bool> audio_output_enabled{true};
SDL_GameController* controller = nullptr;
bool initialized = false;

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

bool key_down(int virtual_key) {
    return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
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

bool process_owns_foreground_window() {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }
    DWORD foreground_process = 0;
    GetWindowThreadProcessId(foreground, &foreground_process);
    return foreground_process == GetCurrentProcessId();
}

float normalize_axis(Sint16 value) {
    if (value > -stick_deadzone && value < stick_deadzone) {
        return 0.0f;
    }
    const float normalized =
        value >= 0 ? value / 32767.0f : value / 32768.0f;
    return std::clamp(normalized, -1.0f, 1.0f) * n64_stick_scale;
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

void poll_input() {
    const bool input_enabled =
        physical_input_enabled.load(std::memory_order_relaxed);
    const bool focused = process_owns_foreground_window();
    if (!input_enabled || !focused) {
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

    if (initialized) {
        SDL_GameControllerUpdate();
        find_controller();
    }

    uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;
    Sint16 raw_lx = 0;
    Sint16 raw_ly = 0;
    Sint16 raw_rx = 0;
    Sint16 raw_ry = 0;

    if (controller != nullptr) {
        auto pressed = [](SDL_GameControllerButton button) {
            return SDL_GameControllerGetButton(controller, button) != 0;
        };
        if (pressed(SDL_CONTROLLER_BUTTON_A)) buttons |= n64_a;
        if (pressed(SDL_CONTROLLER_BUTTON_X) ||
            pressed(SDL_CONTROLLER_BUTTON_B)) buttons |= n64_b;
        // C-left toggles Dash's jetpack. Keep the original right-stick
        // direction and provide an accessible digital Xbox-button alias.
        if (pressed(SDL_CONTROLLER_BUTTON_Y)) buttons |= n64_cl;
        if (pressed(SDL_CONTROLLER_BUTTON_START)) buttons |= n64_start;
        if (pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) buttons |= n64_l;
        if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= n64_r;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= n64_du;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= n64_dd;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= n64_dl;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= n64_dr;
        if (SDL_GameControllerGetAxis(
                controller,
                SDL_CONTROLLER_AXIS_TRIGGERLEFT) > trigger_threshold) {
            buttons |= n64_z;
        }

        raw_rx = SDL_GameControllerGetAxis(
            controller,
            SDL_CONTROLLER_AXIS_RIGHTX);
        raw_ry = SDL_GameControllerGetAxis(
            controller,
            SDL_CONTROLLER_AXIS_RIGHTY);
        if (raw_rx > c_deadzone) buttons |= n64_cr;
        if (raw_rx < -c_deadzone) buttons |= n64_cl;
        if (raw_ry > c_deadzone) buttons |= n64_cd;
        if (raw_ry < -c_deadzone) buttons |= n64_cu;

        raw_lx = SDL_GameControllerGetAxis(
            controller,
            SDL_CONTROLLER_AXIS_LEFTX);
        raw_ly = SDL_GameControllerGetAxis(
            controller,
            SDL_CONTROLLER_AXIS_LEFTY);
        x = normalize_axis(raw_lx);
        y = -normalize_axis(raw_ly);
    }

    // Keyboard fallback: WASD is the N64 stick; Z/X/C are A/B/Z.
    if (key_down('Z') || key_down(VK_SPACE)) buttons |= n64_a;
    if (key_down('X')) buttons |= n64_b;
    if (key_down('C')) buttons |= n64_z;
    if (key_down(VK_RETURN)) buttons |= n64_start;
    if (key_down('Q')) buttons |= n64_l;
    if (key_down('E')) buttons |= n64_r;
    if (key_down(VK_UP)) buttons |= n64_du;
    if (key_down(VK_DOWN)) buttons |= n64_dd;
    if (key_down(VK_LEFT)) buttons |= n64_dl;
    if (key_down(VK_RIGHT)) buttons |= n64_dr;
    if (key_down('I')) buttons |= n64_cu;
    if (key_down('K')) buttons |= n64_cd;
    if (key_down('J')) buttons |= n64_cl;
    if (key_down('L')) buttons |= n64_cr;

    float keyboard_x = 0.0f;
    float keyboard_y = 0.0f;
    if (key_down('A')) keyboard_x -= n64_stick_scale;
    if (key_down('D')) keyboard_x += n64_stick_scale;
    if (key_down('W')) keyboard_y += n64_stick_scale;
    if (key_down('S')) keyboard_y -= n64_stick_scale;
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

void set_physical_input_enabled(bool enabled) {
    physical_input_enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        input_snapshot.store(0, std::memory_order_relaxed);
    }
}

bool get_input(int port, uint16_t* buttons, float* x, float* y) {
    if (port != 0) {
        return false;
    }
    const uint32_t snapshot = input_snapshot.load(std::memory_order_relaxed);
    const uint32_t scripted =
        scripted_input_snapshot.load(std::memory_order_relaxed);
    *buttons = static_cast<uint16_t>(snapshot | scripted);
    const int8_t scripted_x = static_cast<int8_t>(scripted >> 16);
    const int8_t scripted_y = static_cast<int8_t>(scripted >> 24);
    *x = (scripted_x != 0 ? scripted_x : static_cast<int8_t>(snapshot >> 16)) /
        127.0f;
    *y = (scripted_y != 0 ? scripted_y : static_cast<int8_t>(snapshot >> 24)) /
        127.0f;
    sote::graphics_menu::filter_input(buttons, x, y);
    return true;
}

void set_scripted_input(uint16_t buttons, int8_t x, int8_t y) {
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
        sample_count > (256 * 1024) / sizeof(int16_t)) {
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
    if (audio_device == 0) {
        update_muted_audio_clock_locked();
        if (!audio_output_enabled.load(std::memory_order_relaxed)) {
            return static_cast<size_t>(muted_queued_frames);
        }
        return 0;
    }
    return SDL_GetQueuedAudioSize(audio_device) /
        (sizeof(int16_t) * 2);
}

void set_frequency(uint32_t frequency) {
    std::printf("[sote] audio frequency: %u Hz\n", frequency);
    std::fflush(stdout);
    std::lock_guard lock{audio_mutex};
    if (source_frequency == frequency && audio_device != 0) {
        return;
    }
    source_frequency = frequency;
    muted_queued_frames = 0.0;
    muted_audio_clock = std::chrono::steady_clock::now();
    muted_audio_clock_initialized = true;
    open_audio_locked(frequency);
}

} // namespace sote::frontend
