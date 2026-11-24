#include "modern_controls.hpp"
#ifndef SOTE_XBOX
#include "texture_proof_camera.hpp"
#endif
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
std::atomic<bool> recenter_requested{false};
// Quick turn: a 180-degree spin spread over quick_turn_seconds so it reads
// as a fast turn rather than a cut. quick_turn_yaw_rate is added to the
// native yaw rate (degrees/second) while degrees remain.
constexpr float quick_turn_degrees = 180.0f;
constexpr float quick_turn_seconds = 0.4f;
std::atomic<bool> quick_turn_requested{false};
float quick_turn_remaining = 0.0f;
float quick_turn_yaw_rate = 0.0f;
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
bool dead(uint8_t* ram, uint32_t object) {
    if (level_event == 8)
        return (read<uint32_t>(ram, object + 0x74) & 0x400) != 0;
    // Ordinary on-foot bit 0x400 is also set while submerged. The sewer
    // controller keeps health at +B4; use that instead of disabling live
    // swimming input. The train has a separate actor layout.
    return read<float>(ram, object + 0xB4) <= 0;
}
bool blocked(uint8_t* ram, uint32_t object) {
    if (level_event == 8) {
        return read<float>(ram, 0x800E539C) > 0 ||
            read<uint32_t>(ram, 0x800E5838) != 0 ||
            read<float>(ram, 0x800E57B8) > 0 ||
            dead(ram, object);
    }
    // Native input suppression immediately following 800758A8, plus death.
    return read<float>(ram, 0x800E0EA0) > 0 ||
        read<float>(ram, 0x800DEB78) > 0 ||
        dead(ram, object);
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
Stick consume_mouse_stick(float sensitivity, bool invert_y, double seconds) {
    const int x = mouse_delta_x.exchange(0, std::memory_order_relaxed);
    const int y = mouse_delta_y.exchange(0, std::memory_order_relaxed);
    if (!std::isfinite(seconds) || seconds <= 0 || !std::isfinite(sensitivity)) return {};
    // Native vehicles steer through the analog stick. Convert relative counts
    // to a rate, without the gamepad response curve or a persistent held turn.
    const float scale = std::clamp(sensitivity, 0.01f, 1.0f) /
        (180.0f * static_cast<float>(std::clamp(seconds, 1.0/240.0, 0.1)));
    return {std::clamp(x * scale, -1.0f, 1.0f),
        std::clamp(y * scale * (invert_y ? 1.0f : -1.0f), -1.0f, 1.0f)};
}
bool on_foot_active() {
    const auto age = milliseconds() - last_on_foot_ms.load();
    return age >= 0 && age < 150;
}
bool aiming_active() { return active; }
bool reticle_visible() {
    if (std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") &&
        std::getenv("SOTE_TEXTURE_PROOF_CAMERA")) return false;
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
void request_recenter() { recenter_requested.store(true); }
void request_quick_turn() { quick_turn_requested.store(true); }
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
        quick_turn_remaining = quick_turn_yaw_rate = 0;
        quick_turn_requested = false; recenter_requested = false;
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
    static int64_t next_mouse_report = 0;
    if ((mouse_x || mouse_y) && now >= next_mouse_report) {
        std::printf("[sote][mouse-aim] delta=%d,%d connected=%d modern=%d blocked=%d sensitivity=%.3f\n",
            mouse_x, mouse_y, frame.connected, active, blocked(ram, object), tuning.mouse_sensitivity);
        std::fflush(stdout);
        next_mouse_report = now + 5000;
    }
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
    if (!active) {
        quick_turn_remaining = quick_turn_yaw_rate = 0;
        quick_turn_requested = false; recenter_requested = false;
        // Menus, movies and focus loss temporarily disconnect input. Preserve
        // the player's selected camera and pitch across those interruptions;
        // only leaving Modern mode or resetting the actor needs a new camera.
        if (sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot) !=
            sote::controls_menu::ControlScheme::Modern) {
            pitch = 0;
            camera_initialized = false;
            camera_requested = false;
        }
        return;
    }
    frame.move = shape_stick(frame.move, tuning.movement_deadzone, 1.0f);
    frame.look = shape_stick(frame.look, tuning.aim_deadzone, tuning.aim_curve);
    if (blocked(ram, object)) {
        quick_turn_remaining = quick_turn_yaw_rate = 0;
        quick_turn_requested = false; recenter_requested = false;
        if (dead(ram, object)) {
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
    const bool proof_camera_reselect = std::getenv("SOTE_TEXTURE_PROOF_CAMERA") &&
        std::getenv("SOTE_DIAGNOSTIC_OFFSCREEN") &&
        read<int16_t>(ram,0x8018DDE8) != 5;
    if (!camera_initialized || proof_camera_reselect) {
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
    if (recenter_requested.exchange(false)) pitch = 0;
    if (quick_turn_requested.exchange(false) && quick_turn_remaining <= 0.0f) {
        quick_turn_remaining = quick_turn_degrees;
    }
    quick_turn_yaw_rate = 0.0f;
    if (quick_turn_remaining > 0.0f && std::isfinite(dt) && dt > 0.0) {
        const float rate = quick_turn_degrees / quick_turn_seconds;
        const float step = std::min(rate * static_cast<float>(dt), quick_turn_remaining);
        quick_turn_yaw_rate = step / static_cast<float>(dt);
        quick_turn_remaining -= step;
    }
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
    if (std::getenv("SOTE_TRACE_MODERN_CONTROLS") &&
        (frame_number % 30 == 0 || std::getenv("SOTE_TRACE_FRAME_COMMANDS"))) {
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

extern "C" void sote_modern_move_magnitudes(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    if (!active || owner != object) return;
    // The retail decoder boosts forward by 1.25 and snaps shallow strafing
    // to 0.3. Keep its full running speed across the stick circle. Read its
    // direction gates back so stun, death and scripted input locks still win.
    constexpr float run_scale = 1.25f;
    if (read<int16_t>(ram, stack + 0x172) || read<int16_t>(ram, stack + 0x170))
        write<float>(ram, stack + 0x160, std::fabs(frame.move.x) * run_scale);
    if (read<int16_t>(ram, stack + 0x17E) || read<int16_t>(ram, stack + 0x17C))
        write<float>(ram, stack + 0x15C, std::fabs(frame.move.y) * run_scale);
}

// The retail drag calculation multiplies by another 0.9 when there is no
// forward/back input. Keep that braking behavior for a released stick.
extern "C" uint32_t sote_modern_strafing(uint8_t* ram, uint32_t object, uint32_t stack) {
    using namespace sote::modern_controls;
    return active && owner == object && frame.move.x != 0.0f &&
        (read<int16_t>(ram, stack + 0x172) || read<int16_t>(ram, stack + 0x170));
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
    write<float>(ram, object + 0xA8,
        -frame.look.x * tuning.yaw_speed - mouse_yaw_rate + quick_turn_yaw_rate);
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
        -frame.look.x * tuning.yaw_speed - mouse_yaw_rate + quick_turn_yaw_rate);
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
#ifndef SOTE_XBOX
    if (sote::texture_proof::apply(ram,object,stack)) {
        // 80069454 adds Modern's 1.2-unit shoulder lift immediately after
        // this hook; cancel it for the explicitly positioned proof camera.
        write<float>(ram,stack+0x94,read<float>(ram,stack+0x94)-1.2f);
    }
#endif
}

// Flight free look is applied only to the finished camera transform. Never
// rotate the vehicle, its weapons, or the native controller's input vectors.
namespace {
struct FlightView {
    uint32_t object=0, camera=0;
    int event=-1, mode=-1;
    float yaw=0, pitch=0;
    float original[16]{}, applied[16]{};
    bool saved=false;
} flight_view;
struct FlightVec { float x,y,z; };
FlightVec operator+(FlightVec a,FlightVec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
FlightVec operator-(FlightVec a,FlightVec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
FlightVec operator*(FlightVec a,float b){return {a.x*b,a.y*b,a.z*b};}
float flight_dot(FlightVec a,FlightVec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
bool flight_address(uint32_t a){return a>=0x80000000U && a<=0x807FFE00U;}
}
extern "C" void sote_flight_camera_begin(uint8_t* ram) {
    using namespace sote::modern_controls;
    // Restore only our own previous output; native transitions may already
    // have replaced this slot. No offsets feed back into the next simulation.
    auto& v=flight_view;
    if(v.saved && flight_address(v.camera)) {
        auto* matrix=ram+(v.camera&0x7FFFFF)+0x14;
        if(std::memcmp(matrix,v.applied,sizeof(v.applied))==0)
            std::memcpy(matrix,v.original,sizeof(v.original));
    }
    v.saved=false;
}
extern "C" void sote_flight_camera_owner(uint8_t* ram,uint32_t object,int context) {
    using namespace sote::modern_controls;
    int event=read<int16_t>(ram,0x8013CE0E);
    if(!((event==3 && context==1)||(event==30 && context==4))) return;
    // Native Hoth input is decoded only for the zero-index player vehicle.
    if(!flight_address(object) || (event==3 && read<int16_t>(ram,object+4)!=0)) return;
    if(flight_view.object!=object || flight_view.event!=event) {
        sote_flight_camera_begin(ram);
        flight_view={};flight_view.object=object;flight_view.event=event;
    }
}
extern "C" void sote_flight_camera_apply(uint8_t* ram) {
    using namespace sote::modern_controls;
    auto& v=flight_view;
    const int event=read<int16_t>(ram,0x8013CE0E);
    const int mode=read<int32_t>(ram,0x800D08A8);
    Input input;
    {std::lock_guard lock{input_mutex};input=latest;}
    if (event != v.event) {
        v = {};
        return;
    }
    if((event!=3 && event!=30) || !flight_address(v.object) ||
        sote::controls_menu::current_scheme(sote::controls_menu::SchemeSlot::OnFoot)!=
            sote::controls_menu::ControlScheme::Modern || !input.connected ||
        (mode!=4 && mode!=6)) {
        v.yaw=v.pitch=0;v.mode=-1;return;
    }
    if(v.mode!=mode){v.yaw=v.pitch=0;v.mode=mode;}
    // Pausing does not accumulate stick motion or rotate the frozen view.
    if(!read<int32_t>(ram,0x800DD2BC)) {
        const auto tune=sote::controls_menu::modern_controls_tuning().on_foot;
        const Stick look=shape_stick(input.look,tune.aim_deadzone,tune.aim_curve);
        uint64_t bits=(uint64_t(read<uint32_t>(ram,0x8018E998))<<32)|read<uint32_t>(ram,0x8018E99C);
        double dt;std::memcpy(&dt,&bits,sizeof(dt));
        if(std::isfinite(dt)&&dt>0) {
            dt=std::min(dt,0.1);
            v.yaw=std::remainder(v.yaw+look.x*tune.yaw_speed*float(dt),360.0f);
            v.pitch=std::clamp(v.pitch-look.y*(tune.invert_y?-1.f:1.f)*tune.pitch_speed*float(dt),-70.f,70.f);
        }
    }
    // Preserve the native chase transform exactly at rest. Projecting the
    // eye through a rounded camera basis can move it even at zero rotation.
    if (v.yaw == 0.0f && v.pitch == 0.0f) return;
    const uint32_t camera=read<uint32_t>(ram,0x800D08A4);
    if(!flight_address(camera))return;
    auto* matrix=ram+(camera&0x7FFFFF)+0x14;
    std::memcpy(v.original,matrix,sizeof(v.original));
    FlightVec r{v.original[0],v.original[1],v.original[2]};
    FlightVec f{v.original[4],v.original[5],v.original[6]};
    FlightVec u{v.original[8],v.original[9],v.original[10]};
    for(float value:v.original)if(!std::isfinite(value))return;
    constexpr float radians=3.14159265358979323846f/180.f;
    const float cy=std::cos(v.yaw*radians),sy=std::sin(v.yaw*radians);
    const float cp=std::cos(v.pitch*radians),sp=std::sin(v.pitch*radians);
    const FlightVec nr=r*cy-f*sy, nf=(f*cy+r*sy)*cp+u*sp, nu=u*cp-(f*cy+r*sy)*sp;
    auto rotate=[&](FlightVec a){return nr*flight_dot(a,r)+nf*flight_dot(a,f)+nu*flight_dot(a,u);};
    std::memcpy(v.applied,v.original,sizeof(v.applied));
    auto set=[&](int index,FlightVec a){v.applied[index]=a.x;v.applied[index+1]=a.y;v.applied[index+2]=a.z;};
    set(0,nr);set(4,nf);set(8,nu);
    if(mode==6) {
        FlightVec pivot=read<FlightVec>(ram,v.object+0x50);
        FlightVec eye{v.original[12],v.original[13],v.original[14]};
        set(12,pivot+rotate(eye-pivot));
    }
    std::memcpy(matrix,v.applied,sizeof(v.applied));v.camera=camera;v.saved=true;
    if(std::getenv("SOTE_TRACE_FLIGHT_LOOK")) {
        std::printf("[sote][flight-look] event=%d mode=%d yaw=%.3f pitch=%.3f input=%.3f,%.3f forward=%.4f,%.4f,%.4f\n",
            event,mode,v.yaw,v.pitch,input.look.x,input.look.y,nf.x,nf.y,nf.z);
    }
}
