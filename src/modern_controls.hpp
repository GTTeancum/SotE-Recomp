#pragma once

#include <cstdint>

namespace sote::modern_controls {
struct Stick { float x = 0, y = 0; };
struct Input {
    Stick move, look;
    bool connected = false;
};
struct Tuning {
    float movement_deadzone = 0.12f;
    float aim_deadzone = 0.12f;
    float aim_curve = 1.7f;
    float yaw_speed = 180.0f;
    float pitch_speed = 90.0f;
    bool invert_y = false;
    bool convergence = true;
    float convergence_range = 1500.0f;
    float magnetism_strength = 0.0f;
    float magnetism_cone_degrees = 2.0f;
    float magnetism_range = 150.0f;
};

// Radial inner deadzone, rescaled continuously to the circular outer edge.
Stick shape_stick(Stick raw, float deadzone, float exponent);
float advance_pitch(float pitch, float input, float speed, double seconds);
void publish(Input input);
bool on_foot_active();
uint16_t map_buttons(uint16_t canonical);
bool aiming_active();
float pitch_degrees();
bool owns_player(uint32_t object);
}

extern "C" {
void sote_modern_begin(uint8_t* rdram, uint32_t object);
void sote_modern_decode(uint8_t* rdram, uint32_t object, uint32_t stack);
void sote_modern_yaw(uint8_t* rdram, uint32_t object);
}
