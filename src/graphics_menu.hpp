#pragma once

#include <cstdint>
#include <filesystem>

namespace sote::graphics_menu {

enum class ResolutionPreset : int {
    Native,
    Scale2x,
    Scale4x,
    Scale8x,
    WindowScale,
};

enum class AntialiasingPreset : int {
    Off,
    MSAA2x,
    MSAA4x,
    MSAA8x,
};

struct Settings {
    int output_width = 1280;
    int output_height = 720;
    ResolutionPreset resolution = ResolutionPreset::WindowScale;
    bool widescreen = true;
    AntialiasingPreset antialiasing = AntialiasingPreset::MSAA4x;
    bool borderless = false;
};

void initialize(const std::filesystem::path& data_directory);
void sync_display_resolution(int width, int height);
void sync_from_renderer(const Settings& settings);
void sync_window_mode(bool borderless);
bool take_renderer_request(Settings& settings);
bool take_window_request(Settings& settings);
void filter_input(uint16_t* buttons, float* x, float* y);

} // namespace sote::graphics_menu

extern "C" void sote_update_graphics_menu(uint8_t* rdram);
