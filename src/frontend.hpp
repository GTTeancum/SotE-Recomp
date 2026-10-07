#pragma once

#include <cstddef>
#include <cstdint>

#include "ultramodern/input.hpp"

namespace sote::frontend {

bool initialize();
void shutdown();
void set_audio_enabled(bool enabled);
void set_movie_playback(uint64_t token);
void set_movie_guest_advance(bool pressed);
void queue_movie_audio(const int16_t* samples, size_t sample_count);
bool movie_skip_pressed(uint64_t token, int vi);

void poll_input();
// Called only from the game window's WM_INPUT handler.
void add_mouse_delta(int x, int y);
bool get_input(int controller, uint16_t* buttons, float* x, float* y);
void set_physical_input_enabled(bool enabled);
void set_scripted_input(int vi, uint16_t buttons, int8_t x, int8_t y);
ultramodern::input::connected_device_info_t get_connected_device_info(
    int controller);
void set_rumble(int controller, bool enabled);

void queue_samples(int16_t* samples, size_t sample_count);
size_t get_frames_remaining();
void set_frequency(uint32_t frequency);

} // namespace sote::frontend
