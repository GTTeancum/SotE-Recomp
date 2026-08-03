#pragma once

#include <cstddef>
#include <cstdint>

#include "ultramodern/input.hpp"

namespace sote::frontend {

bool initialize();
void shutdown();
void set_audio_enabled(bool enabled);

void poll_input();
bool get_input(int controller, uint16_t* buttons, float* x, float* y);
void set_physical_input_enabled(bool enabled);
void set_scripted_input(uint16_t buttons, int8_t x, int8_t y);
ultramodern::input::connected_device_info_t get_connected_device_info(
    int controller);
void set_rumble(int controller, bool enabled);

void queue_samples(int16_t* samples, size_t sample_count);
size_t get_frames_remaining();
void set_frequency(uint32_t frequency);

} // namespace sote::frontend
