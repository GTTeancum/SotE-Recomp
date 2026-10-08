#include "modern_controls.hpp"
#include "controls_menu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace sote::modern_controls {
namespace {
std::mutex input_mutex;
Input latest;
std::atomic<int> mouse_delta_x{0};
std::atomic<int> mouse_delta_y{0};
std::atomic<int64_t> last_on_foot_ms{0};
std::atomic<int64_t> last_aiming_ms{0};
constexpr uint16_t canonical_buttons[] = {0x8000,0x4000,0x10,0x4,0x2,0x8,0x20,0x2000};
constexpr uint32_t binding_offsets[] = {0x1A,0x18,0x24,0x26,0x28,0x2C,0x2E,0x1C};
std::atomic<uint16_t> bindings[8] = {0x8000,0x4000,0x10,0x4,0x2,0x8,0x20,0x2000};
Input frame;
Tuning tuning;
bool active = false;
uint32_t mouse_probe_frames = 0;
float pitch = 0;
float mouse_yaw_rate = 0;
uint32_t owner = 0;
int16_t level_event = -1;
uint64_t frame_number = 0;
bool camera_initialized = false;
bool camera_requested = false;
bool camera_offsets_owned = false;
int64_t milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class T> T read(uint8_t* ram, uint32_t address) {
    T value;
    const uint32_t offset = (address & 0x7FFFFF) ^ (sizeof(T) == 2 ? 2 : 0);
    std::memcpy(&value, ram + offset, sizeof(value));
    return value;
}
template<class T> void write(uint8_t* ram, uint32_t address, T value) {
    const uint32_t offset = (address & 0x7FFFFF) ^ (sizeof(T) == 2 ? 2 : 0);
    std::memcpy(ram + offset, &value, sizeof(value));
}
bool blocked(uint8_t* ram, uint32_t object) {
    if (level_event == 8) {
        return read<float>(ram, 0x800E539C) > 0 ||
            read<uint32_t>(ram, 0x800E5838) != 0 ||
            read<float>(ram, 0x800E57B8) > 0 ||
            (read<uint32_t>(ram, object + 0x74) & 0x400) != 0;
    }
    // Native input suppression immediately following 800758A8, plus death.
    return read<float>(ram, 0x800E0EA0) > 0 ||
        read<float>(ram, 0x800DEB78) > 0 ||
        (read<uint32_t>(ram, object + 0x74) & 0x400) != 0;
}
}

Stick shape_stick(Stick raw, float deadzone, float exponent) {
    if (!std::isfinite(raw.x) || !std::isfinite(raw.y)) return {};
    raw.x = std::clamp(raw.x, -1.0f, 1.0f);
    raw.y = std::clamp(raw.y, -1.0f, 1.0f);
    const float length = std::hypot(raw.x, raw.y);
    deadzone = std::clamp(deadzone, 0.0f, 1.0f);
    if (length <= deadzone || deadzone >= 1.0f) return {};
    const float magnitude = std::pow(
        (std::min(length, 1.0f) - deadzone) / (1.0f - deadzone),
        std::clamp(exponent, 0.25f, 4.0f));
    return {raw.x * magnitude / length, raw.y * magnitude / length};
}
float advance_pitch(float value, float input, float speed, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0) return value;
    return std::clamp(value + input * speed * static_cast<float>(
        std::min(seconds, 0.1)), -55.0f, 55.0f);
}
void publish(Input input) {
    std::lock_guard lock{input_mutex};
    latest = input;
    if (!input.connected) {
        mouse_delta_x.store(0, std::memory_order_relaxed);
        mouse_delta_y.store(0, std::memory_order_relaxed);
        last_aiming_ms.store(0, std::memory_order_relaxed);
    }
}
void add_mouse_delta(int x, int y) {
    mouse_delta_x.fetch_add(x, std::memory_order_relaxed);
    mouse_delta_y.fetch_add(y, std::memory_order_relaxed);
}
bool on_foot_active() {
    const auto age = milliseconds() - last_on_foot_ms.load();
    return age >= 0 && age < 150;
}
bool aiming_active() { return active; }
bool reticle_visible() {
    const int64_t age = milliseconds() - last_aiming_ms.load(std::memory_order_relaxed);
    return age >= 0 && age < 150;
}
uint16_t map_buttons(uint16_t canonical) {
    uint16_t result = canonical & ~uint16_t(0xE03E);
    for (unsigned i=0; i<8; ++i)
        if (canonical & canonical_buttons[i]) result |= bindings[i].load();
    return result;
}
float pitch_degrees() { return pitch; }
bool owns_player(uint32_t object) { return active && owner == object; }
}

extern "C" void sote_modern_begin(uint8_t* ram, uint32_t object) {
    using namespace sote::modern_controls;
    const auto now = milliseconds();
    last_on_foot_ms.store(now);
    const int16_t event = read<int16_t>(ram, 0x8013CE0E);
    if (owner != object || level_event != event) {
        pitch = 0;
        frame_number = 0;
        mouse_probe_frames = 0;
        camera_initialized = false;
    }
    owner = object;
    level_event = event;
    {
        std::lock_guard lock{input_mutex};
        frame = latest;
    }
    int mouse_x = mouse_delta_x.exchange(0, std::memory_order_relaxed);
    int mouse_y = mouse_delta_y.exchange(0, std::memory_order_relaxed);
    mouse_yaw_rate = 0;
    tuning = sote::controls_menu::modern_controls_tuning().on_foot;
    const auto preset = read<int16_t>(ram, 0x800D252C);
    if (preset >= 0 && preset < 8) {
        for (unsigned i=0; i<8; ++i)
            bindings[i].store(read<uint16_t>(ram,
                0x800E6808 + preset*96 + binding_offsets[i]));
    }
    active = frame.connected && sote::controls_menu::current_scheme(
        sote::controls_menu::SchemeSlot::OnFoot) ==
        sote::controls_menu::ControlScheme::Modern;
    // Explicit process-local diagnostic. No host input or device manipulation.
    if (std::getenv("SOTE_TEST_MODERN_INPUT") != nullptr) {
        active = true;
        const unsigned phase = static_cast<unsigned>((frame_number / 120) % 8);
        frame = {};
        frame.connected = true;
        if (phase == 1) frame.look.x = 0.25f;
        if (phase == 2) frame.look.x = 0.5f;
        if (phase == 3) frame.look.x = 1.0f;
        if (phase == 4) frame.look.y = -0.5f;
        if (phase == 5) frame.look.y = 0.5f;
        if (phase == 6) frame.move = {0.5f, 0.5f};
        if (phase == 7) { frame.move = {-0.5f, 0.5f}; frame.look.x = -0.5f; }
    }
    if (std::getenv("SOTE_TEST_MOUSE_AIM") != nullptr ||
        std::getenv("SOTE_TEST_MODERN_IDLE") != nullptr) {
        // A game-process-only mouse probe: relative counts, with no
        // host cursor movement or desktop input, through the production hook.
        active = true;
        frame = {};
        frame.connected = true;
        if (std::getenv("SOTE_TEST_MOUSE_AIM") != nullptr &&
            !blocked(ram, object) && mouse_probe_frames < 120) {
            if (mouse_probe_frames % 6 == 0) {
                mouse_x += 10;
                if (mouse_probe_frames % 12 == 0) mouse_y -= 2;
            }
            ++mouse_probe_frames;
        }
    }
    ++frame_number;
    if (std::getenv("SOTE_TRACE_MODERN_AIM") && frame_number % 60 == 0) {
        std::printf("[sote][aim-input] active=%d fire_bit=%04X buttons=%08X flags=%08X gate=%.4f weapon=%d\n",
            active, bindings[1].load(), read<uint32_t>(ram,0x801130AC),
            read<uint32_t>(ram,object+0x74),read<float>(ram,0x800DEB78),read<int16_t>(ram,object+0xB8));
    }
    if (!active) { pitch = 0; camera_initialized = false; return; }
    frame.move = shape_stick(frame.move, tuning.movement_deadzone, 1.0f);
    frame.look = shape_stick(frame.look, tuning.aim_deadzone, tuning.aim_curve);
    if (blocked(ram, object)) {
        if ((read<uint32_t>(ram, object + 0x74) & 0x400) != 0) {
            pitch = 0;
            camera_initialized = false;
        }
        if (std::getenv("SOTE_TRACE_MODERN_CONTROLS") && frame_number % 60 == 0)
            std::printf("[sote][modern-blocked] frame=%llu flags=%08X gate1=%.3f gate2=%.3f\n",
                static_cast<unsigned long long>(frame_number),
                read<uint32_t>(ram, object+0x74), read<float>(ram,0x800E0EA0),
                read<float>(ram,0x800DEB78));
        active = false; return;
    }
    last_aiming_ms.store(now, std::memory_order_relaxed);
    if (!camera_initialized) {
        // Activate native camera slot 5/mode 6 through 8006A214. Our 69404
        // hooks adapt its overhead placement to a shoulder view.
        camera_requested = true;
        camera_initialized = true;
    }
    // The guest uses big-endian doubles in word-swapped RDRAM.
    const uint64_t bits = (uint64_t(read<uint32_t>(ram, 0x8018E998)) << 32) |
        read<uint32_t>(ram, 0x8018E99C);
    double dt;
    std::memcpy(&dt, &bits, sizeof(dt));
    pitch = advance_pitch(pitch, frame.look.y * (tuning.invert_y ? 1 : -1),
        tuning.pitch_speed, dt);
    if (std::isfinite(dt) && dt > 0) {
        const float mouse_pitch = std::clamp(
            static_cast<float>(mouse_y) * tuning.mouse_sensitivity,
            -30.0f, 30.0f);
        pitch = std::clamp(
            pitch + mouse_pitch * (tuning.invert_y ? 1.0f : -1.0f),
            -55.0f, 55.0f);
        mouse_yaw_rate = std::clamp(
            static_cast<float>(mouse_x) * tuning.mouse_sensitivity,
            -30.0f, 30.0f) / static_cast<float>(std::max(dt, 1.0 / 240.0));
    }
    if (std::getenv("SOTE_TRACE_MODERN_CONTROLS") && frame_number % 30 == 0) {
        std::printf("[sote][modern] frame=%llu event=%d move=%.4f,%.4f look=%.4f,%.4f yaw_rate=%.3f pitch=%.3f dt=%.6f heading=%.3f\n",
            static_cast<unsigned long long>(frame_number), event,
            frame.move.x, frame.move.y, frame.look.x, frame.look.y,
            -frame.look.x * tuning.yaw_speed, pitch, dt,
            read<float>(ram, object + 0x14));
        if (event == 8) {
            std::printf("[sote][modern-train-output] local_heading=%.3f angular_rate=%.3f camera=%d preset=%d\n",
                read<float>(ram,object+0x90), read<float>(ram,object+0xA8),
                read<int16_t>(ram,0x8018DDE8), preset);
        } else {
            std::printf("[sote][modern-output] weapon_pitch=%.3f body_pitch=%.3f camera=%d preset=%d\n",
                read<float>(ram,object+0x1B4), read<float>(ram,object+0x1C0),
                read<int16_t>(ram,0x8018DDE8), preset);
        }
        std::fflush(stdout);
    }
}

extern "C" void sote_modern_decode(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    if (!active || owner != object) return;
    // 74FA4's decoded directions: forward/back, turn left/right, strafe left/right.
    // Replace before the native stun/death gates, not after them.
    write<int16_t>(ram, stack + 0x17E, frame.move.y > 0);
    write<int16_t>(ram, stack + 0x17C, frame.move.y < 0);
    write<int16_t>(ram, stack + 0x17A, 0);
    write<int16_t>(ram, stack + 0x178, 0);
    write<int16_t>(ram, stack + 0x172, frame.move.x < 0);
    write<int16_t>(ram, stack + 0x170, frame.move.x > 0);
    write<float>(ram, stack + 0x160, std::fabs(frame.move.x));
    write<float>(ram, stack + 0x15C, std::fabs(frame.move.y));
}

extern "C" void sote_modern_train_begin(uint8_t* ram, uint32_t object) {
    sote_modern_begin(ram, object);
    // Train cameras have their own native update; never request the ordinary
    // on-foot camera or use its actor-specific pitch fields here.
    sote::modern_controls::camera_requested = false;
}

extern "C" void sote_modern_train_decode(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    if (!active || owner != object || level_event != 8) return;
    write<int16_t>(ram, stack + 0x1BE, frame.move.y > 0);
    write<int16_t>(ram, stack + 0x1BC, frame.move.y < 0);
    write<int16_t>(ram, stack + 0x1BA, 0);
    write<int16_t>(ram, stack + 0x1B8, 0);
    write<int16_t>(ram, stack + 0x1B2, frame.move.x < 0);
    write<int16_t>(ram, stack + 0x1B0, frame.move.x > 0);
    write<float>(ram, stack + 0x19C, std::fabs(frame.move.y));
    write<float>(ram, stack + 0x1A0, std::fabs(frame.move.x));
}

extern "C" void sote_modern_train_yaw(uint8_t* ram, uint32_t object) {
    using namespace sote::modern_controls;
    if (!active || owner != object || level_event != 8 || blocked(ram, object)) return;
    // The train integrates local angular velocity at +A8 into +90, then
    // combines the result with its carriage rotation. +A4 is linear velocity.
    write<float>(ram, object + 0xA8, -frame.look.x * tuning.yaw_speed - mouse_yaw_rate);
}

extern "C" uint32_t sote_modern_train_pose(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    if (!active || owner != object || level_event != 8 || blocked(ram, object)) return 0;
    // 800A6384 builds the train's camera and weapon transforms from these
    // relative Euler angles. Its +1B4 actor field is a pointer, not pitch.
    // Body heading already supplies yaw; use identical pitch for both poses.
    write<float>(ram, stack + 0xD0, 0);
    write<float>(ram, stack + 0xD4, pitch);
    write<float>(ram, stack + 0xD8, 0);
    write<float>(ram, stack + 0xDC, 0);
    write<float>(ram, stack + 0xE0, pitch);
    write<float>(ram, stack + 0xE4, 0);
    return 1;
}

extern "C" void sote_modern_yaw(uint8_t* ram, uint32_t object) {
    using namespace sote::modern_controls;
    if (!active || owner != object || blocked(ram, object)) return;
    // Native ground/air integration consumes degrees/sec at object+A4, then
    // wraps heading and rebuilds its own transforms. No per-poll acceleration.
    write<float>(ram, object + 0xA4,
        -frame.look.x * tuning.yaw_speed - mouse_yaw_rate);
}

extern "C" uint32_t sote_modern_aim(uint8_t* ram, uint32_t stack) {
    using namespace sote::modern_controls;
    if (!active) return 0;
    // Native weapon-aim Euler pitch in degrees, after its stick deadzone and
    // minimum-angle cutoff. Heading is supplied by the body turn path.
    write<float>(ram, stack + 0x108, pitch);
    write<float>(ram, stack + 0x104, 0);
    write<float>(ram, stack + 0xF8, 0);
    write<float>(ram, stack + 0xFC, pitch);
    return 1;
}

extern "C" uint32_t sote_modern_take_camera_request() {
    using namespace sote::modern_controls;
    const bool requested = camera_requested;
    camera_requested = false;
    return requested;
}

extern "C" uint32_t sote_modern_camera_active() {
    return sote::modern_controls::aiming_active();
}

extern "C" void sote_modern_camera_offset(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    const uint32_t cameras = read<uint32_t>(ram, 0x800D08A0);
    if (cameras < 0x80000000 || cameras > 0x807FF000) return;
    if (!active || owner != object) {
        if (camera_offsets_owned) {
            for (uint32_t axis=0; axis<3; ++axis)
                write<float>(ram, cameras + 6*128 + 0x6C + axis*4, 0);
            camera_offsets_owned = false;
        }
        return;
    }
    camera_offsets_owned = true;
    for (uint32_t axis=0; axis<3; ++axis) {
        // Offset the eye over the right shoulder before native wall collision.
        const float right = read<float>(ram, object + 0x108 + axis*4);
        write<float>(ram, stack + 0x8C + axis*4,
            read<float>(ram, stack + 0x8C + axis*4) + right*1.2f);
        // Look ahead along the weapon direction instead of at Dash's body.
        write<float>(ram, cameras + 6*128 + 0x6C + axis*4,
            read<float>(ram, object + 0x118 + axis*4)*20.0f);
    }
}
